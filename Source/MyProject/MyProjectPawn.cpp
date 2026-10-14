// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyProjectPawn.h"
#include "MyProjectWheelFront.h"
#include "MyProjectWheelRear.h"
#include "EngineSoundComponent.h"
#include "EngineSoundProfile.h"
#include "CollisionFxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleManagerAsyncCallback.h"
#include "ChaosVehicleWheel.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "MyProject.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "VehiclePawn"

namespace
{
	/** Shows or hides the glass of a swapped-in body. The showcase cars' windshields read as milky white
	 *  from the inside, so the glass is taken out while looking from the driver's seat. */
	void SetBodyGlassVisible(AActor* Body, bool bVisible)
	{
		if (!Body)
		{
			return;
		}

		TInlineComponentArray<UPrimitiveComponent*> Primitives(Body);

		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (Primitive->GetName().Contains(TEXT("Glass")))
			{
				Primitive->SetVisibility(bVisible);
			}
		}
	}

	/** Counts the wheels touching the ground. The vehicle movement component doesn't implement IsMovingOnGround,
	 *  which always says the car is in the air, so this reads the wheel contacts of the physics vehicle instead */
	int32 CountWheelsOnGround(UChaosWheeledVehicleMovementComponent* Movement)
	{
		const TUniquePtr<FPhysicsVehicleOutput>& PhysicsOutput = Movement->PhysicsVehicleOutput();

		if (!PhysicsOutput.IsValid())
		{
			return 0;
		}

		int32 WheelsOnGround = 0;

		for (const FWheelsOutput& Wheel : PhysicsOutput->Wheels)
		{
			WheelsOnGround += Wheel.InContact ? 1 : 0;
		}

		return WheelsOnGround;
	}
}

AMyProjectPawn::AMyProjectPawn()
{
	// construct the front camera boom
	FrontSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Front Spring Arm"));
	FrontSpringArm->SetupAttachment(GetMesh());
	FrontSpringArm->TargetArmLength = 0.0f;
	FrontSpringArm->bDoCollisionTest = false;
	FrontSpringArm->bEnableCameraRotationLag = true;
	FrontSpringArm->CameraRotationLagSpeed = 15.0f;
	FrontSpringArm->SetRelativeLocation(FVector(30.0f, 0.0f, 120.0f));

	FrontCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Front Camera"));
	FrontCamera->SetupAttachment(FrontSpringArm);
	FrontCamera->bAutoActivate = false;

	// construct the back camera boom
	BackSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Back Spring Arm"));
	BackSpringArm->SetupAttachment(GetMesh());
	BackSpringArm->TargetArmLength = 650.0f;
	BackSpringArm->SocketOffset.Z = 150.0f;
	BackSpringArm->bDoCollisionTest = false;
	BackSpringArm->bInheritPitch = false;
	BackSpringArm->bInheritRoll = false;
	BackSpringArm->bEnableCameraRotationLag = true;
	BackSpringArm->CameraRotationLagSpeed = 2.0f;
	BackSpringArm->CameraLagMaxDistance = 50.0f;

	BackCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Back Camera"));
	BackCamera->SetupAttachment(BackSpringArm);

	// the driver's seat camera rides rigidly with the chassis. It's moved behind the steering wheel
	// of whichever body the car is wearing, see PlaceCockpitCamera
	CockpitCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Cockpit Camera"));
	CockpitCamera->SetupAttachment(GetMesh());
	CockpitCamera->SetRelativeLocation(DefaultCockpitLocation);
	CockpitCamera->bAutoActivate = false;

	// the engine sound rides on the chassis
	EngineSound = CreateDefaultSubobject<UEngineSoundComponent>(TEXT("Engine Sound"));
	EngineSound->SetupAttachment(GetMesh());

	CollisionFx = CreateDefaultSubobject<UCollisionFxComponent>(TEXT("Collision Fx"));

	// Configure the car mesh
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCollisionProfileName(FName("Vehicle"));

	// we need hit events off the chassis so we can absorb collisions ourselves
	GetMesh()->SetNotifyRigidBodyCollision(true);

	// sit the mass low in the chassis so the car leans and flips less
	GetMesh()->BodyInstance.COMNudge = FVector(0.0f, 0.0f, -15.0f);

	// get the Chaos Wheeled movement component
	ChaosVehicleMovement = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());

}

