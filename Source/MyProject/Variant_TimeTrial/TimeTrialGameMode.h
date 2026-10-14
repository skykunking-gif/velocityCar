// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MyProjectPawn.h"
#include "RaceLine.h"
#include "RaceNpcController.h"
#include "TimeTrialGameMode.generated.h"

class ATimeTrialTrackGate;
class APlayerStart;
class APostProcessVolume;
class UTachometerStyle;
class UEngineSoundProfile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSelectedRaceChangedDelegate, int32, RaceIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSelectedCarChangedDelegate, int32, CarIndex);

/**
 *  What kind of race the player picked on the title screen
 */
UENUM(BlueprintType)
enum class ETimeTrialRaceMode : uint8
{
	/** Alone against the clock */
	TimeTrial,

	/** Against NPC cars */
	VersusNpc
};

/**
 *  Where one racer, player or NPC, is in the race
 */
USTRUCT(BlueprintType)
struct FRacerState
{
	GENERATED_BODY()

	/** The racer's controller. Its pawn is the car, which may change if the car gets replaced */
	UPROPERTY()
	TWeakObjectPtr<AController> Controller;

	/** Where along the race line the car last was */
	float LineDistance = 0.0f;

	/** How far the car has come since the start, measured from the finish line. A lap is the race line's length */
	float Progress = 0.0f;

	/** True once the racer has run all the laps */
	bool bFinished = false;

	/** Order the racer finished in, counting from 1 */
	int32 FinishOrder = 0;

	/** Time from the start to the finish, once the racer has finished */
	float FinishTime = 0.0f;

	/** Who the racer is, for the results */
	FText Name;

	/** Name of the car the racer drives, for the results */
	FText CarName;

	/** True for a player, false for an NPC */
	bool bIsPlayer = false;

	/** How deep the car is in another's slipstream, from 0 to 1, eased in and out */
	float Slipstream = 0.0f;
};

/**
 *  One line of the race standings
 */
struct FRaceStanding
{
	/** Place in the race, counting from 1 */
	int32 Position = 0;

	/** Who the racer is */
	FText Name;

	/** Name of the car the racer drives */
	FText CarName;

	/** True for a player */
	bool bIsPlayer = false;

	/** True once the racer has finished */
	bool bFinished = false;

	/** Time from the start to the finish, once finished */
	float FinishTime = 0.0f;
};

/**
 *  A car the player can pick before the race
 */
USTRUCT(BlueprintType)
struct FTimeTrialCarChoice
{
	GENERATED_BODY()

	/** Name shown on the car select screen */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Car")
	FText DisplayName;

	/** A line or two about the car, shown on the car select screen after its name */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Car", meta = (MultiLine = true))
	FText Description;

	/** Actor whose meshes replace the vehicle's looks, such as one of the showcase cars in Content/Vehicles.
	 *  Leave empty to keep the vehicle pawn's own meshes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Car")
	TSubclassOf<AActor> BodyClass;

	/** Tachometer design shown while driving this car. Leave empty to use the vehicle HUD's own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Car")
	TObjectPtr<UTachometerStyle> TachometerStyle;

	/** How this car's driving differs from the vehicle pawn's own. The defaults drive like the pawn itself. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Car")
	FVehicleHandling Handling;

	/** Engine this car sounds like. Leave empty to keep the vehicle pawn's own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Car")
	TObjectPtr<UEngineSoundProfile> EngineSound;
};

/**
 *  A race on offer: one of the level's routes, the way round it's driven, and how many laps
 */
USTRUCT(BlueprintType)
struct FTimeTrialRaceDefinition
{
	GENERATED_BODY()

	/** Name of the race */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Race")
	FText DisplayName;

	/** Which of the level's routes the race runs. Gates at branch points pick their next gate by route
	 *  (NextMarkerOverrides), route blockers open by route (OpenForRoutes), and a PlayerStart tagged
	 *  RaceStart<route> is where the race starts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Race", meta = (ClampMin = "1"))
	int32 Route = 1;

	/** If true, the race runs its route the other way round: the same gates in reverse order. It starts from a
	 *  PlayerStart tagged RaceStart<route>R, or failing that from the level's regular start, turned to face its
	 *  first gate */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Race")
	bool bReversed = false;

