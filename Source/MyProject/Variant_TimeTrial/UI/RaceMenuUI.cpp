// Copyright Epic Games, Inc. All Rights Reserved.


#include "RaceMenuUI.h"
#include "TimeTrialPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Font.h"
#include "UObject/ConstructorHelpers.h"

URaceMenuUI::URaceMenuUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// the menu has to be focusable for key events to reach NativeOnKeyDown
	SetIsFocusable(true);

	// same default font as a Text widget dropped in the designer. Its fallback face covers Japanese.
	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(*UWidget::GetDefaultFontName());
		MenuFont = FSlateFontInfo(RobotoFontObj.Object, 28, FName("Bold"));
	}
}

UVerticalBox* URaceMenuUI::CreatePanelLayout()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// dim the race behind the menu
	UBorder* Shade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Shade"));
	Shade->SetBrushColor(ShadeColor);

	UOverlaySlot* ShadeSlot = Root->AddChildToOverlay(Shade);
	ShadeSlot->SetHorizontalAlignment(HAlign_Fill);
	ShadeSlot->SetVerticalAlignment(VAlign_Fill);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(56.0f, 32.0f));

	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Center);
	PanelSlot->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Content"));
	Panel->SetContent(Content);

	return Content;
}

UWidget* URaceMenuUI::CreateMenuItem(const FText& Label, ERaceMenuAction Action)
{
	const FString BaseName = FString::Printf(TEXT("Item%d"), Items.Num());

	// a fixed width keeps the bars lined up whatever the label length
	USizeBox* ItemBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *(BaseName + TEXT("Box")));
	ItemBox->SetWidthOverride(MenuItemWidth);

	UBorder* Bar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *(BaseName + TEXT("Bar")));
	Bar->SetPadding(FMargin(28.0f, 10.0f));
	Bar->SetVerticalAlignment(VAlign_Center);
	ItemBox->SetContent(Bar);

	UTextBlock* LabelText = CreateText(*(BaseName + TEXT("Label")), Label, MenuFont, ItemTextColor);
	Bar->SetContent(LabelText);

	FRaceMenuItem& Item = Items.AddDefaulted_GetRef();
	Item.Bar = Bar;
	Item.Label = LabelText;
	Item.Action = Action;

	return ItemBox;
}

UTextBlock* URaceMenuUI::CreateText(FName Name, const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Color)
{
	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);

	TextBlock->SetText(Text);
	TextBlock->SetColorAndOpacity(Color);

	if (Font.HasValidFont())
	{
		TextBlock->SetFont(Font);
	}

	// soft drop shadow so the text stays readable over bright scenery
	TextBlock->SetShadowOffset(FVector2D(2.0f, 2.0f));
	TextBlock->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));

	return TextBlock;
}

void URaceMenuUI::NativeConstruct()
{
	Super::NativeConstruct();

	bActionChosen = false;
	bFocusClaimed = false;

	// the items are picked with the mouse too, so the menu has to take mouse events
	SetVisibility(ESlateVisibility::Visible);

	HighlightItem(0);
}

void URaceMenuUI::NativeDestruct()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
	}

	Super::NativeDestruct();
}

void URaceMenuUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// the widget isn't in the Slate hierarchy yet while it's being constructed, so it can't take focus until now
	if (!bFocusClaimed)
	{
		ClaimKeyboardFocus();
	}
}

void URaceMenuUI::ClaimKeyboardFocus()
{
	APlayerController* PC = GetOwningPlayer();

	if (!PC)
	{
		return;
	}

	bFocusClaimed = true;

	// send keyboard and gamepad input to this widget while it's up. The game still gets anything the widget doesn't
	// handle, such as the pause key
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);

	PC->SetInputMode(InputMode);
}

FReply URaceMenuUI::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	// a held key repeating doesn't count: it may still be held from driving when the menu came up
	const bool bPress = !InKeyEvent.IsRepeat();

	// previous item
	if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up
		|| Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left)
	{
		if (bPress)
		{
			MoveHighlight(-1);
		}

		return FReply::Handled();
	}

	// next item
	if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down
		|| Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right)
	{
		if (bPress)
		{
			MoveHighlight(1);
		}

		return FReply::Handled();
	}

	// pick the highlighted item
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom
		|| Key == EKeys::Virtual_Gamepad_Accept.GetVirtualKey())
	{
		if (bPress && Items.IsValidIndex(HighlightedItem))
		{
			ChooseAction(Items[HighlightedItem].Action);
		}

		return FReply::Handled();
	}

	// back out
	if (Key == EKeys::BackSpace || Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right
		|| Key == EKeys::Virtual_Gamepad_Back.GetVirtualKey())
	{
		if (bPress)
		{
			OnBack();
		}

		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply URaceMenuUI::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const int32 Item = FindItemUnder(InMouseEvent.GetScreenSpacePosition());

	if (Item != INDEX_NONE && Item != HighlightedItem)
	{
		HighlightItem(Item);
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply URaceMenuUI::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const int32 Item = FindItemUnder(InMouseEvent.GetScreenSpacePosition());

		if (Item != INDEX_NONE)
		{
			HighlightItem(Item);
			ChooseAction(Items[Item].Action);
			return FReply::Handled();
		}
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

int32 URaceMenuUI::FindItemUnder(const FVector2D& ScreenPosition) const
{
	return Items.IndexOfByPredicate([&ScreenPosition](const FRaceMenuItem& Item)
	{
		return Item.Bar && Item.Bar->GetCachedGeometry().IsUnderLocation(ScreenPosition);
	});
}

void URaceMenuUI::MoveHighlight(int32 Delta)
{
	const int32 Count = Items.Num();

	if (Count == 0)
	{
		return;
	}

	// wrap around the ends so the list cycles
	const int32 Current = FMath::Max(HighlightedItem, 0);

	HighlightItem(((Current + Delta) % Count + Count) % Count);
}

void URaceMenuUI::HighlightItem(int32 ItemIndex)
{
	HighlightedItem = Items.IsValidIndex(ItemIndex) ? ItemIndex : INDEX_NONE;

	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		const bool bHighlighted = Index == HighlightedItem;

		if (Items[Index].Bar)
		{
			Items[Index].Bar->SetBrushColor(bHighlighted ? HighlightColor : ItemColor);
		}

		if (Items[Index].Label)
		{
			Items[Index].Label->SetColorAndOpacity(bHighlighted ? HighlightTextColor : ItemTextColor);
		}
	}
}

void URaceMenuUI::ChooseAction(ERaceMenuAction Action)
{
	ATimeTrialPlayerController* PC = Cast<ATimeTrialPlayerController>(GetOwningPlayer());

	// a click and a key press could both land before the menu is torn down, so only the first one counts
	if (bActionChosen || !PC)
	{
		return;
	}

	bActionChosen = true;

	switch (Action)
	{
	case ERaceMenuAction::Resume:
		PC->ResumeRace();
		break;

	case ERaceMenuAction::Retry:
		PC->RetryRace();
		break;

	case ERaceMenuAction::ReturnToTitle:
		PC->ReturnToTitle();
		break;
	}
}
