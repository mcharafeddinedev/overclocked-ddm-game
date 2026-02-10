#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ObstacleSpawnerComponent.h"
#include "GameHUDWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UImage;
class UHorizontalBox;
class UScoreSystemComponent;
class ULivesSystemComponent;
class UOverclockSystemComponent;
class UObstacleSpawnerComponent;
class UWorldScrollComponent;

/**
 * Main gameplay HUD - shows score, lives, OVERCLOCK meter, and tutorial prompts.
 * Auto-binds to GameMode components on construct and updates via events.
 */
UCLASS(Abstract, Blueprintable)
class STATERUNNER_ARCADE_API UGameHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UGameHUDWidget(const FObjectInitializer& ObjectInitializer);

protected:

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// --- Score Display Widgets ---

protected:

	/** Current score text display */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Score")
	TObjectPtr<UTextBlock> ScoreText;

	/** High score text display */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Score")
	TObjectPtr<UTextBlock> HighScoreText;

	/** Score rate indicator (points per second) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Score")
	TObjectPtr<UTextBlock> ScoreRateText;

	/** "NEW HIGH SCORE!" indicator (hidden by default) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Score")
	TObjectPtr<UTextBlock> NewHighScoreText;

	// --- Lives Display Widgets ---

protected:

	/** Container for life icons - shows/hides based on current lives */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Lives")
	TObjectPtr<UHorizontalBox> LivesContainer;

	/** Alternative: Text-based lives display (e.g., "x5") */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Lives")
	TObjectPtr<UTextBlock> LivesText;

	/** Array of life icon images (populated from LivesContainer children) */
	UPROPERTY(BlueprintReadOnly, Category="Lives")
	TArray<TObjectPtr<UImage>> LifeIcons;

	// --- Overclock Display Widgets ---

protected:

	/** OVERCLOCK meter progress bar (0.0 to 1.0) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="OVERCLOCK")
	TObjectPtr<UProgressBar> OverclockMeterBar;

	/** OVERCLOCK meter percentage text */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="OVERCLOCK")
	TObjectPtr<UTextBlock> OverclockMeterText;

	/** OVERCLOCK active indicator (shows "OVERCLOCK!" when active) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="OVERCLOCK")
	TObjectPtr<UTextBlock> OverclockActiveText;

	/** OVERCLOCK ready indicator (shows when meter is above activation threshold) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="OVERCLOCK")
	TObjectPtr<UTextBlock> OverclockReadyText;

	// --- Tutorial Prompt Widgets ---

protected:

	/** Tutorial prompt text (SWITCH LANES!, JUMP!, SLIDE!, countdown) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Tutorial")
	TObjectPtr<UTextBlock> TutorialPromptText;

	/** Tutorial prompt container (for animation control) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Tutorial")
	TObjectPtr<UWidget> TutorialPromptContainer;

	/** Tutorial prompt background image */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Tutorial")
	TObjectPtr<UWidget> TutorialBackground;

	// --- Controls Tutorial Widgets ---

protected:

	/** Container for controls tutorial popup */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Controls Tutorial")
	TObjectPtr<UWidget> ControlsOverlayContainer;

	/** Background for controls tutorial popup */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Controls Tutorial")
	TObjectPtr<UWidget> ControlsOverlayBackground;

	/** Shows one control at a time: MOVE, JUMP, CAMERA, OVERCLOCK */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Controls Tutorial")
	TObjectPtr<UTextBlock> ControlsOverlayText;

	// --- Pickup Popup Widgets ---

protected:

	/** Pickup popup text (1-Up, EMP, NICE combo, INSANE combo) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Pickup Popup")
	TObjectPtr<UTextBlock> PickupPopupText;

	/** Pickup popup container (for animation control) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Pickup Popup")
	TObjectPtr<UWidget> PickupPopupContainer;

	/** Pickup popup background image */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Pickup Popup")
	TObjectPtr<UWidget> PickupPopupBackground;

	// --- Speed/Multiplier Display ---

