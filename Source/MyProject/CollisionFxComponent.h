// Copyright (c) 2026 skykunking-gif. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CollisionFxComponent.generated.h"

class UCollisionSoundComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPointLightComponent;
class USoundBase;
class UStaticMesh;

/**
 *  Shows and sounds a car's knocks and scrapes against guardrails and other cars: sparks thrown off where it touches,
 *  a flicker of light there, and the noise of the metal. A hard crash plays a recorded crash instead of the
 *  made up knock, which sounds too light for a car slamming into something.
 *
 *  Sparks are glowing streaks, each one instance of a stretched mesh in a single instanced mesh component, moved
 *  in code every frame. They fly off along the slide and away from what was hit, then fall and fade.
 */
UCLASS(ClassGroup = Vehicle, meta = (BlueprintSpawnableComponent))
class UCollisionFxComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UCollisionFxComponent();

	/** Reports the car touching something that isn't the road. Call for every contact, however light.
	 *  Normal points from what was hit towards the car; RelativeVelocity is the car's velocity relative to it */
	void AddContact(const FVector& Point, const FVector& Normal, const FVector& RelativeVelocity);

	/** Returns the sound of the knocks and scrapes */
	UCollisionSoundComponent* GetSound() const { return Sound; }

protected:

	/** Makes the spark instances, the light and the sound */
	virtual void BeginPlay() override;

	/** Gets rid of the spark instances, the light and the sound */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Throws one spark from a point, along a direction, at about a speed */
	void SpawnSpark(const FVector& Point, const FVector& Direction, float Speed);

	/** Throws sparks off a contact: along the way the car is sliding and away from what it hit */
	void SpawnSparks(int32 Count, const FVector& Point, const FVector& Normal, const FVector& SlideDirection, float SlideSpeed);

	/** Plays the recorded crash at a point on the car, as loud as the knock was hard */
	void PlayCrash(const FVector& Point, float Strength);

	/** Mesh stretched into each spark. A cube 100 units across */
	UPROPERTY(EditAnywhere, Category="Sparks")
	TObjectPtr<UStaticMesh> SparkMesh;

	/** Glowing material of the sparks. Reads its fade from per instance custom data 0 */
	UPROPERTY(EditAnywhere, Category="Sparks")
	TObjectPtr<UMaterialInterface> SparkMaterial;

	/** Most sparks in the air at once from this car */
	UPROPERTY(EditAnywhere, Category="Sparks", meta = (ClampMin = "1"))
	int32 MaxSparks = 250;

	/** Sparks a second thrown off a hard scrape at speed. Lighter or slower scrapes throw fewer */
	UPROPERTY(EditAnywhere, Category="Sparks", meta = (ClampMin = "0"))
	float ScrapeSparkRate = 450.0f;

	/** Sparks thrown off the hardest knock all at once. Lighter knocks throw fewer */
	UPROPERTY(EditAnywhere, Category="Sparks", meta = (ClampMin = "0"))
	int32 ImpactSparkCount = 90;

	/** How long a spark glows, at the shortest and longest, in seconds */
	UPROPERTY(EditAnywhere, Category="Sparks", meta = (Units = "s"))
	FVector2D SparkLifetime = FVector2D(0.25f, 0.6f);

	/** Thickness of a spark streak */
	UPROPERTY(EditAnywhere, Category="Sparks", meta = (Units = "cm"))
	float SparkThickness = 4.0f;

	/** How long a spark streak is for every cm/s it flies, so faster sparks are longer streaks */
	UPROPERTY(EditAnywhere, Category="Sparks", meta = (Units = "s"))
	float SparkStreakTime = 0.03f;

	/** Sliding speed along what's hit at which a scrape is as hard as it gets */
	UPROPERTY(EditAnywhere, Category="Collision", meta = (Units = "cm/s"))
	float FullScrapeSpeed = 2500.0f;

	/** Sliding speed below which touching something makes no sparks or scraping */
	UPROPERTY(EditAnywhere, Category="Collision", meta = (Units = "cm/s"))
	float MinScrapeSpeed = 150.0f;

	/** Speed into what's hit below which there's no knock */
	UPROPERTY(EditAnywhere, Category="Collision", meta = (Units = "cm/s"))
	float MinImpactSpeed = 250.0f;

	/** Speed into what's hit at which a knock is as hard as it gets */
	UPROPERTY(EditAnywhere, Category="Collision", meta = (Units = "cm/s"))
	float FullImpactSpeed = 2200.0f;

	/** Shortest time between two knocks, so one crash doesn't knock again every frame the car stays in contact */
	UPROPERTY(EditAnywhere, Category="Collision", meta = (Units = "s"))
	float ImpactCooldown = 0.2f;

	/** Recorded crash played for a hard knock */
	UPROPERTY(EditAnywhere, Category="Sound")
	TObjectPtr<USoundBase> CrashSound;

	/** How hard a knock has to be, from 0 to 1, to play the recorded crash. Lighter knocks play the made up knock */
	UPROPERTY(EditAnywhere, Category="Sound", meta = (ClampMin = "0", ClampMax = "1"))
	float CrashMinStrength = 0.15f;

	/** Loudness of the recorded crash for the lightest and the hardest knock that plays it */
	UPROPERTY(EditAnywhere, Category="Sound")
	FVector2D CrashVolume = FVector2D(0.6f, 1.0f);

	/** Pitch of the recorded crash for the lightest and the hardest knock that plays it, so heavier crashes sound deeper */
	UPROPERTY(EditAnywhere, Category="Sound")
	FVector2D CrashPitch = FVector2D(1.1f, 0.9f);

	/** Shortest time between two recorded crashes, so bouncing along a wall doesn't pile them up */
	UPROPERTY(EditAnywhere, Category="Sound", meta = (Units = "s"))
	float CrashCooldown = 0.5f;

	/** Brightest the light at a scrape flickers to */
	UPROPERTY(EditAnywhere, Category="Light", meta = (Units = "cd"))
	float LightIntensity = 300.0f;

	/** How far the light at a scrape reaches */
	UPROPERTY(EditAnywhere, Category="Light", meta = (Units = "cm"))
	float LightRadius = 500.0f;

	/** Colour of the light at a scrape */
	UPROPERTY(EditAnywhere, Category="Light")
	FLinearColor LightColor = FLinearColor(1.0f, 0.5f, 0.15f);

	/** One spark in flight */
	struct FSpark
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Age = 0.0f;
		float Lifetime = 0.0f;
	};

	/** Sparks in flight. Instance i of the spark mesh shows spark i; the rest are shrunk away */
	TArray<FSpark> Sparks;

	/** Instances the sparks are drawn with */
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> SparkInstances;

	/** The flicker of light at a scrape */
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> ContactLight;

	/** The noise of knocks and scrapes */
	UPROPERTY(Transient)
	TObjectPtr<UCollisionSoundComponent> Sound;

	/** Hardest scrape reported since the last frame, from 0 to 1, and where it was */
	float FrameScrape = 0.0f;
	float FrameScrapeSpeed = 0.0f;
	FVector FrameScrapePoint = FVector::ZeroVector;
	FVector FrameScrapeNormal = FVector::UpVector;
	FVector FrameSlideDirection = FVector::ForwardVector;

	/** How hard the car is scraping now, kept up for a moment after the last contact so a scrape doesn't stutter */
	float Scrape = 0.0f;
	float ScrapeSpeed = 0.0f;
	float TimeSinceContact = 1.0f;
	FVector ScrapePoint = FVector::ZeroVector;
	FVector ScrapeNormal = FVector::UpVector;
	FVector SlideDirection = FVector::ForwardVector;

	/** Sparks owed to the scrape, carried between frames so low rates still throw some */
	float SparkDebt = 0.0f;

	/** Time until another knock can be played */
	float ImpactCooldownLeft = 0.0f;

	/** Time until another recorded crash can be played */
	float CrashCooldownLeft = 0.0f;

	/** Brightness of the flash of light a knock gives, fading away */
	float ImpactFlash = 0.0f;

	/** Instances showing a spark at the last update, so the ones whose spark has gone get shrunk away */
	int32 ShownSparks = 0;
};
