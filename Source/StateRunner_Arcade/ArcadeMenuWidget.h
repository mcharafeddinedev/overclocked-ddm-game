#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArcadeMenuWidget.generated.h"

class UButton;
class USlider;
class UTextBlock;
class UMusicPlayerWidget;
class USoundBase;

/**
 * Enum for focusable widget types in arcade menus.
 */
UENUM(BlueprintType)
enum class EArcadeFocusType : uint8
{
	Button		UMETA(DisplayName = "Button"),
	Slider		UMETA(DisplayName = "Slider"),
	Selector	UMETA(DisplayName = "Selector/Dropdown")
};

/**
 * Struct representing a focusable item in an arcade menu.
 * Stores widget reference and metadata for navigation.
 */
USTRUCT(BlueprintType)
struct FArcadeFocusableItem
{
	GENERATED_BODY()

	/** The focusable widget (Button, Slider, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Focus")
	TObjectPtr<UWidget> Widget = nullptr;

	/** Type of focusable widget */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Focus")
	EArcadeFocusType FocusType = EArcadeFocusType::Button;

	/** Optional label text block for visual focus feedback */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Focus")
	TObjectPtr<UTextBlock> LabelWidget = nullptr;

	/** Optional value text block (for sliders/selectors) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Focus")
	TObjectPtr<UTextBlock> ValueWidget = nullptr;

	/** Whether this item is currently enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Focus")
	bool bIsEnabled = true;
};

/**
 * Delegate for when a menu item is selected (Enter pressed).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMenuItemSelected, int32, ItemIndex);

/**
 * Delegate for when focus changes to a new item.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFocusChanged, int32, NewFocusIndex, int32, OldFocusIndex);

/**
 * Delegate for when back is pressed (Escape).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBackPressed);

/**
 * Delegate for slider value changed via keyboard.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSliderAdjusted, int32, ItemIndex, float, NewValue);

/**
 * Delegate for selector/dropdown option changed.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSelectorChanged, int32, ItemIndex, int32, NewOptionIndex);

/**
 * Base class for all menus in StateRunner Arcade.
 * Keyboard/joystick-only navigation (no mouse) -- arrows to navigate,
 * Enter to select, Escape to go back. Also supports embedded music player zone.
 */
UCLASS(Abstract, Blueprintable)
class STATERUNNER_ARCADE_API UArcadeMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UArcadeMenuWidget(const FObjectInitializer& ObjectInitializer);

protected:

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// --- Configuration ---

protected:

	/**
	 * Whether navigation wraps from top to bottom and vice versa.
	 * True: Bottom → Top, Top → Bottom
	 * False: Stops at boundaries
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation")
	bool bWrapNavigation = true;

	/**
	 * Whether Left/Right arrows also navigate up/down through menu items.
	 * True: Left = Up, Right = Down (in addition to normal behavior)
	 * False: Left/Right only affects music player zone or sliders/selectors
	 * Useful for menus without a music player or with horizontal button layouts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation")
	bool bLeftRightNavigatesMenu = false;

	/**
	 * Step size for slider adjustment (0.0 to 1.0 range).
	 * 0.05 = 5% per press.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation", meta=(ClampMin="0.01", ClampMax="0.25"))
	float SliderStepSize = 0.05f;

	/**
	 * Repeat rate for held arrow keys (seconds between repeats).
	 * Lower = faster adjustment when holding key.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation", meta=(ClampMin="0.05", ClampMax="0.5"))
	float KeyRepeatRate = 0.15f;

	/**
	 * Initial delay before key repeat starts (seconds).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation", meta=(ClampMin="0.1", ClampMax="1.0"))
	float KeyRepeatDelay = 0.3f;

	/**
	 * Whether to set keyboard focus to this widget on construct.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation")
	bool bFocusOnConstruct = true;

	// --- Audio Feedback ---

protected:

	/**
	 * Sound to play when focus changes to a new item (like hover sound).
	 * Played on Up/Down/Left/Right navigation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	TObjectPtr<USoundBase> NavigationSound;

	/**
	 * Sound to play when an item is selected (Enter pressed).
	 * Played when confirming a button/selection.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	TObjectPtr<USoundBase> SelectionSound;

	/**
	 * Sound to play when back/cancel is pressed (Escape).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	TObjectPtr<USoundBase> BackSound;

	/**
	 * Play the navigation (hover) sound.
	 * Prefers the explicit NavigationSound asset if set.
	 * Falls back to the focused button's HoveredSlateSound from its widget style,
	 * ensuring keyboard/gamepad navigation plays the same sounds as mouse hover.
	 * 
	 * @param FallbackButton Optional button to pull style sounds from (for music player zone)
	 */
	void PlayNavigationSound(UButton* FallbackButton = nullptr);

