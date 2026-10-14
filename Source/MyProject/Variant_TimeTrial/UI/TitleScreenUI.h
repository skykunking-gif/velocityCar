// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "TimeTrialGameMode.h"
#include "TitleScreenUI.generated.h"

enum class ESecondPlayerDevice : uint8;

class UBorder;
class UImage;
class UOverlay;
class UTexture2D;
class UTextBlock;
class UVerticalBox;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStartGameDelegate);

/** The menus the title screen steps through */
UENUM(BlueprintType)
enum class ETitleMenuPage : uint8
{
	Main,
	Mode,
	Difficulty,
	Options,
	Controls,
	TwoPlayer,
	Join
};

/** What picking a title screen menu item does */
UENUM()
enum class ETitleMenuAction : uint8
{
	OpenModeSelect,
	OpenOptions,
	OpenControls,
	StartTimeTrial,
	OpenDifficultySelect,
	StartVersusNpc,
	Back,
	AdjustMusicVolume,
	AdjustEngineVolume,
	AdjustGraphicsQuality,
	OpenTwoPlayerSelect,
	ChooseTwoPlayerNpc
};

/** One item of the title screen menus */
USTRUCT()
struct FTitleMenuItem
{
	GENERATED_BODY()

	/** Bar behind the label, lit up while highlighted. Also the area the mouse picks the item with */
	UPROPERTY()
	TObjectPtr<UBorder> Bar;

	/** The item's label */
	UPROPERTY()
	TObjectPtr<UTextBlock> Label;

	/** The setting's value, on a settings item */
	UPROPERTY()
	TObjectPtr<UTextBlock> ValueText;

	/** The picture shown in place of the label, on an NPC level item that has a button image */
	UPROPERTY()
	TObjectPtr<UImage> Image;

	/** Menu the item is on */
	ETitleMenuPage Page = ETitleMenuPage::Main;

	/** What picking the item does */
	ETitleMenuAction Action = ETitleMenuAction::Back;

	/** Extra value for the action, such as the difficulty level */
	int32 Value = 0;
};

/** One line of the controls list: what it does, and the keys and the gamepad buttons that do it */
USTRUCT()
struct FTitleControlsRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Controls")
	FText Action;

	UPROPERTY(EditAnywhere, Category="Controls")
	FText Keyboard;

	UPROPERTY(EditAnywhere, Category="Controls")
	FText Gamepad;
};

/**
 *  The title screen widget. Shows the game's title and menus over the title camera flythrough:
 *  Game Start leads to the choice between a time trial and a race against NPCs, and the NPC race to how
 *  well the NPCs drive. Options sets the volumes and the picture quality, and Controls lists what the keys and gamepad buttons do. Up/down (W/S, d-pad) move the highlight, Enter (Space, gamepad accept) picks it,
 *  and Backspace (Escape, gamepad back) goes back a menu. The mouse works too.
 *
 *  The whole layout is built in code.
 */
UCLASS()
class UTitleScreenUI : public UUserWidget
{
	GENERATED_BODY()

public:

	UTitleScreenUI(const FObjectInitializer& ObjectInitializer);

	/** Starts the game in the race mode the game mode already has */
	UFUNCTION(BlueprintCallable, Category="Title Screen")
	void OnStartPressed();

	/** Quits the game */
	UFUNCTION(BlueprintCallable, Category="Title Screen")
	void OnQuitPressed();

	/** Shows one of the menus */
	UFUNCTION(BlueprintCallable, Category="Title Screen")
	void ShowPage(ETitleMenuPage NewPage);

	/** Returns the menu on screen */
	UFUNCTION(BlueprintPure, Category="Title Screen")
	ETitleMenuPage GetPage() const { return CurrentPage; };

	/** Shows the menu the race mode on the game mode was picked from, on the item that picked it: the mode menu
	 *  for a time trial, the NPC strength menu for a race against NPCs. For coming back from the car select */
	UFUNCTION(BlueprintCallable, Category="Title Screen")
	void ShowModeChoice();

public:

	/** Broadcast once the player has picked what to play. The race mode is on the game mode by then */
	FOnStartGameDelegate OnStartGame;

protected:

	/** Builds the layout */
	virtual void NativeOnInitialized() override;

