#pragma once

#include "CoreMinimal.h"
#include "ArcadeMenuWidget.h"
#include "GameOverWidget.generated.h"

class UTextBlock;
class UButton;
class UScoreSystemComponent;
class UMusicPlayerWidget;
class ULeaderboardWidget;

/**
 * Game Over Widget for StateRunner Arcade
 * 
 * Displays final score and provides restart/quit options.
 * Uses arcade keyboard navigation (NO MOUSE).
 * 
 * DISPLAYS:
 * - Final Score (large, prominent)
 * - High Score
 * - "NEW HIGH SCORE!" indicator if beaten
 * 
 * BUTTONS (keyboard navigable):
 * - Restart: Resets all systems and starts new run
 * - Quit to Main Menu: Returns to main menu
 * 
 * ARCADE NAVIGATION:
 * - Arrow Up/Down: Navigate between buttons
 * - Enter: Select focused button
 * - Default focus on Restart button
 */
UCLASS(Abstract, Blueprintable)
class STATERUNNER_ARCADE_API UGameOverWidget : public UArcadeMenuWidget
{
	GENERATED_BODY()

public:

	UGameOverWidget(const FObjectInitializer& ObjectInitializer);

protected:

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//=============================================================================
	// SCORE DISPLAY WIDGETS
	//=============================================================================

protected:

	/** Final score text (large display) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Score")
	TObjectPtr<UTextBlock> FinalScoreText;

	/** High score text */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Score")
	TObjectPtr<UTextBlock> HighScoreText;

	/** "NEW HIGH SCORE!" indicator */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Score")
	TObjectPtr<UTextBlock> NewHighScoreText;

	/** Background/backdrop image for NewHighScoreText (shows/hides with the text) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Score")
	TObjectPtr<UWidget> NewHighScoreBackdrop;

	/** Score difference text (how much above/below high score) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Score")
	TObjectPtr<UTextBlock> ScoreDifferenceText;

	/** "GAME OVER" title text */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Title")
	TObjectPtr<UTextBlock> GameOverTitleText;

	//=============================================================================
	// BUTTON WIDGETS
	//=============================================================================

protected:

	/** Restart button - resets game and starts new run */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidget), Category="Buttons")
	TObjectPtr<UButton> RestartButton;

	/** Leaderboard button - opens leaderboard view */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Buttons")
	TObjectPtr<UButton> LeaderboardButton;

	/** Quit to Main Menu button */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidget), Category="Buttons")
	TObjectPtr<UButton> QuitToMenuButton;

	//=============================================================================
	// OPTIONAL LABELS
	// For visual focus feedback
	//=============================================================================

protected:

	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> RestartLabel;

	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> LeaderboardLabel;

	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> QuitToMenuLabel;

	//=============================================================================
	// MUSIC PLAYER (embedded widget)
	//=============================================================================

protected:

	/**
	 * Reference to embedded Music Player widget.
	 * If bound, its buttons will be added to keyboard navigation.
	 */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Music Player")
	TObjectPtr<UMusicPlayerWidget> MusicPlayerWidget;

	//=============================================================================
	// INITIALS ENTRY WIDGETS
	// Shown only when player qualifies for top 10 leaderboard
	//=============================================================================

protected:

	/** Container for initials entry UI (show/hide based on qualification) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Initials")
	TObjectPtr<UWidget> InitialsEntryContainer;

	/** "ENTER YOUR INITIALS" prompt text */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Initials")
	TObjectPtr<UTextBlock> InitialsPromptText;

	/** First initial letter display */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Initials")
	TObjectPtr<UTextBlock> Initial1Text;

	/** Second initial letter display */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Initials")
	TObjectPtr<UTextBlock> Initial2Text;

	/** Third initial letter display */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Initials")
	TObjectPtr<UTextBlock> Initial3Text;

	/** Underscore/cursor for first initial slot */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Initials")
	TObjectPtr<UTextBlock> Underscore1Text;

	/** Underscore/cursor for second initial slot */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Initials")
	TObjectPtr<UTextBlock> Underscore2Text;

	/** Underscore/cursor for third initial slot */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Initials")
	TObjectPtr<UTextBlock> Underscore3Text;

	/** Confirm initials button */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Initials")
	TObjectPtr<UButton> ConfirmInitialsButton;

	/** Confirm button label */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Initials")
	TObjectPtr<UTextBlock> ConfirmInitialsLabel;

	// --- Menu Item Indices ---
	// When player qualifies for leaderboard, ENTER_INITIALS is index 0
	// and all other indices shift by 1