void AMyProjectPawn::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// steering 
		EnhancedInputComponent->BindAction(SteeringAction, ETriggerEvent::Triggered, this, &AMyProjectPawn::Steering);
		EnhancedInputComponent->BindAction(SteeringAction, ETriggerEvent::Completed, this, &AMyProjectPawn::Steering);

		// throttle 
		EnhancedInputComponent->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &AMyProjectPawn::Throttle);
		EnhancedInputComponent->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &AMyProjectPawn::Throttle);

		// break 
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &AMyProjectPawn::Brake);
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Started, this, &AMyProjectPawn::StartBrake);
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Completed, this, &AMyProjectPawn::StopBrake);

		// handbrake
		EnhancedInputComponent->BindAction(HandbrakeAction, ETriggerEvent::Started, this, &AMyProjectPawn::StartHandbrake);
		EnhancedInputComponent->BindAction(HandbrakeAction, ETriggerEvent::Completed, this, &AMyProjectPawn::StopHandbrake);

		// manual gear shifting
		EnhancedInputComponent->BindAction(GearUpAction, ETriggerEvent::Started, this, &AMyProjectPawn::GearUp);
		EnhancedInputComponent->BindAction(GearDownAction, ETriggerEvent::Started, this, &AMyProjectPawn::GearDown);

		// look around 
		EnhancedInputComponent->BindAction(LookAroundAction, ETriggerEvent::Triggered, this, &AMyProjectPawn::LookAround);

		// toggle camera 
		EnhancedInputComponent->BindAction(ToggleCameraAction, ETriggerEvent::Triggered, this, &AMyProjectPawn::ToggleCamera);

		// reset the vehicle 
		EnhancedInputComponent->BindAction(ResetVehicleAction, ETriggerEvent::Triggered, this, &AMyProjectPawn::ResetVehicle);
	}
	else
	{
		UE_LOG(LogMyProject, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AMyProjectPawn::BeginPlay()
{
	Super::BeginPlay();

	// manual gears unless told otherwise: the player's controller picks AT or MT, and NPC drivers shift automatically
	ChaosVehicleMovement->SetUseAutomaticGears(bUseAutomaticGears);

	// listen for chassis collisions so we can soak up the impulse
	BaseLinearDamping = GetMesh()->GetLinearDamping();
	GetMesh()->OnComponentHit.AddDynamic(this, &AMyProjectPawn::OnChassisHit);

	// the bodies of the car take their hit events setting from its physics asset, which has them off, so they're
	// turned on here once the bodies exist. Setting it on the mesh in the constructor alone never reaches them
	GetMesh()->SetNotifyRigidBodyCollision(true);

	// set up the flipped check timer
	GetWorld()->GetTimerManager().SetTimer(FlipCheckTimer, this, &AMyProjectPawn::FlippedCheck, FlipCheckTime, true);

	PlaceCockpitCamera();

	// start the engine, unless a race already picked one
	if (!EngineSound->GetProfile())
	{
		SetEngineSound(EngineSoundProfile);
	}
}

void AMyProjectPawn::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	// clear the flipped check timer
	GetWorld()->GetTimerManager().ClearTimer(FlipCheckTimer);

	GetMesh()->OnComponentHit.RemoveDynamic(this, &AMyProjectPawn::OnChassisHit);

	// the swapped-in body is a separate actor, so it has to go with us
	DestroyBody();

	Super::EndPlay(EndPlayReason);
}

void AMyProjectPawn::Tick(float Delta)
{
	Super::Tick(Delta);

	// count down the extra damping we apply in the moments after a collision
	ImpactRecoveryRemaining = FMath::Max(ImpactRecoveryRemaining - Delta, 0.0f);
	const bool bRecoveringFromImpact = ImpactRecoveryRemaining > 0.0f;

	// add some angular damping if the vehicle is in midair, and more still while it settles after a hit
	const bool bMovingOnGround = bGroundedTurnDamping ? CountWheelsOnGround(ChaosVehicleMovement) > 0 : ChaosVehicleMovement->IsMovingOnGround();
	const float BaseAngularDamping = bMovingOnGround ? GroundAngularDamping : AirborneAngularDamping;

	GetMesh()->SetAngularDamping(BaseAngularDamping + (bRecoveringFromImpact ? ImpactAngularDamping : 0.0f));
	GetMesh()->SetLinearDamping(BaseLinearDamping + (bRecoveringFromImpact ? ImpactLinearDamping : 0.0f));

	// never let the chassis tumble faster than this, however hard it was hit
	if (bStabilizeImpacts)
	{
		ClampChassisRotationRate(MaxTumbleRate, 0.0f);
	}

	// realign the camera yaw to face front
	float CameraYaw = BackSpringArm->GetRelativeRotation().Yaw;
	CameraYaw = FMath::FInterpTo(CameraYaw, 0.0f, Delta, 1.0f);

	// swing the camera up while the car points downhill so it looks down on the road dropping away ahead. The arm
	// doesn't inherit pitch, so this is its pitch in the world, and the arm's rotation lag smooths it out
	const float CarPitch = GetMesh()->GetComponentRotation().Pitch;
	const float DownhillAngle = FMath::Max(-CarPitch, 0.0f);
	float CameraPitch = -FMath::Min(DownhillAngle * DownhillCameraPitchScale, MaxDownhillCameraPitch);

	// on half of a split screen the view is too short to keep a climb in it from a level camera, so the camera tips
	// up with the car as well
	const APlayerController* PC = Cast<APlayerController>(GetController());
	const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;

	if (LocalPlayer && LocalPlayer->Size.Y < 0.99f)
	{
		const float UphillAngle = FMath::Max(CarPitch, 0.0f);
		CameraPitch += FMath::Min(UphillAngle * SplitscreenUphillCameraPitchScale, MaxSplitscreenUphillCameraPitch);
	}

	BackSpringArm->SetRelativeRotation(FRotator(CameraPitch, CameraYaw, 0.0f));

	UpdateDrift(Delta);

	UpdateAirborne(Delta);

	UpdateRaceBoost();

	// the pawn ticks before physics, so this is our speed going into any collision resolved this frame
	PreImpactVelocity = GetMesh()->GetPhysicsLinearVelocity();

	UpdateBodyWheels();

	UpdateEngineSound();

	UpdateAutoDownshift();
}

void AMyProjectPawn::Steering(const FInputActionValue& Value)
{
	// route the input
	DoSteering(Value.Get<float>());
}

void AMyProjectPawn::Throttle(const FInputActionValue& Value)
{
	// route the input
	DoThrottle(Value.Get<float>());
}

void AMyProjectPawn::Brake(const FInputActionValue& Value)
{
	// route the input
	DoBrake(Value.Get<float>());
}

void AMyProjectPawn::StartBrake(const FInputActionValue& Value)
{
	// route the input
	DoBrakeStart();
}

void AMyProjectPawn::StopBrake(const FInputActionValue& Value)
{
	// route the input
	DoBrakeStop();
}

void AMyProjectPawn::StartHandbrake(const FInputActionValue& Value)
{
	// route the input
	DoHandbrakeStart();
}

void AMyProjectPawn::StopHandbrake(const FInputActionValue& Value)
{
	// route the input
	DoHandbrakeStop();
}

void AMyProjectPawn::GearUp(const FInputActionValue& Value)
{
	// route the input
	DoGearUp();
}

void AMyProjectPawn::GearDown(const FInputActionValue& Value)
{
	// route the input
	DoGearDown();
}

void AMyProjectPawn::LookAround(const FInputActionValue& Value)
{
	// route the input
	DoLookAround(Value.Get<float>());
}

void AMyProjectPawn::ToggleCamera(const FInputActionValue& Value)
{
	// route the input
	DoToggleCamera();
}

void AMyProjectPawn::ResetVehicle(const FInputActionValue& Value)
{
	// route the input
	DoResetVehicle();
}

void AMyProjectPawn::DoSteering(float SteeringValue)
{
	// add the input
	ChaosVehicleMovement->SetSteeringInput(SteeringValue);

	SteeringInput = SteeringValue;
}

void AMyProjectPawn::DoThrottle(float ThrottleValue)
{
	// add the input
	ChaosVehicleMovement->SetThrottleInput(ThrottleValue);

	// reset the brake input
	ChaosVehicleMovement->SetBrakeInput(0.0f);

	ThrottleInput = ThrottleValue;
	BrakeInput = 0.0f;
}

void AMyProjectPawn::DoBrake(float BrakeValue)
{
	// add the input
	ChaosVehicleMovement->SetBrakeInput(BrakeValue);

	// reset the throttle input
	ChaosVehicleMovement->SetThrottleInput(0.0f);

	BrakeInput = BrakeValue;
	ThrottleInput = 0.0f;
}

void AMyProjectPawn::DoBrakeStart()
{
	// call the Blueprint hook for the brake lights
	BrakeLights(true);
}

void AMyProjectPawn::DoBrakeStop()
{
	// call the Blueprint hook for the brake lights
	BrakeLights(false);

	// reset brake input to zero
	ChaosVehicleMovement->SetBrakeInput(0.0f);

	BrakeInput = 0.0f;
}

void AMyProjectPawn::DoHandbrakeStart()
{
	// call the Blueprint hook for the break lights
	BrakeLights(true);

	// give the driver a moment to steer, so handbrake-then-steer throws the car sideways as well as steer-then-handbrake
	DriftEntryTimeLeft = DriftEntryWindow;

	// a drift keeps its speed, so it doesn't lock the rear wheels
	if (bDrifting || TryStartDrift())
	{
		return;
	}

	// add the input
	ChaosVehicleMovement->SetHandbrakeInput(true);
}

void AMyProjectPawn::DoHandbrakeStop()
{
	// add the input
	ChaosVehicleMovement->SetHandbrakeInput(false);

	// call the Blueprint hook for the break lights
	BrakeLights(false);
}

void AMyProjectPawn::DoGearUp()
{
	// an automatic gearbox would only shift straight back
	if (bUseAutomaticGears)
	{
		return;
	}

	// shift up one gear from whichever gear we're currently targeting, clamped to the highest forward gear
	const int32 MaxGear = ChaosVehicleMovement->TransmissionSetup.ForwardGearRatios.Num();
	const int32 NewGear = FMath::Min(ChaosVehicleMovement->GetTargetGear() + 1, MaxGear);

	ChaosVehicleMovement->SetTargetGear(NewGear, true);
}

void AMyProjectPawn::DoGearDown()
{
	if (bUseAutomaticGears)
	{
		return;
	}

	// shift down one gear from whichever gear we're currently targeting, clamped to the lowest reverse gear
	const int32 MinGear = -ChaosVehicleMovement->TransmissionSetup.ReverseGearRatios.Num();
	const int32 NewGear = FMath::Max(ChaosVehicleMovement->GetTargetGear() - 1, MinGear);

	ChaosVehicleMovement->SetTargetGear(NewGear, true);
}

void AMyProjectPawn::DoLookAround(float YawDelta)
{
	// rotate the spring arm around the car's up axis, so it turns cleanly even while tipped down on a descent
	BackSpringArm->AddRelativeRotation(FRotator(0.0f, YawDelta, 0.0f));
}

void AMyProjectPawn::DoToggleCamera()
{
	// cycle behind the car -> on the bonnet -> driver's seat
	CameraView = static_cast<ECameraView>((static_cast<uint8>(CameraView) + 1) % static_cast<uint8>(ECameraView::Count));

	// only one camera may be active: the view uses the first active one it finds
	BackCamera->SetActive(CameraView == ECameraView::Back);
	FrontCamera->SetActive(CameraView == ECameraView::Front);
	CockpitCamera->SetActive(CameraView == ECameraView::Cockpit);

	SetBodyGlassVisible(BodyActor, CameraView != ECameraView::Cockpit);
}

void AMyProjectPawn::PlaceCockpitCamera()
{
	FVector EyeLocation = DefaultCockpitLocation;

	// sit behind the swapped-in body's steering wheel, so each car gets its own driving position
	if (BodyActor && BodySteeringWheel.IsValid())
	{
		EyeLocation = BodyActor->GetRootComponent()->GetRelativeLocation() + BodySteeringWheel->GetRelativeLocation() + CockpitEyeOffset;
	}

	// tip the view down a touch so the dashboard and steering wheel are in shot
	CockpitCamera->SetRelativeLocationAndRotation(EyeLocation, FRotator(-4.0f, 0.0f, 0.0f));
	CockpitCamera->SetFieldOfView(CockpitFieldOfView);
}

void AMyProjectPawn::DoResetVehicle()
{
	FTransform ResetTransform;

	if (!FindResetTransform.IsBound() || !FindResetTransform.Execute(ResetTransform))
	{
		// reset to a location slightly above our current one
		const FVector ResetLocation = GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);

		// reset to our yaw. Ignore pitch and roll
		FRotator ResetRotation = GetActorRotation();
		ResetRotation.Pitch = 0.0f;
		ResetRotation.Roll = 0.0f;

		ResetTransform = FTransform(ResetRotation, ResetLocation, FVector::OneVector);
	}

	// teleport the actor to the reset spot and reset physics
	SetActorTransform(ResetTransform, false, nullptr, ETeleportType::TeleportPhysics);

	GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);

	PreImpactVelocity = FVector::ZeroVector;
	ImpactRecoveryRemaining = 0.0f;

	StopDrift();
}

