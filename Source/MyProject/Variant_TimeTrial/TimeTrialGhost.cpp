// Copyright (c) 2026 skykunking-gif. All Rights Reserved.


#include "TimeTrialGhost.h"
#include "Components/LightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MyProject.h"

FTransform FGhostTrack::Sample(float Time) const
{
	if (!IsValid())
	{
		return FTransform::Identity;
	}

	const float Position = FMath::Clamp(Time / SampleInterval, 0.0f, static_cast<float>(Locations.Num() - 1));
	const int32 Index = FMath::Min(FMath::FloorToInt(Position), Locations.Num() - 2);

	if (Index < 0)
	{
		// a single sample
		return FTransform(FQuat(Rotations[0]), FVector(Locations[0]));
	}

	const float Alpha = Position - Index;

	const FVector Location = FMath::Lerp(FVector(Locations[Index]), FVector(Locations[Index + 1]), Alpha);
	const FQuat Rotation = FQuat::Slerp(FQuat(Rotations[Index]), FQuat(Rotations[Index + 1]), Alpha);

	return FTransform(Rotation, Location);
}

void FGhostRecorder::Start(float SampleInterval, const FTransform& StartTransform)
{
	Track = FGhostTrack();
	Track.SampleInterval = FMath::Max(SampleInterval, 0.001f);

	bRecording = true;
	LastTime = 0.0f;
	LastTransform = StartTransform;

	AddSample(StartTransform.GetLocation(), StartTransform.GetRotation());
	NextSampleTime = Track.SampleInterval;
}

void FGhostRecorder::AddFrame(float Time, const FTransform& Transform)
{
	if (!bRecording || Time <= LastTime)
	{
		return;
	}

	// every sample that fell due since the last frame, placed between that frame and this one
	while (NextSampleTime <= Time)
	{
		const float Alpha = (NextSampleTime - LastTime) / (Time - LastTime);

		AddSample(FMath::Lerp(LastTransform.GetLocation(), Transform.GetLocation(), Alpha),
			FQuat::Slerp(LastTransform.GetRotation(), Transform.GetRotation(), Alpha));

		NextSampleTime += Track.SampleInterval;
	}

	LastTime = Time;
	LastTransform = Transform;
}

void FGhostRecorder::AddSample(const FVector& Location, const FQuat& Rotation)
{
	Track.Locations.Add(FVector3f(Location));
	Track.Rotations.Add(FQuat4f(Rotation.GetNormalized()));
}

ATimeTrialGhostCar::ATimeTrialGhostCar()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	FallbackMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FallbackMesh"));
	FallbackMesh->SetupAttachment(RootComponent);
	FallbackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FallbackMesh->SetCastShadow(false);
}

void ATimeTrialGhostCar::SetUp(const FGhostTrack& InTrack, TSubclassOf<AActor> BodyClass, USkeletalMesh* FallbackMeshAsset,
	UMaterialInterface* Material, const FLinearColor& Color, float Opacity)
{
	Track = InTrack;
	bPlaying = false;

	// on the grid until the race starts
	SetActorTransform(Track.Sample(0.0f));

	if (BodyClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		BodyActor = GetWorld()->SpawnActor<AActor>(BodyClass, GetActorTransform(), SpawnParams);
	}

	if (BodyActor)
	{
		// the body rides along with the ghost, so every part of it has to be allowed to move
		TInlineComponentArray<USceneComponent*> BodyComponents(BodyActor.Get());

		for (USceneComponent* Component : BodyComponents)
		{
			Component->SetMobility(EComponentMobility::Movable);
		}

		BodyActor->SetActorEnableCollision(false);
		BodyActor->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

		FallbackMesh->SetVisibility(false);
		MakeGhostly(BodyActor, Material, Color, Opacity);

	} else {

		FallbackMesh->SetSkeletalMesh(FallbackMeshAsset);
		MakeGhostly(this, Material, Color, Opacity);
	}
}

void ATimeTrialGhostCar::MakeGhostly(AActor* Actor, UMaterialInterface* Material, const FLinearColor& Color, float Opacity)
{
	UMaterialInstanceDynamic* GhostMaterial = Material ? UMaterialInstanceDynamic::Create(Material, this) : nullptr;

	if (GhostMaterial)
	{
		GhostMaterial->SetVectorParameterValue(TEXT("Glass Color"), Color);
		GhostMaterial->SetScalarParameterValue(TEXT("Base Glass Opacity"), Opacity);

	} else {

		UE_LOG(LogMyProject, Warning, TEXT("[Ghost] No ghost material set, so the ghost keeps the car's own paint"));
	}

	TInlineComponentArray<USceneComponent*> Components(Actor);

	for (USceneComponent* Component : Components)
	{
		// the showcase cars come with their headlights on
		if (Component->IsA<ULightComponent>())
		{
			Component->SetVisibility(false);
			continue;
		}

		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);

		if (!Primitive)
		{
			continue;
		}

		Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Primitive->SetCastShadow(false);

		if (GhostMaterial)
		{
			for (int32 Slot = 0; Slot < Primitive->GetNumMaterials(); ++Slot)
			{
				Primitive->SetMaterial(Slot, GhostMaterial);
			}
		}
	}
}

void ATimeTrialGhostCar::StartPlayback(float StartTime)
{
	PlaybackStartTime = StartTime;
	bPlaying = true;
}

void ATimeTrialGhostCar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bPlaying)
	{
		return;
	}

	const float Time = GetWorld()->GetTimeSeconds() - PlaybackStartTime;

	// the ghost's race is over: it leaves the track rather than parking on the line
	if (Time > Track.GetDuration())
	{
		bPlaying = false;
		SetActorHiddenInGame(true);

		if (BodyActor)
		{
			BodyActor->SetActorHiddenInGame(true);
		}

		return;
	}

	SetActorTransform(Track.Sample(Time));
}

void ATimeTrialGhostCar::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BodyActor)
	{
		BodyActor->Destroy();
		BodyActor = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}
