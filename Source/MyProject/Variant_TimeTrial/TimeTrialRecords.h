// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimeTrialGhost.h"
#include "TimeTrialRecords.generated.h"

/**
 *  One record time, and the car it was set in
 */
USTRUCT(BlueprintType)
struct FTimeRecord
{
	GENERATED_BODY()

	/** The time, in seconds. Negative until the record has been set */
	UPROPERTY(BlueprintReadOnly, Category="Records")
	float Time = -1.0f;

	/** Name of the car the record was set in, as the car select screen showed it */
	UPROPERTY(BlueprintReadOnly, Category="Records")
	FString CarName;

	/** True once a time has been set */
	bool IsSet() const { return Time >= 0.0f; }
};

/**
 *  The records of one course
 */
USTRUCT(BlueprintType)
struct FCourseRecord
{
	GENERATED_BODY()

	/** Fastest single lap, from any race on the course */
	UPROPERTY(BlueprintReadOnly, Category="Records")
	FTimeRecord BestLap;

	/** Fastest race from start to finish, keyed by the number of laps, since a race of fewer laps is shorter */
	UPROPERTY(BlueprintReadOnly, Category="Records")
	TMap<int32, FTimeRecord> BestTotals;
};

/**
 *  What a finished race did to the records of its course
 */
USTRUCT(BlueprintType)
struct FRaceRecordUpdate
{
	GENERATED_BODY()

	/** True if the race beat the course's best total time for its number of laps, or set the first one */
	UPROPERTY(BlueprintReadOnly, Category="Records")
	bool bNewBestTotal = false;

	/** True if one of the race's laps beat the course's best lap, or set the first one */
	UPROPERTY(BlueprintReadOnly, Category="Records")
	bool bNewBestLap = false;

	/** The course's best total time for the race's number of laps, after the race */
	UPROPERTY(BlueprintReadOnly, Category="Records")
	float BestTotal = -1.0f;

	/** The course's best lap, after the race */
	UPROPERTY(BlueprintReadOnly, Category="Records")
	float BestLap = -1.0f;
};

/**
 *  What gets written to the save file. Anything added later goes here too, such as the grand prix progress.
 */
UCLASS()
class UTimeTrialSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	/** Version of the file layout, for converting old files should the layout change */
	UPROPERTY()
	int32 SaveVersion = 1;

	/** Records of every course raced so far, keyed by course ID (see ATimeTrialGameMode::GetCourseId) */
	UPROPERTY()
	TMap<FName, FCourseRecord> CourseRecords;

	/** NPC levels the player has won a race against, by FNpcDifficulty::Id */
	UPROPERTY()
	TSet<FName> BeatenNpcLevels;
};

/**
 *  Keeps the player's records. Loads the save file when the game starts and writes it whenever a record is broken.
 *  Also keeps the ghost of the fastest race on each course, in a file of its own per course and lap count.
 *  Lives as long as the game does, so going back to the title screen, which reloads the level, keeps it.
 */
UCLASS()
class UTimeTrialRecords : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** Save slot the records are written to, as Saved/SaveGames/<slot>.sav */
	static const FString SlotName;

	/** Loads the save file, or starts empty if there isn't one */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Returns the records subsystem of the game the object is in, or null */
	static UTimeTrialRecords* Get(const UObject* WorldContextObject);

	/** Returns the records of a course. Courses never raced have no times set */
	UFUNCTION(BlueprintPure, Category="Records")
	FCourseRecord GetCourseRecord(FName CourseId) const;

	/** Returns the best total time of a course for a number of laps, or a negative time if there isn't one */
	UFUNCTION(BlueprintPure, Category="Records")
	float GetBestTotal(FName CourseId, int32 Laps) const;

	/** Returns the best lap of a course, or a negative time if there isn't one */
	UFUNCTION(BlueprintPure, Category="Records")
	float GetBestLap(FName CourseId) const;

	/** Checks a finished race against the course's records, keeps any it broke and saves them.
	 *  LapTimes holds every lap of the race in order; their sum is the total time */
	UFUNCTION(BlueprintCallable, Category="Records")
	FRaceRecordUpdate SubmitRace(FName CourseId, const TArray<float>& LapTimes, const FString& CarName);

	/** Finds the ghost of the fastest race recorded on a course over a number of laps. Returns false if there isn't one */
	bool FindGhost(FName CourseId, int32 Laps, FGhostTrack& OutTrack);

	/** Keeps a finished race as the course's ghost if it beat the one there, or there wasn't one, and saves it.
	 *  Returns true if it was kept */
	bool SubmitGhost(FName CourseId, int32 Laps, const FGhostTrack& Track);

	/** Returns true if the player has won a race against the NPC level (see FNpcDifficulty::Id) */
	UFUNCTION(BlueprintPure, Category="Records")
	bool HasBeatenNpcLevel(FName LevelId) const;

	/** Remembers that the player won a race against the NPC level, and saves it. Returns true the first time */
	bool SubmitNpcWin(FName LevelId);

protected:

	/** Writes the save file */
	void Save();

	/** Save slot of a course's ghost */
	static FString GetGhostSlotName(FName CourseId, int32 Laps);

	/** Ghosts loaded or recorded so far, keyed by save slot. A slot found to have no file is kept as an empty track */
	TMap<FString, FGhostTrack> Ghosts;

	/** Everything in the save file */
	UPROPERTY(Transient)
	TObjectPtr<UTimeTrialSaveGame> SaveData;
};
