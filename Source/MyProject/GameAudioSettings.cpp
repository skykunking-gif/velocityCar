// Copyright Epic Games, Inc. All Rights Reserved.


#include "GameAudioSettings.h"

int32 UGameAudioSettings::GetMusicLevel()
{
	return FMath::Clamp(GetDefault<UGameAudioSettings>()->MusicLevel, 0, MaxLevel);
}

void UGameAudioSettings::SetMusicLevel(int32 NewLevel)
{
	UGameAudioSettings* Settings = GetMutableDefault<UGameAudioSettings>();
	Settings->MusicLevel = FMath::Clamp(NewLevel, 0, MaxLevel);
	Settings->SaveConfig();

	OnChanged().Broadcast();
}

int32 UGameAudioSettings::GetEngineLevel()
{
	return FMath::Clamp(GetDefault<UGameAudioSettings>()->EngineLevel, 0, MaxLevel);
}

void UGameAudioSettings::SetEngineLevel(int32 NewLevel)
{
	UGameAudioSettings* Settings = GetMutableDefault<UGameAudioSettings>();
	Settings->EngineLevel = FMath::Clamp(NewLevel, 0, MaxLevel);
	Settings->SaveConfig();

	OnChanged().Broadcast();
}

FSimpleMulticastDelegate& UGameAudioSettings::OnChanged()
{
	static FSimpleMulticastDelegate Delegate;
	return Delegate;
}

float UGameAudioSettings::LevelToVolume(int32 Level)
{
	return FMath::Square(static_cast<float>(FMath::Clamp(Level, 0, MaxLevel)) / MaxLevel);
}