void AMyProjectPawn::ResetDrivingState()
{
	DoSteering(0.0f);
	DoThrottle(0.0f);
	DoBrakeStop();
	DoHandbrakeStop();

	// a car on the grid sits in neutral; the throttle puts it in first
	ChaosVehicleMovement->SetTargetGear(0, true);

	StopDrift();
	DriftEntryTimeLeft = 0.0f;

	PreImpactVelocity = FVector::ZeroVector;
	ImpactRecoveryRemaining = 0.0f;
	bPreviousFlipCheck = false;
}

void AMyProjectPawn::SetBodyClass(TSubclassOf<AActor> NewBodyClass)
{
	// already wearing this body
	if (NewBodyClass == BodyClass && (BodyActor || !NewBodyClass))
	{
		return;
	}

	DestroyBody();

	if (!NewBodyClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	BodyActor = GetWorld()->SpawnActor<AActor>(NewBodyClass, GetMesh()->GetComponentTransform(), SpawnParams);

	if (!BodyActor)
	{
		UE_LOG(LogMyProject, Warning, TEXT("[Body] %s: could not spawn body %s"), *GetName(), *NewBodyClass->GetName());
		return;
	}

	BodyClass = NewBodyClass;

	// the body rides along with the chassis, so every part of it has to be allowed to move
	TInlineComponentArray<USceneComponent*> BodyComponents(BodyActor.Get());

	for (USceneComponent* Component : BodyComponents)
	{
		Component->SetMobility(EComponentMobility::Movable);

		// the showcase cars come with their headlights on, which just leaves a noisy patch on the road in daylight,
		// so they're only lit when asked for
		if (Component->IsA<ULightComponent>())
		{
			Component->SetVisibility(bBodyLightsOn);
		}
	}

	// the body is only for show: the pawn's own physics body does all the colliding
	BodyActor->SetActorEnableCollision(false);
	BodyActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	SetUpBody();

	PlaceCockpitCamera();

	// a body swapped in while looking from the driver's seat starts without its glass too
	SetBodyGlassVisible(BodyActor, CameraView != ECameraView::Cockpit);

	// hide our own meshes and lights so only the new body shows
	TInlineComponentArray<USceneComponent*> OwnComponents(this);

	for (USceneComponent* Component : OwnComponents)
	{
		const bool bVisual = Component->IsA<UPrimitiveComponent>() || Component->IsA<ULightComponent>();

		if (bVisual && Component->IsVisible() && !Component->bHiddenInGame)
		{
			Component->SetVisibility(false);
			HiddenOwnComponents.Add(Component);
		}
	}
}

void AMyProjectPawn::SetBodyLightsOn(bool bOn)
{
	bBodyLightsOn = bOn;

	if (!BodyActor)
	{
		return;
	}

	TInlineComponentArray<ULightComponent*> BodyLights(BodyActor.Get());

	for (ULightComponent* Light : BodyLights)
	{
		Light->SetVisibility(bOn);
	}
}

void AMyProjectPawn::DestroyBody()
{
	for (const TWeakObjectPtr<USceneComponent>& Component : HiddenOwnComponents)
	{
		if (Component.IsValid())
		{
			Component->SetVisibility(true);
		}
	}

	HiddenOwnComponents.Reset();
	BodyWheels.Reset();
	BodySteeringWheel.Reset();

	if (BodyActor)
	{
		BodyActor->Destroy();
		BodyActor = nullptr;
	}

	BodyClass = nullptr;

	// back to the pawn's own driving position
	PlaceCockpitCamera();
}

void AMyProjectPawn::SetUpBody()
{
	USceneComponent* BodyRoot = BodyActor->GetRootComponent();

	FVector BodyWheelCenter = FVector::ZeroVector;
	float BodyWheelRadius = 0.0f;

	// the showcase Blueprints name their parts consistently, but not their wheels
	// (Wheel_LF, LF_Wheel, Wheel_FL...), so wheels are matched by position further down
	TInlineComponentArray<UStaticMeshComponent*> Meshes(BodyActor.Get());

	for (UStaticMeshComponent* MeshComponent : Meshes)
	{
		const FString Name = MeshComponent->GetName();
		const bool bSteeringWheel = Name.Contains(TEXT("Steering"));
		const bool bWheel = !bSteeringWheel && Name.Contains(TEXT("Wheel"));

		if (!bWheel && !bSteeringWheel)
		{
			continue;
		}

		// hang it straight off the body root so its relative transform is in body space,
		// even when the Blueprint grouped the wheels under an extra scene component
		MeshComponent->AttachToComponent(BodyRoot, FAttachmentTransformRules::KeepWorldTransform);

		if (bSteeringWheel)
		{
			BodySteeringWheel = MeshComponent;
			BodySteeringWheelBaseRotation = MeshComponent->GetRelativeTransform().GetRotation();
			continue;
		}

		FBodyWheel& Wheel = BodyWheels.AddDefaulted_GetRef();
		Wheel.Component = MeshComponent;
		Wheel.BaseLocation = MeshComponent->GetRelativeLocation();
		Wheel.BaseRotation = MeshComponent->GetRelativeTransform().GetRotation();

		BodyWheelCenter += Wheel.BaseLocation;

		// a wheel is as tall as it is long, and both are more than its width, whichever way the mesh was modeled
		if (const UStaticMesh* StaticMesh = MeshComponent->GetStaticMesh())
		{
			BodyWheelRadius += StaticMesh->GetBounds().BoxExtent.GetMax() * MeshComponent->GetRelativeScale3D().GetMax();
		}
	}

	// where the physics wheels sit on the chassis, the same way the movement component works it out
	TArray<FVector> PhysicsWheelLocations;
	FVector PhysicsWheelCenter = FVector::ZeroVector;
	float PhysicsWheelRadius = 0.0f;

	const USkinnedAsset* SkinnedAsset = GetMesh()->GetSkinnedAsset();

	for (const FChaosWheelSetup& Setup : ChaosVehicleMovement->WheelSetups)
	{
		const UChaosVehicleWheel* WheelDefaults = Setup.WheelClass ? Setup.WheelClass->GetDefaultObject<UChaosVehicleWheel>() : nullptr;

		FVector Location = Setup.AdditionalOffset + (WheelDefaults ? WheelDefaults->Offset : FVector::ZeroVector);

		if (SkinnedAsset && Setup.BoneName != NAME_None)
		{
			Location += SkinnedAsset->GetComposedRefPoseMatrix(Setup.BoneName).GetOrigin();
		}

		PhysicsWheelLocations.Add(Location);
		PhysicsWheelCenter += Location;
		PhysicsWheelRadius += WheelDefaults ? WheelDefaults->WheelRadius : 0.0f;
	}

	if (BodyWheels.Num() == 0 || PhysicsWheelLocations.Num() == 0)
	{
		UE_LOG(LogMyProject, Warning, TEXT("[Body] %s: %s has %d wheel meshes and the vehicle has %d physics wheels, so the body can't be lined up"),
			*GetName(), *BodyActor->GetClass()->GetName(), BodyWheels.Num(), PhysicsWheelLocations.Num());
		return;
	}

	BodyWheelCenter /= BodyWheels.Num();
	BodyWheelRadius /= BodyWheels.Num();
	PhysicsWheelCenter /= PhysicsWheelLocations.Num();
	PhysicsWheelRadius /= PhysicsWheelLocations.Num();

	// center the body's wheels over the physics wheels, and stand its tires on the same ground
	const FVector BodyOffset(
		PhysicsWheelCenter.X - BodyWheelCenter.X,
		PhysicsWheelCenter.Y - BodyWheelCenter.Y,
		(PhysicsWheelCenter.Z - PhysicsWheelRadius) - (BodyWheelCenter.Z - BodyWheelRadius));

	BodyActor->SetActorRelativeLocation(BodyOffset);

	// each wheel mesh follows the physics wheel closest to it
	for (FBodyWheel& Wheel : BodyWheels)
	{
		const FVector LocationOnChassis = Wheel.BaseLocation + BodyOffset;
		float ClosestDistance = TNumericLimits<float>::Max();

		for (int32 Index = 0; Index < PhysicsWheelLocations.Num(); ++Index)
		{
			const float Distance = FVector::DistSquared2D(LocationOnChassis, PhysicsWheelLocations[Index]);

			if (Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				Wheel.PhysicsWheelIndex = Index;
			}
		}
	}

	UE_LOG(LogMyProject, Log, TEXT("[Body] %s wears %s: %d wheels (radius %.1f vs physics %.1f), offset %s"),
		*GetName(), *BodyActor->GetClass()->GetName(), BodyWheels.Num(), BodyWheelRadius, PhysicsWheelRadius, *BodyOffset.ToString());
}

void AMyProjectPawn::UpdateBodyWheels()
{
	if (!BodyActor || !ChaosVehicleMovement)
	{
		return;
	}

	// the wheel getters assert if the physics vehicle hasn't produced any output yet
	const TUniquePtr<FPhysicsVehicleOutput>& PhysicsOutput = ChaosVehicleMovement->PhysicsVehicleOutput();

	if (!PhysicsOutput.IsValid())
	{
		return;
	}

	const TArray<TObjectPtr<UChaosVehicleWheel>>& PhysicsWheels = ChaosVehicleMovement->Wheels;

	auto IsWheelReady = [&](int32 Index)
	{
		return PhysicsWheels.IsValidIndex(Index) && PhysicsWheels[Index] && PhysicsOutput->Wheels.IsValidIndex(Index);
	};

	for (const FBodyWheel& Wheel : BodyWheels)
	{
		USceneComponent* Component = Wheel.Component.Get();

		if (!Component || !IsWheelReady(Wheel.PhysicsWheelIndex))
		{
			continue;
		}

		const UChaosVehicleWheel* PhysicsWheel = PhysicsWheels[Wheel.PhysicsWheelIndex];

		// the same spin, steer and suspension offsets the vehicle anim instance puts on wheel bones
		const FRotator WheelRotation(PhysicsWheel->GetRotationAngle(), PhysicsWheel->GetSteerAngle(), 0.0f);
		const FVector SuspensionOffset = -PhysicsWheel->GetSuspensionAxis() * PhysicsWheel->GetSuspensionOffset();

		Component->SetRelativeLocationAndRotation(Wheel.BaseLocation + SuspensionOffset, WheelRotation.Quaternion() * Wheel.BaseRotation);
	}

	if (USceneComponent* SteeringWheel = BodySteeringWheel.Get())
	{
		float SteerAngle = 0.0f;
		int32 SteeredWheels = 0;

		for (int32 Index = 0; Index < PhysicsWheels.Num(); ++Index)
		{
			if (IsWheelReady(Index) && PhysicsWheels[Index]->bAffectedBySteering)
			{
				SteerAngle += PhysicsWheels[Index]->GetSteerAngle();
				++SteeredWheels;
			}
		}

		if (SteeredWheels > 0)
		{
			SteerAngle /= SteeredWheels;
		}

		// turning right spins the wheel clockwise from the driver's seat, which is negative around its column
		const FQuat SteeringTurn(FVector::XAxisVector, FMath::DegreesToRadians(-SteerAngle * BodySteeringWheelRatio));

		SteeringWheel->SetRelativeRotation(BodySteeringWheelBaseRotation * SteeringTurn);
	}
}

void AMyProjectPawn::SetHandling(const FVehicleHandling& NewHandling)
{
	if (NewHandling == Handling)
	{
		return;
	}

	Handling = NewHandling;

	// every change starts again from the pawn's own setup, so changes never pile up
	const UChaosWheeledVehicleMovementComponent* OwnSetup = GetClass()->GetDefaultObject<AMyProjectPawn>()->GetChaosVehicleMovement();

	FVehicleDifferentialConfig& Differential = ChaosVehicleMovement->DifferentialSetup;
	Differential.DifferentialType = Handling.bAllWheelDrive ? EVehicleDifferential::AllWheelDrive : OwnSetup->DifferentialSetup.DifferentialType;
	Differential.FrontRearSplit = Handling.bAllWheelDrive ? Handling.RearTorqueShare : OwnSetup->DifferentialSetup.FrontRearSplit;

	ChaosVehicleMovement->EngineSetup.MaxTorque = OwnSetup->EngineSetup.MaxTorque * Handling.EngineTorqueScale;

	// the steering curve gives the share of the full lock the wheels get at each speed. The vehicle divides the
	// curve by its highest value, so capping at that value keeps the full lock at low speed where it was
	FRichCurve SteeringCurve = *OwnSetup->SteeringSetup.SteeringCurve.GetRichCurveConst();

	float MinSteering = 0.0f;
	float MaxSteering = 1.0f;
	SteeringCurve.GetValueRange(MinSteering, MaxSteering);

	for (FRichCurveKey& Key : SteeringCurve.Keys)
	{
		Key.Value = FMath::Min(Key.Value * Handling.SteeringAtSpeedScale, MaxSteering);
	}

	// written into the component's own curve, so a curve asset shared with other vehicles is never touched
	ChaosVehicleMovement->SteeringSetup.SteeringCurve.EditorCurveData = SteeringCurve;
	ChaosVehicleMovement->SteeringSetup.SteeringCurve.ExternalCurve = nullptr;

	// the drive layout, the engine and the steering curve are only read when the physics vehicle is built
	StopDrift();
	ChaosVehicleMovement->RecreatePhysicsState();

	// the rebuilt bodies come back with hit events off again
	GetMesh()->SetNotifyRigidBodyCollision(true);

	// the rebuilt transmission starts out automatic again
	ChaosVehicleMovement->SetUseAutomaticGears(bUseAutomaticGears);

	// the rebuilt wheels start with their class's grip. The drift reads the grip back off the wheel, so it keeps this too
	for (int32 Index = 0; Index < ChaosVehicleMovement->Wheels.Num(); ++Index)
	{
		UChaosVehicleWheel* Wheel = ChaosVehicleMovement->Wheels[Index];

		if (!Wheel)
		{
			continue;
		}

		const float GripScale = Wheel->GetAxleType() == EAxleType::Front ? Handling.FrontGripScale : Handling.RearGripScale;

		if (GripScale != 1.0f)
		{
			Wheel->FrictionForceMultiplier *= GripScale;
			ChaosVehicleMovement->SetWheelFrictionMultiplier(Index, Wheel->FrictionForceMultiplier);
		}
	}

	UE_LOG(LogMyProject, Log, TEXT("[Handling] %s: %s (rear share %.2f), engine torque %.0f, steering at speed x%.2f, front grip x%.2f, rear grip x%.2f"),
		*GetName(), *UEnum::GetValueAsString(Differential.DifferentialType), Differential.FrontRearSplit,
		ChaosVehicleMovement->EngineSetup.MaxTorque, Handling.SteeringAtSpeedScale, Handling.FrontGripScale, Handling.RearGripScale);
}

void AMyProjectPawn::SetEngineSound(UEngineSoundProfile* NewProfile)
{
	EngineSound->SetProfile(NewProfile);

	UpdateEngineSound();
}

void AMyProjectPawn::UpdateEngineSound()
{
	// the recording runs from idle to the red line, so the revs go in as how far they are between the two
	EngineSound->SetEngineState(FMath::Clamp(GetEngineRPMFraction(), 0.0f, 1.0f), FMath::Abs(ChaosVehicleMovement->GetThrottleInput()));
}

float AMyProjectPawn::GetEngineRPMFraction() const
{
	const float IdleRPM = ChaosVehicleMovement->EngineSetup.EngineIdleRPM;
	const float MaxRPM = ChaosVehicleMovement->GetEngineMaxRotationSpeed();

	return (FMath::Abs(ChaosVehicleMovement->GetEngineRotationSpeed()) - IdleRPM) / FMath::Max(MaxRPM - IdleRPM, 1.0f);
}

float AMyProjectPawn::GetEngineRPM() const
{
	const float PhysicsRPM = FMath::Abs(ChaosVehicleMovement->GetEngineRotationSpeed());
	const UEngineSoundProfile* Profile = EngineSound->GetProfile();

	if (!Profile || Profile->RecordingRPM.IsEmpty())
	{
		return PhysicsRPM;
	}

	// below idle, as when the engine has stalled, scale down from the recording's idle
	const float IdleRPM = ChaosVehicleMovement->EngineSetup.EngineIdleRPM;

	if (PhysicsRPM < IdleRPM)
	{
		return PhysicsRPM * Profile->GetRecordingRPM(0.0f) / FMath::Max(IdleRPM, 1.0f);
	}

	return Profile->GetRecordingRPM(GetEngineRPMFraction());
}

float AMyProjectPawn::GetEngineMaxRPM() const
{
	const UEngineSoundProfile* Profile = EngineSound->GetProfile();

	return Profile && !Profile->RecordingRPM.IsEmpty() ? Profile->GetRecordingRPM(1.0f) : ChaosVehicleMovement->GetEngineMaxRotationSpeed();
}

void AMyProjectPawn::SetUseAutomaticGears(bool bAutomatic)
{
	bUseAutomaticGears = bAutomatic;

	ChaosVehicleMovement->SetUseAutomaticGears(bAutomatic);
}

void AMyProjectPawn::UpdateAutoDownshift()
{
	// NPC drivers keep the gearbox's own shifting, which their pace was tuned with
	if (!bUseAutomaticGears || AutoDownshiftRPM <= 0.0f || !IsPlayerControlled())
	{
		return;
	}

	const int32 CurrentGear = ChaosVehicleMovement->GetCurrentGear();
	const float Now = GetWorld()->GetTimeSeconds();

	// wait for our change under way, or the revs still read off the old gear would start it over. If the gear
	// hasn't come down well after the change time, the gearbox overrode it and we may try again
	const float ChangeTime = ChaosVehicleMovement->TransmissionSetup.GearChangeTime;
	const bool bChangingDown = CurrentGear == AutoDownshiftFromGear && Now - AutoDownshiftTime < ChangeTime + 0.3f;

	if (CurrentGear < 2 || bChangingDown)
	{
		return;
	}

	const float EngineRPM = GetEngineRPM();

	if (EngineRPM <= AutoDownshiftRPM)
	{
		// the gearbox's own shifts don't update the movement's target gear, so it may already read the gear we want
		// and the change would be skipped. Setting the gear we're in first makes the change go through
		ChaosVehicleMovement->SetTargetGear(CurrentGear, true);
		ChaosVehicleMovement->SetTargetGear(CurrentGear - 1, false);

		AutoDownshiftFromGear = CurrentGear;
		AutoDownshiftTime = Now;

		UE_LOG(LogMyProject, Verbose, TEXT("[AT] %s: down %d -> %d at %.0f rpm, %.0f km/h"), *GetName(), CurrentGear, CurrentGear - 1,
			EngineRPM, ChaosVehicleMovement->GetForwardSpeed() * 0.036f);
	}
}

void AMyProjectPawn::FlippedCheck()
{
	// check the difference in angle between the mesh's up vector and world up
	const float UpDot = FVector::DotProduct(FVector::UpVector, GetMesh()->GetUpVector());

	if (UpDot < FlipCheckMinDot)
	{
		// is this the second time we've checked that the vehicle is still flipped?
		if (bPreviousFlipCheck)
		{
			// reset the vehicle to upright
			DoResetVehicle();
		}
		
		// set the flipped check flag so the next check resets the car
		bPreviousFlipCheck = true;

	} else {

		// we're upright. reset the flipped check flag
		bPreviousFlipCheck = false;
	}
}

void AMyProjectPawn::OnChassisHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// sparks and noise for anything but the road, measured before the solver pushed us back out
	if (CollisionFx && Hit.ImpactNormal.Z < GroundImpactNormalZ)
	{
		const FVector OtherVelocity = OtherComp ? OtherComp->GetComponentVelocity() : FVector::ZeroVector;

		CollisionFx->AddContact(Hit.ImpactPoint, Hit.ImpactNormal, PreImpactVelocity - OtherVelocity);
	}

	if (!bReactToHits)
	{
		return;
	}

	// even a graze against a wall breaks the slide. Held on, the drift would hand the speed the wall took back to the car
	// along the wall, and its forced yaw would lever the car round the touching corner, shooting it forward
	if (bDrifting && Hit.ImpactNormal.Z < GroundImpactNormalZ)
	{
		StopDrift();

		// don't leave the car with the spin the drift was forcing on it
		ClampChassisRotationRate(0.0f, MaxImpactSpinRate);
	}

	if (!bStabilizeImpacts)
	{
		return;
	}

	// how hard we ran into this, measured before the solver pushed us back out
	const FVector ImpactNormal = Hit.ImpactNormal;
	const float ClosingSpeed = FVector::DotProduct(-PreImpactVelocity, ImpactNormal);

	if (ClosingSpeed < ImpactSpeedThreshold)
	{
		// a light scrape. leave it to the physics
		return;
	}

	FVector Velocity = GetMesh()->GetPhysicsLinearVelocity();
	const float VerticalSpeed = Velocity.Z;

	if (ImpactNormal.Z < GroundImpactNormalZ)
	{
		// a wall. strip out the bounce and let the car scrub along the surface instead of being thrown off it
		const FVector WallNormal = FVector(ImpactNormal.X, ImpactNormal.Y, 0.0f).GetSafeNormal();

		if (!WallNormal.IsNearlyZero())
		{
			const FVector HorizontalVelocity(Velocity.X, Velocity.Y, 0.0f);
			const float OutwardSpeed = FVector::DotProduct(HorizontalVelocity, WallNormal);
			const FVector AlongWall = HorizontalVelocity - (WallNormal * OutwardSpeed);

			Velocity = (AlongWall * WallScrapeRetention) + (WallNormal * FMath::Min(OutwardSpeed, MaxWallReboundSpeed));
		}
	}

	// nothing we hit gets to launch the car into the air
	Velocity.Z = FMath::Min(VerticalSpeed, MaxImpactLaunchSpeed);

	GetMesh()->SetPhysicsLinearVelocity(Velocity);

	// and nothing gets to send it tumbling or spinning either
	ClampChassisRotationRate(MaxImpactTumbleRate, MaxImpactSpinRate);

	// keep the car damped while it settles
	ImpactRecoveryRemaining = ImpactRecoveryTime;

	// further hits this frame should measure against the velocity we just settled on
	PreImpactVelocity = Velocity;
}

