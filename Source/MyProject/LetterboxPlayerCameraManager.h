// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "MyProject.h"
#include "LetterboxPlayerCameraManager.generated.h"

/**
 *  Player camera manager that constrains the rendered view to a fixed aspect ratio, adding black
 *  bars when the window doesn't match. Forcing it here instead of on the individual camera
 *  components covers every view target, including the title screen cine camera and any camera a
 *  Blueprint swaps in, and can't be accidentally overridden by a Blueprint's component defaults.
 */
UCLASS()
class ALetterboxPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

protected:

	/** Aspect ratio the view is locked to, for a player who has the whole screen. A player with half of a split screen
	 *  gets a view twice as wide for its height. Must match the UI box in ULetterboxGameViewportClient. */
	UPROPERTY(EditDefaultsOnly, Category="Letterbox")
	float LockedAspectRatio = MyProjectTargetAspectRatio;

	/** Applies the aspect ratio lock on top of whatever the view target asked for */
	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;
};
