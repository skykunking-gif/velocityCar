// Copyright Epic Games, Inc. All Rights Reserved.


#include "CollisionFxComponent.h"
#include "CollisionSoundComponent.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "MyProject.h"

namespace
{
	/** How long after the last reported contact a scrape is still counted as going on, in seconds. Contacts
	 *  aren't reported every frame of a scrape, so without this the sparks and the noise would stutter */
	constexpr float ContactHoldTime = 0.08f;

	/** How quickly a scrape dies away once contact is lost, per second */
	constexpr float ScrapeFadeRate = 20.0f;

	/** How quickly the flash of a knock fades, per second */
	constexpr float FlashFadeRate = 12.0f;

	/** Gravity pulling the sparks down, in cm/s² */
	constexpr float SparkGravity = 980.0f;

	/** How quickly the air slows a spark, per second */
	constexpr float SparkDrag = 1.5f;
}

UCollisionFxComponent::UCollisionFxComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// after the car has moved this frame, so the light and new sparks start where the car is now
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	SparkMesh = CubeMesh.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SparkMat(TEXT("/Game/Effects/M_Spark.M_Spark"));
	SparkMaterial = SparkMat.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> Crash(TEXT("/Game/Effects/Crash_Car_Fender_Bender_01.Crash_Car_Fender_Bender_01"));
	CrashSound = Crash.Object;
}

void UCollisionFxComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();

	if (!Owner)
	{
		return;
	}

	// sparks are left behind where they were thrown, so their instances live in the world rather than on the car
	SparkInstances = NewObject<UInstancedStaticMeshComponent>(Owner, TEXT("SparkInstances"));
	SparkInstances->SetUsingAbsoluteLocation(true);
	SparkInstances->SetUsingAbsoluteRotation(true);
	SparkInstances->SetUsingAbsoluteScale(true);
	SparkInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SparkInstances->SetCastShadow(false);
	SparkInstances->SetStaticMesh(SparkMesh);
	SparkInstances->SetMaterial(0, SparkMaterial);
	SparkInstances->NumCustomDataFloats = 1;
	SparkInstances->SetupAttachment(Owner->GetRootComponent());
	SparkInstances->RegisterComponent();
	SparkInstances->SetWorldTransform(FTransform::Identity);

	// every spark has an instance kept for it, shrunk away while it isn't flying
	TArray<FTransform> Hidden;
	Hidden.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), MaxSparks);
	SparkInstances->AddInstances(Hidden, false);
	SparkInstances->SetVisibility(false);

	Sparks.Reserve(MaxSparks);

	ContactLight = NewObject<UPointLightComponent>(Owner, TEXT("ContactLight"));
	ContactLight->SetUsingAbsoluteLocation(true);
	ContactLight->SetIntensityUnits(ELightUnits::Candelas);
	ContactLight->SetIntensity(0.0f);
	ContactLight->SetAttenuationRadius(LightRadius);
	ContactLight->SetLightColor(LightColor);
	ContactLight->SetCastShadows(false);
	ContactLight->SetupAttachment(Owner->GetRootComponent());
	ContactLight->RegisterComponent();
	ContactLight->SetVisibility(false);

	Sound = NewObject<UCollisionSoundComponent>(Owner, TEXT("CollisionSound"));
	Sound->SetupAttachment(Owner->GetRootComponent());
	Sound->RegisterComponent();
	Sound->Start();
}

void UCollisionFxComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (UActorComponent* Made : TArray<UActorComponent*>{ SparkInstances, ContactLight, Sound })
	{
		if (Made)
		{
			Made->DestroyComponent();
		}
	}

	SparkInstances = nullptr;
	ContactLight = nullptr;
	Sound = nullptr;
	Sparks.Reset();

	Super::EndPlay(EndPlayReason);
}