void AMyProjectPawn::ClampChassisRotationRate(float MaxRollPitchRate, float MaxYawRate)
{
	if (MaxRollPitchRate <= 0.0f && MaxYawRate <= 0.0f)
	{
		return;
	}

	// the chassis local axes are roll, pitch and yaw in that order
	const FTransform ChassisTransform = GetMesh()->GetComponentTransform();
	const FVector RotationRate = ChassisTransform.InverseTransformVectorNoScale(GetMesh()->GetPhysicsAngularVelocityInDegrees());

	FVector ClampedRate = RotationRate;

	if (MaxRollPitchRate > 0.0f)
	{
		ClampedRate.X = FMath::Clamp(RotationRate.X, -MaxRollPitchRate, MaxRollPitchRate);
		ClampedRate.Y = FMath::Clamp(RotationRate.Y, -MaxRollPitchRate, MaxRollPitchRate);
	}

	if (MaxYawRate > 0.0f)
	{
		ClampedRate.Z = FMath::Clamp(RotationRate.Z, -MaxYawRate, MaxYawRate);
	}

	if (!ClampedRate.Equals(RotationRate))
	{
		GetMesh()->SetPhysicsAngularVelocityInDegrees(ChassisTransform.TransformVectorNoScale(ClampedRate));
	}
}

