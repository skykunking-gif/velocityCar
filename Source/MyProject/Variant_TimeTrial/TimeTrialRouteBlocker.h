// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimeTrialRouteBlocker.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 *  A barrier that closes off a road for every race except those on the routes listed in OpenForRoutes.
 *  Place it across a road near a branch point, with the actor's X axis pointing along the road.
 *  It becomes solid and visible whenever the selected race must not use that road, and
 *  disappears (no collision) for races that do.
 */
UCLASS()
class ATimeTrialRouteBlocker : public AActor
{
	GENERATED_BODY()

	/** The barrier wall */
	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* BarrierMesh;

protected:

	/** Routes that are allowed through. The barrier is active for any race on a route not in this list. */
	UPROPERTY(EditAnywhere, Category="Route Blocker")
	TArray<int32> OpenForRoutes;

	/** Barrier width across the road, in cm */
	UPROPERTY(EditAnywhere, Category="Route Blocker", meta = (Units = "cm"))
	float Width = 1600.0f;

	/** Barrier height, in cm */
	UPROPERTY(EditAnywhere, Category="Route Blocker", meta = (Units = "cm"))
	float Height = 300.0f;

	/** Barrier thickness along the road, in cm */
	UPROPERTY(EditAnywhere, Category="Route Blocker", meta = (Units = "cm"))
	float Thickness = 100.0f;

	/** Barrier color */
	UPROPERTY(EditAnywhere, Category="Route Blocker")
	FLinearColor BarrierColor = FLinearColor(1.0f, 0.08f, 0.08f);

	/** Material for the barrier. Should expose "Color" and "Intensity" parameters. */
	UPROPERTY(EditAnywhere, Category="Route Blocker")
	TObjectPtr<UMaterialInterface> BarrierMaterial;

	/** Dynamic material instance driving the barrier color */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BarrierMID;

public:

	/** Constructor */
	ATimeTrialRouteBlocker();

	/** Returns true if the barrier lets races on the given route through */
	UFUNCTION(BlueprintPure, Category="Route Blocker")
	bool IsOpenForRoute(int32 Route) const { return OpenForRoutes.Contains(Route); };

protected:

	/** Sizes the wall in the editor whenever the actor is placed or edited */
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Raises or lowers the barrier for the newly selected race */
	UFUNCTION()
	void OnSelectedRaceChanged(int32 RaceIndex);

	/** Applies width, height and thickness to the wall mesh */
	void UpdateBarrierShape();
};
