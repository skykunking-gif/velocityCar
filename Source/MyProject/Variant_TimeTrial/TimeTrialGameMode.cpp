// Copyright Epic Games, Inc. All Rights Reserved.


#include "TimeTrialGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "TimeTrialTrackGate.h"
#include "TimeTrialRecords.h"
#include "TachometerStyle.h"
#include "EngineSoundProfile.h"
#include "EngineSoundComponent.h"
#include "CollisionFxComponent.h"
#include "CollisionSoundComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "Algo/Reverse.h"
#include "EngineUtils.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PostProcessVolume.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "MyProject.h"
#include "GameGraphicsSettings.h"
#include "TwoPlayerInput.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

ATimeTrialGameMode::ATimeTrialGameMode()
{
	// the level's three routes, routes 1 and 3 driven both ways round, three laps each: route 1 red, route 2 blue,
	// route 3 yellow
	FTimeTrialRaceDefinition Race1;
	Race1.DisplayName = NSLOCTEXT("TimeTrialGameMode", "Race1", "コース1");
	Race1.Route = 1;
	Race1.Color = FLinearColor(1.0f, 0.1f, 0.1f);

	FTimeTrialRaceDefinition Race2;
	Race2.DisplayName = NSLOCTEXT("TimeTrialGameMode", "Race2", "コース2");
	Race2.Route = 2;
	Race2.Color = FLinearColor(0.1f, 0.45f, 1.0f);

	FTimeTrialRaceDefinition Race3;
	Race3.DisplayName = NSLOCTEXT("TimeTrialGameMode", "Race3", "コース3");
	Race3.Route = 3;
	Race3.Color = FLinearColor(1.0f, 0.85f, 0.1f);

	FTimeTrialRaceDefinition Race1Reversed = Race1;
	Race1Reversed.bReversed = true;

	// route 2 can't be driven the other way round, so it's only on offer forwards
	FTimeTrialRaceDefinition Race3Reversed = Race3;
	Race3Reversed.bReversed = true;

	Races = { Race1, Race1Reversed, Race2, Race3, Race3Reversed };

	// times of day, earliest first. Daytime is the level as it was lit before there was a choice, and keeps the
	// records set back then
	FTimeTrialTimeOfDay Morning;
	Morning.DisplayName = NSLOCTEXT("TimeTrialGameMode", "Morning", "朝");
	Morning.CourseIdSuffix = TEXT("Morning");
	Morning.SunRotation = FRotator(-10.0f, 150.0f, 0.0f);
	Morning.SunIntensity = 6.0f;
	Morning.SunTemperature = 5000.0f;
	Morning.SkyLightIntensity = 1.0f;

	FTimeTrialTimeOfDay Day;
	Day.DisplayName = NSLOCTEXT("TimeTrialGameMode", "Day", "昼");
	Day.SunRotation = FRotator(-49.5f, -10.3f, 0.0f);
	Day.SunIntensity = 6.0f;
	Day.SunTemperature = 6500.0f;
	Day.SkyLightIntensity = 1.0f;

	// the directional light stays above the horizon as moonlight: dim, blue, and the sky dims with it
	FTimeTrialTimeOfDay Night;
	Night.DisplayName = NSLOCTEXT("TimeTrialGameMode", "Night", "夜");
	Night.CourseIdSuffix = TEXT("Night");
	Night.SunRotation = FRotator(-45.0f, 60.0f, 0.0f);
	Night.SunIntensity = 0.1f;
	Night.SunTemperature = 12000.0f;
	Night.SkyLightIntensity = 0.5f;
	Night.ExposureBias = -2.0f;
	Night.bCarLights = true;

	TimesOfDay = { Morning, Day, Night };

	// NPC difficulty levels, easiest first
	FNpcDifficulty Easy;
	Easy.DisplayName = NSLOCTEXT("TimeTrialGameMode", "Easy", "EASY");
	Easy.Id = TEXT("Easy");
	Easy.MaxSpeed = 2600.0f;
	Easy.CornerLimit = 0.6f;
	Easy.BrakeDecel = 500.0f;
	Easy.MaxThrottle = 0.75f;

	FNpcDifficulty Normal;
	Normal.DisplayName = NSLOCTEXT("TimeTrialGameMode", "Normal", "NORMAL");
	Normal.Id = TEXT("Normal");
	Normal.MaxSpeed = 6000.0f;
	Normal.CornerLimit = 0.9f;
	Normal.BrakeDecel = 900.0f;
	Normal.MaxThrottle = 1.0f;
	Normal.bGroundedHandling = true;
	Normal.LookAheadTime = 0.6f;
	Normal.BrakeLookAheadTime = 0.4f;
	Normal.CarTorqueScale = 1.5f;
	Normal.CarGripScale = 1.6f;
	Normal.TargetLapTimes.Add(1, 180.0f);
	Normal.TargetLapTimes.Add(2, 135.0f);
	Normal.TargetLapTimes.Add(3, 55.0f);

	FNpcDifficulty Hard;
	Hard.DisplayName = NSLOCTEXT("TimeTrialGameMode", "Hard", "HARD");
	Hard.Id = TEXT("Hard");
	Hard.MaxSpeed = 7000.0f;
	Hard.CornerLimit = 0.95f;
	Hard.BrakeDecel = 1000.0f;
	Hard.MaxThrottle = 1.0f;
	Hard.PaceSpread = 0.02f;
	Hard.bGroundedHandling = true;
	Hard.LookAheadTime = 0.6f;
	Hard.BrakeLookAheadTime = 0.35f;
	Hard.CarTorqueScale = 1.5f;
	Hard.CarGripScale = 2.0f;
	Hard.TargetLapTimes.Add(1, 165.0f);
	Hard.TargetLapTimes.Add(2, 118.0f);
	Hard.TargetLapTimes.Add(3, 46.0f);
	Hard.SlipstreamAccel = 300.0f;
	Hard.CatchUpAccel = 600.0f;
	Hard.CatchUpFullGap = 30000.0f;
	Hard.PlayerCatchUpScale = 0.6f;
	Hard.LeadEase = 0.05f;

	// unlocked by winning against HARD
	FNpcDifficulty Extreme;
	Extreme.DisplayName = NSLOCTEXT("TimeTrialGameMode", "Extreme", "ELITE");
	Extreme.Id = TEXT("Extreme");
	Extreme.bLockedUntilPreviousBeaten = true;
	Extreme.MaxSpeed = 8000.0f;
	Extreme.CornerLimit = 0.95f;
	Extreme.BrakeDecel = 1200.0f;
	Extreme.MaxThrottle = 1.0f;
	Extreme.PaceSpread = 0.02f;
	Extreme.bGroundedHandling = true;
	Extreme.LookAheadTime = 0.6f;
	Extreme.CarTorqueScale = 3.0f;
	Extreme.BrakeLookAheadTime = 0.2f;
	Extreme.CarGripScale = 2.5f;
	Extreme.TargetLapTimes.Add(1, 144.0f);
	Extreme.TargetLapTimes.Add(2, 103.0f);
	Extreme.TargetLapTimes.Add(3, 36.0f);
	Extreme.SlipstreamAccel = 300.0f;
	Extreme.CatchUpAccel = 800.0f;
	Extreme.CatchUpFullGap = 30000.0f;
	Extreme.PlayerCatchUpScale = 0.6f;
	Extreme.LeadEase = 0.05f;

	// unlocked by winning against ELITE
	FNpcDifficulty Pro;
	Pro.DisplayName = NSLOCTEXT("TimeTrialGameMode", "Pro", "PRO");
	Pro.Id = TEXT("Pro");
	Pro.bLockedUntilPreviousBeaten = true;
	Pro.MaxSpeed = 9000.0f;
	Pro.CornerLimit = 0.95f;
	Pro.BrakeDecel = 1800.0f;
	Pro.MaxThrottle = 1.0f;
	Pro.PaceSpread = 0.02f;
	Pro.bGroundedHandling = true;
	Pro.LookAheadTime = 0.6f;
	Pro.CarTorqueScale = 5.0f;
	Pro.BrakeLookAheadTime = 0.2f;
	Pro.CarGripScale = 3.5f;
	Pro.TargetLapTimes.Add(1, 135.0f);
	Pro.TargetLapTimes.Add(2, 97.0f);
	Pro.TargetLapTimes.Add(3, 34.0f);
	Pro.SlipstreamAccel = 300.0f;
	Pro.CatchUpAccel = 800.0f;
	Pro.CatchUpFullGap = 30000.0f;
	Pro.PlayerCatchUpScale = 0.6f;
	Pro.LeadEase = 0.05f;

	NpcDifficulties = { Easy, Normal, Hard, Extreme, Pro };

	// race positions are tracked every frame
	PrimaryActorTick.bCanEverTick = true;
}