bool AMyProjectPawn::TryStartDrift()
{
	if (!bEnableArcadeDrift || bDrifting || CountWheelsOnGround(ChaosVehicleMovement) < 2)
	{
		return false;
	}

	if (ChaosVehicleMovement->GetForwardSpeed() < DriftMinSpeed || FMath::Abs(SteeringInput) < DriftEntrySteer)
	{
		return false;
	}

	bDrifting = true;
	DriftDirection = FMath::Sign(SteeringInput);
	DriftEntryTimeLeft = 0.0f;

	const FVector Velocity = GetMesh()->GetPhysicsLinearVelocity();
	DriftSpeed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();

	// the drift carries the car through the corner, not the handbrake
	ChaosVehicleMovement->SetHandbrakeInput(false);

	// loosen the tires so they slide instead of fighting the slide
	const TArray<TObjectPtr<UChaosVehicleWheel>>& Wheels = ChaosVehicleMovement->Wheels;

	for (int32 Index = 0; Index < Wheels.Num(); ++Index)
	{
		if (Wheels[Index])
		{
			ChaosVehicleMovement->SetWheelFrictionMultiplier(Index, Wheels[Index]->FrictionForceMultiplier * DriftTireGrip);
		}
	}

	return true;
}

void AMyProjectPawn::StopDrift()
{
	if (!bDrifting)
	{
		return;
	}

	bDrifting = false;
	DriftDirection = 0.0f;

	// give the tires their grip back
	const TArray<TObjectPtr<UChaosVehicleWheel>>& Wheels = ChaosVehicleMovement->Wheels;

	for (int32 Index = 0; Index < Wheels.Num(); ++Index)
	{
		if (Wheels[Index])
		{
			ChaosVehicleMovement->SetWheelFrictionMultiplier(Index, Wheels[Index]->FrictionForceMultiplier);
		}
	}
}