protected:

	/** Current speed indicator */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Speed")
	TObjectPtr<UTextBlock> SpeedText;

	/** Speed multiplier (shows 2.5x during OVERCLOCK, etc.) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Speed")
	TObjectPtr<UTextBlock> MultiplierText;

	// --- Difficulty Display Widgets ---

	/** Current difficulty level text (bottom-right corner) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Difficulty")
	TObjectPtr<UTextBlock> DifficultyText;

	// --- Configuration ---

protected:

	/** Duration to show tutorial prompts (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
	float TutorialPromptDuration = 2.0f;

	// --- Controls Tutorial Timing ---

	/**
	 * Delay after "GO!!" before first controls popup appears (seconds).
	 * Allows countdown text to clear and player to start moving.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Controls Tutorial", meta=(ClampMin="0.5", ClampMax="5.0"))
	float ControlsTutorialStartDelay = 2.0f;

	/**
	 * Duration each controls popup is shown (seconds).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Controls Tutorial", meta=(ClampMin="1.0", ClampMax="5.0"))
	float ControlsPopupDuration = 2.5f;

	/**
	 * Gap between controls popups (seconds).
	 * Time from one popup disappearing to next appearing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Controls Tutorial", meta=(ClampMin="0.5", ClampMax="3.0"))
	float ControlsPopupGap = 1.0f;

	/** Text for MOVE controls popup (Arcade / PC / Gamepad) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Controls Tutorial")
	FString ControlsTextMove = TEXT("MOVE\nArrows / Left Stick\nChange lanes!");

	/** Text for JUMP controls popup (Arcade / PC / Gamepad) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Controls Tutorial")
	FString ControlsTextJump = TEXT("JUMP\nUP / W / A\nHold for height!");

	/** Text for SLIDE controls popup (Arcade / PC / Gamepad) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Controls Tutorial")
	FString ControlsTextSlide = TEXT("SLIDE\nDOWN / S / B\nDodge barriers!");

	/** Text for CAMERA controls popup (Arcade / PC / Gamepad) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Controls Tutorial")
	FString ControlsTextCamera = TEXT("CAMERA\nTop BTN / Ctrl / R2\nToggle view!");
	
	/** Text for OVERCLOCK controls popup (Arcade / PC / Gamepad) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Controls Tutorial")
	FString ControlsTextOverclock = TEXT("OVERCLOCK\nBot BTN / Shift / L2\nWhen meter full!");

	/** Duration of the shrink-out animation for popups (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Animation", meta=(ClampMin="0.1", ClampMax="1.0"))
	float PopupShrinkDuration = 0.25f;

	/** Duration of the grow-in animation for popups (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Animation", meta=(ClampMin="0.1", ClampMax="1.0"))
	float PopupGrowDuration = 0.15f;

	/** Easing exponent for shrink animation (higher = more dramatic ease-in) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Animation", meta=(ClampMin="1.0", ClampMax="5.0"))
	float PopupShrinkEaseExponent = 2.5f;

	/** Easing exponent for grow animation (higher = more dramatic ease-out) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Animation", meta=(ClampMin="1.0", ClampMax="5.0"))
	float PopupGrowEaseExponent = 2.0f;

	/** Duration to show 1-Up popup (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Pickup Popup", meta=(ClampMin="0.5", ClampMax="10.0"))
	float OneUpPopupDuration = 2.0f;

	/** Duration to show EMP popup (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Pickup Popup", meta=(ClampMin="0.5", ClampMax="10.0"))
	float EMPPopupDuration = 2.5f;

	/** Duration to show Magnet popup (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Pickup Popup", meta=(ClampMin="0.5", ClampMax="10.0"))
	float MagnetPopupDuration = 2.0f;

	/** Duration to show NICE combo popup (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Pickup Popup", meta=(ClampMin="0.5", ClampMax="10.0"))
	float NiceComboPopupDuration = 2.5f;

	/** Duration to show INSANE combo popup (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Pickup Popup", meta=(ClampMin="0.5", ClampMax="10.0"))
	float InsaneComboPopupDuration = 3.0f;

	/**
	 * Difficulty level at which the display reads "MAX DIFFICULTY" instead of a number.
	 * Set to the functional cap where obstacles are maxed out.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Difficulty", meta=(ClampMin="1", ClampMax="30"))
	int32 MaxDifficultyDisplay = 13;

	/** Color for active life icons */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
	FLinearColor LifeActiveColor = FLinearColor::White;

	/** Color for lost life icons (grayed out) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
	FLinearColor LifeLostColor = FLinearColor(0.2f, 0.2f, 0.2f, 0.5f);

	/** Color for OVERCLOCK meter when ready to activate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
	FLinearColor OverclockReadyColor = FLinearColor(0.0f, 1.0f, 1.0f, 1.0f); // Cyan

	/** Color for OVERCLOCK meter when active (depleting) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
	FLinearColor OverclockActiveColor = FLinearColor(1.0f, 0.5f, 0.0f, 1.0f); // Orange default

	/** Color for OVERCLOCK meter when charging */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
	FLinearColor OverclockChargingColor = FLinearColor(0.3f, 0.7f, 1.0f, 1.0f); // Light blue

	// --- Cached Component References ---