	/** Number of laps */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Race", meta = (ClampMin = "1"))
	int32 Laps = 3;

	/** Color of the race's gate markers and HUD */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Race")
	FLinearColor Color = FLinearColor::White;
};

/**
 *  A time of day the races can be run at: how the level is lit, and whether the cars drive with their lights on.
 *  Picked on the race select screen, separately from the race
 */
USTRUCT(BlueprintType)
struct FTimeTrialTimeOfDay
{
	GENERATED_BODY()

	/** Name shown on the race select screen */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Time of Day")
	FText DisplayName;

	/** Added to the course ID the records and ghosts are kept under, so each time of day has records of its own
	 *  (Race1R_Night). Leave empty to use the plain course ID: daytime does, as its records were set before there was
	 *  a choice of time */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Time of Day")
	FString CourseIdSuffix;

	/** Direction the level's directional light shines in. Pitch is minus the light's height above the horizon.
	 *  The sky follows it, so a low light gives a low sun's colors */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Time of Day")
	FRotator SunRotation = FRotator(-50.0f, -10.0f, 0.0f);

	/** Brightness of the directional light, in lux. The sky's brightness goes with it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Time of Day", meta = (ClampMin = "0"))
	float SunIntensity = 6.0f;

	/** Color temperature of the directional light, in kelvin. Lower is warmer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Time of Day", meta = (ClampMin = "1700", ClampMax = "12000"))
	float SunTemperature = 6500.0f;

	/** Brightness of the level's sky light, which lights everything the sun doesn't reach */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Time of Day", meta = (ClampMin = "0"))
	float SkyLightIntensity = 1.0f;

	/** Exposure compensation, in stops. The camera adjusts to the brightness of the scene, which would turn a dark
	 *  scene back into daylight: a negative value keeps it dark */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Time of Day")
	float ExposureBias = 0.0f;

	/** If true, the cars drive with their lights on */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Time of Day")
	bool bCarLights = false;
};

/**
 *  A simple GameMode for a Time Trial racing game
 */
UCLASS(abstract)
class ATimeTrialGameMode : public AGameModeBase
{
	GENERATED_BODY()

protected:

	/** Actor tag used to find the finish line marker on the level */
	UPROPERTY(EditAnywhere, Category="Time Trial")
	FName FinishTag;

	/** The races on offer, in order. Race N is entry N-1: race numbers count from 1 */
	UPROPERTY(EditAnywhere, Category="Time Trial")
	TArray<FTimeTrialRaceDefinition> Races;

	/** Number of the race being run, counting from 1 */
	UPROPERTY(EditAnywhere, Category="Time Trial")
	int32 SelectedRace = 1;

	/** Cars offered on the car select screen, in the order they're shown. Every car is the same vehicle pawn;
	 *  each entry changes how it looks and, through its Handling, how it drives. */
	UPROPERTY(EditAnywhere, Category="Time Trial|Cars")
	TArray<FTimeTrialCarChoice> CarChoices;

	/** Index into CarChoices of the car the player drives */
	UPROPERTY(EditAnywhere, Category="Time Trial|Cars")
	int32 SelectedCar = 0;

	/** Index into CarChoices of the car the second player drives in a two player game */
	UPROPERTY(EditAnywhere, Category="Time Trial|Cars")
	int32 SecondPlayerCar = 1;

	/** Times of day offered on the race select screen, in the order they're shown. The level is lit the way the
	 *  selected one says from the start of play, whatever its own lights are set to */
	UPROPERTY(EditAnywhere, Category="Time Trial|Time of Day")
	TArray<FTimeTrialTimeOfDay> TimesOfDay;

	/** Index into TimesOfDay of the time the races are run at */
	UPROPERTY(EditAnywhere, Category="Time Trial|Time of Day")
	int32 SelectedTimeOfDay = 1;

	/** Unbound post process volume that carries the time of day's exposure, spawned at the start of play */
	UPROPERTY(Transient)
	TObjectPtr<APostProcessVolume> TimeOfDayVolume;

