// Copyright (c) 2026 skykunking-gif. All Rights Reserved.


#include "GameGraphicsSettings.h"
#include "GameFramework/GameUserSettings.h"
#include "Scalability.h"

int32 UGameGraphicsSettings::GetQualityLevel()
{
	const UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	const int32 Level = Settings ? Settings->GetOverallScalabilityLevel() : INDEX_NONE;

	// -1 when the groups differ, as on a first run before anything was picked
	return Level == INDEX_NONE ? MaxLevel : FMath::Clamp(Level, 0, MaxLevel);
}

void UGameGraphicsSettings::SetQualityLevel(int32 NewLevel)
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();

	if (!Settings)
	{
		return;
	}

	// only the scalability groups. Leaves the window mode and resolution alone
	Settings->SetOverallScalabilityLevel(FMath::Clamp(NewLevel, 0, MaxLevel));
	Settings->ApplyNonResolutionSettings();
	Settings->SaveSettings();
}

FText UGameGraphicsSettings::GetQualityName(int32 Level)
{
	switch (FMath::Clamp(Level, 0, MaxLevel))
	{
	case 0:
		return NSLOCTEXT("GameGraphicsSettings", "Low", "低");

	case 1:
		return NSLOCTEXT("GameGraphicsSettings", "Medium", "中");

	case 2:
		return NSLOCTEXT("GameGraphicsSettings", "High", "高");

	default:
		return NSLOCTEXT("GameGraphicsSettings", "Epic", "最高");
	}
}

void UGameGraphicsSettings::DetectQualityOnFirstRun()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();

	// the editor keeps its own quality. A result of 0 or more means the benchmark ran on an earlier start
	if (GIsEditor || !Settings || Settings->GetLastCPUBenchmarkResult() >= 0.0f)
	{
		return;
	}

	Settings->RunHardwareBenchmark();
	Settings->ApplyHardwareBenchmarkResults();

	// the benchmark sets each group on its own. One level for all of them, the lowest, so the options show it
	SetQualityLevel(Scalability::GetQualityLevels().GetMinQualityLevel());
}
