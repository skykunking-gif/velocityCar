// Copyright (c) 2026 skykunking-gif. All Rights Reserved.


#include "LetterboxGameViewportClient.h"
#include "TwoPlayerInput.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"

void ULetterboxGameViewportClient::Init(struct FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice)
{
	Super::Init(WorldContext, OwningGameInstance, bCreateNewAudioDevice);

	if (FSlateApplication::IsInitialized())
	{
		TwoPlayerInput = MakeShared<FTwoPlayerInput>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(TwoPlayerInput);
	}
}

void ULetterboxGameViewportClient::BeginDestroy()
{
	if (TwoPlayerInput.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(TwoPlayerInput);
	}

	TwoPlayerInput.Reset();

	Super::BeginDestroy();
}

TSharedRef<SWidget> ULetterboxGameViewportClient::WrapInLetterbox(TSharedRef<SWidget> ViewportContent)
{
	PruneLetterboxes();

	// pinning the min and the max to the same value holds the content at exactly that ratio,
	// centered in whatever space the window gives us
	TSharedRef<SWidget> Wrapper = SNew(SBox)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.MinAspectRatio(LockedAspectRatio)
		.MaxAspectRatio(LockedAspectRatio)
		[
			ViewportContent
		];

	LetterboxWrappers.Add(&ViewportContent.Get(), Wrapper);

	return Wrapper;
}

TSharedRef<SWidget> ULetterboxGameViewportClient::WrapForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent)
{
	PruneLetterboxes();

	TWeakObjectPtr<ULocalPlayer> WeakPlayer(Player);
	const float BaseRatio = LockedAspectRatio;
	const float SplitScale = SplitscreenWidgetScale;

	// the player's part of the screen: the whole of it alone, half its height in a two player split. The camera
	// manager locks the player's view to the same shape, so the box fills the view exactly
	auto Ratio = [WeakPlayer, BaseRatio]()
	{
		const ULocalPlayer* LocalPlayer = WeakPlayer.Get();
		const float Ratio = LocalPlayer && LocalPlayer->Size.X > 0.0f && LocalPlayer->Size.Y > 0.0f
			? BaseRatio * LocalPlayer->Size.X / LocalPlayer->Size.Y
			: BaseRatio;

		return FOptionalSize(Ratio);
	};

	// a HUD laid out for the whole screen, drawn smaller in a part of it
	auto Scale = [WeakPlayer, SplitScale]()
	{
		const ULocalPlayer* LocalPlayer = WeakPlayer.Get();

		return LocalPlayer && LocalPlayer->Size.Y < 0.99f ? SplitScale : 1.0f;
	};

	TSharedRef<SWidget> Wrapper = SNew(SBox)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.MinAspectRatio_Lambda(Ratio)
		.MaxAspectRatio_Lambda(Ratio)
		[
			SNew(SDPIScaler)
			.DPIScale_Lambda(Scale)
			[
				ViewportContent
			]
		];

	LetterboxWrappers.Add(&ViewportContent.Get(), Wrapper);

	return Wrapper;
}

TSharedRef<SWidget> ULetterboxGameViewportClient::FindLetterbox(TSharedRef<SWidget> ViewportContent)
{
	// the entry is left in place: UMG removes a widget by calling both RemoveViewportWidgetContent
	// and RemoveViewportWidgetForPlayer with the same widget, and the second call has to find the
	// wrapper too. It drops out of the map once the wrapper itself is gone
	if (const TWeakPtr<SWidget>* Wrapper = LetterboxWrappers.Find(&ViewportContent.Get()))
	{
		if (TSharedPtr<SWidget> PinnedWrapper = Wrapper->Pin())
		{
			return PinnedWrapper.ToSharedRef();
		}
	}

	return ViewportContent;
}

void ULetterboxGameViewportClient::PruneLetterboxes()
{
	for (auto It = LetterboxWrappers.CreateIterator(); It; ++It)
	{
		if (!It.Value().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void ULetterboxGameViewportClient::AddViewportWidgetContent(TSharedRef<SWidget> ViewportContent, const int32 ZOrder)
{
	Super::AddViewportWidgetContent(WrapInLetterbox(ViewportContent), ZOrder);
}

void ULetterboxGameViewportClient::RemoveViewportWidgetContent(TSharedRef<SWidget> ViewportContent)
{
	Super::RemoveViewportWidgetContent(FindLetterbox(ViewportContent));

	PruneLetterboxes();
}

void ULetterboxGameViewportClient::AddViewportWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent, const int32 ZOrder)
{
	Super::AddViewportWidgetForPlayer(Player, WrapForPlayer(Player, ViewportContent), ZOrder);
}

void ULetterboxGameViewportClient::RemoveViewportWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent)
{
	Super::RemoveViewportWidgetForPlayer(Player, FindLetterbox(ViewportContent));

	PruneLetterboxes();
}
