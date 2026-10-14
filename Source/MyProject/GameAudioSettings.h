// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameAudioSettings.generated.h"

/**
 *  The player's volume settings, set on the options screen. Each is a level from 0 (silent) to MaxLevel (as loud
 *  as the game mixes it). Kept in GameUserSettings.ini so they last from one session to the next.
 */
UCLASS(Config=GameUserSettings)
class UGameAudioSettings : public UObject
{
	GENERATED_BODY()

public:

	/** Highest volume level */
	static constexpr int32 MaxLevel = 10;

	/** Returns the music volume level, 0 to MaxLevel */
	UFUNCTION(BlueprintPure, Category="Audio Settings")
	static int32 GetMusicLevel();

	/** Sets the music volume level, saves it and tells the listeners */
	UFUNCTION(BlueprintCallable, Category="Audio Settings")
	static void SetMusicLevel(int32 NewLevel);

	/** Returns the engine sound volume level, 0 to MaxLevel */
	UFUNCTION(BlueprintPure, Category="Audio Settings")
	static int32 GetEngineLevel();

	/** Sets the engine sound volume level, saves it and tells the listeners */
	UFUNCTION(BlueprintCallable, Category="Audio Settings")
	static void SetEngineLevel(int32 NewLevel);

	/** Volume multiplier for the music level */
	static float GetMusicVolume() { return LevelToVolume(GetMusicLevel()); }

	/** Volume multiplier for the engine sound level */
	static float GetEngineVolume() { return LevelToVolume(GetEngineLevel()); }

	/** Broadcast whenever a volume level changes */
	static FSimpleMulticastDelegate& OnChanged();

protected:

	/** Turns a level into a volume multiplier. Squared, so each step sounds about as big as the last */
	static float LevelToVolume(int32 Level);

	/** Music volume level */
	UPROPERTY(Config)
	int32 MusicLevel = MaxLevel;

	/** Engine sound volume level */
	UPROPERTY(Config)
	int32 EngineLevel = MaxLevel;
};