public:

	/** Index for Enter Initials button (only present when qualified) */
	static const int32 INDEX_ENTER_INITIALS = 0;
	
	/** Get actual index for Restart (shifts if initials button present) */
	int32 GetRestartIndex() const { return bQualifiedForLeaderboard && !bInitialsSubmitted ? 1 : 0; }
	
	/** Get actual index for Leaderboard (shifts if initials button present) */
	int32 GetLeaderboardIndex() const { return bQualifiedForLeaderboard && !bInitialsSubmitted ? 2 : 1; }
	
	/** Get actual index for Quit (shifts if initials button present) */
	int32 GetQuitIndex() const { return bQualifiedForLeaderboard && !bInitialsSubmitted ? 3 : 2; }

	// Music player uses separate zone navigation (Left/Right keys)
	// See ArcadeMenuWidget::SetEmbeddedMusicPlayer()

	//=============================================================================
	// CONFIGURATION
	//=============================================================================

protected:

	/**
	 * Main menu level name to load when quitting.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
	FName MainMenuLevelName = TEXT("SR_MainMenu");

	/**
	 * Gameplay level name to load when restarting.
	 * Set this to the current gameplay level for a clean restart.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
	FName GameplayLevelName = TEXT("SR_OfficialTrack");

	/**
	 * Leaderboard widget class to spawn when Leaderboard is pressed.
	 * Set in Blueprint to the WBP_Leaderboard class.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
	TSubclassOf<ULeaderboardWidget> LeaderboardWidgetClass;

	//=============================================================================
	// CACHED REFERENCES
	//=============================================================================

protected:

	UPROPERTY()
	TObjectPtr<UScoreSystemComponent> ScoreSystem;

	//=============================================================================
	// RUNTIME STATE
	//=============================================================================

protected:

	/** Cached final score */
	UPROPERTY(BlueprintReadOnly, Category="State")
	int32 FinalScore = 0;

	/** Cached high score */
	UPROPERTY(BlueprintReadOnly, Category="State")
	int32 HighScore = 0;

	/** Whether this is a new high score */
	UPROPERTY(BlueprintReadOnly, Category="State")
	bool bIsNewHighScore = false;

	/** Rank achieved on leaderboard this run (0 = didn't qualify) */
	UPROPERTY(BlueprintReadOnly, Category="State")
	int32 LeaderboardRank = 0;

	/** Reference to spawned leaderboard widget (if open) */
	UPROPERTY()
	TObjectPtr<ULeaderboardWidget> LeaderboardWidget;

	//=============================================================================
	// INITIALS ENTRY STATE
	//=============================================================================

protected:

	/** Whether player qualified for leaderboard (needs to enter initials) */
	UPROPERTY(BlueprintReadOnly, Category="Initials")
	bool bQualifiedForLeaderboard = false;

	/** Whether initials entry mode is active */
	UPROPERTY(BlueprintReadOnly, Category="Initials")
	bool bInitialsEntryActive = false;

	/** Whether initials have been submitted (prevents re-entry) */
	UPROPERTY(BlueprintReadOnly, Category="Initials")
	bool bInitialsSubmitted = false;

	/** Current initials (3 characters) */
	UPROPERTY(BlueprintReadOnly, Category="Initials")
	FString CurrentInitials = TEXT("AAA");

	/** Currently selected initial slot (0, 1, or 2) */
	UPROPERTY(BlueprintReadOnly, Category="Initials")
	int32 CurrentInitialSlot = 0;

	/** Color for the currently selected initial slot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Initials|Config")
	FLinearColor SelectedInitialColor = FLinearColor(1.0f, 0.84f, 0.0f, 1.0f); // Gold

	/** Color for unselected initial slots */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Initials|Config")
	FLinearColor UnselectedInitialColor = FLinearColor::White;

	/** How fast the underscore blinks (seconds per cycle) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Initials|Config", meta=(ClampMin="0.1", ClampMax="2.0"))
	float UnderscoreBlinkRate = 0.4f;

	/** Timer handle for underscore blink animation (unused, kept for compatibility) */
	FTimerHandle UnderscoreBlinkTimerHandle;

	/** Current blink state (visible or hidden) */
	bool bUnderscoreVisible = true;

	/** Last time underscore was toggled (for real-time blink) */
	double LastUnderscoreToggleTime = 0.0;

	/** Start the underscore blink animation */
	void StartUnderscoreBlink();

	/** Stop the underscore blink animation */
	void StopUnderscoreBlink();

	/** Called by timer to toggle underscore visibility */
	void ToggleUnderscoreBlink();

	/** Update which underscore is shown based on current slot */
	void UpdateUnderscoreDisplay();

	//=============================================================================
	// INPUT DELAY STATE
	// Prevents accidental button presses when transitioning to game over
	//=============================================================================