void AMyProjectPawn::UpdateDrift(float Delta)
{
	if (!bDrifting)
	{
		// the handbrake was pressed a moment ago and the driver may only now be steering
		if (DriftEntryTimeLeft > 0.0f)
		{
			DriftEntryTimeLeft = FMath::Max(DriftEntryTimeLeft - Delta, 0.0f);
			TryStartDrift();
		}

		return;
	}

	// hold the slide as it is while the wheels are off the ground and pick it back up on landing
	if (CountWheelsOnGround(ChaosVehicleMovement) < 2 || Delta <= 0.0f)
	{
		return;
	}

	const FVector Velocity = GetMesh()->GetPhysicsLinearVelocity();
	const FVector HorizontalVelocity(Velocity.X, Velocity.Y, 0.0f);
	const float Speed = HorizontalVelocity.Size();

	const FVector Forward = FVector(GetActorForwardVector().X, GetActorForwardVector().Y, 0.0f).GetSafeNormal();

	if (Speed < DriftExitSpeed || Forward.IsNearlyZero())
	{
		StopDrift();
		return;
	}

	const FVector TravelDirection = HorizontalVelocity / Speed;

	// how far the nose points into the corner from the direction of travel. Positive yaw turns right
	const float SlipAngle = FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(TravelDirection, Forward).Z, FVector::DotProduct(TravelDirection, Forward)));
	const float DriftAngle = SlipAngle * DriftDirection;

	// steering into the drift deepens the angle, counter-steering straightens the car out
	const float SteerIntoDrift = FMath::Clamp(SteeringInput * DriftDirection, -1.0f, 1.0f);
	const float TargetAngle = SteerIntoDrift >= 0.0f
		? FMath::Lerp(DriftNeutralAngle, DriftMaxAngle, SteerIntoDrift)
		: FMath::Lerp(DriftNeutralAngle, DriftMinAngle, -SteerIntoDrift);

	// counter-steered back into line, or spun the other way or clean round: the slide is over
	const bool bStraightenedOut = SteerIntoDrift < -0.3f && DriftAngle < DriftExitAngle;

	if (bStraightenedOut || DriftAngle < -DriftExitAngle || DriftAngle > 100.0f)
	{
		StopDrift();
		return;
	}

	// keep the slide's speed up against the tires scrubbing it off. The brake is let through to slow the car
	if (BrakeInput > 0.0f)
	{
		DriftSpeed = FMath::Min(DriftSpeed, Speed);
	}
	else
	{
		DriftSpeed = FMath::Max(DriftSpeed - (DriftSpeedLoss * (1.0f - (0.8f * ThrottleInput)) * Delta), Speed);
	}

	// the direction of travel swings round into the corner, faster the deeper the angle
	const float TurnRate = DriftTurnRate * FMath::Clamp(DriftAngle / DriftMaxAngle, 0.0f, 1.0f);
	const FVector NewTravelDirection = TravelDirection.RotateAngleAxis(TurnRate * DriftDirection * Delta, FVector::UpVector);

	GetMesh()->SetPhysicsLinearVelocity(FVector(NewTravelDirection * DriftSpeed) + FVector(0.0f, 0.0f, Velocity.Z));

	// the nose follows the direction of travel round and gets pulled onto the target angle
	const float TargetYawRate = FMath::Clamp((TurnRate + ((TargetAngle - DriftAngle) * DriftAngleStiffness)) * DriftDirection, -DriftMaxYawRate, DriftMaxYawRate);

	const FTransform ChassisTransform = GetMesh()->GetComponentTransform();
	FVector RotationRate = ChassisTransform.InverseTransformVectorNoScale(GetMesh()->GetPhysicsAngularVelocityInDegrees());

	RotationRate.Z = FMath::FInterpTo(RotationRate.Z, TargetYawRate, Delta, DriftYawResponse);

	GetMesh()->SetPhysicsAngularVelocityInDegrees(ChassisTransform.TransformVectorNoScale(RotationRate));
}

