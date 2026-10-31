// Copyright (c) 2026 skykunking-gif. All Rights Reserved.


#include "TitleScreenUI.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "GameAudioSettings.h"
#include "GameGraphicsSettings.h"
#include "TwoPlayerInput.h"

UTitleScreenUI::UTitleScreenUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// the screen has to be focusable for key events to reach NativeOnKeyDown
	SetIsFocusable(true);

	// same default font as a Text widget dropped in the designer. Its fallback face covers Japanese.
	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(*UWidget::GetDefaultFontName());
		TitleFont = FSlateFontInfo(RobotoFontObj.Object, 88, FName("Bold Italic"));
		MenuFont = FSlateFontInfo(RobotoFontObj.Object, 28, FName("Bold"));
		SmallFont = FSlateFontInfo(RobotoFontObj.Object, 18, FName("Regular"));

		// the NPC level buttons, by the Id the game mode gives each level
		static ConstructorHelpers::FObjectFinder<UTexture2D> EasyButtonObj(TEXT("/Game/UI/NpcDifficulty/T_NpcButton_Easy.T_NpcButton_Easy"));
		static ConstructorHelpers::FObjectFinder<UTexture2D> NormalButtonObj(TEXT("/Game/UI/NpcDifficulty/T_NpcButton_Normal.T_NpcButton_Normal"));
		static ConstructorHelpers::FObjectFinder<UTexture2D> HardButtonObj(TEXT("/Game/UI/NpcDifficulty/T_NpcButton_Hard.T_NpcButton_Hard"));
		static ConstructorHelpers::FObjectFinder<UTexture2D> EliteButtonObj(TEXT("/Game/UI/NpcDifficulty/T_NpcButton_Elite.T_NpcButton_Elite"));
		static ConstructorHelpers::FObjectFinder<UTexture2D> ProButtonObj(TEXT("/Game/UI/NpcDifficulty/T_NpcButton_Pro.T_NpcButton_Pro"));

		DifficultyButtonImages.Add(TEXT("Easy"), EasyButtonObj.Object);
		DifficultyButtonImages.Add(TEXT("Normal"), NormalButtonObj.Object);
		DifficultyButtonImages.Add(TEXT("Hard"), HardButtonObj.Object);
		DifficultyButtonImages.Add(TEXT("Extreme"), EliteButtonObj.Object);
		DifficultyButtonImages.Add(TEXT("Pro"), ProButtonObj.Object);
	}

	auto AddControl = [this](const TCHAR* Action, const TCHAR* Keyboard, const TCHAR* Gamepad)
	{
		FTitleControlsRow& Row = ControlsRows.AddDefaulted_GetRef();
		Row.Action = FText::FromString(Action);
		Row.Keyboard = FText::FromString(Keyboard);
		Row.Gamepad = FText::FromString(Gamepad);
	};

	AddControl(TEXT("アクセル"), TEXT("W / ↑"), TEXT("RT"));
	AddControl(TEXT("ブレーキ / バック"), TEXT("S / ↓"), TEXT("LT"));
	AddControl(TEXT("ハンドル"), TEXT("A・D / ← →"), TEXT("左スティック"));
	AddControl(TEXT("サイドブレーキ"), TEXT("Space"), TEXT("A / LB"));
	AddControl(TEXT("シフトアップ（MT）"), TEXT("Q"), TEXT("X"));
	AddControl(TEXT("シフトダウン（MT）"), TEXT("E"), TEXT("B"));
	AddControl(TEXT("まわりを見る"), TEXT("マウス"), TEXT("右スティック"));
	AddControl(TEXT("カメラ切り替え"), TEXT("Tab"), TEXT("ビュー"));
	AddControl(TEXT("コースに戻る"), TEXT("Backspace"), TEXT("Y"));
	AddControl(TEXT("ライト"), TEXT("L"), TEXT("ー"));
	AddControl(TEXT("ポーズ"), TEXT("Esc / P"), TEXT("メニュー"));
}

void UTitleScreenUI::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		CreateLayout();
	}
}