void ATimeTrialGameMode::BeginPlay()
{
	Super::BeginPlay();

	// on the first run, start at the picture quality this PC can handle
	UGameGraphicsSettings::DetectQualityOnFirstRun();

	// get the finish line marker
	TArray<AActor*> ActorList;

	UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), ATimeTrialTrackGate::StaticClass(), FinishTag, ActorList);

	if (ActorList.Num() > 0)
	{
		// get the first returned track marker that matches the tag
		FinishLineMarker = Cast<ATimeTrialTrackGate>(ActorList[0]);
	}

	// build the route for the default race so gates and blockers start in the right state,
	// regardless of whether they ran BeginPlay before or after the game mode
	RebuildActiveRoute();
	OnSelectedRaceChanged.Broadcast(SelectedRace);

	// light the level for the time of day, and give cars spawned from now on their lights too
	ApplyTimeOfDay();

	ActorSpawnedHandle = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ATimeTrialGameMode::OnActorSpawned));

	// create each additional local player.
	// Player 0 will be created automatically as part of regular game init
	for (int32 i = 2; i <= NumberOfLocalPlayers; ++i)
	{
		UGameplayStatics::CreatePlayer(GetWorld(), -1, true);
	}

}

void ATimeTrialGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}

	Super::EndPlay(EndPlayReason);
}

AActor* ATimeTrialGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// build the current player tag
	FName PlayerTag = FName(*FString::Printf(TEXT("Player%d"), CurrentPlayerStartAssignment));

	// find all player starts with the matching player tag
	TArray<AActor*> PlayerStarts;

	UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), APlayerStart::StaticClass(), PlayerTag, PlayerStarts);

	// increment the player start assignment index
	++CurrentPlayerStartAssignment;

	// if no PlayerStarts were found, default to all PlayerStarts instead
	if (PlayerStarts.IsEmpty())
	{
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), PlayerStarts);

		// race-specific starts are only used once a race has been chosen, so keep them out of the initial spawn
		PlayerStarts.RemoveAll([this](AActor* Start) { return IsRaceStart(Start); });
	}

	// have we found at least one PlayerStart?
	if (!PlayerStarts.IsEmpty())
	{
		return PlayerStarts[ FMath::RandRange(0, PlayerStarts.Num() - 1) ];
	}

	// no PlayerStarts in the level
	return nullptr;
}

APawn* ATimeTrialGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	FTransform Transform = SpawnTransform;

	// the second player joins with the first player's car already on the start, so their car goes in its place on
	// the grid, or failing that beside the first player's
	if (GetLocalPlayerIndex(NewPlayer) > 0 && !GetPlayerGridTransform(NewPlayer, Transform))
	{
		const APlayerController* FirstPlayer = GetWorld()->GetFirstPlayerController();

		if (const APawn* FirstCar = FirstPlayer ? FirstPlayer->GetPawn() : nullptr)
		{
			Transform = FTransform(FRotator(0.0f, FirstCar->GetActorRotation().Yaw, 0.0f),
				FirstCar->GetActorLocation() + FirstCar->GetActorRightVector() * 2.0f * GridLaneOffset + FVector(0.0f, 0.0f, 50.0f));
		}
	}

	return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, Transform);
}

ATimeTrialTrackGate* ATimeTrialGameMode::GetFinishLine() const
{
	return FinishLineMarker;
}

