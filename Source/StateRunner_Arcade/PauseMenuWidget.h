#pragma once

#include "CoreMinimal.h"
#include "ArcadeMenuWidget.h"
#include "PauseMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UMusicPlayerWidget;
class UThemeSelectorWidget;

/**
 * Pause Menu Widget for StateRunner Arcade
 * 
 * Displayed when player pauses the game.
 * Pauses game time while visible.
 * 
 * ARCADE CABINET TRIGGER KEYS:
 * - P key (Pause button on arcade cabinet)
 * - Tab key (Menu button on arcade cabinet)
 * 
 * BUTTONS (keyboard navigable):
 * - Resume: Continue gameplay
 * - Settings: Open settings menu
 * - Quit to Main Menu: Exit to main menu
 * 
 * ARCADE NAVIGATION:
 * - Arrow Up/Down: Navigate between menu buttons
 * - Arrow Left: Navigate to theme selector (from bottom buttons)
 * - Arrow Right: Navigate to music player (from top/middle buttons)
 * - Enter (Select button): Select focused button
 * - Escape (Exit button): Resume gameplay (same as Resume button)
 * - Default focus on Resume button
 * 
 * THREE-ZONE NAVIGATION:
 * - Left Zone: Theme button (accessible from Settings/Quit via Left arrow)
 * - Main Zone: Resume, Settings, Quit buttons (Up/Down navigation)
 * - Right Zone: Music player (accessible from Resume/Settings via Right arrow)
 * - When in left zone, Right returns to main, Enter opens theme selector
 * - When in right zone, Left returns to main, Left/Right navigates music player buttons
 */
UCLASS(Abstract, Blueprintable)
class STATERUNNER_ARCADE_API UPauseMenuWidget : public UArcadeMenuWidget
{
	GENERATED_BODY()

public:

	UPauseMenuWidget(const FObjectInitializer& ObjectInitializer);

protected:

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//=============================================================================
	// BUTTON WIDGETS
	//=============================================================================

protected:

	/** Resume button - continues gameplay */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidget), Category="Buttons")
	TObjectPtr<UButton> ResumeButton;

	/** Settings button - opens settings menu */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Buttons")
	TObjectPtr<UButton> SettingsButton;

	/** Quit to Main Menu button */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidget), Category="Buttons")
	TObjectPtr<UButton> QuitToMenuButton;

	//=============================================================================
	// OPTIONAL LABELS
	// For visual focus feedback
	//=============================================================================

protected:

	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> ResumeLabel;

	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> SettingsLabel;

	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> QuitToMenuLabel;

	/** "PAUSED" title text */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Title")
	TObjectPtr<UTextBlock> PausedTitleText;

	//=============================================================================
	// THEME SELECTOR (Left Zone)
	//=============================================================================

protected:

	/**
	 * Theme selector button in the left zone.
	 * Accessible via Left arrow from bottom menu buttons.
	 */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Theme")
	TObjectPtr<UButton> ThemeSelectorButton;

	/** Label for Theme selector button */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Theme")
	TObjectPtr<UTextBlock> ThemeSelectorLabel;

	/** Theme selector widget class to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
	TSubclassOf<UThemeSelectorWidget> ThemeSelectorWidgetClass;

	//=============================================================================
	// MUSIC PLAYER (Right Zone)
	//=============================================================================

protected:

	/**
	 * Reference to embedded Music Player widget.
	 * Uses Left/Right zone navigation (separate from Up/Down menu navigation).
	 */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Music Player")
	TObjectPtr<UMusicPlayerWidget> MusicPlayerWidget;

	// --- Menu Item Indices ---
	// Music player uses separate zone navigation (Left/Right keys)

public:

	static const int32 INDEX_RESUME = 0;
	static const int32 INDEX_SETTINGS = 1;
	static const int32 INDEX_QUIT_TO_MENU = 2;

	//=============================================================================
	// CONFIGURATION
	//=============================================================================

protected:

	/**
	 * Main menu level name to load when quitting.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
	FName MainMenuLevelName = TEXT("MainMenu");

	/**
	 * Settings widget class to spawn when Settings is pressed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
	TSubclassOf<UArcadeMenuWidget> SettingsWidgetClass;

	//=============================================================================
	// RUNTIME STATE
	//=============================================================================

protected:

	/** Reference to spawned settings widget (if open) */
	UPROPERTY()
	TObjectPtr<UArcadeMenuWidget> SettingsWidget;

	/** Reference to spawned theme selector widget (if open) */
	UPROPERTY()
	TObjectPtr<UThemeSelectorWidget> ThemeSelectorWidget;

	/** Was game paused before we opened? */
	bool bWasPausedBefore = false;

	/** Whether the left zone (Theme button) has focus */
	UPROPERTY(BlueprintReadOnly, Category="Navigation")
	bool bLeftZoneHasFocus = false;

	//=============================================================================
	// PUBLIC FUNCTIONS
	//=============================================================================

public:

	/**
	 * Resume gameplay.
	 * Unpauses the game and removes this widget.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void ResumeGame();

	/**
	 * Open settings menu.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void OpenSettings();

	/**
	 * Quit to main menu.
	 * Unpauses and loads main menu level.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void QuitToMainMenu();

	/**
	 * Close settings menu and return to pause menu.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void CloseSettings();

	/**
	 * Open theme selector.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void OpenThemeSelector();

	/**
	 * Close theme selector and return to pause menu.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void CloseThemeSelector();

	//=============================================================================
	// BLUEPRINT EVENTS & DELEGATES
	//=============================================================================

public:

	/** Delegate broadcast when pause menu is closed and game resumes */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPauseMenuClosed);

	/** Broadcast when the pause menu closes (resume or quit). Use this to clear references. */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnPauseMenuClosed OnPauseMenuClosed;

	/** Called before resuming (animations/sounds hook) */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnBeforeResume();

	/** Called before quitting to menu */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnBeforeQuitToMenu();

	/** Called when settings menu closes */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnSettingsMenuClosed();

protected:

	//=============================================================================
	// BUTTON HANDLERS
	//=============================================================================

	UFUNCTION()
	void OnResumeClicked();

	UFUNCTION()
	void OnSettingsClicked();

	UFUNCTION()
	void OnQuitToMenuClicked();

	UFUNCTION()
	void OnThemeSelectorClicked();

	//=============================================================================
	// ZONE NAVIGATION
	//=============================================================================

protected:

	/** Focus the left zone (Theme button) */
	void FocusLeftZone();

	/** Focus the main menu zone from left zone */
	void FocusMainZoneFromLeft();

	/** Update visuals for left zone focus state */
	void UpdateLeftZoneFocusVisuals();

	//=============================================================================
	// OVERRIDES
	//=============================================================================

	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void OnItemSelected_Implementation(int32 ItemIndex) override;
	virtual void OnBackAction_Implementation() override;
};