	/** Lets cars spawned during play, such as NPCs and respawned players, pick up the time of day's car lights */
	FDelegateHandle ActorSpawnedHandle;

	/** Tag prefix for route-specific PlayerStarts. A PlayerStart tagged "RaceStart3" is where races on route 3 start,
	 *  and one tagged "RaceStart3R" where races on route 3 driven the other way round start. Races without one start
	 *  from the regular PlayerStart. */
	UPROPERTY(EditAnywhere, Category="Time Trial")
	FString RaceStartTagPrefix = TEXT("RaceStart");

	/** Pointer to the finish line track marker */
	TObjectPtr<ATimeTrialTrackGate> FinishLineMarker;

	/** Gates that make up the selected race route, in order, starting after the finish line */
	UPROPERTY()
	TArray<TObjectPtr<ATimeTrialTrackGate>> ActiveRouteGates;

	/** Race against the clock alone, or against NPCs. Picked on the title screen */
	UPROPERTY(EditAnywhere, Category="Time Trial|NPC")
	ETimeTrialRaceMode RaceMode = ETimeTrialRaceMode::TimeTrial;

	/** True for a two player split screen game. The race mode still says whether NPCs join in */
	UPROPERTY(EditAnywhere, Category="Time Trial|Two Players")
	bool bTwoPlayers = false;

	/** Index into NpcDifficulties of how well the NPCs drive */
	UPROPERTY(EditAnywhere, Category="Time Trial|NPC")
	int32 SelectedDifficulty = 1;

	/** Difficulty levels offered on the title screen, easiest first */
	UPROPERTY(EditAnywhere, Category="Time Trial|NPC")
	TArray<FNpcDifficulty> NpcDifficulties;

	/** How many NPC cars race against the player */
	UPROPERTY(EditAnywhere, Category="Time Trial|NPC", meta = (ClampMin = "0", ClampMax = "7"))
	int32 NpcCount = 3;

	/** Vehicle the NPCs drive. Leave empty to give them the same vehicle as the player; their looks and handling
	 *  come from the car choices either way */
	UPROPERTY(EditAnywhere, Category="Time Trial|NPC")
	TSubclassOf<AMyProjectPawn> NpcVehicleClass;

	/** Actors with this tag carry the road splines the race line follows */
	UPROPERTY(EditAnywhere, Category="Time Trial|NPC")
	FName RoadTag = TEXT("RaceRoad");

	/** NPC engines are heard at full volume out to this far from the car */
	UPROPERTY(EditAnywhere, Category="Time Trial|NPC", meta = (Units = "cm", ClampMin = "0"))
	float NpcEngineSoundRadius = 500.0f;

	/** Past NpcEngineSoundRadius, NPC engines grow 60 dB quieter over every this much further.
	 *  Shorter fades them faster. The player's car keeps the engine sound's own falloff */
	UPROPERTY(EditAnywhere, Category="Time Trial|NPC", meta = (Units = "cm", ClampMin = "1"))
	float NpcEngineSoundFalloff = 4000.0f;

	/** Distance between the rows of the starting grid. Two cars stand side by side in each row */
	UPROPERTY(EditAnywhere, Category="Time Trial|NPC", meta = (Units = "cm"))
	float GridRowSpacing = 1000.0f;

	/** Distance of each grid slot from the middle of the road */
	UPROPERTY(EditAnywhere, Category="Time Trial|NPC", meta = (Units = "cm"))
	float GridLaneOffset = 280.0f;

	/** Grid slot the player starts from, counting from pole position at 0. Slots past the last car put the player last */
	UPROPERTY(EditAnywhere, Category="Time Trial|NPC", meta = (ClampMin = "0"))
	int32 PlayerGridSlot = 3;

	/** Furthest behind another car a slipstream reaches. Its push fades out towards here (see FNpcDifficulty::SlipstreamAccel) */
	UPROPERTY(EditAnywhere, Category="Time Trial|NPC", meta = (Units = "cm", ClampMin = "100"))
	float SlipstreamRange = 3000.0f;