void ATimeTrialGameMode::SetSelectedRace(int32 NewRace)
{
	if (!FindRace(NewRace))
	{
		UE_LOG(LogMyProject, Warning, TEXT("[Race] There's no race %d; staying on race %d"), NewRace, SelectedRace);
		return;
	}

	SelectedRace = NewRace;

	// the route changed: recompute which gates are in play and let gates, blockers and players react
	RebuildActiveRoute();
	OnSelectedRaceChanged.Broadcast(SelectedRace);
}

int32 ATimeTrialGameMode::GetLapsForRace(int32 RaceIndex) const
{
	const FTimeTrialRaceDefinition* Race = FindRace(RaceIndex);

	return Race ? FMath::Max(Race->Laps, 1) : 0;
}

int32 ATimeTrialGameMode::GetRouteForRace(int32 RaceIndex) const
{
	const FTimeTrialRaceDefinition* Race = FindRace(RaceIndex);

	return Race ? Race->Route : 0;
}

bool ATimeTrialGameMode::IsRaceReversed(int32 RaceIndex) const
{
	const FTimeTrialRaceDefinition* Race = FindRace(RaceIndex);

	return Race && Race->bReversed;
}

FName ATimeTrialGameMode::GetCourseId(int32 RaceIndex, int32 TimeOfDay) const
{
	if (!FindRace(RaceIndex))
	{
		return NAME_None;
	}

	FString CourseId = FString::Printf(TEXT("Race%d%s"), GetRouteForRace(RaceIndex), IsRaceReversed(RaceIndex) ? TEXT("R") : TEXT(""));

	if (TimesOfDay.IsValidIndex(TimeOfDay) && !TimesOfDay[TimeOfDay].CourseIdSuffix.IsEmpty())
	{
		CourseId += TEXT("_") + TimesOfDay[TimeOfDay].CourseIdSuffix;
	}

	return FName(*CourseId);
}

void ATimeTrialGameMode::SetSelectedCar(int32 NewCar)
{
	if (!CarChoices.IsValidIndex(NewCar))
	{
		return;
	}

	SelectedCar = NewCar;

	OnSelectedCarChanged.Broadcast(SelectedCar);
}

void ATimeTrialGameMode::SetCarForPlayer(int32 PlayerIndex, int32 NewCar)
{
	if (PlayerIndex <= 0)
	{
		SetSelectedCar(NewCar);
		return;
	}

	if (!CarChoices.IsValidIndex(NewCar))
	{
		return;
	}

	SecondPlayerCar = NewCar;

	OnSelectedCarChanged.Broadcast(SecondPlayerCar);
}

const FTimeTrialCarChoice* ATimeTrialGameMode::GetCarChoiceForPlayer(int32 PlayerIndex) const
{
	const int32 Car = GetCarForPlayer(PlayerIndex);

	return CarChoices.IsValidIndex(Car) ? &CarChoices[Car] : nullptr;
}

TSubclassOf<AActor> ATimeTrialGameMode::GetSelectedCarBodyClass() const
{
	return CarChoices.IsValidIndex(SelectedCar) ? CarChoices[SelectedCar].BodyClass : nullptr;
}

UTachometerStyle* ATimeTrialGameMode::GetSelectedCarTachometerStyle() const
{
	return CarChoices.IsValidIndex(SelectedCar) ? CarChoices[SelectedCar].TachometerStyle.Get() : nullptr;
}

FVehicleHandling ATimeTrialGameMode::GetSelectedCarHandling() const
{
	return CarChoices.IsValidIndex(SelectedCar) ? CarChoices[SelectedCar].Handling : FVehicleHandling();
}

UEngineSoundProfile* ATimeTrialGameMode::GetSelectedCarEngineSound() const
{
	return CarChoices.IsValidIndex(SelectedCar) ? CarChoices[SelectedCar].EngineSound.Get() : nullptr;
}

FLinearColor ATimeTrialGameMode::GetColorForRace(int32 RaceIndex) const
{
	const FTimeTrialRaceDefinition* Race = FindRace(RaceIndex);

	return Race ? Race->Color : FLinearColor::White;
}

void ATimeTrialGameMode::SetSelectedTimeOfDay(int32 NewTimeOfDay)
{
	if (!TimesOfDay.IsValidIndex(NewTimeOfDay) || NewTimeOfDay == SelectedTimeOfDay)
	{
		return;
	}

	SelectedTimeOfDay = NewTimeOfDay;

	ApplyTimeOfDay();
}

void ATimeTrialGameMode::ApplyTimeOfDay()
{
	if (!TimesOfDay.IsValidIndex(SelectedTimeOfDay))
	{
		return;
	}

	const FTimeTrialTimeOfDay& Time = TimesOfDay[SelectedTimeOfDay];
	UWorld* World = GetWorld();

	// the level's sun. The sky atmosphere takes its colors from it, and the sky light captures the sky as it goes
	TActorIterator<ADirectionalLight> SunIt(World);

	if (ADirectionalLight* Sun = SunIt ? *SunIt : nullptr)
	{
		Sun->SetActorRotation(FRotator(Time.SunRotation.Pitch, Time.SunRotation.Yaw, 0.0f));

		if (ULightComponent* SunLight = Sun->GetLightComponent())
		{
			SunLight->SetIntensity(Time.SunIntensity);
			SunLight->SetUseTemperature(true);
			SunLight->SetTemperature(Time.SunTemperature);
		}
	}

	TActorIterator<ASkyLight> SkyIt(World);

	if (ASkyLight* Sky = SkyIt ? *SkyIt : nullptr)
	{
		Sky->GetLightComponent()->SetIntensity(Time.SkyLightIntensity);
	}

	// the camera's exposure goes in a post process volume of our own that covers the whole level
	if (!TimeOfDayVolume)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		TimeOfDayVolume = World->SpawnActor<APostProcessVolume>(SpawnParams);

		if (TimeOfDayVolume)
		{
			TimeOfDayVolume->bUnbound = true;
			TimeOfDayVolume->Settings.bOverride_AutoExposureBias = true;
		}
	}

	if (TimeOfDayVolume)
	{
		TimeOfDayVolume->Settings.AutoExposureBias = Time.ExposureBias;
	}

	for (TActorIterator<AMyProjectPawn> It(World); It; ++It)
	{
		ApplyCarLights(*It);
	}

	UE_LOG(LogMyProject, Log, TEXT("[TimeOfDay] %s"), *Time.DisplayName.ToString());
}