void UTitleScreenUI::CreateLayout()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	Pages.SetNum(static_cast<int32>(ETitleMenuPage::Join) + 1);

	// title in the upper left, leaving the middle of the screen to the flythrough
	if (!GameTitle.IsEmpty())
	{
		UTextBlock* TitleText = CreateText(TEXT("TitleText"), GameTitle, TitleFont, FLinearColor::White);
		TitleText->SetShadowOffset(FVector2D(3.0f, 3.0f));

		UOverlaySlot* TitleSlot = Root->AddChildToOverlay(TitleText);
		TitleSlot->SetHorizontalAlignment(HAlign_Left);
		TitleSlot->SetVerticalAlignment(VAlign_Top);
		TitleSlot->SetPadding(FMargin(ScreenMargin, 96.0f, 0.0f, 0.0f));
	}

	// main menu
	UVerticalBox* MainMenu = CreateMenuPage(Root, ETitleMenuPage::Main, FText::GetEmpty());
	AddMenuItem(MainMenu, ETitleMenuPage::Main, StartLabel, ETitleMenuAction::OpenModeSelect);
	AddMenuItem(MainMenu, ETitleMenuPage::Main, OptionsLabel, ETitleMenuAction::OpenOptions);
	AddMenuItem(MainMenu, ETitleMenuPage::Main, ControlsLabel, ETitleMenuAction::OpenControls);

	// time trial or a race against NPCs
	UVerticalBox* ModeMenu = CreateMenuPage(Root, ETitleMenuPage::Mode, ModeHeading);
	AddMenuItem(ModeMenu, ETitleMenuPage::Mode, TimeTrialLabel, ETitleMenuAction::StartTimeTrial);
	AddMenuItem(ModeMenu, ETitleMenuPage::Mode, VersusLabel, ETitleMenuAction::OpenDifficultySelect);
	AddMenuItem(ModeMenu, ETitleMenuPage::Mode, TwoPlayerLabel, ETitleMenuAction::OpenTwoPlayerSelect);
	AddMenuItem(ModeMenu, ETitleMenuPage::Mode, BackLabel, ETitleMenuAction::Back);

	// how well the NPCs drive, as offered by the game mode. Two players can race on their own or with NPCs
	UVerticalBox* DifficultyMenu = CreateMenuPage(Root, ETitleMenuPage::Difficulty, DifficultyHeading);
	UVerticalBox* TwoPlayerMenu = CreateMenuPage(Root, ETitleMenuPage::TwoPlayer, TwoPlayerHeading);

	AddMenuItem(TwoPlayerMenu, ETitleMenuPage::TwoPlayer, NoNpcLabel, ETitleMenuAction::ChooseTwoPlayerNpc, INDEX_NONE);

	const ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(UGameplayStatics::GetGameMode(this));
	const int32 LevelCount = GM ? GM->GetNpcDifficulties().Num() : 0;

	for (int32 Level = 0; Level < LevelCount; ++Level)
	{
		// a level still to be unlocked isn't offered yet
		if (!GM->IsDifficultyUnlocked(Level))
		{
			continue;
		}

		const FNpcDifficulty& Difficulty = GM->GetNpcDifficulties()[Level];
		FText Name = Difficulty.DisplayName;

		if (Name.IsEmpty())
		{
			Name = FText::Format(NSLOCTEXT("TitleScreenUI", "Level", "レベル {0}"), FText::AsNumber(Level + 1));
		}

		// the level's button picture if it has one, otherwise its name
		const TObjectPtr<UTexture2D>* ButtonImage = DifficultyButtonImages.Find(Difficulty.Id);
		UTexture2D* Image = ButtonImage ? ButtonImage->Get() : nullptr;

		AddMenuItem(DifficultyMenu, ETitleMenuPage::Difficulty, Name, ETitleMenuAction::StartVersusNpc, Level, Image);
		AddMenuItem(TwoPlayerMenu, ETitleMenuPage::TwoPlayer, Name, ETitleMenuAction::ChooseTwoPlayerNpc, Level, Image);
	}

	AddMenuItem(DifficultyMenu, ETitleMenuPage::Difficulty, BackLabel, ETitleMenuAction::Back);
	AddMenuItem(TwoPlayerMenu, ETitleMenuPage::TwoPlayer, BackLabel, ETitleMenuAction::Back);

	// waiting for the second player, in a panel in the middle of the screen
	UVerticalBox* JoinContent = CreatePanelPage(Root, ETitleMenuPage::Join, JoinTitleLabel);

	UTextBlock* JoinText = CreateText(TEXT("JoinText"), JoinBodyLabel, SmallFont, FLinearColor::White);
	JoinText->SetJustification(ETextJustify::Center);
	JoinContent->AddChildToVerticalBox(JoinText)->SetPadding(FMargin(0.0f, 24.0f, 0.0f, 24.0f));

	UVerticalBox* JoinMenu = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("JoinMenu"));
	JoinContent->AddChildToVerticalBox(JoinMenu);
	AddMenuItem(JoinMenu, ETitleMenuPage::Join, BackLabel, ETitleMenuAction::Back);

	// options panel in the middle of the screen
	UVerticalBox* PanelContent = CreatePanelPage(Root, ETitleMenuPage::Options, OptionsTitleLabel);

	// the volumes and the picture quality, under the heading
	UVerticalBox* Settings = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OptionsSettings"));
	PanelContent->AddChildToVerticalBox(Settings)->SetPadding(FMargin(0.0f, 24.0f, 0.0f, 0.0f));

	AddMenuItem(Settings, ETitleMenuPage::Options, MusicVolumeLabel, ETitleMenuAction::AdjustMusicVolume);
	AddMenuItem(Settings, ETitleMenuPage::Options, EngineVolumeLabel, ETitleMenuAction::AdjustEngineVolume);
	AddMenuItem(Settings, ETitleMenuPage::Options, GraphicsQualityLabel, ETitleMenuAction::AdjustGraphicsQuality);
	AddMenuItem(Settings, ETitleMenuPage::Options, BackLabel, ETitleMenuAction::Back);

	RefreshSettingValues();

	// the controls list, in a panel of its own
	UVerticalBox* ControlsContent = CreatePanelPage(Root, ETitleMenuPage::Controls, ControlsTitleLabel);
	CreateControlsTable(ControlsContent);

	// the second player's keys when the two share the keyboard, under the table
	ControlsContent->AddChildToVerticalBox(CreateText(TEXT("SecondPlayerKeysHeading"), SecondPlayerKeysHeading, SmallFont, FLinearColor(0.55f, 0.55f, 0.55f, 1.0f)));
	ControlsContent->AddChildToVerticalBox(CreateText(TEXT("SecondPlayerKeys"), SecondPlayerKeysLabel, SmallFont, ItemTextColor))->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 16.0f));

	UVerticalBox* ControlsMenu = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ControlsMenu"));
	ControlsContent->AddChildToVerticalBox(ControlsMenu)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
	AddMenuItem(ControlsMenu, ETitleMenuPage::Controls, BackLabel, ETitleMenuAction::Back);

	// controls hint along the bottom
	HintText = CreateText(TEXT("HintText"), HintLabel, SmallFont, ItemTextColor);

	UOverlaySlot* HintSlot = Root->AddChildToOverlay(HintText);
	HintSlot->SetHorizontalAlignment(HAlign_Left);
	HintSlot->SetVerticalAlignment(VAlign_Bottom);
	HintSlot->SetPadding(FMargin(ScreenMargin, 0.0f, 0.0f, 80.0f));
}