	/** Shows the main menu and grabs keyboard focus */
	virtual void NativeConstruct() override;

	/** Hands the input back to the game */
	virtual void NativeDestruct() override;

	/** Handles menu navigation from the keyboard and gamepad */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Highlights the item under the mouse */
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Picks the item under the mouse */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Builds the title, the menus and the options panel */
	void CreateLayout();

	/** Makes a panel in the middle of the screen for a menu, with a heading at the top. Returns where its content goes */
	UVerticalBox* CreatePanelPage(UOverlay* Root, ETitleMenuPage Page, const FText& Heading);

	/** Fills the controls panel with a table of what each key and gamepad button does */
	void CreateControlsTable(UVerticalBox* Panel);

	/** Makes a menu: a list of items in the lower left with an optional heading above */
	UVerticalBox* CreateMenuPage(UOverlay* Root, ETitleMenuPage Page, const FText& Heading);

	/** Adds an item to a menu. An item with an image shows the picture in place of the label */
	void AddMenuItem(UVerticalBox* Menu, ETitleMenuPage Page, const FText& Label, ETitleMenuAction Action, int32 Value = 0, UTexture2D* Image = nullptr);

	/** Makes a text widget for the generated layout */
	UTextBlock* CreateText(FName Name, const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Color);

	/** Moves the highlight by Delta items on the current menu, wrapping around the ends */
	void MoveHighlight(int32 Delta);

	/** Highlights an item (index into Items) */
	void HighlightItem(int32 ItemIndex);

	/** Does what an item (index into Items) is for */
	void ActivateItem(int32 ItemIndex);

	/** Goes back to the menu the current one was opened from */
	void GoBack();

	/** Turns the setting of the highlighted item up or down by Delta steps. Returns false if it isn't a setting */
	bool AdjustHighlightedSetting(int32 Delta);

	/** Shows the current value on every settings item */
	void RefreshSettingValues();

	/** Lights up the highlighted item's bar and dims the rest */
	void RefreshHighlight();

	/** Returns the item on the current menu under a screen position, or INDEX_NONE */
	int32 FindItemUnder(const FVector2D& ScreenPosition) const;

	/** Returns the first item on a menu that does Action, or INDEX_NONE */
	int32 FindItem(ETitleMenuPage Page, ETitleMenuAction Action) const;

	/** Tells the game mode what to play and starts the game */
	void StartGame(ETimeTrialRaceMode Mode, int32 Difficulty, bool bTwoPlayers = false);

	/** Starts the two player game once the second player has said what they play with */
	void OnSecondPlayerJoined(ESecondPlayerDevice Device, FInputDeviceId Gamepad);

	/** Routes keyboard/gamepad input to this widget. Deferred to the tick after Construct,
	 *  because the widget isn't in the Slate hierarchy yet while it's being constructed. */
	void ClaimKeyboardFocus();

	/** Every item of every menu */
	UPROPERTY(Transient)
	TArray<FTitleMenuItem> Items;

	/** The widget of each menu, indexed by ETitleMenuPage, shown one at a time */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> Pages;

	/** Controls hint along the bottom */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintText;

	/** Game title shown above the menu. Leave empty for no title. */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FText GameTitle = NSLOCTEXT("TitleScreenUI", "GameTitle", "VELOCITY RUSH");

	/** Label of the start item */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FText StartLabel = NSLOCTEXT("TitleScreenUI", "Start", "ゲームスタート");

	/** Label of the options item */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FText OptionsLabel = NSLOCTEXT("TitleScreenUI", "Options", "オプション");

	/** Label of the controls item */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FText ControlsLabel = NSLOCTEXT("TitleScreenUI", "Controls", "操作方法");

	/** Heading of the controls panel */
	UPROPERTY(EditAnywhere, Category="Title Screen|Controls")
	FText ControlsTitleLabel = NSLOCTEXT("TitleScreenUI", "ControlsTitle", "CONTROLS");

	/** Heading over the keyboard column of the controls list */
	UPROPERTY(EditAnywhere, Category="Title Screen|Controls")
	FText KeyboardHeading = NSLOCTEXT("TitleScreenUI", "KeyboardHeading", "キーボード");