void ATimeTrialGameMode::ApplyCarLights(AMyProjectPawn* Car) const
{
	if (Car && TimesOfDay.IsValidIndex(SelectedTimeOfDay))
	{
		Car->SetBodyLightsOn(TimesOfDay[SelectedTimeOfDay].bCarLights);
	}
}

void ATimeTrialGameMode::OnActorSpawned(AActor* Actor)
{
	ApplyCarLights(Cast<AMyProjectPawn>(Actor));
}

bool ATimeTrialGameMode::IsGateOnSelectedRoute(const ATimeTrialTrackGate* Gate) const
{
	if (!Gate)
	{
		return false;
	}

	// the finish line is part of every route
	if (Gate == FinishLineMarker.Get())
	{
		return true;
	}

	return ActiveRouteGates.ContainsByPredicate([Gate](const TObjectPtr<ATimeTrialTrackGate>& RouteGate)
	{
		return RouteGate.Get() == Gate;
	});
}

ATimeTrialTrackGate* ATimeTrialGameMode::GetFirstGateForSelectedRace() const
{
	return GetNextGateOnRoute(FinishLineMarker);
}

ATimeTrialTrackGate* ATimeTrialGameMode::GetNextGateOnRoute(const ATimeTrialTrackGate* Gate) const
{
	if (!Gate || !FinishLineMarker)
	{
		return nullptr;
	}

	// the route's gates are kept in the order the race runs, starting after the finish line and ending back at it
	if (Gate == FinishLineMarker.Get())
	{
		return ActiveRouteGates.IsEmpty() ? FinishLineMarker.Get() : ActiveRouteGates[0].Get();
	}

	const int32 Index = ActiveRouteGates.IndexOfByPredicate([Gate](const TObjectPtr<ATimeTrialTrackGate>& RouteGate)
	{
		return RouteGate.Get() == Gate;
	});

	if (Index == INDEX_NONE)
	{
		return nullptr;
	}

	return ActiveRouteGates.IsValidIndex(Index + 1) ? ActiveRouteGates[Index + 1].Get() : FinishLineMarker.Get();
}

APlayerStart* ATimeTrialGameMode::FindRaceStart(int32 RaceIndex) const
{
	if (!FindRace(RaceIndex))
	{
		return nullptr;
	}

	const FName StartTag = FName(*FString::Printf(TEXT("%s%d%s"), *RaceStartTagPrefix, GetRouteForRace(RaceIndex), IsRaceReversed(RaceIndex) ? TEXT("R") : TEXT("")));

	TArray<AActor*> Starts;
	UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), APlayerStart::StaticClass(), StartTag, Starts);

	return Starts.IsEmpty() ? nullptr : Cast<APlayerStart>(Starts[0]);
}

APlayerStart* ATimeTrialGameMode::FindDefaultStart() const
{
	TArray<AActor*> Starts;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), Starts);

	for (AActor* Start : Starts)
	{
		if (!IsRaceStart(Start))
		{
			return Cast<APlayerStart>(Start);
		}
	}

	return nullptr;
}

bool ATimeTrialGameMode::IsRaceStart(const AActor* Actor) const
{
	if (!Actor || RaceStartTagPrefix.IsEmpty())
	{
		return false;
	}

	for (const FName& Tag : Actor->Tags)
	{
		if (Tag.ToString().StartsWith(RaceStartTagPrefix))
		{
			return true;
		}
	}

	return false;
}

void ATimeTrialGameMode::RebuildActiveRoute()
{
	ActiveRouteGates.Reset();

	// the race line runs through these gates, so it's out of date too
	bRaceLineDirty = true;

	if (!FinishLineMarker)
	{
		return;
	}

	// walk the chain from the finish line until it loops back, guarding against broken or cyclic chains
	const int32 Route = GetSelectedRoute();

	ATimeTrialTrackGate* Current = FinishLineMarker->GetNextMarkerForRoute(Route);

	while (Current && Current != FinishLineMarker.Get() && !IsGateOnSelectedRoute(Current))
	{
		ActiveRouteGates.Add(Current);
		Current = Current->GetNextMarkerForRoute(Route);
	}

	// a reversed race passes the same gates the other way round, still starting and ending at the finish line
	if (IsRaceReversed(SelectedRace))
	{
		Algo::Reverse(ActiveRouteGates);
	}
}

void ATimeTrialGameMode::SetRaceMode(ETimeTrialRaceMode NewMode, int32 NewDifficulty)
{
	RaceMode = NewMode;
	SelectedDifficulty = NpcDifficulties.Num() > 0 ? FMath::Clamp(NewDifficulty, 0, NpcDifficulties.Num() - 1) : 0;

	// a level still locked falls back to the hardest one open
	while (SelectedDifficulty > 0 && !IsDifficultyUnlocked(SelectedDifficulty))
	{
		--SelectedDifficulty;
	}

	UE_LOG(LogMyProject, Log, TEXT("[Race] Mode %s, NPC difficulty %d"), *UEnum::GetValueAsString(RaceMode), SelectedDifficulty);
}

void ATimeTrialGameMode::SetTwoPlayers(bool bNewTwoPlayers)
{
	bTwoPlayers = bNewTwoPlayers;

	UE_LOG(LogMyProject, Log, TEXT("[Race] %s"), bTwoPlayers ? TEXT("Two players") : TEXT("One player"));
}

APlayerController* ATimeTrialGameMode::AddSecondPlayer()
{
	if (APlayerController* Existing = GetSecondPlayer())
	{
		return Existing;
	}

	// the second player's car comes in on their slot of the grid (SpawnDefaultPawnAtTransform)
	APlayerController* SecondPlayer = UGameplayStatics::CreatePlayer(this, 1, true);

	if (!SecondPlayer)
	{
		UE_LOG(LogMyProject, Warning, TEXT("[2P] Could not add the second player"));
		return nullptr;
	}

	// keys and buttons sent on as the second player's reach the game through their Slate user, which needs the game
	// viewport focused like the first player's
	if (FTwoPlayerInput* TwoPlayerInput = FTwoPlayerInput::Find(this))
	{
		TwoPlayerInput->PrepareSecondPlayer();
	}

	UE_LOG(LogMyProject, Log, TEXT("[2P] Second player joined as %s, driving %s"), *SecondPlayer->GetName(), *GetNameSafe(SecondPlayer->GetPawn()));

	return SecondPlayer;
}

