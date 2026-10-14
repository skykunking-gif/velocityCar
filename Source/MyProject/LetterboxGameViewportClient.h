// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "MyProject.h"
#include "LetterboxGameViewportClient.generated.h"

class FTwoPlayerInput;

/**
 *  Viewport client that keeps everything added to the viewport inside a fixed aspect ratio box.
 *  Widgets added with AddToViewport normally fill the whole window, which would leave the HUD
 *  hanging over the black bars of the letterboxed 3D view; boxing them here lines the two up
 *  without every call site having to wrap its own widget.
 *
 *  A player's own widgets (AddToPlayerScreen) are boxed to the shape of that player's part of the screen, which is
 *  half the height in a two player split screen, and drawn smaller there so the HUD fits.
 *
 *  Also owns the input router that shares the keyboard and gamepads out between two players (FTwoPlayerInput).
 */
UCLASS()
class ULetterboxGameViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:

	virtual void Init(struct FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice = true) override;
	virtual void BeginDestroy() override;

	virtual void AddViewportWidgetContent(TSharedRef<SWidget> ViewportContent, const int32 ZOrder = 0) override;
	virtual void RemoveViewportWidgetContent(TSharedRef<SWidget> ViewportContent) override;
	virtual void AddViewportWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent, const int32 ZOrder) override;
	virtual void RemoveViewportWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent) override;

	/** Returns the input router that shares input out between two players */
	FTwoPlayerInput* GetTwoPlayerInput() const { return TwoPlayerInput.Get(); };

protected:

	/** Aspect ratio the UI is boxed into. Must match ALetterboxPlayerCameraManager. */
	UPROPERTY(EditDefaultsOnly, Category="Letterbox")
	float LockedAspectRatio = MyProjectTargetAspectRatio;

	/** Size a player's own widgets are drawn at while the screen is split, so a HUD made for the whole screen fits
	 *  in their part of it */
	UPROPERTY(EditDefaultsOnly, Category="Letterbox", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float SplitscreenWidgetScale = 0.6f;

private:

	/** Wraps the content in a letterbox box, remembering it so the matching Remove call finds the same wrapper */
	TSharedRef<SWidget> WrapInLetterbox(TSharedRef<SWidget> ViewportContent);

	/** Wraps a player's own widget in a box the shape of their part of the screen, scaled down while it's split */
	TSharedRef<SWidget> WrapForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent);

	/** Returns the wrapper this content was added inside, or the content itself if it wasn't wrapped */
	TSharedRef<SWidget> FindLetterbox(TSharedRef<SWidget> ViewportContent);

	/** Drops entries whose wrapper has been destroyed, so removals we never see don't pile up */
	void PruneLetterboxes();

	/** Maps content widgets to the letterbox wrapper they were added inside. Keys are compared, never
	 *  dereferenced, so an entry outliving its widget is harmless until the next prune. */
	TMap<const SWidget*, TWeakPtr<SWidget>> LetterboxWrappers;

	/** Shares the keyboard and gamepads out between two players */
	TSharedPtr<FTwoPlayerInput> TwoPlayerInput;
};
