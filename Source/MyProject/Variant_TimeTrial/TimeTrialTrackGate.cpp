// Copyright Epic Games, Inc. All Rights Reserved.


#include "TimeTrialTrackGate.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "TimeTrialPlayerController.h"
#include "TimeTrialGameMode.h"
#include "MyProject.h"

ATimeTrialTrackGate::ATimeTrialTrackGate()
{
 	PrimaryActorTick.bCanEverTick = true;

	// create the root component
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// create the collision box
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision Box"));
	CollisionBox->SetupAttachment(RootComponent);

	CollisionBox->SetBoxExtent(FVector(1000.0f));
	CollisionBox->SetLineThickness(32.0f);
	// the route marker replaces the debug wireframe as the in-game visual
	CollisionBox->bHiddenInGame = true;
	CollisionBox->SetCollisionProfileName(FName("OverlapAllDynamic"));

	// create the route marker: a flat plane that gets stood upright and turned across the road at runtime
	RouteMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Route Marker"));
	RouteMarker->SetupAttachment(RootComponent);
	RouteMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RouteMarker->SetCastShadow(false);
	RouteMarker->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane"));
	if (PlaneMesh.Succeeded())
	{
		RouteMarker->SetStaticMesh(PlaneMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GateMaterial(TEXT("/Game/Variant_TimeTrial/Materials/M_RouteGate"));
	if (GateMaterial.Succeeded())
	{
		MarkerMaterial = GateMaterial.Object;
	}
}

void ATimeTrialTrackGate::BeginPlay()
{
	Super::BeginPlay();

	// create the material instance that drives the marker color
	if (RouteMarker && MarkerMaterial)
	{
		MarkerMID = RouteMarker->CreateDynamicMaterialInstance(0, MarkerMaterial);
	}

	// follow the selected route. The game mode re-broadcasts in its own BeginPlay, so it doesn't
	// matter whether this gate or the game mode initializes first
	if (ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->OnSelectedRaceChanged.AddDynamic(this, &ATimeTrialTrackGate::OnSelectedRaceChanged);
		OnSelectedRaceChanged(GM->GetSelectedRace());
	}
}

void ATimeTrialTrackGate::NotifyActorBeginOverlap(AActor* OtherActor)
{
	// get the player controller of the overlapping actor
	if (ATimeTrialPlayerController* PC = Cast<ATimeTrialPlayerController>(OtherActor->GetInstigatorController()))
	{
		UE_LOG(LogMyProject, Log, TEXT("[Gate] %s overlapped by %s. TargetGate=%s, IsMatch=%s, bIsFinishLine=%s"),
			*GetName(), *OtherActor->GetName(),
			PC->GetTargetGate() ? *PC->GetTargetGate()->GetName() : TEXT("None"),
			(PC->GetTargetGate() == this) ? TEXT("true") : TEXT("false"),
			bIsFinishLine ? TEXT("true") : TEXT("false"));

		// is this the current target marker for the player?
		if (PC->GetTargetGate() == this)
		{
			HandlePlayerPass(PC);
		}
	}
}

void ATimeTrialTrackGate::HandlePlayerPass(ATimeTrialPlayerController* PC)
{
	if (!PC)
	{
		return;
	}

	// point the player to the next gate of the selected race's route, which knows about branch points and races
	// that run their route the other way round
	if (const ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(GetWorld()->GetAuthGameMode()))
	{
		PC->SetTargetGate(GM->GetNextGateOnRoute(this));

	} else {

		PC->SetTargetGate(GetNextMarker());
	}

	// if this is the finish line, increment the lap
	if (bIsFinishLine)
	{
		PC->IncrementLapCount();
	}
}

void ATimeTrialTrackGate::OnSelectedRaceChanged(int32 RaceIndex)
{
	const ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(GetWorld()->GetAuthGameMode());

	if (!GM || !RouteMarker)
	{
		return;
	}

	// gates that aren't part of the chosen route stay invisible so only the correct road is marked
	const bool bOnRoute = GM->IsGateOnSelectedRoute(this);
	RouteMarker->SetHiddenInGame(!bOnRoute);

	if (!bOnRoute)
	{
		return;
	}

	// stand the plane upright and turn it across the road. The gate actor's own yaw is authored to
	// follow the road direction, so the marker sits square across the lane. Aiming it at the next
	// gate instead would skew it wherever the road bends between the two, and would also swing the
	// marker around whenever a branch point picks a different next gate per race.
	const float Yaw = GetActorRotation().Yaw;

	RouteMarker->SetWorldRotation(FRotator(90.0f, Yaw, 0.0f));
	RouteMarker->SetRelativeScale3D(FVector(MarkerHeight / 100.0f, MarkerWidth / 100.0f, 1.0f));
	RouteMarker->SetRelativeLocation(FVector(0.0f, 0.0f, MarkerHeight * 0.5f));

	if (MarkerMID)
	{
		MarkerMID->SetVectorParameterValue(TEXT("Color"), GM->GetColorForRace(RaceIndex));
	}

	RefreshTargetHighlight();
}

void ATimeTrialTrackGate::RefreshTargetHighlight()
{
	if (!MarkerMID)
	{
		return;
	}

	// highlight the gate if any local player is currently aiming for it
	bool bIsTarget = false;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const ATimeTrialPlayerController* PC = Cast<ATimeTrialPlayerController>(It->Get()))
		{
			if (PC->GetTargetGate() == this)
			{
				bIsTarget = true;
				break;
			}
		}
	}

	MarkerMID->SetScalarParameterValue(TEXT("Intensity"), bIsTarget ? TargetIntensity : RouteIntensity);
	MarkerMID->SetScalarParameterValue(TEXT("Opacity"), bIsTarget ? TargetOpacity : RouteOpacity);
}

ATimeTrialTrackGate* ATimeTrialTrackGate::GetNextMarker() const
{
	return NextMarker;
}

ATimeTrialTrackGate* ATimeTrialTrackGate::GetNextMarkerForRoute(int32 Route) const
{
	if (ATimeTrialTrackGate* const* Override = NextMarkerOverrides.Find(Route))
	{
		if (*Override)
		{
			return *Override;
		}
	}

	return NextMarker;
}