void ATimeTrialGameMode::RemoveSecondPlayer()
{
	if (FTwoPlayerInput* TwoPlayerInput = FTwoPlayerInput::Find(this))
	{
		TwoPlayerInput->SetSecondPlayerDevice(ESecondPlayerDevice::None);
	}

	APlayerController* SecondPlayer = GetSecondPlayer();

	if (!SecondPlayer)
	{
		return;
	}

	// the car goes with the player, rather than being replaced the way a car destroyed in play is
	if (APawn* Car = SecondPlayer->GetPawn())
	{
		Car->OnDestroyed.RemoveAll(SecondPlayer);
		Car->Destroy();
	}

	UGameplayStatics::RemovePlayer(SecondPlayer, true);

	UE_LOG(LogMyProject, Log, TEXT("[2P] Second player left"));
}

APlayerController* ATimeTrialGameMode::GetSecondPlayer() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const ULocalPlayer* LocalPlayer = GameInstance ? GameInstance->GetLocalPlayerByIndex(1) : nullptr;

	return LocalPlayer ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
}

int32 ATimeTrialGameMode::GetLocalRacerCount() const
{
	return bTwoPlayers && GetSecondPlayer() ? 2 : 1;
}

bool ATimeTrialGameMode::AreAllPlayersFinished() const
{
	bool bAnyPlayer = false;

	for (const FRacerState& Racer : Racers)
	{
		if (Racer.bIsPlayer)
		{
			bAnyPlayer = true;

			if (!Racer.bFinished)
			{
				return false;
			}
		}
	}

	// with nobody's position tracked, there's only the one player, and they've just finished
	return bAnyPlayer || !bTwoPlayers;
}

bool ATimeTrialGameMode::IsDifficultyUnlocked(int32 Level) const
{
	if (!NpcDifficulties.IsValidIndex(Level))
	{
		return false;
	}

	if (!NpcDifficulties[Level].bLockedUntilPreviousBeaten || Level == 0)
	{
		return true;
	}

	const UTimeTrialRecords* Records = UTimeTrialRecords::Get(this);

	return Records && Records->HasBeatenNpcLevel(NpcDifficulties[Level - 1].Id);
}

const FRaceLine& ATimeTrialGameMode::GetRaceLine()
{
	if (bRaceLineDirty)
	{
		BuildRaceLine();
	}

	return RaceLine;
}

void ATimeTrialGameMode::BuildRaceLine()
{
	bRaceLineDirty = false;
	RaceLine.Reset();
	FinishLineDistance = 0.0f;

	if (!FinishLineMarker)
	{
		return;
	}

	// the route's gates in driving order, starting at the finish line
	TArray<FTransform> Gates;
	Gates.Add(FinishLineMarker->GetActorTransform());

	for (const ATimeTrialTrackGate* Gate : ActiveRouteGates)
	{
		if (Gate)
		{
			Gates.Add(Gate->GetActorTransform());
		}
	}

	// the roads the line follows between them
	TArray<AActor*> RoadActors;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), RoadTag, RoadActors);

	TArray<FRaceRoad> Roads;

	for (const AActor* RoadActor : RoadActors)
	{
		TInlineComponentArray<USplineComponent*> Splines(RoadActor);

		for (const USplineComponent* Spline : Splines)
		{
			// the road meshes are laid out from the spline's points in the actor's space
			FRaceRoad& Road = Roads.AddDefaulted_GetRef();
			Road.Spline = Spline;
			Road.Transform = RoadActor->GetActorTransform();
		}
	}

	if (Roads.IsEmpty())
	{
		UE_LOG(LogMyProject, Warning, TEXT("[Race] No actors tagged %s carry road splines, so the race line just joins up the gates."), *RoadTag.ToString());
	}

	RaceLine.Build(Gates, Roads);

	FinishLineDistance = RaceLine.FindDistance(FinishLineMarker->GetActorLocation());
}

bool ATimeTrialGameMode::GetGridSlotTransform(int32 Slot, FTransform& OutTransform)
{
	const FRaceLine& Line = GetRaceLine();

	if (!Line.IsValid())
	{
		return false;
	}

	// the grid builds back from where the player starts this race on their own
	const AActor* Reference = FindRaceStart(SelectedRace);

	if (!Reference)
	{
		Reference = FindDefaultStart();
	}

	if (!Reference)
	{
		Reference = FinishLineMarker;
	}

	if (!Reference)
	{
		return false;
	}

	// two cars to a row, pole position on the left of the front row
	const int32 Row = Slot / 2;
	const float Side = Slot % 2 == 0 ? -1.0f : 1.0f;
	const float Distance = Line.FindDistance(Reference->GetActorLocation()) - Row * GridRowSpacing;

	const FVector Location = Line.GetLocation(Distance) + Line.GetRight(Distance) * Side * GridLaneOffset + FVector(0.0f, 0.0f, 150.0f);

	OutTransform = FTransform(FRotator(0.0f, Line.GetDirection(Distance).Rotation().Yaw, 0.0f), Location);
	return true;
}

bool ATimeTrialGameMode::GetPlayerGridTransform(const AController* Player, FTransform& OutTransform)
{
	if (!UsesStartingGrid())
	{
		return false;
	}

	// the second player starts beside the first
	return GetGridSlotTransform(GetFirstPlayerGridSlot() + FMath::Min(GetLocalPlayerIndex(Player), 1), OutTransform);
}

TArray<int32> ATimeTrialGameMode::GetNpcGridSlots() const
{
	const int32 FirstPlayerSlot = GetFirstPlayerGridSlot();
	const int32 PlayerCount = bTwoPlayers ? 2 : 1;

	TArray<int32> Slots;

	for (int32 Slot = 0; Slot < GetRaceNpcCount() + PlayerCount; ++Slot)
	{
		if (Slot < FirstPlayerSlot || Slot >= FirstPlayerSlot + PlayerCount)
		{
			Slots.Add(Slot);
		}
	}

	return Slots;
}