UVerticalBox* UTitleScreenUI::CreatePanelPage(UOverlay* Root, ETitleMenuPage Page, const FText& Heading)
{
	const FString BaseName = StaticEnum<ETitleMenuPage>()->GetNameStringByValue(static_cast<int64>(Page));

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *(BaseName + TEXT("Panel")));
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(56.0f, 32.0f));
	Pages[static_cast<int32>(Page)] = Panel;

	// centred below the game title, so a tall panel doesn't run into it
	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Center);
	PanelSlot->SetVerticalAlignment(VAlign_Center);
	PanelSlot->SetPadding(FMargin(0.0f, 120.0f, 0.0f, 0.0f));

	UVerticalBox* PanelContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *(BaseName + TEXT("Content")));
	Panel->SetContent(PanelContent);

	UTextBlock* PanelTitle = CreateText(*(BaseName + TEXT("TitleText")), Heading, MenuFont, FLinearColor::White);
	PanelContent->AddChildToVerticalBox(PanelTitle)->SetHorizontalAlignment(HAlign_Center);

	return PanelContent;
}

void UTitleScreenUI::CreateControlsTable(UVerticalBox* Panel)
{
	UVerticalBox* Table = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ControlsTable"));
	Panel->AddChildToVerticalBox(Table)->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 16.0f));

	const float Widths[] = { ControlsColumnWidths.X, ControlsColumnWidths.Y, ControlsColumnWidths.Z };

	// the column headings, then a line per control
	auto AddRow = [this, Table, &Widths](int32 RowIndex, const FText& Action, const FText& Keyboard, const FText& Gamepad, bool bHeading)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *FString::Printf(TEXT("ControlsRow%d"), RowIndex));
		Table->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, bHeading ? 0.0f : 3.0f, 0.0f, bHeading ? 4.0f : 0.0f));

		const FText* Cells[] = { &Action, &Keyboard, &Gamepad };

		for (int32 Column = 0; Column < UE_ARRAY_COUNT(Cells); ++Column)
		{
			USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("ControlsCell%d_%d"), RowIndex, Column));
			Cell->SetWidthOverride(Widths[Column]);
			Row->AddChildToHorizontalBox(Cell);

			// what the control does in white, the keys and buttons a little dimmer, the headings dimmer still
			const FLinearColor Color = bHeading ? FLinearColor(0.55f, 0.55f, 0.55f, 1.0f) : (Column == 0 ? FLinearColor::White : ItemTextColor);
			Cell->SetContent(CreateText(*FString::Printf(TEXT("ControlsText%d_%d"), RowIndex, Column), *Cells[Column], SmallFont, Color));
		}
	};

	AddRow(0, FText::GetEmpty(), KeyboardHeading, GamepadHeading, true);

	for (int32 Index = 0; Index < ControlsRows.Num(); ++Index)
	{
		const FTitleControlsRow& Control = ControlsRows[Index];
		AddRow(Index + 1, Control.Action, Control.Keyboard, Control.Gamepad, false);
	}
}

