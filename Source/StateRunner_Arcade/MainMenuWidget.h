#pragma once

#include "CoreMinimal.h"
#include "ArcadeMenuWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UMusicPlayerWidget;
class ULeaderboardWidget;
class UThemeSelectorWidget;

/**
 * Main Menu Widget for StateRunner Arcade
 * 
 * First menu players see. Options: Start Game, Settings, Controls, Credits, Quit
 * 
 * THREE-ZONE NAVIGATION:
 * 
 * LEFT ZONE (Theme button - bottom-left):
 * - Arrow Left from Credits/Quit → Theme button
 * - Arrow Right from Theme → Credits
 * - Enter: Opens theme selector
 * 
 * MAIN ZONE (center buttons):
 * - Arrow Up/Down: Navigate between menu buttons (wrapping)
 * - Arrow Left (from Credits/Quit): Go to Theme button (left zone)
 * - Arrow Right: Go to right zone (Leaderboard/Music Player)
 * - Enter: Select focused button
 * - Escape: No action
 * 
 * RIGHT ZONE (Leaderboard top-right, Music Player bottom-right):
 * - Arrow Right from main zone → position-aware target:
 *   - From Start/Settings/Controls → Leaderboard
 *   - From Credits/Quit → Music Player
 * - Arrow Left → return to main zone (Credits)
 * - Up/Down: Navigate between Leaderboard and Music Player
 * - When music player focused, Left/Right cycles Skip/Shuffle
 * 
 * Requires: StartGameButton, SettingsButton, CreditsButton, QuitButton in Blueprint
 * Optional: ControlsButton, ThemeButton, LeaderboardButton, MusicPlayerWidget
 */
UCLASS(Abstract, Blueprintable)
class STATERUNNER_ARCADE_API UMainMenuWidget : public UArcadeMenuWidget
{
	GENERATED_BODY()

public:

	UMainMenuWidget(const FObjectInitializer& ObjectInitializer);

protected:

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	//=============================================================================
	// BUTTON REFERENCES
	//=============================================================================

protected:

	/**
	 * Start Game button.
	 * Clicking this starts gameplay.
	 */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidget), Category="Buttons")
	TObjectPtr<UButton> StartGameButton;

	/**
	 * Settings button.
	 * Opens the settings menu.
	 */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidget), Category="Buttons")
	TObjectPtr<UButton> SettingsButton;

	/**
	 * Controls button.
	 * Opens the controls display popup.
	 */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Buttons")
	TObjectPtr<UButton> ControlsButton;

	/**
	 * Credits button.
	 * Opens the credits display popup.
	 */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Buttons")
	TObjectPtr<UButton> CreditsButton;

	/**
	 * Theme button.
	 * Opens the theme selector popup.
	 */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Buttons")
	TObjectPtr<UButton> ThemeButton;

	/**
	 * Leaderboard button.
	 * Opens the leaderboard display popup.
	 */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Buttons")
	TObjectPtr<UButton> LeaderboardButton;

	/**
	 * Quit button.
	 * Exits the application.
	 */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidget), Category="Buttons")
	TObjectPtr<UButton> QuitButton;

	//=============================================================================
	// OPTIONAL LABEL REFERENCES
	// For visual focus feedback
	//=============================================================================

protected:

	/** Text label for Start Game button */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> StartGameLabel;

	/** Text label for Settings button */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> SettingsLabel;

	/** Text label for Controls button */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> ControlsLabel;

	/** Text label for Credits button */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> CreditsLabel;

	/** Text label for Theme button */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> ThemeLabel;

	/** Text label for Leaderboard button */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> LeaderboardLabel;

	/** Text label for Quit button */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> QuitLabel;

	//=============================================================================
	// THEME NOTIFICATION POPUP
	//=============================================================================

