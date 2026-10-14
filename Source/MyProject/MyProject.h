// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Main log category used across the project */
DECLARE_LOG_CATEGORY_EXTERN(LogMyProject, Log, All);

/** Aspect ratio the game view and the UI are both locked to. See ALetterboxPlayerCameraManager
 *  for the 3D view and ULetterboxGameViewportClient for the widgets. */
inline constexpr float MyProjectTargetAspectRatio = 16.0f / 9.0f;
class AController;

/** Returns which local player a controller belongs to: 0 for the first player, 1 for the second of a two player
 *  game. Anything that isn't a local player's controller, such as an NPC's, counts as 0 */
int32 GetLocalPlayerIndex(const AController* Controller);