UVerticalBox* UTitleScreenUI::CreateMenuPage(UOverlay* Root, ETitleMenuPage Page, const FText& Heading)
{
	UVerticalBox* Menu = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("Menu%d"), static_cast<int32>(Page)));
	Pages[static_cast<int32>(Page)] = Menu;

	// lower left, growing upwards as items are added
	UOverlaySlot* MenuSlot = Root->AddChildToOverlay(Menu);
	MenuSlot->SetHorizontalAlignment(HAlign_Left);
	MenuSlot->SetVerticalAlignment(VAlign_Bottom);
	MenuSlot->SetPadding(FMargin(ScreenMargin, 0.0f, 0.0f, 150.0f));

	if (!Heading.IsEmpty())
	{
		UTextBlock* HeadingText = CreateText(*FString::Printf(TEXT("Heading%d"), static_cast<int32>(Page)), Heading, SmallFont, FLinearColor::White);
		Menu->AddChildToVerticalBox(HeadingText)->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 10.0f));
	}

	return Menu;
}

void UTitleScreenUI::AddMenuItem(UVerticalBox* Menu, ETitleMenuPage Page, const FText& Label, ETitleMenuAction Action, int32 Value, UTexture2D* Image)
{
	const FString BaseName = FString::Printf(TEXT("Item%d"), Items.Num());

	// a fixed width keeps the bars lined up whatever the label length
	USizeBox* ItemBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *(BaseName + TEXT("Box")));
	ItemBox->SetWidthOverride(MenuItemWidth);

	UBorder* Bar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *(BaseName + TEXT("Bar")));
	Bar->SetPadding(FMargin(28.0f, 10.0f));
	Bar->SetVerticalAlignment(VAlign_Center);
	ItemBox->SetContent(Bar);

	UTextBlock* LabelText = nullptr;
	UTextBlock* ValueText = nullptr;
	UImage* ImageWidget = nullptr;

	if (Image)
	{
		// a picture button: drawn at a fixed height in the picture's own shape, on a bar that stays clear and only
		// gives the mouse something to pick it with
		const float Aspect = Image->GetSurfaceHeight() > 0.0f ? Image->GetSurfaceWidth() / Image->GetSurfaceHeight() : 5.0f;

		ItemBox->SetWidthOverride(DifficultyButtonHeight * Aspect);
		ItemBox->SetHeightOverride(DifficultyButtonHeight);

		Bar->SetPadding(FMargin(0.0f));
		Bar->SetHorizontalAlignment(HAlign_Fill);
		Bar->SetVerticalAlignment(VAlign_Fill);

		ImageWidget = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *(BaseName + TEXT("Image")));
		ImageWidget->SetBrushFromTexture(Image, false);

		// grows from its left edge when highlighted, so it stays lined up with the rest of the menu
		ImageWidget->SetRenderTransformPivot(FVector2D(0.0f, 0.5f));
		Bar->SetContent(ImageWidget);
	}
	else if (Action == ETitleMenuAction::AdjustMusicVolume || Action == ETitleMenuAction::AdjustEngineVolume || Action == ETitleMenuAction::AdjustGraphicsQuality)
	{
		LabelText = CreateText(*(BaseName + TEXT("Label")), Label, MenuFont, ItemTextColor);

		// a setting: its name on the left and its value on the right
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *(BaseName + TEXT("Row")));
		Bar->SetContent(Row);

		UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelText);
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);

		// a fixed width with the value centred keeps the arrows in place whether it's 0 or 10
		USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *(BaseName + TEXT("ValueBox")));
		ValueBox->SetWidthOverride(SettingValueWidth);
		Row->AddChildToHorizontalBox(ValueBox)->SetVerticalAlignment(VAlign_Center);

		ValueText = CreateText(*(BaseName + TEXT("Value")), FText::GetEmpty(), MenuFont, ItemTextColor);
		ValueText->SetJustification(ETextJustify::Center);
		ValueBox->SetContent(ValueText);
	}
	else
	{
		LabelText = CreateText(*(BaseName + TEXT("Label")), Label, MenuFont, ItemTextColor);
		Bar->SetContent(LabelText);
	}

	// a gap above every item but the first, and a wider one above a back item
	const bool bFirst = !Items.ContainsByPredicate([Page](const FTitleMenuItem& Item) { return Item.Page == Page; });
	const float Gap = bFirst ? 0.0f : (Action == ETitleMenuAction::Back ? 24.0f : 12.0f);

	UVerticalBoxSlot* ItemSlot = Menu->AddChildToVerticalBox(ItemBox);
	ItemSlot->SetPadding(FMargin(0.0f, Gap, 0.0f, 0.0f));

	if (Page == ETitleMenuPage::Options || Page == ETitleMenuPage::Controls || Page == ETitleMenuPage::Join)
	{
		ItemSlot->SetHorizontalAlignment(HAlign_Center);
	}
	else if (Image)
	{
		// left aligned at its own width, as stretching it to the menu's width would squash the picture
		ItemSlot->SetHorizontalAlignment(HAlign_Left);
	}

	FTitleMenuItem& Item = Items.AddDefaulted_GetRef();
	Item.Bar = Bar;
	Item.Label = LabelText;
	Item.ValueText = ValueText;
	Item.Image = ImageWidget;
	Item.Page = Page;
	Item.Action = Action;
	Item.Value = Value;
}