	/** How far to either side of the car ahead's path a car can be and still be in its slipstream */
	UPROPERTY(EditAnywhere, Category="Time Trial|NPC", meta = (Units = "cm", ClampMin = "10"))
	float SlipstreamHalfWidth = 200.0f;

	/** Speed from which a slipstream gives its whole push. Slower, it gives less, down to none at a standstill */
	UPROPERTY(EditAnywhere, Category="Time Trial|NPC", meta = (Units = "cm/s", ClampMin = "100"))
	float SlipstreamFullSpeed = 2500.0f;

	/** How quickly a car's slipstream eases in and out, per second */
	UPROPERTY(EditAnywhere, Category="Time Trial|NPC", meta = (ClampMin = "0.1"))
	float SlipstreamEaseRate = 3.0f;

	/** The line racers follow round the selected route. Built when first needed after the route changes */
	FRaceLine RaceLine;

	/** True when RaceLine needs building for the current route */
	bool bRaceLineDirty = true;

	/** Distance along the race line of the finish line */
	float FinishLineDistance = 0.0f;

	/** Everyone in the race, the player first */
	UPROPERTY()
	TArray<FRacerState> Racers;

	/** True while the racers are on track and their positions are being tracked */
	bool bRaceRunning = false;

	/** NPC cars spawned for the race */
	UPROPERTY()
	TArray<TObjectPtr<AMyProjectPawn>> NpcVehicles;

	/** Drivers of the NPC cars */
	UPROPERTY()
	TArray<TObjectPtr<ARaceNpcController>> NpcDrivers;

	/** Index into CarChoices of the car each NPC drives, matching NpcDrivers */
	TArray<int32> NpcCarIndices;

	/** Game time the race started at */
	float RaceStartTime = 0.0f;

	/** Number of racers that have finished so far */
	int32 FinishedCount = 0;

	/** Index into NpcDifficulties of the level the race just run unlocked, or INDEX_NONE */
	int32 NewlyUnlockedDifficulty = INDEX_NONE;

protected:

	/** Determines how many local players should be spawned on game start */
	UPROPERTY(EditDefaultsOnly, Category="Local Multiplayer", meta = (ClampMin = 1, ClampMax = 4))
	int32 NumberOfLocalPlayers = 1;

	/** Used to assign players to different PlayerStarts in the level */
	int32 CurrentPlayerStartAssignment = 0;

public:

	/** Broadcast whenever the selected race changes. Track gates and route blockers listen to this
	 *  to update their visuals and collision for the new route. */
	UPROPERTY(BlueprintAssignable, Category="Time Trial")
	FSelectedRaceChangedDelegate OnSelectedRaceChanged;

	/** Broadcast whenever the selected car changes. Players listen to this to swap their car's looks and handling. */
	UPROPERTY(BlueprintAssignable, Category="Time Trial|Cars")
	FSelectedCarChangedDelegate OnSelectedCarChanged;

public:

	/** Constructor */
	ATimeTrialGameMode();

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Stops listening for spawned cars */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Assigns a PlayerStart to a specific player */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** Puts a second player's car beside the first player's rather than on top of it */
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

public:

	/** Returns the track marker for the finish line */
	ATimeTrialTrackGate* GetFinishLine() const;

	/** Returns the races on offer, in order. Race N is entry N-1 */
	const TArray<FTimeTrialRaceDefinition>& GetRaces() const { return Races; };

	/** Returns how many races are on offer */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	int32 GetRaceCount() const { return Races.Num(); };

	/** Returns a race's definition, or null if there's no race of that number */
	const FTimeTrialRaceDefinition* FindRace(int32 RaceIndex) const { return Races.IsValidIndex(RaceIndex - 1) ? &Races[RaceIndex - 1] : nullptr; };

	/** Returns the number of the race being run, counting from 1 */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	int32 GetSelectedRace() const { return SelectedRace; };

	/** Picks the race to run and notifies listeners. Call before the race starts so branch gates route correctly.
	 *  Numbers with no race are ignored */
	UFUNCTION(BlueprintCallable, Category="Time Trial")
	void SetSelectedRace(int32 NewRace);

