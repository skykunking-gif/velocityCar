// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimeTrialTrackGate.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 *  A track gate volume for a Time Trial racing game.
 *  Players must pass through the track gates in order to complete a lap.
 *  Each gate shows a colored marker while it is part of the selected race route,
 *  highlighted more strongly while it is the player's current target.
 */
UCLASS(abstract)
class ATimeTrialTrackGate : public AActor
{
	GENERATED_BODY()

	/** Collision Box */
	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UBoxComponent* CollisionBox;

	/** Visible marker standing across the road. Colored with the selected race's color and hidden while the gate is not on the route.
	 *  Turned to the gate actor's own yaw, so place each gate with its forward axis pointing along the road. */
	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* RouteMarker;

protected:

	/** If this is set to true, this track gate is considered the finish line and will increase the lap when passed */
	UPROPERTY(EditAnywhere, Category="Track Gate")
	bool bIsFinishLine = false;

	/** Pointer to the next track marker in the sequence */
	UPROPERTY(EditAnywhere, Category="Track Gate")
	ATimeTrialTrackGate* NextMarker;

	/** Per-route override for the next track marker. Used at branch points where the routes part ways.
	 *  Keyed by route; falls back to NextMarker for routes without an entry. Links run the way the route is
	 *  driven forwards: a race that runs its route the other way round takes them in reverse */
	UPROPERTY(EditAnywhere, Category="Track Gate")
	TMap<int32, ATimeTrialTrackGate*> NextMarkerOverrides;

	/** Material for the route marker. Should expose "Color", "Intensity" and "Opacity" parameters. */
	UPROPERTY(EditAnywhere, Category="Track Gate|Marker")
	TObjectPtr<UMaterialInterface> MarkerMaterial;

	/** Width of the route marker across the road, in cm */
	UPROPERTY(EditAnywhere, Category="Track Gate|Marker", meta = (Units = "cm"))
	float MarkerWidth = 2400.0f;

	/** Height of the route marker, in cm */
	UPROPERTY(EditAnywhere, Category="Track Gate|Marker", meta = (Units = "cm"))
	float MarkerHeight = 600.0f;

	/** Emissive intensity of the marker while it is a player's current target */
	UPROPERTY(EditAnywhere, Category="Track Gate|Marker")
	float TargetIntensity = 6.0f;

	/** Emissive intensity of markers on the route that are not the current target */
	UPROPERTY(EditAnywhere, Category="Track Gate|Marker")
	float RouteIntensity = 1.5f;

	/** Opacity of the marker while it is a player's current target */
	UPROPERTY(EditAnywhere, Category="Track Gate|Marker")
	float TargetOpacity = 0.55f;

	/** Opacity of markers on the route that are not the current target */
	UPROPERTY(EditAnywhere, Category="Track Gate|Marker")
	float RouteOpacity = 0.2f;

	/** Dynamic material instance driving the marker color and highlight */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MarkerMID;

public:

	/** Constructor */
	ATimeTrialTrackGate();

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Handle collision */
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

	/** Shows, orients and colors the marker for the newly selected race, or hides it if the gate is not on that route */
	UFUNCTION()
	void OnSelectedRaceChanged(int32 RaceIndex);

public:

	/** Returns the next marker on the track */
	ATimeTrialTrackGate* GetNextMarker() const;

	/** Returns the next marker on a route driven forwards, respecting any per-route override */
	ATimeTrialTrackGate* GetNextMarkerForRoute(int32 Route) const;

	/** Returns true if this gate is the finish line */
	bool IsFinishLine() const { return bIsFinishLine; };

	/** Refreshes the marker highlight based on whether any local player is currently targeting this gate */
	void RefreshTargetHighlight();

	/** Registers the player as having passed this gate: advances their target to the next gate on the
	 *  selected route, and counts a lap if this is the finish line. Called on overlap while this gate
	 *  is the player's target, and also when the gate becomes the target while the player is already inside it. */
	void HandlePlayerPass(class ATimeTrialPlayerController* PC);
};