UTextBlock* UTitleScreenUI::CreateText(FName Name, const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Color)
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

void UTitleScreenUI::NativeConstruct()
{
	Super::NativeConstruct();

	bStartChosen = false;

	// the menus are picked from with the mouse too, so the screen has to take mouse events
	SetVisibility(ESlateVisibility::Visible);

	ShowPage(ETitleMenuPage::Main);

	// the widget isn't in the Slate hierarchy yet while it's being constructed, so it can't
	// take focus until the next tick
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &UTitleScreenUI::ClaimKeyboardFocus);
	}
}

void UTitleScreenUI::NativeDestruct()
{
	// nobody is waiting for the second player any more
	if (FTwoPlayerInput* TwoPlayerInput = FTwoPlayerInput::Find(this))
	{
		TwoPlayerInput->EndJoin();
	}

	// hand input back to the game
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
	}

	Super::NativeDestruct();
}

void UTitleScreenUI::ClaimKeyboardFocus()
{
	APlayerController* PC = GetOwningPlayer();

	if (!PC)
	{
		return;
	}

	// send keyboard and gamepad input to this widget while the screen is up. The game still
	// gets anything the widget doesn't handle.
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);

	PC->SetInputMode(InputMode);
}

FReply UTitleScreenUI::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	// previous item
	if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up)
	{
		MoveHighlight(-1);
		return FReply::Handled();
	}

	// next item
	if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
	{
		MoveHighlight(1);
		return FReply::Handled();
	}

	// turn the highlighted setting down or up
	if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left)
	{
		if (AdjustHighlightedSetting(-1))
		{
			return FReply::Handled();
		}
	}

	if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right)
	{
		if (AdjustHighlightedSetting(1))
		{
			return FReply::Handled();
		}
	}

	// pick the highlighted item
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom
		|| Key == EKeys::Virtual_Gamepad_Accept.GetVirtualKey())
	{
		ActivateItem(HighlightedItem);
		return FReply::Handled();
	}

	// back a menu
	if (Key == EKeys::BackSpace || Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right
		|| Key == EKeys::Virtual_Gamepad_Back.GetVirtualKey())
	{
		GoBack();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UTitleScreenUI::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const int32 Item = FindItemUnder(InMouseEvent.GetScreenSpacePosition());

	if (Item != INDEX_NONE && Item != HighlightedItem)
	{
		HighlightItem(Item);
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UTitleScreenUI::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const int32 Item = FindItemUnder(InMouseEvent.GetScreenSpacePosition());

		if (Item != INDEX_NONE)
		{
			HighlightItem(Item);

			// a click on the left half of a setting turns it down, on the right half up
			const FGeometry& BarGeometry = Items[Item].Bar->GetCachedGeometry();
			const float ClickX = BarGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()).X;

			if (!AdjustHighlightedSetting(ClickX < 0.5f * BarGeometry.GetLocalSize().X ? -1 : 1))
			{
				ActivateItem(Item);
			}

			return FReply::Handled();
		}
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

int32 UTitleScreenUI::FindItemUnder(const FVector2D& ScreenPosition) const
{
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		const FTitleMenuItem& Item = Items[Index];

		if (Item.Page == CurrentPage && Item.Bar && Item.Bar->GetCachedGeometry().IsUnderLocation(ScreenPosition))
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

int32 UTitleScreenUI::FindItem(ETitleMenuPage Page, ETitleMenuAction Action) const
{
	return Items.IndexOfByPredicate([Page, Action](const FTitleMenuItem& Item)
	{
		return Item.Page == Page && Item.Action == Action;
	});
}

void UTitleScreenUI::ShowPage(ETitleMenuPage NewPage)
{
	CurrentPage = NewPage;

	// the join panel waits for the second player's first button press
	if (FTwoPlayerInput* TwoPlayerInput = FTwoPlayerInput::Find(this))
	{
		if (NewPage == ETitleMenuPage::Join)
		{
			TWeakObjectPtr<UTitleScreenUI> WeakThis(this);

			TwoPlayerInput->BeginJoin([WeakThis](ESecondPlayerDevice Device, FInputDeviceId Gamepad)
			{
				if (UTitleScreenUI* StrongThis = WeakThis.Get())
				{
					StrongThis->OnSecondPlayerJoined(Device, Gamepad);
				}
			});

		} else {

			TwoPlayerInput->EndJoin();
		}
	}

	for (int32 Index = 0; Index < Pages.Num(); ++Index)
	{
		if (Pages[Index])
		{
			Pages[Index]->SetVisibility(Index == static_cast<int32>(NewPage) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}

	if (HintText)
	{
		switch (NewPage)
		{
		case ETitleMenuPage::Main:
			HintText->SetText(HintLabel);
			break;

		case ETitleMenuPage::Options:
			HintText->SetText(OptionsHintLabel);
			break;

		case ETitleMenuPage::Controls:
			HintText->SetText(ControlsHintLabel);
			break;

		case ETitleMenuPage::Join:
			HintText->SetText(JoinHintLabel);
			break;

		default:
			HintText->SetText(SubMenuHintLabel);
			break;
		}
	}

	// start on the menu's first item
	HighlightItem(Items.IndexOfByPredicate([NewPage](const FTitleMenuItem& Item) { return Item.Page == NewPage; }));
}

void UTitleScreenUI::ShowModeChoice()
{
	const ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(UGameplayStatics::GetGameMode(this));

	// a two player game comes back to who else races, on the pick it was started with
	if (GM && GM->IsTwoPlayers())
	{
		ShowPage(ETitleMenuPage::TwoPlayer);

		const int32 Pick = GM->GetRaceMode() == ETimeTrialRaceMode::VersusNpc ? GM->GetSelectedDifficulty() : INDEX_NONE;
		const int32 PickItem = Items.IndexOfByPredicate([Pick](const FTitleMenuItem& Item)
		{
			return Item.Page == ETitleMenuPage::TwoPlayer && Item.Action == ETitleMenuAction::ChooseTwoPlayerNpc && Item.Value == Pick;
		});

		if (PickItem != INDEX_NONE)
		{
			HighlightItem(PickItem);
		}

		return;
	}

	if (GM && GM->GetRaceMode() == ETimeTrialRaceMode::VersusNpc)
	{
		ShowPage(ETitleMenuPage::Difficulty);

		const int32 Difficulty = GM->GetSelectedDifficulty();
		const int32 LevelItem = Items.IndexOfByPredicate([Difficulty](const FTitleMenuItem& Item)
		{
			return Item.Page == ETitleMenuPage::Difficulty && Item.Action == ETitleMenuAction::StartVersusNpc && Item.Value == Difficulty;
		});

		if (LevelItem != INDEX_NONE)
		{
			HighlightItem(LevelItem);
		}

		return;
	}

	ShowPage(ETitleMenuPage::Mode);

	const int32 TimeTrialItem = FindItem(ETitleMenuPage::Mode, ETitleMenuAction::StartTimeTrial);

	if (TimeTrialItem != INDEX_NONE)
	{
		HighlightItem(TimeTrialItem);
	}
}

void UTitleScreenUI::MoveHighlight(int32 Delta)
{
	TArray<int32> PageItems;

	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		if (Items[Index].Page == CurrentPage)
		{
			PageItems.Add(Index);
		}
	}

	if (PageItems.Num() == 0 || Delta == 0)
	{
		return;
	}

	// wrap around the ends so the list cycles
	const int32 Current = FMath::Max(PageItems.IndexOfByKey(HighlightedItem), 0);
	const int32 Count = PageItems.Num();

	HighlightItem(PageItems[((Current + Delta) % Count + Count) % Count]);
}

void UTitleScreenUI::HighlightItem(int32 ItemIndex)
{
	HighlightedItem = Items.IsValidIndex(ItemIndex) ? ItemIndex : INDEX_NONE;

	RefreshHighlight();
}

void UTitleScreenUI::RefreshHighlight()
{
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		const bool bHighlighted = Index == HighlightedItem;

		// a picture button lights up and grows instead of having a bar behind it
		if (UImage* Image = Items[Index].Image)
		{
			Image->SetColorAndOpacity(bHighlighted ? FLinearColor::White : DifficultyButtonDimColor);
			Image->SetRenderScale(FVector2D(bHighlighted ? DifficultyButtonHighlightScale : 1.0f));

			if (Items[Index].Bar)
			{
				Items[Index].Bar->SetBrushColor(FLinearColor::Transparent);
			}

			continue;
		}

		if (Items[Index].Bar)
		{
			Items[Index].Bar->SetBrushColor(bHighlighted ? HighlightColor : ItemColor);
		}

		if (Items[Index].Label)
		{
			Items[Index].Label->SetColorAndOpacity(bHighlighted ? HighlightTextColor : ItemTextColor);
		}

		if (Items[Index].ValueText)
		{
			Items[Index].ValueText->SetColorAndOpacity(bHighlighted ? HighlightTextColor : ItemTextColor);
		}
	}
}

void UTitleScreenUI::ActivateItem(int32 ItemIndex)
{
	if (!Items.IsValidIndex(ItemIndex) || Items[ItemIndex].Page != CurrentPage || bStartChosen)
	{
		return;
	}

	const FTitleMenuItem& Item = Items[ItemIndex];

	switch (Item.Action)
	{
	case ETitleMenuAction::OpenModeSelect:
		ShowPage(ETitleMenuPage::Mode);
		break;

	case ETitleMenuAction::OpenOptions:
		ShowPage(ETitleMenuPage::Options);
		break;

	case ETitleMenuAction::OpenControls:
		ShowPage(ETitleMenuPage::Controls);
		break;

	case ETitleMenuAction::StartTimeTrial:
		StartGame(ETimeTrialRaceMode::TimeTrial, 0);
		break;

	case ETitleMenuAction::OpenDifficultySelect:
		ShowPage(ETitleMenuPage::Difficulty);
		break;

	case ETitleMenuAction::StartVersusNpc:
		StartGame(ETimeTrialRaceMode::VersusNpc, Item.Value);
		break;

	case ETitleMenuAction::OpenTwoPlayerSelect:
		ShowPage(ETitleMenuPage::TwoPlayer);
		break;

	case ETitleMenuAction::ChooseTwoPlayerNpc:
		// the game starts once the second player has pressed a button
		PendingTwoPlayerDifficulty = Item.Value;
		ShowPage(ETitleMenuPage::Join);
		break;

	case ETitleMenuAction::Back:
		GoBack();
		break;

	case ETitleMenuAction::AdjustMusicVolume:
	case ETitleMenuAction::AdjustEngineVolume:
	case ETitleMenuAction::AdjustGraphicsQuality:
		// settings change with left and right
		break;
	}
}

bool UTitleScreenUI::AdjustHighlightedSetting(int32 Delta)
{
	if (!Items.IsValidIndex(HighlightedItem) || Items[HighlightedItem].Page != CurrentPage)
	{
		return false;
	}

	switch (Items[HighlightedItem].Action)
	{
	case ETitleMenuAction::AdjustMusicVolume:
		UGameAudioSettings::SetMusicLevel(UGameAudioSettings::GetMusicLevel() + Delta);
		break;

	case ETitleMenuAction::AdjustEngineVolume:
		UGameAudioSettings::SetEngineLevel(UGameAudioSettings::GetEngineLevel() + Delta);
		break;

	case ETitleMenuAction::AdjustGraphicsQuality:
		UGameGraphicsSettings::SetQualityLevel(UGameGraphicsSettings::GetQualityLevel() + Delta);
		break;

	default:
		return false;
	}

	RefreshSettingValues();
	return true;
}

void UTitleScreenUI::RefreshSettingValues()
{
	for (const FTitleMenuItem& Item : Items)
	{
		if (!Item.ValueText)
		{
			continue;
		}

		FText Value;

		switch (Item.Action)
		{
		case ETitleMenuAction::AdjustMusicVolume:
			Value = FText::AsNumber(UGameAudioSettings::GetMusicLevel());
			break;

		case ETitleMenuAction::AdjustEngineVolume:
			Value = FText::AsNumber(UGameAudioSettings::GetEngineLevel());
			break;

		case ETitleMenuAction::AdjustGraphicsQuality:
			Value = UGameGraphicsSettings::GetQualityName(UGameGraphicsSettings::GetQualityLevel());
			break;

		default:
			break;
		}

		Item.ValueText->SetText(FText::Format(SettingValueFormat, Value));
	}
}

void UTitleScreenUI::GoBack()
{
	// back to the menu this one was opened from, on the item that opened it
	ETitleMenuPage Parent = ETitleMenuPage::Main;
	ETitleMenuAction Opener = ETitleMenuAction::OpenModeSelect;

	switch (CurrentPage)
	{
	case ETitleMenuPage::Main:
		return;

	case ETitleMenuPage::Mode:
		Parent = ETitleMenuPage::Main;
		Opener = ETitleMenuAction::OpenModeSelect;
		break;

	case ETitleMenuPage::Difficulty:
		Parent = ETitleMenuPage::Mode;
		Opener = ETitleMenuAction::OpenDifficultySelect;
		break;

	case ETitleMenuPage::Options:
		Parent = ETitleMenuPage::Main;
		Opener = ETitleMenuAction::OpenOptions;
		break;

	case ETitleMenuPage::Controls:
		Parent = ETitleMenuPage::Main;
		Opener = ETitleMenuAction::OpenControls;
		break;

	case ETitleMenuPage::TwoPlayer:
		Parent = ETitleMenuPage::Mode;
		Opener = ETitleMenuAction::OpenTwoPlayerSelect;
		break;

	case ETitleMenuPage::Join:
	{
		// back on the pick that opened it
		ShowPage(ETitleMenuPage::TwoPlayer);

		const int32 Pick = PendingTwoPlayerDifficulty;
		const int32 PickItem = Items.IndexOfByPredicate([Pick](const FTitleMenuItem& Item)
		{
			return Item.Page == ETitleMenuPage::TwoPlayer && Item.Action == ETitleMenuAction::ChooseTwoPlayerNpc && Item.Value == Pick;
		});

		if (PickItem != INDEX_NONE)
		{
			HighlightItem(PickItem);
		}

		return;
	}
	}

	ShowPage(Parent);

	const int32 OpenerItem = FindItem(Parent, Opener);

	if (OpenerItem != INDEX_NONE)
	{
		HighlightItem(OpenerItem);
	}
}

void UTitleScreenUI::OnSecondPlayerJoined(ESecondPlayerDevice Device, FInputDeviceId Gamepad)
{
	if (CurrentPage != ETitleMenuPage::Join || bStartChosen)
	{
		return;
	}

	if (FTwoPlayerInput* TwoPlayerInput = FTwoPlayerInput::Find(this))
	{
		TwoPlayerInput->SetSecondPlayerDevice(Device, Gamepad);
	}

	const bool bWithNpcs = PendingTwoPlayerDifficulty != INDEX_NONE;

	StartGame(bWithNpcs ? ETimeTrialRaceMode::VersusNpc : ETimeTrialRaceMode::TimeTrial, FMath::Max(PendingTwoPlayerDifficulty, 0), true);
}

void UTitleScreenUI::StartGame(ETimeTrialRaceMode Mode, int32 Difficulty, bool bTwoPlayers)
{
	// a click and a key press could both land before the screen is torn down, so only the first one counts
	if (bStartChosen)
	{
		return;
	}

	bStartChosen = true;

	if (ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->SetRaceMode(Mode, Difficulty);
		GM->SetTwoPlayers(bTwoPlayers);
	}

	OnStartGame.Broadcast();
}

void UTitleScreenUI::OnStartPressed()
{
	const ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(UGameplayStatics::GetGameMode(this));

	StartGame(GM ? GM->GetRaceMode() : ETimeTrialRaceMode::TimeTrial, GM ? GM->GetSelectedDifficulty() : 0, GM && GM->IsTwoPlayers());
}

void UTitleScreenUI::OnQuitPressed()
{
	UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
}
