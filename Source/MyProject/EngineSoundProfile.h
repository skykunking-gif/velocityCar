// Copyright (c) 2026 skykunking-gif. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EngineSoundProfile.generated.h"

/**
 *  Decoded samples of an engine recording, shared by every car that uses it
 */
struct FEngineSoundSamples
{
	/** Mono samples, -1 to 1 */
	TArray<float> Samples;

	/** Samples per second */
	float SampleRate = 0.0f;
};

/**
 *  How one engine sounds, made from a single recording of the engine revving steadily from idle up to the red line.
 *  The car's engine speed picks the moment in the recording the engine was at that speed, so the recording plays
 *  forwards, backwards or holds still as the revs move.
 */
UCLASS(BlueprintType)
class UEngineSoundProfile : public UDataAsset
{
	GENERATED_BODY()

public:

	/** The recording, a .wav file under the project's Content folder such as "engineAudio/V8n7000rpm.wav".
	 *  It's read straight from disk, not imported, so a packaged game has to stage the folder (DefaultGame.ini) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recording")
	FString SoundFile;

	/** When in the recording the engine was at which speed. X is the RPM and Y the time in seconds.
	 *  Points must be in ascending RPM order. The car's idle plays the first point and its red line the last */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recording")
	TArray<FVector2D> RecordingRPM;

	/** Loudness of the engine with the throttle wide open */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound", meta = (ClampMin = "0.0"))
	float Volume = 1.0f;

	/** Loudness with the throttle closed, as a fraction of Volume. The recording is all on the throttle,
	 *  so lifting off only makes it quieter and duller */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OffThrottleVolume = 0.55f;

	/** Cutoff of the low pass filter with the throttle closed. Wide open, the filter is off */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound", meta = (Units = "Hz", ClampMin = "100.0"))
	float OffThrottleCutoff = 2500.0f;

	/** Length of each snippet of the recording played. Snippets overlap by half and blend into each other.
	 *  Longer sounds smoother at steady revs, shorter follows quick changes of revs better */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound", meta = (Units = "s", ClampMin = "0.02", ClampMax = "0.5"))
	float GrainLength = 0.08f;

	/** How far each snippet's start is moved at random, so steady revs don't sound like a loop */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound", meta = (Units = "s", ClampMin = "0.0", ClampMax = "0.2"))
	float GrainJitter = 0.005f;

	/** How far each snippet's start may shift to line its waveform up with the snippet it blends into. Without it the
	 *  two partly cancel out and the sound flutters at the rate snippets start. Should cover one firing of the engine
	 *  at idle: 0.016 s reaches down to about 600 RPM on a six cylinder */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound", meta = (Units = "s", ClampMin = "0.0", ClampMax = "0.05"))
	float GrainAlignment = 0.016f;

	/** Returns the time in the recording to play for an engine speed, given as a fraction of the way
	 *  from the car's idle (0) to its red line (1) */
	float GetRecordingTime(float RPMFraction) const;

	/** Returns the RPM in the recording for an engine speed given as a fraction from idle to red line */
	float GetRecordingRPM(float RPMFraction) const;

	/** Reads the recording, or returns the copy already read. Null if the file is missing or not a .wav it understands */
	TSharedPtr<const FEngineSoundSamples> LoadSamples() const;
};
