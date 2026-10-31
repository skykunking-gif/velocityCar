// Copyright (c) 2026 skykunking-gif. All Rights Reserved.


#include "LetterboxPlayerCameraManager.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

void ALetterboxPlayerCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	Super::UpdateViewTarget(OutVT, DeltaTime);

	// a split screen gives each player a part of the screen the same width but less tall, which they see the same
	// width of the track in
	float AspectRatio = LockedAspectRatio;

	const ULocalPlayer* LocalPlayer = PCOwner ? PCOwner->GetLocalPlayer() : nullptr;

	if (LocalPlayer && LocalPlayer->Size.X > 0.0f && LocalPlayer->Size.Y > 0.0f)
	{
		AspectRatio *= LocalPlayer->Size.X / LocalPlayer->Size.Y;
	}

	// a window that isn't this shape gets black bars rather than a wider or taller view of the track.
	// This runs for the pending view target too, so the lock holds through camera blends
	OutVT.POV.bConstrainAspectRatio = true;
	OutVT.POV.AspectRatio = AspectRatio;
}