	/** Returns the number of laps of the race being run */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	int32 GetLapsForSelectedRace() const { return GetLapsForRace(SelectedRace); };

	/** Returns the number of laps of a race, or 0 if there's no race of that number */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	int32 GetLapsForRace(int32 RaceIndex) const;

	/** Returns which of the level's routes a race runs, or 0 if there's no race of that number */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	int32 GetRouteForRace(int32 RaceIndex) const;

	/** Returns which of the level's routes the race being run takes */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	int32 GetSelectedRoute() const { return GetRouteForRace(SelectedRace); };

	/** Returns true if a race runs its route the other way round */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	bool IsRaceReversed(int32 RaceIndex) const;

	/** Returns the name a race's records are kept under when run at a time of day (an index into the times of day):
	 *  Race<route>, with an R for the route driven the other way round, then the time of day's suffix (Race1R_Night).
	 *  Races over the same roads at the same time share their records, kept apart by the number of laps */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	FName GetCourseId(int32 RaceIndex, int32 TimeOfDay) const;

	/** Returns the name the selected race's records are kept under at the selected time of day */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	FName GetSelectedCourseId() const { return GetCourseId(SelectedRace, SelectedTimeOfDay); };

	/** Returns the display color for a race, or white if there's no race of that number */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	FLinearColor GetColorForRace(int32 RaceIndex) const;

	/** Returns the display color for the currently selected race */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	FLinearColor GetSelectedRaceColor() const { return GetColorForRace(SelectedRace); };

	/** Returns the cars offered on the car select screen */
	UFUNCTION(BlueprintPure, Category="Time Trial|Cars")
	const TArray<FTimeTrialCarChoice>& GetCarChoices() const { return CarChoices; };

	/** Returns the index into the car choices of the car the player drives */
	UFUNCTION(BlueprintPure, Category="Time Trial|Cars")
	int32 GetSelectedCar() const { return SelectedCar; };

	/** Picks the car the player drives and notifies listeners. Out of range indices are ignored. */
	UFUNCTION(BlueprintCallable, Category="Time Trial|Cars")
	void SetSelectedCar(int32 NewCar);

	/** Returns the body class of the selected car, or null to keep the vehicle's own looks */
	UFUNCTION(BlueprintPure, Category="Time Trial|Cars")
	TSubclassOf<AActor> GetSelectedCarBodyClass() const;

	/** Returns the tachometer design of the selected car, or null to use the vehicle HUD's own */
	UFUNCTION(BlueprintPure, Category="Time Trial|Cars")
	UTachometerStyle* GetSelectedCarTachometerStyle() const;

	/** Returns how the selected car's driving differs from the vehicle pawn's own */
	UFUNCTION(BlueprintPure, Category="Time Trial|Cars")
	FVehicleHandling GetSelectedCarHandling() const;

	/** Returns the engine sound of the selected car, or null to keep the vehicle pawn's own */
	UFUNCTION(BlueprintPure, Category="Time Trial|Cars")
	UEngineSoundProfile* GetSelectedCarEngineSound() const;

	/** Returns the index into the car choices of the car a local player drives: 0 for the first player, 1 for the second */
	UFUNCTION(BlueprintPure, Category="Time Trial|Cars")
	int32 GetCarForPlayer(int32 PlayerIndex) const { return PlayerIndex > 0 ? SecondPlayerCar : SelectedCar; };

	/** Picks the car a local player drives and notifies listeners. Out of range indices are ignored */
	UFUNCTION(BlueprintCallable, Category="Time Trial|Cars")
	void SetCarForPlayer(int32 PlayerIndex, int32 NewCar);

	/** Returns the car a local player drives, or null if the level offers no cars */
	const FTimeTrialCarChoice* GetCarChoiceForPlayer(int32 PlayerIndex) const;

	/** Returns the times of day offered on the race select screen */
	const TArray<FTimeTrialTimeOfDay>& GetTimesOfDay() const { return TimesOfDay; };