void AMyProjectPawn::UpdateRaceBoost()
{
	// only driving forwards with the wheels on the road, so it never pushes a car that's braking, reversing or flying
	const float Throttle = ChaosVehicleMovement->GetThrottleInput();

	if (RaceBoost <= 0.0f || Throttle <= 0.0f || ChaosVehicleMovement->GetCurrentGear() <= 0
		|| ChaosVehicleMovement->GetForwardSpeed() < 300.0f || CountWheelsOnGround(ChaosVehicleMovement) < 2)
	{
		return;
	}

	GetMesh()->AddForce(GetActorForwardVector() * RaceBoost * Throttle, NAME_None, true);
}

void AMyProjectPawn::UpdateAirborne(float Delta)
{
	// only while every wheel is off the ground
	if (Delta <= 0.0f || CountWheelsOnGround(ChaosVehicleMovement) > 0)
	{
		return;
	}

	// bring the car down sooner than plain gravity would
	if (AirborneExtraGravity > 0.0f)
	{
		GetMesh()->AddForce(FVector(0.0f, 0.0f, -AirborneExtraGravity), NAME_None, true);
	}

	if (!bEnableRoadStick || RoadStickRate <= 0.0f)
	{
		return;
	}

	// look for the road close below. The roads ignore the visibility channel, so search by object type instead.
	// The guardrails' invisible barriers only take part in physics, so this can't mistake their tops for road
	FHitResult Hit;
	const FVector Start = GetActorLocation();
	const FVector End = Start - FVector(0.0f, 0.0f, RoadStickDistance);
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RoadStick), false, this);

	if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), QueryParams))
	{
		return;
	}

	// a wall or a cliff face isn't road to stick to
	if (Hit.ImpactNormal.Z < GroundImpactNormalZ)
	{
		return;
	}

	const FVector Velocity = GetMesh()->GetPhysicsLinearVelocity();
	const float AwaySpeed = FVector::DotProduct(Velocity, Hit.ImpactNormal);

	if (AwaySpeed <= 0.0f)
	{
		return;
	}

	// turn the part of the speed carrying the car off the road back along the road, keeping the car's speed,
	// so it follows the road down past the crest instead of taking off
	const float Speed = Velocity.Size();
	const FVector AlongRoad = Velocity - (Hit.ImpactNormal * AwaySpeed * FMath::Min(RoadStickRate * Delta, 1.0f));

	GetMesh()->SetPhysicsLinearVelocity(AlongRoad.GetSafeNormal() * Speed);
}

#undef LOCTEXT_NAMESPACE