	/**
	 * Play the selection (click) sound.
	 * Prefers the explicit SelectionSound asset if set.
	 * Falls back to the focused button's PressedSlateSound from its widget style,
	 * ensuring keyboard/gamepad selection plays the same sounds as mouse click.
	 * 
	 * @param FallbackButton Optional button to pull style sounds from (for music player zone)
	 */
	void PlaySelectionSound(UButton* FallbackButton = nullptr);

	/** Play the back (cancel) sound */
	void PlayBackSound();

	// --- Focus System ---

protected:

	/**
	 * Array of focusable items in this menu.
	 * Items are navigated in order (index 0 = top, higher = lower).
	 */
	UPROPERTY(BlueprintReadOnly, Category="Focus")
	TArray<FArcadeFocusableItem> FocusableItems;

	/**
	 * Current focus index (-1 = no focus).
	 */
	UPROPERTY(BlueprintReadOnly, Category="Focus")
	int32 CurrentFocusIndex = -1;

	/**
	 * Default focus index (set focus to this on construct).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Focus")
	int32 DefaultFocusIndex = 0;

	/**
	 * Whether keyboard navigation has been used.
	 * Focus visuals only show up after the first keyboard input,
	 * so we don't highlight buttons before the player actually starts navigating.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Focus")
	bool bKeyboardNavigationStarted = false;

	// --- Key Repeat State ---

protected:

	/** Timer for key repeat */
	FTimerHandle KeyRepeatTimerHandle;

	/** Currently held navigation key (for repeat) */
	FKey CurrentHeldKey;

	/** Whether initial delay has passed */
	bool bKeyRepeatActive = false;

	// --- Music Player Zone Navigation ---

protected:

	/**
	 * Optional reference to embedded Music Player widget.
	 * When set, Left/Right keys can navigate between menu and music player.
	 * The music player uses Left/Right for its internal button navigation.
	 */
	UPROPERTY(BlueprintReadWrite, Category="Music Player")
	TObjectPtr<UMusicPlayerWidget> EmbeddedMusicPlayer;

	/**
	 * Whether the music player zone currently has focus.
	 * When true, Left/Right navigates music player buttons.
	 * When false, Up/Down navigates menu items.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Music Player")
	bool bMusicPlayerHasFocus = false;

	/**
	 * Current focus index within the music player (0=Skip, 1=Shuffle).
	 */
	UPROPERTY(BlueprintReadOnly, Category="Music Player")
	int32 MusicPlayerFocusIndex = 0;

	/** Number of focusable buttons in music player */
	static const int32 MUSIC_PLAYER_BUTTON_COUNT = 2;
	static const int32 MUSIC_PLAYER_INDEX_SKIP = 0;
	static const int32 MUSIC_PLAYER_INDEX_SHUFFLE = 1;

	// --- Events ---

public:

	/** Broadcast when an item is selected (Enter pressed) */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnMenuItemSelected OnMenuItemSelected;

	/** Broadcast when focus changes */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnFocusChanged OnFocusChanged;

	/** Broadcast when back is pressed (Escape) */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnBackPressed OnBackPressed;

	/** Broadcast when a slider is adjusted */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnSliderAdjusted OnSliderAdjusted;

	/** Broadcast when a selector option changes */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnSelectorChanged OnSelectorChanged;

	// --- Public Functions ---

public:

	/**
	 * Register a focusable item in the menu.
	 * Called from Blueprint's Construct event for each menu item.
	 * 
	 * @param Widget The widget to register (Button, Slider, etc.)
	 * @param FocusType Type of focusable widget
	 * @param LabelWidget Optional text block for label (for visual focus feedback)
	 * @param ValueWidget Optional text block for value display (sliders/selectors)
	 * @return Index of the registered item
	 */
	UFUNCTION(BlueprintCallable, Category="Focus")
	int32 RegisterFocusableItem(UWidget* Widget, EArcadeFocusType FocusType, UTextBlock* LabelWidget = nullptr, UTextBlock* ValueWidget = nullptr);