	/** Heading over the gamepad column of the controls list */
	UPROPERTY(EditAnywhere, Category="Title Screen|Controls")
	FText GamepadHeading = NSLOCTEXT("TitleScreenUI", "GamepadHeading", "ゲームパッド");

	/** The controls list. Follows the vehicle's input mapping context (IMC_Vehicle_Default) and the pause keys of
	 *  the player controller, so it needs changing along with them */
	UPROPERTY(EditAnywhere, Category="Title Screen|Controls")
	TArray<FTitleControlsRow> ControlsRows;

	/** Width of the controls list's columns: what it does, keyboard, gamepad */
	UPROPERTY(EditAnywhere, Category="Title Screen|Controls")
	FVector ControlsColumnWidths = FVector(300.0f, 260.0f, 220.0f);

	/** Controls hint while the controls are up */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FText ControlsHintLabel = NSLOCTEXT("TitleScreenUI", "ControlsHint", "Enter / Backspace で戻る");

	/** Heading of the race mode menu */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FText ModeHeading = NSLOCTEXT("TitleScreenUI", "ModeHeading", "モード選択");

	/** Label of the time trial item */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FText TimeTrialLabel = NSLOCTEXT("TitleScreenUI", "TimeTrial", "タイムトライアル");

	/** Label of the race against NPCs item */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FText VersusLabel = NSLOCTEXT("TitleScreenUI", "Versus", "NPC対戦");

	/** Heading of the NPC difficulty menu */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FText DifficultyHeading = NSLOCTEXT("TitleScreenUI", "DifficultyHeading", "NPC の強さ");

	/** Button pictures of the NPC levels, by the level's Id (FNpcDifficulty::Id). A level without one gets a text
	 *  item with its name */
	UPROPERTY(EditAnywhere, Category="Title Screen|Difficulty")
	TMap<FName, TObjectPtr<UTexture2D>> DifficultyButtonImages;

	/** Height the NPC level buttons are drawn at. The width follows each picture's shape */
	UPROPERTY(EditAnywhere, Category="Title Screen|Difficulty")
	float DifficultyButtonHeight = 84.0f;

	/** Tint of the NPC level buttons that aren't highlighted */
	UPROPERTY(EditAnywhere, Category="Title Screen|Difficulty")
	FLinearColor DifficultyButtonDimColor = FLinearColor(0.42f, 0.42f, 0.42f, 1.0f);

	/** How much bigger the highlighted NPC level button is drawn */
	UPROPERTY(EditAnywhere, Category="Title Screen|Difficulty")
	float DifficultyButtonHighlightScale = 1.06f;

	/** Label of the two player item */
	UPROPERTY(EditAnywhere, Category="Title Screen|Two Players")
	FText TwoPlayerLabel = NSLOCTEXT("TitleScreenUI", "TwoPlayer", "2人対戦");

	/** Heading of the menu of who else races in a two player game */
	UPROPERTY(EditAnywhere, Category="Title Screen|Two Players")
	FText TwoPlayerHeading = NSLOCTEXT("TitleScreenUI", "TwoPlayerHeading", "2人対戦　いっしょに走る NPC");

	/** Label of the two player item without NPCs */
	UPROPERTY(EditAnywhere, Category="Title Screen|Two Players")
	FText NoNpcLabel = NSLOCTEXT("TitleScreenUI", "NoNpc", "NPC なし");

	/** Heading of the panel the second player joins on */
	UPROPERTY(EditAnywhere, Category="Title Screen|Two Players")
	FText JoinTitleLabel = NSLOCTEXT("TitleScreenUI", "JoinTitle", "2P の参加");

	/** What the second player does to join */
	UPROPERTY(EditAnywhere, Category="Title Screen|Two Players", meta = (MultiLine = true))
	FText JoinBodyLabel = NSLOCTEXT("TitleScreenUI", "JoinBody",
		"2P の人は、使うゲームパッドの A ボタンを押してください。\n"
		"1P はキーボードと、ほかのゲームパッドで遊べます。\n\n"
		"キーボードを2人で使うときは Enter を押してください。\n"
		"1P は WASD 側、2P は矢印キー側で運転します。");

	/** Controls hint while waiting for the second player */
	UPROPERTY(EditAnywhere, Category="Title Screen|Two Players")
	FText JoinHintLabel = NSLOCTEXT("TitleScreenUI", "JoinHint", "Backspace で戻る");

