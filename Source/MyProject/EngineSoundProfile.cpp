// Copyright Epic Games, Inc. All Rights Reserved.


#include "EngineSoundProfile.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "MyProject.h"

namespace
{
	/** Recordings already read, by full path. Weak so a recording no car uses any more is let go */
	TMap<FString, TWeakPtr<const FEngineSoundSamples>> LoadedRecordings;

	/** Reads a little-endian unsigned integer of a few bytes */
	uint32 ReadLittleEndian(const uint8* Data, int32 NumBytes)
	{
		uint32 Value = 0;

		for (int32 Index = 0; Index < NumBytes; ++Index)
		{
			Value |= static_cast<uint32>(Data[Index]) << (8 * Index);
		}

		return Value;
	}

	/** Decodes a RIFF wave file of 16, 24 or 32 bit integer or 32 bit float samples, mixing any channels down to mono */
	bool DecodeWave(const TArray<uint8>& File, FEngineSoundSamples& Out, FString& OutError)
	{
		if (File.Num() < 12 || FMemory::Memcmp(File.GetData(), "RIFF", 4) != 0 || FMemory::Memcmp(File.GetData() + 8, "WAVE", 4) != 0)
		{
			OutError = TEXT("not a RIFF wave file");
			return false;
		}

		uint32 Format = 0;
		int32 NumChannels = 0;
		int32 BitsPerSample = 0;
		const uint8* Data = nullptr;
		int64 DataSize = 0;

		// walk the chunks for the format and the samples
		int64 Offset = 12;

		while (Offset + 8 <= File.Num())
		{
			const uint8* Chunk = File.GetData() + Offset;
			const int64 ChunkSize = ReadLittleEndian(Chunk + 4, 4);
			const uint8* Body = Chunk + 8;
			const int64 BodySize = FMath::Min<int64>(ChunkSize, File.Num() - Offset - 8);

			if (FMemory::Memcmp(Chunk, "fmt ", 4) == 0 && BodySize >= 16)
			{
				Format = ReadLittleEndian(Body, 2);
				NumChannels = ReadLittleEndian(Body + 2, 2);
				Out.SampleRate = static_cast<float>(ReadLittleEndian(Body + 4, 4));
				BitsPerSample = ReadLittleEndian(Body + 14, 2);

				// extensible format: the real format is the start of the sub format GUID
				if (Format == 0xFFFE && BodySize >= 26)
				{
					Format = ReadLittleEndian(Body + 24, 2);
				}
			}
			else if (FMemory::Memcmp(Chunk, "data", 4) == 0)
			{
				Data = Body;
				DataSize = BodySize;
			}

			// chunks are padded to an even size
			Offset += 8 + ChunkSize + (ChunkSize & 1);
		}

		const bool bIntegerPCM = Format == 1 && (BitsPerSample == 16 || BitsPerSample == 24 || BitsPerSample == 32);
		const bool bFloatPCM = Format == 3 && BitsPerSample == 32;

		if (!Data || NumChannels <= 0 || Out.SampleRate <= 0.0f || (!bIntegerPCM && !bFloatPCM))
		{
			OutError = FString::Printf(TEXT("unsupported format %u, %d bits, %d channels"), Format, BitsPerSample, NumChannels);
			return false;
		}

		const int32 BytesPerSample = BitsPerSample / 8;
		const int32 NumFrames = static_cast<int32>(DataSize / (BytesPerSample * NumChannels));
		const float IntegerScale = 1.0f / static_cast<float>(1u << (BitsPerSample - 1));

		Out.Samples.SetNumUninitialized(NumFrames);

		for (int32 Frame = 0; Frame < NumFrames; ++Frame)
		{
			float Sum = 0.0f;

			for (int32 Channel = 0; Channel < NumChannels; ++Channel)
			{
				const uint8* Sample = Data + (static_cast<int64>(Frame) * NumChannels + Channel) * BytesPerSample;
				const uint32 Raw = ReadLittleEndian(Sample, BytesPerSample);

				if (bFloatPCM)
				{
					float Value;
					FMemory::Memcpy(&Value, &Raw, sizeof(float));
					Sum += Value;
				}
				else
				{
					// sign extend from the sample's width
					const int32 Shift = 32 - BitsPerSample;
					Sum += static_cast<float>(static_cast<int32>(Raw << Shift) >> Shift) * IntegerScale;
				}
			}

			Out.Samples[Frame] = Sum / NumChannels;
		}

		return Out.Samples.Num() > 0;
	}
}

float UEngineSoundProfile::GetRecordingRPM(float RPMFraction) const
{
	if (RecordingRPM.IsEmpty())
	{
		return 0.0f;
	}

	return FMath::Lerp(RecordingRPM[0].X, RecordingRPM.Last().X, FMath::Clamp(RPMFraction, 0.0f, 1.0f));
}

float UEngineSoundProfile::GetRecordingTime(float RPMFraction) const
{
	if (RecordingRPM.IsEmpty())
	{
		return 0.0f;
	}

	const float RPM = GetRecordingRPM(RPMFraction);

	if (RPM <= RecordingRPM[0].X)
	{
		return static_cast<float>(RecordingRPM[0].Y);
	}

	// find the pair of points the engine speed falls between
	for (int32 Index = 1; Index < RecordingRPM.Num(); ++Index)
	{
		const FVector2D& Low = RecordingRPM[Index - 1];
		const FVector2D& High = RecordingRPM[Index];

		if (RPM <= High.X)
		{
			const double Alpha = (RPM - Low.X) / FMath::Max(High.X - Low.X, UE_KINDA_SMALL_NUMBER);
			return static_cast<float>(FMath::Lerp(Low.Y, High.Y, Alpha));
		}
	}

	return static_cast<float>(RecordingRPM.Last().Y);
}

TSharedPtr<const FEngineSoundSamples> UEngineSoundProfile::LoadSamples() const
{
	if (SoundFile.IsEmpty())
	{
		return nullptr;
	}

	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / SoundFile);

	if (const TWeakPtr<const FEngineSoundSamples>* Loaded = LoadedRecordings.Find(Path))
	{
		if (TSharedPtr<const FEngineSoundSamples> Samples = Loaded->Pin())
		{
			return Samples;
		}
	}

	TArray<uint8> File;

	if (!FFileHelper::LoadFileToArray(File, *Path))
	{
		UE_LOG(LogMyProject, Warning, TEXT("[EngineSound] %s: can't read %s"), *GetName(), *Path);
		return nullptr;
	}

	TSharedRef<FEngineSoundSamples> Samples = MakeShared<FEngineSoundSamples>();
	FString Error;

	if (!DecodeWave(File, *Samples, Error))
	{
		UE_LOG(LogMyProject, Warning, TEXT("[EngineSound] %s: can't decode %s (%s)"), *GetName(), *Path, *Error);
		return nullptr;
	}

	UE_LOG(LogMyProject, Log, TEXT("[EngineSound] Read %s: %.1f s at %.0f Hz"), *SoundFile, Samples->Samples.Num() / Samples->SampleRate, Samples->SampleRate);

	LoadedRecordings.Add(Path, Samples);

	return Samples;
}
