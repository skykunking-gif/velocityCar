// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameGraphicsSettings.generated.h"

/**
 *  The player's picture quality, set on the options screen. One level from 0 (low, lightest to run) to MaxLevel
 *  (the full look), which sets every scalability group at once: render resolution, view distance, shadows,
 *  global illumination, reflections, post processing and so on.
 *  The engine keeps it in GameUserSettings.ini and puts it back when the game starts.
 */
UCLASS()
class UGameGraphicsSettings : public UObject
{
	GENERATED_BODY()

public:

	/** Highest quality level */
	static constexpr int32 MaxLevel = 3;

	/** Returns the quality level, 0 to MaxLevel. Groups left at mixed levels count as the highest */
	UFUNCTION(BlueprintPure, Category="Graphics Settings")
	static int32 GetQualityLevel();

	/** Sets the quality level, applies it at once and saves it */
	UFUNCTION(BlueprintCallable, Category="Graphics Settings")
	static void SetQualityLevel(int32 NewLevel);

	/** Returns the name the options screen shows for a quality level */
	static FText GetQualityName(int32 Level);

	/** On the game's first run, picks the quality level from a quick hardware benchmark, so a slower PC doesn't start
	 *  on the heaviest settings. Does nothing once a benchmark has run, or in the editor */
	static void DetectQualityOnFirstRun();
};