void UCollisionFxComponent::AddContact(const FVector& Point, const FVector& Normal, const FVector& RelativeVelocity)
{
	// split the car's motion into going into what it hit and sliding along it
	const float ClosingSpeed = -FVector::DotProduct(RelativeVelocity, Normal);
	const FVector Slide = RelativeVelocity + Normal * ClosingSpeed;
	const float SlideSpeed = Slide.Size();
	const FVector SlideDir = SlideSpeed > KINDA_SMALL_NUMBER ? Slide / SlideSpeed : FVector::ZeroVector;

	UE_LOG(LogMyProject, Verbose, TEXT("[Contact] %s into %.0f, sliding %.0f at %s"), *GetNameSafe(GetOwner()), ClosingSpeed, SlideSpeed, *Point.ToCompactString());

	// a scrape, as hard as the car slides along it
	const float ScrapeAmount = FMath::Clamp((SlideSpeed - MinScrapeSpeed) / (FullScrapeSpeed - MinScrapeSpeed), 0.0f, 1.0f);

	if (ScrapeAmount > FrameScrape)
	{
		FrameScrape = ScrapeAmount;
		FrameScrapeSpeed = FMath::Clamp(SlideSpeed / FullScrapeSpeed, 0.0f, 1.0f);
		FrameScrapePoint = Point;
		FrameScrapeNormal = Normal;
		FrameSlideDirection = SlideDir;
	}

	// a knock, as hard as the car went into it
	if (ClosingSpeed > MinImpactSpeed && ImpactCooldownLeft <= 0.0f)
	{
		const float Strength = FMath::Clamp((ClosingSpeed - MinImpactSpeed) / (FullImpactSpeed - MinImpactSpeed), 0.0f, 1.0f);

		ImpactCooldownLeft = ImpactCooldown;
		ImpactFlash = FMath::Max(ImpactFlash, 0.4f + 0.6f * Strength);
		ScrapePoint = Point;
		ScrapeNormal = Normal;

		if (CrashSound && Strength >= CrashMinStrength)
		{
			// a hard knock straight after a crash is the same crash bouncing on, already in the recording
			if (CrashCooldownLeft <= 0.0f)
			{
				PlayCrash(Point, Strength);
			}

		} else if (Sound) {

			Sound->PlayImpact(FMath::Max(Strength, 0.08f));
		}

		SpawnSparks(3 + FMath::RoundToInt(ImpactSparkCount * Strength), Point, Normal, SlideDir, FMath::Max(SlideSpeed, ClosingSpeed));
	}
}

void UCollisionFxComponent::PlayCrash(const FVector& Point, float Strength)
{
	AActor* Owner = GetOwner();

	if (!Owner)
	{
		return;
	}

	CrashCooldownLeft = CrashCooldown;

	// how far it is between the lightest crash and the hardest
	const float Hardness = FMath::Clamp((Strength - CrashMinStrength) / FMath::Max(1.0f - CrashMinStrength, KINDA_SMALL_NUMBER), 0.0f, 1.0f);

	// a component of its own for each crash, so one still ringing out isn't cut off by the next
	UAudioComponent* Crash = NewObject<UAudioComponent>(Owner);
	Crash->SetSound(CrashSound);
	Crash->bAutoDestroy = true;
	Crash->SetVolumeMultiplier(FMath::Lerp(CrashVolume.X, CrashVolume.Y, Hardness));
	Crash->SetPitchMultiplier(FMath::Lerp(CrashPitch.X, CrashPitch.Y, Hardness) * FMath::FRandRange(0.96f, 1.04f));

	// fades with distance just like the scrape, which NPC cars have set to fade like their engines
	if (Sound)
	{
		Crash->bAllowSpatialization = true;
		Crash->bOverrideAttenuation = true;
		Crash->AttenuationOverrides = Sound->AttenuationOverrides;
	}

	Crash->SetupAttachment(Owner->GetRootComponent());
	Crash->RegisterComponent();
	Crash->SetWorldLocation(Point);
	Crash->Play();
}

void UCollisionFxComponent::SpawnSparks(int32 Count, const FVector& Point, const FVector& Normal, const FVector& SlideDir, float SlideSpeed)
{
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// mostly along the slide, kicked out from what was hit and a little up, scattered
		FVector Direction = SlideDir * FMath::FRandRange(0.6f, 1.0f)
			+ Normal * FMath::FRandRange(0.2f, 0.7f)
			+ FVector::UpVector * FMath::FRandRange(0.0f, 0.5f)
			+ FMath::VRand() * 0.35f;

		if (!Direction.Normalize())
		{
			Direction = Normal;
		}

		SpawnSpark(Point, Direction, SlideSpeed * FMath::FRandRange(0.3f, 0.8f) + FMath::FRandRange(200.0f, 700.0f));
	}
}

void UCollisionFxComponent::SpawnSpark(const FVector& Point, const FVector& Direction, float Speed)
{
	FSpark* Spark = nullptr;

	if (Sparks.Num() < MaxSparks)
	{
		Spark = &Sparks.AddDefaulted_GetRef();

	} else {

		// all in the air: the oldest makes way
		Spark = &Sparks[0];

		for (FSpark& Candidate : Sparks)
		{
			if (Candidate.Age / Candidate.Lifetime > Spark->Age / Spark->Lifetime)
			{
				Spark = &Candidate;
			}
		}
	}

	Spark->Location = Point;
	Spark->Velocity = Direction * Speed;
	Spark->Age = 0.0f;
	Spark->Lifetime = FMath::FRandRange(SparkLifetime.X, SparkLifetime.Y);
}

void UCollisionFxComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ImpactCooldownLeft -= DeltaTime;
	CrashCooldownLeft -= DeltaTime;
	TimeSinceContact += DeltaTime;

	// the scrape: the hardest contact this frame, held a moment so it doesn't stutter between reports
	if (FrameScrape > 0.0f)
	{
		Scrape = FrameScrape;
		ScrapeSpeed = FrameScrapeSpeed;
		ScrapePoint = FrameScrapePoint;
		ScrapeNormal = FrameScrapeNormal;
		SlideDirection = FrameSlideDirection;
		TimeSinceContact = 0.0f;

	} else if (TimeSinceContact > ContactHoldTime) {

		Scrape = FMath::FInterpTo(Scrape, 0.0f, DeltaTime, ScrapeFadeRate);
		ScrapeSpeed = FMath::FInterpTo(ScrapeSpeed, 0.0f, DeltaTime, ScrapeFadeRate);

		if (Scrape < 0.01f)
		{
			Scrape = 0.0f;
			ScrapeSpeed = 0.0f;
		}
	}

	FrameScrape = 0.0f;

	if (Sound)
	{
		Sound->SetScrape(Scrape, ScrapeSpeed);
	}

	// sparks off the scrape while it's in contact, at a rate that grows with how hard it is
	if (TimeSinceContact <= ContactHoldTime && Scrape > 0.0f)
	{
		SparkDebt += ScrapeSparkRate * Scrape * DeltaTime;

		const int32 Count = FMath::FloorToInt(SparkDebt);
		SparkDebt -= Count;

		SpawnSparks(Count, ScrapePoint, ScrapeNormal, SlideDirection, ScrapeSpeed * FullScrapeSpeed);

	} else {

		SparkDebt = 0.0f;
	}

	// the light where it touches flickers with the scrape and flashes with a knock
	ImpactFlash = FMath::Max(ImpactFlash - FlashFadeRate * DeltaTime * ImpactFlash, 0.0f);

	if (ContactLight)
	{
		const float Brightness = Scrape * FMath::FRandRange(0.4f, 1.0f) + ImpactFlash;
		const bool bLit = Brightness > 0.02f;

		ContactLight->SetVisibility(bLit);

		if (bLit)
		{
			ContactLight->SetWorldLocation(ScrapePoint + ScrapeNormal * 30.0f);
			ContactLight->SetIntensity(LightIntensity * Brightness);
		}
	}

	// fly the sparks: falling, slowed by the air, gone when they've burnt out
	const float Drag = FMath::Exp(-SparkDrag * DeltaTime);

	for (int32 Index = Sparks.Num() - 1; Index >= 0; --Index)
	{
		FSpark& Spark = Sparks[Index];
		Spark.Age += DeltaTime;

		if (Spark.Age >= Spark.Lifetime)
		{
			Sparks.RemoveAtSwap(Index, EAllowShrinking::No);
			continue;
		}

		Spark.Velocity.Z -= SparkGravity * DeltaTime;
		Spark.Velocity *= Drag;
		Spark.Location += Spark.Velocity * DeltaTime;
	}

	if (!SparkInstances)
	{
		return;
	}

	// draw them: each a streak trailing back from where it is, as long as it is fast, fading as it burns out.
	// Instances whose spark has gone since the last update are shrunk away
	const int32 ToUpdate = FMath::Max(Sparks.Num(), ShownSparks);

	if (ToUpdate == 0)
	{
		return;
	}

	TArray<FTransform> Transforms;
	Transforms.Reserve(ToUpdate);

	for (int32 Index = 0; Index < ToUpdate; ++Index)
	{
		if (!Sparks.IsValidIndex(Index))
		{
			Transforms.Add(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector));
			SparkInstances->SetCustomDataValue(Index, 0, 0.0f);
			continue;
		}

		const FSpark& Spark = Sparks[Index];
		const float Speed = Spark.Velocity.Size();
		const FVector Direction = Speed > KINDA_SMALL_NUMBER ? Spark.Velocity / Speed : FVector::UpVector;
		const float Life = 1.0f - Spark.Age / Spark.Lifetime;
		const float Length = FMath::Max(Speed * SparkStreakTime, SparkThickness * 2.0f);
		const float Thickness = SparkThickness * (0.5f + 0.5f * Life);

		Transforms.Add(FTransform(Direction.ToOrientationQuat(), Spark.Location - Direction * (Length * 0.5f),
			FVector(Length, Thickness, Thickness) / 100.0f));

		SparkInstances->SetCustomDataValue(Index, 0, Life * Life);
	}

	SparkInstances->BatchUpdateInstancesTransforms(0, Transforms, false, true, true);

	ShownSparks = Sparks.Num();
	SparkInstances->SetVisibility(ShownSparks > 0);
}