protected:

	UPROPERTY()
	TObjectPtr<UScoreSystemComponent> ScoreSystem;

	UPROPERTY()
	TObjectPtr<ULivesSystemComponent> LivesSystem;

	UPROPERTY()
	TObjectPtr<UOverclockSystemComponent> OverclockSystem;

	UPROPERTY()
	TObjectPtr<UObstacleSpawnerComponent> ObstacleSpawner;

	UPROPERTY()
	TObjectPtr<UWorldScrollComponent> WorldScroll;

	// --- Runtime State ---

protected:

	/** Timer for hiding tutorial prompt */
	FTimerHandle TutorialPromptTimerHandle;

	/** Timer for delayed OVERCLOCK tutorial prompt after SLIDE */
	FTimerHandle OverclockTutorialTimerHandle;

	/** Timer for hiding pickup popup */
	FTimerHandle PickupPopupTimerHandle;

	/** Timer for controls tutorial sequence */
	FTimerHandle ControlsTutorialTimerHandle;

	/** Whether we've beaten high score this session */
	bool bHasBeatenHighScore = false;

	/** Whether the OVERCLOCK tutorial has been shown this session */
	bool bHasShownOverclockTutorial = false;

	/** Whether the controls tutorial sequence has started this session */
	bool bHasStartedControlsTutorial = false;

	/** Current step in controls tutorial (0=MOVE, 1=JUMP, 2=CAMERA, 3=OVERCLOCK, 4=done) */
	int32 ControlsTutorialStep = 0;

	/** Whether a controls popup is currently visible */
	bool bIsControlsPopupVisible = false;

	// --- Popup Animation State ---

	/** Whether tutorial popup is currently shrinking */
	bool bTutorialPopupShrinking = false;

	/** Tutorial popup shrink animation progress (0.0 to 1.0) */
	float TutorialShrinkProgress = 0.0f;

	/** Whether tutorial popup is currently growing (scale 0.0 → 1.0) */
	bool bTutorialPopupGrowing = false;

	/** Tutorial popup grow animation progress (0.0 to 1.0) */
	float TutorialGrowProgress = 0.0f;

	/** Whether a pickup popup is currently being displayed (for queue gating) */
	bool bIsPickupPopupActive = false;

	/**
	 * Queue of pending pickup popups.
	 * When a popup is requested while one is already showing, it queues here.
	 * FIFO order — except Magnet which takes priority.
	 */
	struct FQueuedPickupPopup
	{
		FString Text;
		float Duration;
	};
	TArray<FQueuedPickupPopup> PickupPopupQueue;

	/** Cached difficulty level — only update text when this changes */
	int32 CachedDifficultyLevel = -1;

	/** Whether pickup popup is currently shrinking (scale 1.0 → 0.0) */
	bool bPickupPopupShrinking = false;

	/** Pickup popup shrink animation progress (0.0 to 1.0) */
	float PickupShrinkProgress = 0.0f;

	/** Whether pickup popup is currently growing (scale 0.0 → 1.0) */
	bool bPickupPopupGrowing = false;

	/** Pickup popup grow animation progress (0.0 to 1.0) */
	float PickupGrowProgress = 0.0f;

	// --- Public Functions ---