protected:

	/**
	 * Text block for theme applied notification.
	 * Shows "[Theme Name] Applied!" briefly after selecting a theme.
	 */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Theme Notification")
	TObjectPtr<UTextBlock> ThemeNotificationText;

	/**
	 * Optional container/background for the theme notification.
	 */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Theme Notification")
	TObjectPtr<UWidget> ThemeNotificationContainer;

	/**
	 * Duration to show the theme notification (seconds).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Theme Notification")
	float ThemeNotificationDuration = 2.0f;

	/** Timer handle for hiding theme notification */
	FTimerHandle ThemeNotificationTimerHandle;

	/** Show the theme applied notification */
	UFUNCTION()
	void ShowThemeNotification(const FString& ThemeName);

	/** Hide the theme notification */
	void HideThemeNotification();

	//=============================================================================
	// MUSIC PLAYER (embedded widget)
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

	/** Index for Start Game button in focusable items array */
	static const int32 INDEX_START_GAME = 0;

	/** Index for Settings button in focusable items array */
	static const int32 INDEX_SETTINGS = 1;

	/** Index for Controls button in focusable items array */
	static const int32 INDEX_CONTROLS = 2;

	/** Index for Credits button in focusable items array */
	static const int32 INDEX_CREDITS = 3;

	/** Index for Quit button in focusable items array */
	static const int32 INDEX_QUIT = 4;
	
	// Theme button is NOT in the main list — it's in the left zone

	//=============================================================================
	// RIGHT ZONE NAVIGATION (Leaderboard + Music Player)
	// Right arrow from main zone → right zone
	// Left arrow from right zone → main zone
	// Up/Down in right zone swaps between Leaderboard and Music Player
	//=============================================================================

	/** Right zone item indices */
	static const int32 RIGHT_ZONE_INDEX_LEADERBOARD = 0;
	static const int32 RIGHT_ZONE_INDEX_MUSIC_PLAYER = 1;

protected:

	/** Whether the right zone (Leaderboard/MusicPlayer) has focus */
	UPROPERTY(BlueprintReadOnly, Category="Navigation")
	bool bRightZoneHasFocus = false;

	/** Currently focused item in the right zone (0=Leaderboard, 1=MusicPlayer) */
	UPROPERTY(BlueprintReadOnly, Category="Navigation")
	int32 RightZoneFocusIndex = RIGHT_ZONE_INDEX_LEADERBOARD;

	/** Focus the right zone (Leaderboard/Music Player area) */
	void FocusRightZone();

	/** Focus the main menu zone from right zone */
	void FocusMainZoneFromRight();

	/** Navigate within the right zone (up/down) */
	void NavigateRightZone(int32 Direction);

	/** Update visual focus for right zone items */
	void UpdateRightZoneFocusVisuals();

	//=============================================================================
	// LEFT ZONE NAVIGATION (Theme Button)
	// Left arrow from main zone (bottom buttons) → left zone
	// Right arrow from left zone → main zone (Credits)
	//=============================================================================

protected:

	/** Whether the left zone (Theme button) has focus */
	UPROPERTY(BlueprintReadOnly, Category="Navigation")
	bool bLeftZoneHasFocus = false;

	/** Focus the left zone (Theme button) */
	void FocusLeftZone();

	/** Focus the main menu zone from left zone */
	void FocusMainZoneFromLeft();

	/** Update visual focus for left zone (Theme button) */
	void UpdateLeftZoneFocusVisuals();

	//=============================================================================
	// CONFIGURATION
	//=============================================================================