	/**
	 * Register a button as a focusable item (convenience function).
	 * 
	 * @param Button The button to register
	 * @param LabelWidget Optional text block for label
	 * @return Index of the registered item
	 */
	UFUNCTION(BlueprintCallable, Category="Focus")
	int32 RegisterButton(UButton* Button, UTextBlock* LabelWidget = nullptr);

	/**
	 * Register a slider as a focusable item (convenience function).
	 * 
	 * @param Slider The slider to register
	 * @param LabelWidget Optional text block for label
	 * @param ValueWidget Optional text block for value display
	 * @return Index of the registered item
	 */
	UFUNCTION(BlueprintCallable, Category="Focus")
	int32 RegisterSlider(USlider* Slider, UTextBlock* LabelWidget = nullptr, UTextBlock* ValueWidget = nullptr);

	/**
	 * Clear all registered focusable items.
	 */
	UFUNCTION(BlueprintCallable, Category="Focus")
	void ClearFocusableItems();

	/**
	 * Set focus to a specific item index.
	 * 
	 * @param NewIndex Index to focus (-1 to clear focus)
	 */
	UFUNCTION(BlueprintCallable, Category="Focus")
	void SetFocusIndex(int32 NewIndex);

	/**
	 * Get the currently focused item index.
	 */
	UFUNCTION(BlueprintPure, Category="Focus")
	int32 GetFocusIndex() const { return CurrentFocusIndex; }

	/**
	 * Get the number of registered focusable items.
	 */
	UFUNCTION(BlueprintPure, Category="Focus")
	int32 GetFocusableItemCount() const { return FocusableItems.Num(); }

	/**
	 * Enable or disable a focusable item.
	 * Disabled items are skipped during navigation.
	 * 
	 * @param ItemIndex Index of the item
	 * @param bEnabled Whether the item should be enabled
	 */
	UFUNCTION(BlueprintCallable, Category="Focus")
	void SetItemEnabled(int32 ItemIndex, bool bEnabled);

	/**
	 * Navigate focus up (previous item).
	 */
	UFUNCTION(BlueprintCallable, Category="Navigation")
	void NavigateUp();

	/**
	 * Navigate focus down (next item).
	 */
	UFUNCTION(BlueprintCallable, Category="Navigation")
	void NavigateDown();

	/**
	 * Adjust focused item left (decrease slider/previous option).
	 */
	UFUNCTION(BlueprintCallable, Category="Navigation")
	void NavigateLeft();

	/**
	 * Adjust focused item right (increase slider/next option).
	 */
	UFUNCTION(BlueprintCallable, Category="Navigation")
	void NavigateRight();

	/**
	 * Select the currently focused item (Enter).
	 */
	UFUNCTION(BlueprintCallable, Category="Navigation")
	void SelectFocusedItem();

	/**
	 * Handle back action (Escape).
	 */
	UFUNCTION(BlueprintCallable, Category="Navigation")
	void HandleBack();

	/**
	 * Take keyboard focus for this widget.
	 * Should be called when showing the menu.
	 */
	UFUNCTION(BlueprintCallable, Category="Focus")
	void TakeKeyboardFocus();

	/**
	 * Reset keyboard navigation state.
	 * Call this to hide focus visuals until the next keyboard input.
	 * Useful when re-showing a menu.
	 */
	UFUNCTION(BlueprintCallable, Category="Focus")
	void ResetKeyboardNavigationState();

	/**
	 * Manually activate keyboard navigation mode.
	 * Shows focus visuals immediately without waiting for keyboard input.
	 */
	UFUNCTION(BlueprintCallable, Category="Focus")
	void ActivateKeyboardNavigation();

	// --- Music Player Zone Functions ---

	/**
	 * Set the embedded music player widget for zone navigation.
	 * When set, Left/Right navigates to/from the music player.
	 * Call this in NativeConstruct before Super::NativeConstruct().
	 * 
	 * @param MusicPlayer Reference to the embedded music player widget
	 */
	UFUNCTION(BlueprintCallable, Category="Music Player")
	void SetEmbeddedMusicPlayer(UMusicPlayerWidget* MusicPlayer);

	/**
	 * Check if the music player zone currently has focus.
	 */
	UFUNCTION(BlueprintPure, Category="Music Player")
	bool IsMusicPlayerFocused() const { return bMusicPlayerHasFocus; }