public:

	/**
	 * Initialize HUD by binding to game mode components.
	 * Called automatically in NativeConstruct, but can be called manually.
	 */
	UFUNCTION(BlueprintCallable, Category="Initialization")
	void InitializeHUD();

	/**
	 * Show a tutorial prompt.
	 * 
	 * @param PromptText The text to display
	 * @param Duration How long to show the prompt (0 = use default)
	 */
	UFUNCTION(BlueprintCallable, Category="Tutorial")
	void ShowTutorialPrompt(const FString& PromptText, float Duration = 0.0f);

	/**
	 * Hide the tutorial prompt immediately.
	 */
	UFUNCTION(BlueprintCallable, Category="Tutorial")
	void HideTutorialPrompt();

	// --- Controls Tutorial ---

public:

	/**
	 * Start the controls tutorial sequence.
	 * Shows MOVE, JUMP, CAMERA, OVERCLOCK popups one at a time during intro.
	 * Call this after the countdown ends.
	 */
	UFUNCTION(BlueprintCallable, Category="Controls Tutorial")
	void StartControlsTutorial();

	/**
	 * Show the next controls popup in the sequence.
	 * Automatically advances through: MOVE -> JUMP -> CAMERA -> OVERCLOCK -> done
	 */
	UFUNCTION(BlueprintCallable, Category="Controls Tutorial")
	void ShowNextControlsPopup();

	/**
	 * Hide the current controls popup.
	 */
	UFUNCTION(BlueprintCallable, Category="Controls Tutorial")
	void HideControlsPopup();

	/**
	 * Stop the controls tutorial immediately (with scale-down animation).
	 * Called when obstacle tutorial prompts need to take over.
	 */
	UFUNCTION(BlueprintCallable, Category="Controls Tutorial")
	void StopControlsTutorial();

	/**
	 * Check if controls tutorial is still in progress.
	 */
	UFUNCTION(BlueprintPure, Category="Controls Tutorial")
	bool IsControlsTutorialInProgress() const { return bHasStartedControlsTutorial && ControlsTutorialStep < 5; }

	/**
	 * Check if a controls popup is currently visible.
	 */
	UFUNCTION(BlueprintPure, Category="Controls Tutorial")
	bool IsControlsPopupVisible() const { return bIsControlsPopupVisible; }

	// --- Countdown Display ---

public:

	/**
	 * Show countdown text using the tutorial prompt display.
	 * Called from Blueprint during the countdown sequence (5-bzzt, 4-bzzt, THREE, TWO, ONE, GO!!).
	 * 
	 * For 5 and 4, pass empty string (no text, just sound effect).
	 * For 3, 2, 1, GO pass the text to display.
	 * 
	 * @param CountdownText The text to display (e.g., "THREE", "TWO", "ONE", "GO!!")
	 * @param Duration How long to show the text (default 0.9s for countdown rhythm)
	 */
	UFUNCTION(BlueprintCallable, Category="Countdown")
	void ShowCountdownText(const FString& CountdownText, float Duration = 0.9f);

	/**
	 * Hide countdown text immediately.
	 */
	UFUNCTION(BlueprintCallable, Category="Countdown")
	void HideCountdownText();

	/**
	 * Updates score display (normally called via event binding).
	 */
	UFUNCTION(BlueprintCallable, Category="Display")
	void UpdateScoreDisplay(int32 Score);

	/**
	 * Updates lives display (normally called via event binding).
	 */
	UFUNCTION(BlueprintCallable, Category="Display")
	void UpdateLivesDisplay(int32 CurrentLives, int32 MaxLives);

	/**
	 * Updates OVERCLOCK meter display (normally called via event binding).
	 */
	UFUNCTION(BlueprintCallable, Category="Display")
	void UpdateOverclockDisplay(float CurrentMeter, float MaxMeter);

	/**
	 * Update OVERCLOCK active state display.
	 */
	UFUNCTION(BlueprintCallable, Category="Display")
	void UpdateOverclockActiveState(bool bIsActive);

	/**
	 * Updates speed display with current scroll speed.
	 * Speed is shown in "MU/s" (Memory Units per second) - a thematic unit for this cyberpunk runner.
	 * 
	 * @param ScrollSpeed Current scroll speed in units/second
	 */
	UFUNCTION(BlueprintCallable, Category="Display")
	void UpdateSpeedDisplay(float ScrollSpeed);

	// --- Blueprint Events ---