	/** Returns the index into the times of day of the time the races are run at */
	UFUNCTION(BlueprintPure, Category="Time Trial|Time of Day")
	int32 GetSelectedTimeOfDay() const { return SelectedTimeOfDay; };

	/** Picks the time of day and lights the level for it straight away. Out of range indices are ignored */
	UFUNCTION(BlueprintCallable, Category="Time Trial|Time of Day")
	void SetSelectedTimeOfDay(int32 NewTimeOfDay);

	/** Returns true if the gate is part of the currently selected race route */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	bool IsGateOnSelectedRoute(const ATimeTrialTrackGate* Gate) const;

	/** Returns the first gate a player should target when the race starts, respecting the selected race */
	ATimeTrialTrackGate* GetFirstGateForSelectedRace() const;

	/** Returns the gate that follows a gate on the selected race's route, in the direction the race runs.
	 *  Returns null if the gate isn't on the route */
	ATimeTrialTrackGate* GetNextGateOnRoute(const ATimeTrialTrackGate* Gate) const;

	/** Returns the PlayerStart for a race (tagged RaceStart<route>, or RaceStart<route>R for a reversed race),
	 *  or null if the level has none */
	APlayerStart* FindRaceStart(int32 RaceIndex) const;

	/** Returns the first PlayerStart that isn't tied to a race, where races without a start of their own begin */
	APlayerStart* FindDefaultStart() const;

	/** Returns true if the actor is a race-specific PlayerStart */
	bool IsRaceStart(const AActor* Actor) const;

	/** Picks between a time trial and a race against NPCs, and how well the NPCs drive */
	UFUNCTION(BlueprintCallable, Category="Time Trial|NPC")
	void SetRaceMode(ETimeTrialRaceMode NewMode, int32 NewDifficulty);

	/** Returns the kind of race picked on the title screen */
	UFUNCTION(BlueprintPure, Category="Time Trial|NPC")
	ETimeTrialRaceMode GetRaceMode() const { return RaceMode; };

	/** Returns the index into the NPC difficulty levels of how well the NPCs drive */
	UFUNCTION(BlueprintPure, Category="Time Trial|NPC")
	int32 GetSelectedDifficulty() const { return SelectedDifficulty; };

	/** Returns true when NPC cars join the race */
	UFUNCTION(BlueprintPure, Category="Time Trial|NPC")
	bool IsVersusNpc() const { return RaceMode == ETimeTrialRaceMode::VersusNpc && NpcCount > 0; };

	/** Picks between one player and a two player split screen game. Call along with SetRaceMode */
	UFUNCTION(BlueprintCallable, Category="Time Trial|Two Players")
	void SetTwoPlayers(bool bNewTwoPlayers);

	/** Returns true for a two player split screen game */
	UFUNCTION(BlueprintPure, Category="Time Trial|Two Players")
	bool IsTwoPlayers() const { return bTwoPlayers; };

	/** Adds the second local player, which splits the screen, and puts both cars side by side on the start.
	 *  Does nothing if there already is one. Returns the second player's controller */
	APlayerController* AddSecondPlayer();

	/** Takes the second local player and their car away, which gives the first player the whole screen again */
	void RemoveSecondPlayer();

	/** Returns the second local player's controller, or null if there isn't one */
	APlayerController* GetSecondPlayer() const;

	/** Returns how many local players race: 2 in a two player game once the second player is in, otherwise 1 */
	int32 GetLocalRacerCount() const;

	/** Returns true once every local player in the race has run all the laps */
	bool AreAllPlayersFinished() const;

	/** Returns true when the cars line up on a starting grid: against NPCs, and with two players */
	bool UsesStartingGrid() const { return IsVersusNpc() || bTwoPlayers; };

	/** Returns how many NPCs the race has */
	int32 GetRaceNpcCount() const { return IsVersusNpc() ? NpcCount : 0; };

	/** Returns the grid slot of the first player. The second player starts beside them */
	int32 GetFirstPlayerGridSlot() const { return FMath::Min(PlayerGridSlot, GetRaceNpcCount()); };

