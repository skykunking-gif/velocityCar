// Copyright Epic Games, Inc. All Rights Reserved.


#include "TimeTrialRecords.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "MyProject.h"

const FString UTimeTrialRecords::SlotName = TEXT("Records");

void UTimeTrialRecords::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		SaveData = Cast<UTimeTrialSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));

		if (!SaveData)
		{
			UE_LOG(LogMyProject, Warning, TEXT("[Records] Couldn't read save slot %s. Starting with no records; the file is overwritten once one is set."), *SlotName);
		}
	}

	if (!SaveData)
	{
		SaveData = Cast<UTimeTrialSaveGame>(UGameplayStatics::CreateSaveGameObject(UTimeTrialSaveGame::StaticClass()));
	}

	UE_LOG(LogMyProject, Log, TEXT("[Records] %d course(s) with records"), SaveData->CourseRecords.Num());
}

UTimeTrialRecords* UTimeTrialRecords::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;

	return GameInstance ? GameInstance->GetSubsystem<UTimeTrialRecords>() : nullptr;
}

FCourseRecord UTimeTrialRecords::GetCourseRecord(FName CourseId) const
{
	const FCourseRecord* Record = SaveData->CourseRecords.Find(CourseId);

	return Record ? *Record : FCourseRecord();
}

float UTimeTrialRecords::GetBestTotal(FName CourseId, int32 Laps) const
{
	const FCourseRecord* Record = SaveData->CourseRecords.Find(CourseId);
	const FTimeRecord* Total = Record ? Record->BestTotals.Find(Laps) : nullptr;

	return Total ? Total->Time : -1.0f;
}

float UTimeTrialRecords::GetBestLap(FName CourseId) const
{
	const FCourseRecord* Record = SaveData->CourseRecords.Find(CourseId);

	return Record ? Record->BestLap.Time : -1.0f;
}

FRaceRecordUpdate UTimeTrialRecords::SubmitRace(FName CourseId, const TArray<float>& LapTimes, const FString& CarName)
{
	FRaceRecordUpdate Update;

	if (CourseId.IsNone() || LapTimes.IsEmpty())
	{
		return Update;
	}

	FCourseRecord& Record = SaveData->CourseRecords.FindOrAdd(CourseId);
	FTimeRecord& BestTotal = Record.BestTotals.FindOrAdd(LapTimes.Num());

	float TotalTime = 0.0f;
	float FastestLap = -1.0f;

	for (const float LapTime : LapTimes)
	{
		TotalTime += LapTime;

		if (FastestLap < 0.0f || LapTime < FastestLap)
		{
			FastestLap = LapTime;
		}
	}

	if (!BestTotal.IsSet() || TotalTime < BestTotal.Time)
	{
		BestTotal.Time = TotalTime;
		BestTotal.CarName = CarName;
		Update.bNewBestTotal = true;
	}

	if (!Record.BestLap.IsSet() || FastestLap < Record.BestLap.Time)
	{
		Record.BestLap.Time = FastestLap;
		Record.BestLap.CarName = CarName;
		Update.bNewBestLap = true;
	}

	Update.BestTotal = BestTotal.Time;
	Update.BestLap = Record.BestLap.Time;

	UE_LOG(LogMyProject, Log, TEXT("[Records] %s, %d laps: total %.3f%s, best lap %.3f%s"),
		*CourseId.ToString(), LapTimes.Num(),
		Update.BestTotal, Update.bNewBestTotal ? TEXT(" (new)") : TEXT(""),
		Update.BestLap, Update.bNewBestLap ? TEXT(" (new)") : TEXT(""));

	if (Update.bNewBestTotal || Update.bNewBestLap)
	{
		Save();
	}

	return Update;
}

bool UTimeTrialRecords::HasBeatenNpcLevel(FName LevelId) const
{
	return !LevelId.IsNone() && SaveData->BeatenNpcLevels.Contains(LevelId);
}

bool UTimeTrialRecords::SubmitNpcWin(FName LevelId)
{
	if (LevelId.IsNone() || SaveData->BeatenNpcLevels.Contains(LevelId))
	{
		return false;
	}

	SaveData->BeatenNpcLevels.Add(LevelId);

	UE_LOG(LogMyProject, Log, TEXT("[Records] First win against NPC level %s"), *LevelId.ToString());

	Save();

	return true;
}

void UTimeTrialRecords::Save()
{
	// a few hundred bytes, so writing it there and then doesn't hold up the frame
	if (!UGameplayStatics::SaveGameToSlot(SaveData, SlotName, 0))
	{
		UE_LOG(LogMyProject, Warning, TEXT("[Records] Couldn't write save slot %s"), *SlotName);
	}
}

FString UTimeTrialRecords::GetGhostSlotName(FName CourseId, int32 Laps)
{
	return FString::Printf(TEXT("Ghost_%s_%dLaps"), *CourseId.ToString(), Laps);
}

bool UTimeTrialRecords::FindGhost(FName CourseId, int32 Laps, FGhostTrack& OutTrack)
{
	const FString GhostSlot = GetGhostSlotName(CourseId, Laps);

	FGhostTrack* Ghost = Ghosts.Find(GhostSlot);

	// read the file the first time the ghost is asked for
	if (!Ghost)
	{
		Ghost = &Ghosts.Add(GhostSlot);

		if (UGameplayStatics::DoesSaveGameExist(GhostSlot, 0))
		{
			if (const UTimeTrialGhostSave* GhostSave = Cast<UTimeTrialGhostSave>(UGameplayStatics::LoadGameFromSlot(GhostSlot, 0)))
			{
				*Ghost = GhostSave->Track;

			} else {

				UE_LOG(LogMyProject, Warning, TEXT("[Records] Couldn't read ghost slot %s"), *GhostSlot);
			}
		}
	}

	if (!Ghost->IsValid())
	{
		return false;
	}

	OutTrack = *Ghost;
	return true;
}

bool UTimeTrialRecords::SubmitGhost(FName CourseId, int32 Laps, const FGhostTrack& Track)
{
	if (CourseId.IsNone() || !Track.IsValid() || Track.TotalTime < 0.0f)
	{
		return false;
	}

	// the ghost there now, if any, loaded so the two can be compared
	FGhostTrack Current;

	if (FindGhost(CourseId, Laps, Current) && Current.TotalTime >= 0.0f && Current.TotalTime <= Track.TotalTime)
	{
		return false;
	}

	const FString GhostSlot = GetGhostSlotName(CourseId, Laps);

	Ghosts.Add(GhostSlot, Track);

	UTimeTrialGhostSave* GhostSave = Cast<UTimeTrialGhostSave>(UGameplayStatics::CreateSaveGameObject(UTimeTrialGhostSave::StaticClass()));
	GhostSave->Track = Track;

	// a few hundred kilobytes: it's turned into bytes here, and written out away from the game thread
	UGameplayStatics::AsyncSaveGameToSlot(GhostSave, GhostSlot, 0, FAsyncSaveGameToSlotDelegate::CreateLambda([](const FString& SavedSlot, const int32, bool bSuccess)
	{
		if (!bSuccess)
		{
			UE_LOG(LogMyProject, Warning, TEXT("[Records] Couldn't write ghost slot %s"), *SavedSlot);
		}
	}));

	UE_LOG(LogMyProject, Log, TEXT("[Records] New ghost for %s, %d laps: %.3f s, %d samples"), *CourseId.ToString(), Laps, Track.TotalTime, Track.Locations.Num());

	return true;
}