public:

	/** Called when damage is taken (screen flash hook) */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnDamageTaken(int32 RemainingLives);

	/** Called when high score is beaten */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnHighScoreBeaten(int32 NewHighScore, int32 OldHighScore);

	/** Called when OVERCLOCK activates */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnOverclockActivated();

	/** Called when OVERCLOCK deactivates */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnOverclockDeactivated();

	/** Called when invulnerability starts (for flash effect) */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnInvulnerabilityStarted();

	/** Called when invulnerability ends */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnInvulnerabilityEnded();

	// --- Event Handlers ---

protected:

	/** Handle score change event */
	UFUNCTION()
	void HandleScoreChanged(int32 NewScore);

	/** Handle score rate change event */
	UFUNCTION()
	void HandleScoreRateChanged(int32 NewScoreRate);

	/** Handle high score beaten event */
	UFUNCTION()
	void HandleHighScoreBeaten(int32 NewHighScore, int32 OldHighScore);

	/** Handle lives change event */
	UFUNCTION()
	void HandleLivesChanged(int32 CurrentLives, int32 MaxLives);

	/** Handle damage taken event */
	UFUNCTION()
	void HandleDamageTaken(int32 RemainingLives);

	/** Handle invulnerability change event */
	UFUNCTION()
	void HandleInvulnerabilityChanged(bool bIsInvulnerable);

	/** Handle OVERCLOCK state change event */
	UFUNCTION()
	void HandleOverclockStateChanged(bool bIsActive);

	/** Handle OVERCLOCK meter change event */
	UFUNCTION()
	void HandleOverclockMeterChanged(float CurrentMeter, float MaxMeter);

	/** Handle tutorial prompt event */
	UFUNCTION()
	void HandleTutorialPrompt(ETutorialObstacleType PromptType);

	/** Handle tutorial complete event */
	UFUNCTION()
	void HandleTutorialComplete();

	/** Handle 1-Up collected event */
	UFUNCTION()
	void HandleOneUpCollected(bool bWasAtMaxLives, int32 BonusPoints);

	/** Handle EMP collected event */
	UFUNCTION()
	void HandleEMPCollected(int32 ObstaclesDestroyed, int32 PointsPerObstacle);

	/** Handle Magnet collected event */
	UFUNCTION()
	void HandleMagnetCollected();

	/** Handle NICE! combo event */
	UFUNCTION()
	void HandleNiceCombo(int32 BonusPoints);

	/** Handle INSANE! combo event */
	UFUNCTION()
	void HandleInsaneCombo(int32 BonusPoints);

	/** Handle scroll speed change event */
	UFUNCTION()
	void HandleScrollSpeedChanged(float NewScrollSpeed);

	// --- Internal Functions ---