protected:

	/** 
	 * Delay before input is accepted on the Game Over screen.
	 * Prevents accidental restarts when player is mashing buttons during gameplay.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration", meta=(ClampMin="0.0", ClampMax="5.0"))
	float InputDelayDuration = 1.0f;

	/** Whether input is currently enabled */
	UPROPERTY(BlueprintReadOnly, Category="State")
	bool bInputEnabled = false;

	/** Time when widget was constructed (for input delay) */
	double InputDelayStartTime = 0.0;

	/** Timer handle for input delay (unused, kept for compatibility) */
	FTimerHandle InputDelayTimerHandle;

	/** Called when input delay expires */
	void OnInputDelayExpired();

	//=============================================================================
	// PUBLIC FUNCTIONS
	//=============================================================================

public:

	/**
	 * Sets up the Game Over screen with final score data.
	 * 
	 * @param InFinalScore The player's final score
	 * @param InHighScore The current high score
	 * @param bNewHighScore Whether the player beat the high score
	 */
	UFUNCTION(BlueprintCallable, Category="Setup")
	void SetupGameOver(int32 InFinalScore, int32 InHighScore, bool bNewHighScore);

	/**
	 * Initialize from current GameMode state.
	 * Automatically gets scores from ScoreSystemComponent.
	 */
	UFUNCTION(BlueprintCallable, Category="Setup")
	void InitializeFromGameMode();

	/**
	 * Restart the game.
	 * Resets all systems and begins new run.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void RestartGame();

	/**
	 * Quit to main menu.
	 * Cleans up and loads main menu level.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void QuitToMainMenu();

	/**
	 * Open the leaderboard view.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void OpenLeaderboard();

	/**
	 * Close the leaderboard and return to Game Over.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void CloseLeaderboard();

	//=============================================================================
	// INITIALS ENTRY FUNCTIONS
	//=============================================================================

	/**
	 * Start initials entry mode.
	 * Called automatically if player qualified for leaderboard.
	 */
	UFUNCTION(BlueprintCallable, Category="Initials")
	void StartInitialsEntry();

	/**
	 * Cycle the current initial slot up (A->B->...->Z->A).
	 */
	UFUNCTION(BlueprintCallable, Category="Initials")
	void CycleInitialUp();

	/**
	 * Cycle the current initial slot down (A->Z->Y->...->B->A).
	 */
	UFUNCTION(BlueprintCallable, Category="Initials")
	void CycleInitialDown();

	/**
	 * Move to the next initial slot (wraps 2->0).
	 */
	UFUNCTION(BlueprintCallable, Category="Initials")
	void MoveToNextSlot();

	/**
	 * Move to the previous initial slot (wraps 0->2).
	 */
	UFUNCTION(BlueprintCallable, Category="Initials")
	void MoveToPreviousSlot();

	/**
	 * Confirm and submit the initials.
	 */
	UFUNCTION(BlueprintCallable, Category="Initials")
	void ConfirmInitials();

	/**
	 * Cancel initials entry and return to normal menu navigation.
	 * Called when Escape is pressed during initials entry.
	 */
	UFUNCTION(BlueprintCallable, Category="Initials")
	void CancelInitialsEntry();

	/**
	 * Update the visual display of initials.
	 */
	UFUNCTION(BlueprintCallable, Category="Initials")
	void UpdateInitialsDisplay();
	
	/**
	 * Update the initials button text based on current state.
	 */
	UFUNCTION(BlueprintCallable, Category="Initials")
	void UpdateInitialsButtonText();

	//=============================================================================
	// BLUEPRINT EVENTS
	//=============================================================================

public:

	/** Called before restarting (animations/sounds hook) */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnBeforeRestart();

	/** Called before quitting to menu (animations/sounds hook) */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnBeforeQuitToMenu();

	/** Called when new high score is set (celebration effects hook) */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnNewHighScoreSet(int32 NewScore, int32 OldScore);

protected:

	//=============================================================================
	// BUTTON HANDLERS
	//=============================================================================

	UFUNCTION()
	void OnRestartClicked();

	UFUNCTION()
	void OnLeaderboardClicked();

	UFUNCTION()
	void OnQuitToMenuClicked();

	UFUNCTION()
	void OnConfirmInitialsClicked();

	//=============================================================================
	// INTERNAL FUNCTIONS
	//=============================================================================

	/** Reset all game systems for restart */
	void ResetAllSystems();

	/** Update score displays */
	void UpdateScoreDisplays();

	/** Cache component references */
	void CacheComponentReferences();

	//=============================================================================
	// INTERNAL INITIALS FUNCTIONS
	//=============================================================================

	/** Set the character at the specified slot */
	void SetInitialAtSlot(int32 SlotIndex, TCHAR Character);

	/** Get the character at the specified slot */
	TCHAR GetInitialAtSlot(int32 SlotIndex) const;

	/** Get the text block for the specified slot */
	UTextBlock* GetInitialTextBlock(int32 SlotIndex) const;

	/** Hide initials entry UI */
	void HideInitialsEntry();

	//=============================================================================
	// OVERRIDES
	//=============================================================================

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void OnItemSelected_Implementation(int32 ItemIndex) override;
	virtual void OnBackAction_Implementation() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
};
