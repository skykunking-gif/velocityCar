// Copyright (c) 2026 skykunking-gif. All Rights Reserved.


#include "TimeTrialRouteBlocker.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "TimeTrialGameMode.h"

ATimeTrialRouteBlocker::ATimeTrialRouteBlocker()
{
	PrimaryActorTick.bCanEverTick = false;

	// create the root component
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// create the wall. The cube mesh is 100cm on each side; X runs along the road, Y across it
	BarrierMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Barrier"));
	BarrierMesh->SetupAttachment(RootComponent);
	BarrierMesh->SetCollisionProfileName(FName("BlockAll"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube"));
	if (CubeMesh.Succeeded())
	{
		BarrierMesh->SetStaticMesh(CubeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WallMaterial(TEXT("/Game/Variant_TimeTrial/Materials/M_RouteBlocker"));
	if (WallMaterial.Succeeded())
	{
		BarrierMaterial = WallMaterial.Object;
	}

	UpdateBarrierShape();
}

void ATimeTrialRouteBlocker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	UpdateBarrierShape();

	if (BarrierMesh && BarrierMaterial)
	{
		BarrierMesh->SetMaterial(0, BarrierMaterial);
	}
}

void ATimeTrialRouteBlocker::BeginPlay()
{
	Super::BeginPlay();

	if (BarrierMesh && BarrierMaterial)
	{
		BarrierMID = BarrierMesh->CreateDynamicMaterialInstance(0, BarrierMaterial);

		if (BarrierMID)
		{
			BarrierMID->SetVectorParameterValue(TEXT("Color"), BarrierColor);
		}
	}

	// follow the selected race. The game mode re-broadcasts in its own BeginPlay, so initialization order doesn't matter
	if (ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->OnSelectedRaceChanged.AddDynamic(this, &ATimeTrialRouteBlocker::OnSelectedRaceChanged);
		OnSelectedRaceChanged(GM->GetSelectedRace());
	}
}

void ATimeTrialRouteBlocker::OnSelectedRaceChanged(int32 RaceIndex)
{
	if (!BarrierMesh)
	{
		return;
	}

	// open roads have no wall at all; closed roads get a solid, visible one
	const ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(GetWorld()->GetAuthGameMode());
	const bool bOpen = GM && IsOpenForRoute(GM->GetRouteForRace(RaceIndex));

	BarrierMesh->SetHiddenInGame(bOpen);
	BarrierMesh->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
}

void ATimeTrialRouteBlocker::UpdateBarrierShape()
{
	if (!BarrierMesh)
	{
		return;
	}

	BarrierMesh->SetRelativeScale3D(FVector(Thickness, Width, Height) / 100.0f);
	BarrierMesh->SetRelativeLocation(FVector(0.0f, 0.0f, Height * 0.5f));
}
