// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DrivingSettings.generated.h"

/**
 *  How the players drive, picked on the car select screen. Kept in GameUserSettings.ini so the choice lasts
 *  from one session to the next, and through the level reload that takes the game back to the title screen.
 *  The second player of a two player game has a gearbox setting of their own.
 */
UCLASS(Config=GameUserSettings)
class UDrivingSettings : public UObject
{
	GENERATED_BODY()

public:

	/** Returns true if the player's gearbox shifts by itself (AT), false if the player shifts by hand (MT).
	 *  PlayerIndex is the local player: 0 for the first, 1 for the second of a two player game */
	UFUNCTION(BlueprintPure, Category="Driving Settings")
	static bool IsAutomaticTransmission(int32 PlayerIndex = 0);

	/** Picks the automatic or the manual gearbox for a player, saves it and tells the listeners */
	UFUNCTION(BlueprintCallable, Category="Driving Settings")
	static void SetAutomaticTransmission(bool bAutomatic, int32 PlayerIndex = 0);

	/** Broadcast whenever a setting changes */
	static FSimpleMulticastDelegate& OnChanged();

protected:

	/** True for the automatic gearbox. Automatic to start with, so a first race needs no gear changes */
	UPROPERTY(Config)
	bool bAutomaticTransmission = true;

	/** The second player's gearbox in a two player game */
	UPROPERTY(Config)
	bool bSecondPlayerAutomaticTransmission = true;
};