protected:

	/** Cache component references from GameMode */
	void CacheComponentReferences();

	/** Bind to component events */
	void BindToComponentEvents();

	/** Initialize life icons from container children */
	void InitializeLifeIcons();

	/** Called by timer to show OVERCLOCK tutorial after SLIDE prompt */
	UFUNCTION()
	void OnOverclockTutorialTimerExpired();

	/** Called by timer to start tutorial popup shrink animation */
	UFUNCTION()
	void OnTutorialPopupStartShrink();

	/** Called by timer to start pickup popup shrink animation */
	UFUNCTION()
	void OnPickupPopupStartShrink();

	/** Called by timer to show next controls popup */
	UFUNCTION()
	void OnControlsTutorialTimer();

	/** Get the text for a controls tutorial step */
	FString GetControlsTutorialText(int32 Step) const;

	/** Get text for tutorial prompt type */
	FString GetTutorialPromptText(ETutorialObstacleType PromptType) const;

	/** Update OVERCLOCK meter bar color based on state */
	void UpdateOverclockMeterColor();

	/** Process tutorial popup shrink animation (called from NativeTick) */
	void UpdateTutorialShrinkAnimation(float DeltaTime);

	/** Process tutorial popup grow animation (called from NativeTick) */
	void UpdateTutorialGrowAnimation(float DeltaTime);

	/** Process pickup popup shrink animation (called from NativeTick) */
	void UpdatePickupShrinkAnimation(float DeltaTime);

	/** Process pickup popup grow animation (called from NativeTick) */
	void UpdatePickupGrowAnimation(float DeltaTime);

	/** Apply scale to tutorial popup widgets */
	void SetTutorialPopupScale(float Scale);

	/** Apply scale to pickup popup widgets */
	void SetPickupPopupScale(float Scale);

	/** Reset tutorial popup scale to full size */
	void ResetTutorialPopupScale();

	/** Reset pickup popup scale to full size */
	void ResetPickupPopupScale();

	/** Show the next popup from the queue if one exists */
	void ShowNextQueuedPopup();

	// --- Pickup Popups ---

public:

	/**
	 * Show a pickup-related popup (for 1-Up, EMP, NICE, INSANE bonuses).
	 * Uses separate PickupPopupText widget for different font size than tutorial prompts.
	 * 
	 * @param PopupText The text to display
	 * @param Duration How long to show the popup (default 2.5s)
	 */
	UFUNCTION(BlueprintCallable, Category="Pickup Popup")
	void ShowPickupPopup(const FString& PopupText, float Duration = 2.5f);

	/**
	 * Hide the pickup popup immediately.
	 */
	UFUNCTION(BlueprintCallable, Category="Pickup Popup")
	void HidePickupPopup();

	/**
	 * Called when a 1-Up is collected to show appropriate tutorial text.
	 * @param bWasAtMaxLives True if player was at max lives (bonus score instead of life)
	 * @param BonusPoints Points awarded if at max lives
	 */
	UFUNCTION(BlueprintCallable, Category="Tutorial")
	void ShowOneUpCollectedPopup(bool bWasAtMaxLives, int32 BonusPoints = 5000);

	/**
	 * Called when an EMP is collected to show appropriate tutorial text.
	 * @param ObstaclesDestroyed Number of obstacles destroyed by the EMP
	 * @param PointsPerObstacle Points awarded per obstacle
	 */
	UFUNCTION(BlueprintCallable, Category="Tutorial")
	void ShowEMPCollectedPopup(int32 ObstaclesDestroyed, int32 PointsPerObstacle = 500);

	/**
	 * Called when a Magnet is collected to show popup notification.
	 * Magnet awards no points — popup just informs the player.
	 */
	UFUNCTION(BlueprintCallable, Category="Tutorial")
	void ShowMagnetCollectedPopup();

	/**
	 * Called when player achieves NICE! combo (6 data packets in 2 seconds).
	 * @param BonusPoints Bonus points awarded
	 */
	UFUNCTION(BlueprintCallable, Category="Tutorial")
	void ShowNiceComboPopup(int32 BonusPoints = 2000);

	/**
	 * Called when player achieves INSANE! combo (10 data packets in 2 seconds).
	 * @param BonusPoints Bonus points awarded
	 */
	UFUNCTION(BlueprintCallable, Category="Tutorial")
	void ShowInsaneComboPopup(int32 BonusPoints = 5000);
};