void ATimeTrialGameMode::PrepareRaceGrid()
{
	DestroyNpcs();

	// the player's car carries over from the last race, so it mustn't keep that race's push
	for (const FRacerState& Racer : Racers)
	{
		const AController* Controller = Racer.Controller.Get();

		if (AMyProjectPawn* Car = Controller ? Cast<AMyProjectPawn>(Controller->GetPawn()) : nullptr)
		{
			Car->SetRaceBoost(0.0f);
		}
	}

	Racers.Reset();
	bRaceRunning = false;
	FinishedCount = 0;

	if (!IsVersusNpc())
	{
		return;
	}

	UWorld* World = GetWorld();

	// the NPCs drive the player's vehicle unless told otherwise; the car choices give them their own looks
	TSubclassOf<AMyProjectPawn> VehicleClass = NpcVehicleClass;

	if (!VehicleClass)
	{
		const APlayerController* PC = World->GetFirstPlayerController();
		const APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;

		if (PlayerPawn && PlayerPawn->IsA<AMyProjectPawn>())
		{
			VehicleClass = PlayerPawn->GetClass();

		} else if (DefaultPawnClass && DefaultPawnClass->IsChildOf<AMyProjectPawn>()) {

			VehicleClass = DefaultPawnClass.Get();
		}
	}

	if (!VehicleClass)
	{
		UE_LOG(LogMyProject, Warning, TEXT("[Race] No vehicle class for the NPCs. Set NpcVehicleClass on the game mode."));
		return;
	}

	const FNpcDifficulty Difficulty = NpcDifficulties.IsValidIndex(SelectedDifficulty) ? NpcDifficulties[SelectedDifficulty] : FNpcDifficulty();
	const TArray<int32> Slots = GetNpcGridSlots();

	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		FTransform SlotTransform;

		if (!GetGridSlotTransform(Slots[Index], SlotTransform))
		{
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AMyProjectPawn* Car = World->SpawnActor<AMyProjectPawn>(VehicleClass, SlotTransform, SpawnParams);

		if (!Car)
		{
			continue;
		}

		// a different car from the player's, working through the rest of the list
		const int32 CarIndex = CarChoices.Num() > 0 ? (SelectedCar + 1 + Index) % CarChoices.Num() : INDEX_NONE;

		if (CarChoices.IsValidIndex(CarIndex))
		{
			const FTimeTrialCarChoice& Choice = CarChoices[CarIndex];

			// the car as the player would drive it, with whatever boost the level gives the NPCs on top
			FVehicleHandling Handling = Choice.Handling;
			Handling.EngineTorqueScale *= Difficulty.CarTorqueScale;
			Handling.FrontGripScale *= Difficulty.CarGripScale;
			Handling.RearGripScale *= Difficulty.CarGripScale;

			Car->SetBodyClass(Choice.BodyClass);
			Car->SetHandling(Handling);

			// other cars fade away faster than the player's own
			Car->GetEngineSound()->SetDistanceFalloff(NpcEngineSoundRadius, NpcEngineSoundFalloff);

			if (UCollisionSoundComponent* CollisionSound = Car->GetCollisionFx() ? Car->GetCollisionFx()->GetSound() : nullptr)
			{
				CollisionSound->SetDistanceFalloff(NpcEngineSoundRadius, NpcEngineSoundFalloff);
			}

			if (Choice.EngineSound)
			{
				Car->SetEngineSound(Choice.EngineSound);
			}
		}

		ARaceNpcController* Driver = World->SpawnActor<ARaceNpcController>(ARaceNpcController::StaticClass(), SlotTransform, SpawnParams);

		if (!Driver)
		{
			Car->Destroy();
			continue;
		}

		Driver->Possess(Car);

		// spread the pace so the NPCs don't drive as one, the one on pole the quickest
		const float Spread = Slots.Num() > 1 ? 1.0f - 2.0f * Index / (Slots.Num() - 1) : 0.0f;
		Driver->SetDifficulty(Difficulty, 1.0f + Difficulty.PaceSpread * Spread);

		// keep a little to the side of the grid slot it started from
		Driver->SetPreferredLane(Slots[Index] % 2 == 0 ? -100.0f : 100.0f);

		NpcVehicles.Add(Car);
		NpcDrivers.Add(Driver);
		NpcCarIndices.Add(CarIndex);
	}

	UE_LOG(LogMyProject, Log, TEXT("[Race] %d NPCs on the grid at %s"), NpcDrivers.Num(), *Difficulty.DisplayName.ToString());
}

void ATimeTrialGameMode::DestroyNpcs()
{
	for (ARaceNpcController* Driver : NpcDrivers)
	{
		if (IsValid(Driver))
		{
			Driver->Destroy();
		}
	}

	for (AMyProjectPawn* Car : NpcVehicles)
	{
		if (IsValid(Car))
		{
			Car->Destroy();
		}
	}

	NpcDrivers.Reset();
	NpcCarIndices.Reset();
	NpcVehicles.Reset();
}

void ATimeTrialGameMode::StartRacers()
{
	Racers.Reset();
	FinishedCount = 0;
	NewlyUnlockedDifficulty = INDEX_NONE;
	RaceStartTime = GetWorld()->GetTimeSeconds();

	// players first, then the NPCs
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			const int32 PlayerIndex = GetLocalPlayerIndex(PC);
			const FTimeTrialCarChoice* Car = GetCarChoiceForPlayer(PlayerIndex);

			FRacerState& Racer = Racers.AddDefaulted_GetRef();
			Racer.Controller = PC;
			Racer.bIsPlayer = true;
			Racer.Name = bTwoPlayers
				? FText::Format(NSLOCTEXT("TimeTrialGameMode", "PlayerNumberName", "{0}P"), FText::AsNumber(PlayerIndex + 1))
				: NSLOCTEXT("TimeTrialGameMode", "PlayerName", "あなた");
			Racer.CarName = Car ? Car->DisplayName : FText::GetEmpty();
		}
	}

	for (int32 Index = 0; Index < NpcDrivers.Num(); ++Index)
	{
		ARaceNpcController* Driver = NpcDrivers[Index];

		if (IsValid(Driver))
		{
			FRacerState& Racer = Racers.AddDefaulted_GetRef();
			Racer.Controller = Driver;
			Racer.Name = NSLOCTEXT("TimeTrialGameMode", "NpcName", "NPC");
			Racer.CarName = NpcCarIndices.IsValidIndex(Index) && CarChoices.IsValidIndex(NpcCarIndices[Index]) ? CarChoices[NpcCarIndices[Index]].DisplayName : FText::GetEmpty();

			Driver->StartDriving();
		}
	}

	const FRaceLine& Line = GetRaceLine();

	// measure everyone from the finish line: a car on the grid just behind it starts a little below zero
	for (FRacerState& Racer : Racers)
	{
		const AController* Controller = Racer.Controller.Get();
		const APawn* Car = Controller ? Controller->GetPawn() : nullptr;

		if (Car && Line.IsValid())
		{
			Racer.LineDistance = Line.FindDistance(Car->GetActorLocation());
			Racer.Progress = Line.Delta(FinishLineDistance, Racer.LineDistance);
		}
	}

	bRaceRunning = Line.IsValid();
}

void ATimeTrialGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bRaceRunning || !RaceLine.IsValid())
	{
		return;
	}

	const float RaceDistance = GetLapsForSelectedRace() * RaceLine.GetLength();

	for (FRacerState& Racer : Racers)
	{
		AController* Controller = Racer.Controller.Get();
		const APawn* Car = Controller ? Controller->GetPawn() : nullptr;

		if (!Car)
		{
			continue;
		}

		const FVector Location = Car->GetActorLocation();
		float NewDistance = RaceLine.FindDistanceNear(Location, Racer.LineDistance, 4000.0f);

		// the car was moved further than a frame's driving, such as a replacement car at the start
		if (FVector::Dist(RaceLine.GetLocation(NewDistance), Location) > 3000.0f)
		{
			NewDistance = RaceLine.FindDistance(Location);
		}

		Racer.Progress += RaceLine.Delta(Racer.LineDistance, NewDistance);
		Racer.LineDistance = NewDistance;

		// NPCs finish once they've covered the race distance; players finish through the gates
		if (!Racer.bFinished && RaceDistance > 0.0f && Racer.Progress >= RaceDistance)
		{
			if (ARaceNpcController* Driver = Cast<ARaceNpcController>(Controller))
			{
				Racer.bFinished = true;
				Racer.FinishOrder = ++FinishedCount;
				Racer.FinishTime = GetWorld()->GetTimeSeconds() - RaceStartTime;

				Driver->NotifyFinished();

				UE_LOG(LogMyProject, Log, TEXT("[Race] %s finished in position %d at %.2f"), *Driver->GetName(), Racer.FinishOrder, GetWorld()->GetTimeSeconds());
			}
		}
	}

	UpdateRaceBoosts(DeltaSeconds);
}

void ATimeTrialGameMode::UpdateRaceBoosts(float DeltaSeconds)
{
	const FNpcDifficulty* Difficulty = GetRaceDifficulty();
	const float SlipstreamAccel = Difficulty ? Difficulty->SlipstreamAccel : 0.0f;
	const float CatchUpAccel = Difficulty ? Difficulty->CatchUpAccel : 0.0f;
	const float CatchUpFullGap = Difficulty ? FMath::Max(Difficulty->CatchUpFullGap, 100.0f) : 1.0f;
	const float PlayerCatchUpScale = Difficulty ? Difficulty->PlayerCatchUpScale : 1.0f;

	// the leader among those still racing
	float LeaderProgress = -TNumericLimits<float>::Max();

	for (const FRacerState& Racer : Racers)
	{
		if (!Racer.bFinished)
		{
			LeaderProgress = FMath::Max(LeaderProgress, Racer.Progress);
		}
	}

	for (FRacerState& Racer : Racers)
	{
		const AController* Controller = Racer.Controller.Get();
		AMyProjectPawn* Car = Controller ? Cast<AMyProjectPawn>(Controller->GetPawn()) : nullptr;

		if (!Car)
		{
			continue;
		}

		// how deep in the slipstream of the nearest car ahead: right behind it, along the way this car is heading
		float Slipstream = 0.0f;

		if (SlipstreamAccel > 0.0f && !Racer.bFinished)
		{
			const FVector Location = Car->GetActorLocation();
			const FVector Forward = Car->GetActorForwardVector();
			const float Speed = Car->GetVelocity().Size();

			for (const FRacerState& Other : Racers)
			{
				const AController* OtherController = Other.Controller.Get();
				const APawn* OtherCar = OtherController ? OtherController->GetPawn() : nullptr;

				if (!OtherCar || OtherCar == Car)
				{
					continue;
				}

				const FVector ToOther = OtherCar->GetActorLocation() - Location;
				const float Behind = FVector::DotProduct(ToOther, Forward);
				const float Aside = (ToOther - Forward * Behind).Size();

				// the car ahead has to be going the same way for its wake to help
				if (Behind <= 0.0f || Behind >= SlipstreamRange || Aside >= SlipstreamHalfWidth
					|| FVector::DotProduct(OtherCar->GetActorForwardVector(), Forward) < 0.7f)
				{
					continue;
				}

				const float Depth = (1.0f - Behind / SlipstreamRange) * (1.0f - Aside / SlipstreamHalfWidth * 0.5f);
				Slipstream = FMath::Max(Slipstream, Depth * FMath::Clamp(Speed / SlipstreamFullSpeed, 0.0f, 1.0f));
			}
		}

		Racer.Slipstream = FMath::FInterpTo(Racer.Slipstream, Slipstream, DeltaSeconds, SlipstreamEaseRate);

		// the further behind the leader, the harder the push
		const float Gap = Racer.bFinished ? 0.0f : FMath::Max(LeaderProgress - Racer.Progress, 0.0f);
		const float CatchUp = CatchUpAccel * FMath::Clamp(Gap / CatchUpFullGap, 0.0f, 1.0f) * (Racer.bIsPlayer ? PlayerCatchUpScale : 1.0f);

		Car->SetRaceBoost(Racer.bFinished ? 0.0f : SlipstreamAccel * Racer.Slipstream + CatchUp);
	}
}