	/** Returns the NPC difficulty levels on offer */
	const TArray<FNpcDifficulty>& GetNpcDifficulties() const { return NpcDifficulties; };

	/** Returns true if the NPC difficulty level can be picked: it isn't locked, or the player has beaten the level before it */
	bool IsDifficultyUnlocked(int32 Level) const;

	/** Returns the NPC difficulty level the race just run unlocked by the player winning it, or INDEX_NONE */
	int32 GetNewlyUnlockedDifficulty() const { return NewlyUnlockedDifficulty; };

	/** Returns the line racers follow round the selected route, building it first if the route changed */
	const FRaceLine& GetRaceLine();

	/** Returns where the finish line is along the race line */
	float GetFinishLineDistance() { GetRaceLine(); return FinishLineDistance; };

	/** Returns everyone in the race */
	const TArray<FRacerState>& GetRacers() const { return Racers; };

	/** How far round the race line a racer was last measured to be. Returns false if it isn't racing */
	bool GetRacerLineDistance(const AController* Racer, float& OutDistance) const;

	/** Finds where to put a car back on the course near a distance along the race line: the middle of the road, facing
	 *  the way the race runs, stepping back along the line while another racer is in the way. Returns the spot's
	 *  distance along the line. The line has to have been built (GetRaceLine) */
	float FindResetSpot(const APawn* Car, float LineDistance, FTransform& OutTransform) const;

	/** Finds where on the starting grid a player's car goes. Returns false for a time trial on your own, where the
	 *  player starts from the level's own start */
	bool GetPlayerGridTransform(const AController* Player, FTransform& OutTransform);

	/** Lines up the cars for the start: spawns the NPCs on the grid in a race against them. Call before the countdown */
	void PrepareRaceGrid();

	/** Starts tracking race positions and sends the NPCs off. Call when the race starts */
	void StartRacers();

	/** Records a player as having run all the laps */
	void NotifyPlayerFinished(AController* Player);

	/** Returns a racer's place in the race, counting from 1, or 0 if the controller isn't racing */
	int32 GetRacePosition(const AController* Racer) const;

	/** Returns true if a player is ahead of this racer in the race */
	bool IsPlayerAhead(const AController* Racer) const;

	/** Returns how far the nearest player still racing behind this racer is behind it, or 0 if none is */
	float GetPlayerDistanceBehind(const AController* Racer) const;

	/** Returns the NPC difficulty the race is run at, or null outside races against NPCs */
	const FNpcDifficulty* GetRaceDifficulty() const;

	/** Lists everyone in the race in race order, for the results */
	void GetStandings(TArray<FRaceStanding>& OutStandings) const;

	/** Returns how many cars are in the race */
	int32 GetRacerCount() const { return FMath::Max(Racers.Num(), 1); };

	/** Tracks how far everyone has come */
	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Rebuilds the active route gate list by walking the gate chain from the finish line for the selected race,
	 *  turned round if the race is reversed */
	void RebuildActiveRoute();

	/** Builds the race line through the active route's gates, along the roads tagged with RoadTag */
	void BuildRaceLine();

	/** Gives every car its push for the slipstream it's in and for how far it's behind the leader */
	void UpdateRaceBoosts(float DeltaSeconds);

	/** Lights the level for the selected time of day: the directional light, the sky light, the exposure, and the
	 *  lights of every car */
	void ApplyTimeOfDay();

	/** Turns a car's lights on or off for the selected time of day */
	void ApplyCarLights(AMyProjectPawn* Car) const;

	/** Gives cars spawned during play the selected time of day's car lights */
	void OnActorSpawned(AActor* Actor);

	/** Finds the transform of a starting grid slot, counting from pole position at 0 */
	bool GetGridSlotTransform(int32 Slot, FTransform& OutTransform);

	/** Removes the NPC cars and their drivers */
	void DestroyNpcs();

	/** Returns the grid slots the NPCs take, in order, skipping the players' */
	TArray<int32> GetNpcGridSlots() const;

	/** Finds the racer entry of a controller */
	FRacerState* FindRacer(const AController* Controller);
	const FRacerState* FindRacer(const AController* Controller) const;

};