protected:

	/**
	 * Level to load when Start Game is pressed.
	 * Gameplay level name to load.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
	FName GameplayLevelName = TEXT("SR_OfficialTrack");

	/**
	 * Settings widget class to spawn when Settings is pressed.
	 * Set in Blueprint to the WBP_Settings class.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
	TSubclassOf<UArcadeMenuWidget> SettingsWidgetClass;

	/**
	 * Controls widget class to spawn when Controls is pressed.
	 * Set in Blueprint to the WBP_Controls class.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
	TSubclassOf<UArcadeMenuWidget> ControlsWidgetClass;

	/**
	 * Credits widget class to spawn when Credits is pressed.
	 * Set in Blueprint to the WBP_Credits class.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
	TSubclassOf<UArcadeMenuWidget> CreditsWidgetClass;

	/**
	 * Leaderboard widget class to spawn when Leaderboard is pressed.
	 * Set in Blueprint to the WBP_Leaderboard class.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
	TSubclassOf<ULeaderboardWidget> LeaderboardWidgetClass;

	/**
	 * Theme selector widget class to spawn when Theme is pressed.
	 * Set in Blueprint to the WBP_ThemeSelector class.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
	TSubclassOf<UThemeSelectorWidget> ThemeSelectorWidgetClass;

	//=============================================================================
	// RUNTIME STATE
	//=============================================================================

protected:

	/** Reference to spawned settings widget (if open) */
	UPROPERTY()
	TObjectPtr<UArcadeMenuWidget> SettingsWidget;

	/** Reference to spawned controls widget (if open) */
	UPROPERTY()
	TObjectPtr<UArcadeMenuWidget> ControlsWidget;

	/** Reference to spawned credits widget (if open) */
	UPROPERTY()
	TObjectPtr<UArcadeMenuWidget> CreditsWidget;

	/** Reference to spawned leaderboard widget (if open) */
	UPROPERTY()
	TObjectPtr<ULeaderboardWidget> LeaderboardWidget;

	/** Reference to spawned theme selector widget (if open) */
	UPROPERTY()
	TObjectPtr<UThemeSelectorWidget> ThemeSelectorWidget;

	//=============================================================================
	// BUTTON CLICK HANDLERS
	//=============================================================================

protected:

	/** Called when Start Game button is clicked */
	UFUNCTION()
	void OnStartGameClicked();

	/** Called when Settings button is clicked */
	UFUNCTION()
	void OnSettingsClicked();

	/** Called when Controls button is clicked */
	UFUNCTION()
	void OnControlsClicked();

	/** Called when Credits button is clicked */
	UFUNCTION()
	void OnCreditsClicked();

	/** Called when Leaderboard button is clicked */
	UFUNCTION()
	void OnLeaderboardClicked();

	/** Called when Theme button is clicked */
	UFUNCTION()
	void OnThemeClicked();

	/** Called when Quit button is clicked */
	UFUNCTION()
	void OnQuitClicked();

	//=============================================================================
	// BLUEPRINT EVENTS
	//=============================================================================

public:

	/**
	 * Blueprint event called before starting the game.
	 * Hook for sounds, animations, etc.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnBeforeStartGame();

	/**
	 * Blueprint event called when settings menu closes.
	 * Refocuses the main menu.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnSettingsMenuClosed();

	//=============================================================================
	// PUBLIC FUNCTIONS
	//=============================================================================

public:

	/**
	 * Start the game (load gameplay level).
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void StartGame();

	/**
	 * Open the settings menu.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void OpenSettings();

	/**
	 * Open the controls display popup.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void OpenControls();

	/**
	 * Open the credits display popup.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void OpenCredits();

	/**
	 * Open the leaderboard display popup (view-only mode).
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void OpenLeaderboard();

	/**
	 * Open the theme selector popup.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void OpenThemeSelector();

	/**
	 * Quit the application.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void QuitGame();

	/**
	 * Close the settings menu and return to main menu.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void CloseSettings();

	/**
	 * Close the controls popup and return to main menu.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void CloseControls();

	/**
	 * Close the credits popup and return to main menu.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void CloseCredits();

	/**
	 * Close the leaderboard popup and return to main menu.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void CloseLeaderboard();

	/**
	 * Close the theme selector popup and return to main menu.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void CloseThemeSelector();

protected:

	//=============================================================================
	// OVERRIDES
	//=============================================================================

	virtual void OnItemSelected_Implementation(int32 ItemIndex) override;
	virtual void OnBackAction_Implementation() override;
};
