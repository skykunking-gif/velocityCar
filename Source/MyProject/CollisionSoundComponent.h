// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include <atomic>
#include "CollisionSoundComponent.generated.h"

/** What the collision sound should be doing, set on the game thread and read by the audio renderer */
struct FCollisionSoundControl
{
	/** Loudness of the scraping, from 0 (none) to 1 (a hard scrape at speed) */
	std::atomic<float> ScrapeLevel { 0.0f };

	/** How fast the car slides along what it's scraping, from 0 to 1. Faster scrapes ring higher and crackle more */
	std::atomic<float> ScrapeSpeed { 0.0f };

	/** Strength of a knock waiting to be played, from 0 to 1. The renderer takes it and sets it back to zero */
	std::atomic<float> PendingImpact { 0.0f };
};

/**
 *  Makes the noise of a car hitting and scraping along guardrails and other cars, made up as it plays rather than
 *  read from a recording: metal ringing at a handful of resonances, rubbed with noise and crackle while scraping,
 *  and struck with a thump and a clang for a knock.
 */
UCLASS(ClassGroup = Synth, meta = (BlueprintSpawnableComponent))
class UCollisionSoundComponent : public USynthComponent
{
	GENERATED_BODY()

public:

	UCollisionSoundComponent(const FObjectInitializer& ObjectInitializer);

	/** Sets how hard and how fast the car is scraping along something. Zero for both when it's clear */
	void SetScrape(float Level, float Speed);

	/** Plays a knock, from 0 (a tap) to 1 (a heavy crash) */
	void PlayImpact(float Strength);

	/** Sets how the sound fades with distance, the same way as the engine sound */
	void SetDistanceFalloff(float InnerRadius, float FalloffDistance);

protected:

	// Begin USynthComponent interface
	virtual bool Init(int32& SampleRate) override;
	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;
	// End USynthComponent interface

	/** Overall loudness */
	UPROPERTY(EditAnywhere, Category="Collision Sound", meta = (ClampMin = "0.0"))
	float Volume = 1.0f;

	/** Shared with the audio renderer */
	TSharedRef<FCollisionSoundControl, ESPMode::ThreadSafe> Control;
};