	/** Heading over the second player's keys in the controls list */
	UPROPERTY(EditAnywhere, Category="Title Screen|Two Players")
	FText SecondPlayerKeysHeading = NSLOCTEXT("TitleScreenUI", "SecondPlayerKeysHeading", "2人対戦でキーボードを分けるとき（2P）");

	/** The second player's keys when the two share the keyboard. Follows FTwoPlayerInput::GetSecondPlayerKeys and
	 *  ATimeTrialPlayerController::AddSecondPlayerKeyboardKeys */
	UPROPERTY(EditAnywhere, Category="Title Screen|Two Players", meta = (MultiLine = true))
	FText SecondPlayerKeysLabel = NSLOCTEXT("TitleScreenUI", "SecondPlayerKeys",
		"↑ ↓ ← → で運転　Enter サイドブレーキ　右Shift / 右Ctrl シフトアップ / ダウン\n"
		"/ カメラ切り替え　Delete コースに戻る　（1P は矢印キー以外のいつものキー）");

	/** The NPC level picked for the two player game while waiting for the second player, or INDEX_NONE for no NPCs */
	int32 PendingTwoPlayerDifficulty = INDEX_NONE;

	/** Heading of the options panel */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FText OptionsTitleLabel = NSLOCTEXT("TitleScreenUI", "OptionsTitle", "OPTIONS");

	/** Label of the music volume setting */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FText MusicVolumeLabel = NSLOCTEXT("TitleScreenUI", "MusicVolume", "BGM");

	/** Label of the engine sound volume setting */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FText EngineVolumeLabel = NSLOCTEXT("TitleScreenUI", "EngineVolume", "エンジン音");

	/** Label of the picture quality setting */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FText GraphicsQualityLabel = NSLOCTEXT("TitleScreenUI", "GraphicsQuality", "画質");

	/** How a setting's value is shown. {0} is the value */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FText SettingValueFormat = NSLOCTEXT("TitleScreenUI", "SettingValue", "◀  {0}  ▶");

	/** Width of a setting's value, arrows included */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	float SettingValueWidth = 120.0f;

	/** Label of the back items */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FText BackLabel = NSLOCTEXT("TitleScreenUI", "Back", "戻る");

	/** Controls hint on the main menu */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FText HintLabel = NSLOCTEXT("TitleScreenUI", "Hint", "↑ ↓ で選択　Enter で決定");

	/** Controls hint on the other menus */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FText SubMenuHintLabel = NSLOCTEXT("TitleScreenUI", "SubMenuHint", "↑ ↓ で選択　Enter で決定　Backspace で戻る");

	/** Controls hint on the options */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FText OptionsHintLabel = NSLOCTEXT("TitleScreenUI", "OptionsHint", "↑ ↓ で選択　← → で調整　Backspace で戻る");

	/** Font of the generated game title */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FSlateFontInfo TitleFont;

	/** Font of the generated menu items */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FSlateFontInfo MenuFont;

	/** Font of the generated hint, headings and options text */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FSlateFontInfo SmallFont;

	/** Bar behind the highlighted menu item */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FLinearColor HighlightColor = FLinearColor(1.0f, 0.32f, 0.02f, 0.92f);

	/** Bar behind the other menu items */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FLinearColor ItemColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.5f);

	/** Label color of the highlighted menu item */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FLinearColor HighlightTextColor = FLinearColor::White;

	/** Label color of the other menu items */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FLinearColor ItemTextColor = FLinearColor(0.78f, 0.78f, 0.78f, 1.0f);

	/** Background of the options panel */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	FLinearColor PanelColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.7f);

	/** Width of the generated menu items */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	float MenuItemWidth = 440.0f;

	/** Distance of the generated title and menus from the left edge of the screen */
	UPROPERTY(EditAnywhere, Category="Title Screen|Layout")
	float ScreenMargin = 112.0f;

	/** Menu on screen */
	ETitleMenuPage CurrentPage = ETitleMenuPage::Main;

	/** Index into Items of the highlighted item */
	int32 HighlightedItem = INDEX_NONE;

	/** True once something to play has been picked, so a stray second confirm can't start the game twice */
	bool bStartChosen = false;
};