const FNpcDifficulty* ATimeTrialGameMode::GetRaceDifficulty() const
{
	return IsVersusNpc() && NpcDifficulties.IsValidIndex(SelectedDifficulty) ? &NpcDifficulties[SelectedDifficulty] : nullptr;
}

float ATimeTrialGameMode::GetPlayerDistanceBehind(const AController* Racer) const
{
	const FRacerState* Self = FindRacer(Racer);

	if (!Self)
	{
		return 0.0f;
	}

	float Nearest = TNumericLimits<float>::Max();

	for (const FRacerState& Other : Racers)
	{
		if (Other.bIsPlayer && !Other.bFinished && Other.Progress < Self->Progress)
		{
			Nearest = FMath::Min(Nearest, Self->Progress - Other.Progress);
		}
	}

	return Nearest < TNumericLimits<float>::Max() ? Nearest : 0.0f;
}

bool ATimeTrialGameMode::IsPlayerAhead(const AController* Racer) const
{
	const int32 Position = GetRacePosition(Racer);

	return Position > 0 && Racers.ContainsByPredicate([this, Position](const FRacerState& Other)
	{
		return Other.bIsPlayer && GetRacePosition(Other.Controller.Get()) < Position;
	});
}

bool ATimeTrialGameMode::GetRacerLineDistance(const AController* Racer, float& OutDistance) const
{
	const FRacerState* State = FindRacer(Racer);

	if (!State || !bRaceRunning)
	{
		return false;
	}

	OutDistance = State->LineDistance;
	return true;
}

float ATimeTrialGameMode::FindResetSpot(const APawn* Car, float LineDistance, FTransform& OutTransform) const
{
	// a little way back, clear of any other car
	float Distance = LineDistance;

	for (int32 Attempt = 0; Attempt < 6; ++Attempt)
	{
		const FVector Spot = RaceLine.GetLocation(Distance);

		const bool bClear = !Racers.ContainsByPredicate([Car, &Spot](const FRacerState& Racer)
		{
			const AController* Controller = Racer.Controller.Get();
			const APawn* Other = Controller ? Controller->GetPawn() : nullptr;

			return Other && Other != Car && FVector::Dist(Other->GetActorLocation(), Spot) < 600.0f;
		});

		if (bClear)
		{
			break;
		}

		Distance -= 600.0f;
	}

	Distance = RaceLine.Wrap(Distance);

	// tilted with the road, so the car comes down on all four wheels on a hill
	const FRotator Direction = RaceLine.GetDirection(Distance).Rotation();

	OutTransform = FTransform(FRotator(Direction.Pitch, Direction.Yaw, 0.0f), RaceLine.GetLocation(Distance) + FVector(0.0f, 0.0f, 100.0f));

	return Distance;
}

void ATimeTrialGameMode::NotifyPlayerFinished(AController* Player)
{
	FRacerState* Racer = FindRacer(Player);

	if (Racer && !Racer->bFinished)
	{
		Racer->bFinished = true;
		Racer->FinishOrder = ++FinishedCount;
		Racer->FinishTime = GetWorld()->GetTimeSeconds() - RaceStartTime;

		UE_LOG(LogMyProject, Log, TEXT("[Race] %s finished in position %d"), *GetNameSafe(Player), Racer->FinishOrder);

		// a win against the NPCs is remembered, and may open up the next level. Two players racing each other
		// leave the saved progress alone
		if (Racer->FinishOrder == 1 && IsVersusNpc() && !bTwoPlayers && NpcDifficulties.IsValidIndex(SelectedDifficulty))
		{
			const int32 NextLevel = SelectedDifficulty + 1;
			const bool bNextWasLocked = NpcDifficulties.IsValidIndex(NextLevel) && !IsDifficultyUnlocked(NextLevel);

			UTimeTrialRecords* Records = UTimeTrialRecords::Get(this);

			if (Records && Records->SubmitNpcWin(NpcDifficulties[SelectedDifficulty].Id) && bNextWasLocked && IsDifficultyUnlocked(NextLevel))
			{
				NewlyUnlockedDifficulty = NextLevel;

				UE_LOG(LogMyProject, Log, TEXT("[Race] Unlocked NPC level %s"), *NpcDifficulties[NextLevel].DisplayName.ToString());
			}
		}
	}
}

int32 ATimeTrialGameMode::GetRacePosition(const AController* Racer) const
{
	const FRacerState* Self = FindRacer(Racer);

	if (!Self)
	{
		return 0;
	}

	// finishers in the order they crossed the line, then everyone else by how far they've come
	int32 Position = 1;

	for (const FRacerState& Other : Racers)
	{
		if (&Other == Self)
		{
			continue;
		}

		bool bOtherAhead = false;

		if (Other.bFinished && Self->bFinished)
		{
			bOtherAhead = Other.FinishOrder < Self->FinishOrder;

		} else if (Other.bFinished != Self->bFinished) {

			bOtherAhead = Other.bFinished;

		} else {

			bOtherAhead = Other.Progress > Self->Progress;
		}

		if (bOtherAhead)
		{
			++Position;
		}
	}

	return Position;
}

void ATimeTrialGameMode::GetStandings(TArray<FRaceStanding>& OutStandings) const
{
	OutStandings.Reset();

	for (const FRacerState& Racer : Racers)
	{
		FRaceStanding& Standing = OutStandings.AddDefaulted_GetRef();
		Standing.Position = GetRacePosition(Racer.Controller.Get());
		Standing.Name = Racer.Name;
		Standing.CarName = Racer.CarName;
		Standing.bIsPlayer = Racer.bIsPlayer;
		Standing.bFinished = Racer.bFinished;
		Standing.FinishTime = Racer.FinishTime;
	}

	OutStandings.Sort([](const FRaceStanding& A, const FRaceStanding& B) { return A.Position < B.Position; });
}

FRacerState* ATimeTrialGameMode::FindRacer(const AController* Controller)
{
	return Racers.FindByPredicate([Controller](const FRacerState& Racer) { return Racer.Controller.Get() == Controller; });
}

const FRacerState* ATimeTrialGameMode::FindRacer(const AController* Controller) const
{
	return Racers.FindByPredicate([Controller](const FRacerState& Racer) { return Racer.Controller.Get() == Controller; });
}