	/**
	 * Transfer focus to the music player zone.
	 * Does nothing if no music player is embedded.
	 */
	UFUNCTION(BlueprintCallable, Category="Music Player")
	void FocusMusicPlayer();

	/**
	 * Transfer focus back to the menu zone.
	 * Does nothing if music player doesn't have focus.
	 */
	UFUNCTION(BlueprintCallable, Category="Music Player")
	void FocusMenu();

	// --- Blueprint Overridable Functions ---

protected:

	/**
	 * Called when an item is selected.
	 * Handles button presses (override in child class).
	 * 
	 * @param ItemIndex Index of the selected item
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Events")
	void OnItemSelected(int32 ItemIndex);
	virtual void OnItemSelected_Implementation(int32 ItemIndex);

	/**
	 * Called when focus changes to update visual styling.
	 * Applies focus highlight effects (override for visual styling).
	 * 
	 * @param NewFocusIndex Index of newly focused item (-1 if none)
	 * @param OldFocusIndex Index of previously focused item (-1 if none)
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Events")
	void UpdateFocusVisuals(int32 NewFocusIndex, int32 OldFocusIndex);
	virtual void UpdateFocusVisuals_Implementation(int32 NewFocusIndex, int32 OldFocusIndex);

	/**
	 * Called when back/escape is pressed.
	 * Handles navigation back (override in child class).
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Events")
	void OnBackAction();
	virtual void OnBackAction_Implementation();

	/**
	 * Called when a slider value changes via keyboard.
	 * Handles slider value changes (override in child class).
	 * 
	 * @param ItemIndex Index of the slider item
	 * @param NewValue New slider value (0.0 to 1.0)
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Events")
	void OnSliderValueChanged(int32 ItemIndex, float NewValue);
	virtual void OnSliderValueChanged_Implementation(int32 ItemIndex, float NewValue);

	/**
	 * Called when a selector option changes.
	 * Handles selector changes (override in child class).
	 * 
	 * @param ItemIndex Index of the selector item
	 * @param Delta Direction of change (-1 = left/previous, +1 = right/next)
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Events")
	void OnSelectorOptionChanged(int32 ItemIndex, int32 Delta);
	virtual void OnSelectorOptionChanged_Implementation(int32 ItemIndex, int32 Delta);

	// --- Internal Functions ---

protected:

	/**
	 * Find the next enabled item in a direction.
	 * 
	 * @param StartIndex Starting index
	 * @param Direction +1 for forward, -1 for backward
	 * @return Index of next enabled item, or -1 if none found
	 */
	int32 FindNextEnabledItem(int32 StartIndex, int32 Direction) const;

	/**
	 * Handle key repeat for held navigation keys.
	 */
	UFUNCTION()
	void OnKeyRepeatTick();

	/**
	 * Start key repeat timer for a navigation key.
	 */
	void StartKeyRepeat(const FKey& Key);

	/**
	 * Stop key repeat timer.
	 */
	void StopKeyRepeat();

	/**
	 * Adjust the focused slider value.
	 * 
	 * @param Delta Amount to adjust (-SliderStepSize or +SliderStepSize)
	 */
	void AdjustFocusedSlider(float Delta);

	/**
	 * Cycle the focused selector option.
	 * 
	 * @param Delta Direction (-1 = previous, +1 = next)
	 */
	void CycleFocusedSelector(int32 Delta);

	// --- Music Player Zone Internal ---

	/**
	 * Navigate within the music player zone.
	 * 
	 * @param Direction -1 for left (previous button), +1 for right (next button)
	 */
	void NavigateMusicPlayer(int32 Direction);

	/**
	 * Select the currently focused music player button.
	 */
	void SelectMusicPlayerButton();

	/**
	 * Update focus visuals for music player buttons.
	 * 
	 * @param NewIndex New focused button index (0=Skip, 1=Shuffle)
	 * @param OldIndex Previous focused button index (-1 if none)
	 */
	void UpdateMusicPlayerFocusVisuals(int32 NewIndex, int32 OldIndex);

	/**
	 * Dim all menu items when switching to music player zone.
	 */
	void DimMenuItems();

	/**
	 * Dim music player buttons when switching to menu zone.
	 */
	void DimMusicPlayerButtons();
};
