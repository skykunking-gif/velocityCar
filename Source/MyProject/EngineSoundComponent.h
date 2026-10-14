// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include <atomic>
#include "EngineSoundComponent.generated.h"

class UEngineSoundProfile;
struct FEngineSoundSamples;

/** What the engine sound should be doing, set on the game thread and read by the audio renderer */
struct FEngineSoundControl
{
	/** Time in the recording to play, in seconds */
	std::atomic<float> RecordingTime { 0.0f };

	/** Loudness */
	std::atomic<float> Gain { 0.0f };

	/** Low pass filter cutoff in Hz */
	std::atomic<float> Cutoff { 20000.0f };
};

/**
 *  Plays a car's engine from a recording of it revving from idle to the red line. Short overlapping snippets
 *  are cut from the point in the recording the engine was at the car's current speed, so the engine sound
 *  follows the revs up and down without changing the pitch of the recording.
 */
UCLASS(ClassGroup = Synth, meta = (BlueprintSpawnableComponent))
class UEngineSoundComponent : public USynthComponent
{
	GENERATED_BODY()

public:

	UEngineSoundComponent(const FObjectInitializer& ObjectInitializer);

	/** Switches to another engine and starts playing it. Null goes quiet */
	UFUNCTION(BlueprintCallable, Category="Engine Sound")
	void SetProfile(UEngineSoundProfile* NewProfile);

	/** Returns the engine being played, or null */
	UFUNCTION(BlueprintPure, Category="Engine Sound")
	UEngineSoundProfile* GetProfile() const { return Profile; };

	/** Updates the sound for the engine's speed, as a fraction from idle (0) to the red line (1), and the throttle (0 to 1) */
	void SetEngineState(float RPMFraction, float Throttle);

	/** Sets how the engine fades with distance: full volume out to InnerRadius, then 60 dB quieter
	 *  for every FalloffDistance further. Takes effect on a sound already playing too */
	UFUNCTION(BlueprintCallable, Category="Engine Sound")
	void SetDistanceFalloff(float InnerRadius, float FalloffDistance);

protected:

	// Begin USynthComponent interface
	virtual bool Init(int32& SampleRate) override;
	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;
	// End USynthComponent interface

	/** Engine being played */
	UPROPERTY(Transient)
	TObjectPtr<UEngineSoundProfile> Profile;

	/** The decoded recording of the engine being played */
	TSharedPtr<const FEngineSoundSamples> Samples;

	/** Snippet settings of the engine being played, copied for the audio renderer */
	float GrainLength = 0.08f;
	float GrainJitter = 0.005f;
	float GrainAlignment = 0.016f;

	/** Shared with the audio renderer */
	TSharedRef<FEngineSoundControl, ESPMode::ThreadSafe> Control;
};
