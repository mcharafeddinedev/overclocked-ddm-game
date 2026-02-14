#include "GameHUDWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/CanvasPanelSlot.h"
#include "TimerManager.h"
#include "StateRunner_ArcadeGameMode.h"
#include "ScoreSystemComponent.h"
#include "LivesSystemComponent.h"
#include "OverclockSystemComponent.h"
#include "ObstacleSpawnerComponent.h"
#include "WorldScrollComponent.h"
#include "StateRunner_Arcade.h"

UGameHUDWidget::UGameHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UGameHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Initialize the HUD
	InitializeHUD();
}

void UGameHUDWidget::NativeDestruct()
{
	// Clear timers
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TutorialPromptTimerHandle);
		World->GetTimerManager().ClearTimer(OverclockTutorialTimerHandle);
		World->GetTimerManager().ClearTimer(PickupPopupTimerHandle);
		World->GetTimerManager().ClearTimer(ControlsTutorialTimerHandle);
	}

	// Unbind from Score System events
	if (ScoreSystem)
	{
		ScoreSystem->OnScoreChanged.RemoveDynamic(this, &UGameHUDWidget::HandleScoreChanged);
		ScoreSystem->OnScoreRateChanged.RemoveDynamic(this, &UGameHUDWidget::HandleScoreRateChanged);
		ScoreSystem->OnHighScoreBeaten.RemoveDynamic(this, &UGameHUDWidget::HandleHighScoreBeaten);
		ScoreSystem->OnOneUpCollected.RemoveDynamic(this, &UGameHUDWidget::HandleOneUpCollected);
		ScoreSystem->OnEMPCollected.RemoveDynamic(this, &UGameHUDWidget::HandleEMPCollected);
		ScoreSystem->OnMagnetCollected.RemoveDynamic(this, &UGameHUDWidget::HandleMagnetCollected);
		ScoreSystem->OnNiceCombo.RemoveDynamic(this, &UGameHUDWidget::HandleNiceCombo);
		ScoreSystem->OnInsaneCombo.RemoveDynamic(this, &UGameHUDWidget::HandleInsaneCombo);
	}

	// Unbind from Lives System events
	if (LivesSystem)
	{
		LivesSystem->OnLivesChanged.RemoveDynamic(this, &UGameHUDWidget::HandleLivesChanged);
		LivesSystem->OnDamageTaken.RemoveDynamic(this, &UGameHUDWidget::HandleDamageTaken);
		LivesSystem->OnInvulnerabilityChanged.RemoveDynamic(this, &UGameHUDWidget::HandleInvulnerabilityChanged);
	}

	// Unbind from OVERCLOCK System events
	if (OverclockSystem)
	{
		OverclockSystem->OnOverclockStateChanged.RemoveDynamic(this, &UGameHUDWidget::HandleOverclockStateChanged);
		OverclockSystem->OnOverclockMeterChanged.RemoveDynamic(this, &UGameHUDWidget::HandleOverclockMeterChanged);
	}

	// Unbind from Obstacle Spawner events
	if (ObstacleSpawner)
	{
		ObstacleSpawner->OnTutorialPrompt.RemoveDynamic(this, &UGameHUDWidget::HandleTutorialPrompt);
		ObstacleSpawner->OnTutorialComplete.RemoveDynamic(this, &UGameHUDWidget::HandleTutorialComplete);
	}

	// Unbind from World Scroll events
	if (WorldScroll)
	{
		WorldScroll->OnScrollSpeedChanged.RemoveDynamic(this, &UGameHUDWidget::HandleScrollSpeedChanged);
	}

	// Clear popup queue state
	PickupPopupQueue.Empty();
	bIsPickupPopupActive = false;

	// Clear cached references
	ScoreSystem = nullptr;
	LivesSystem = nullptr;
	OverclockSystem = nullptr;
	ObstacleSpawner = nullptr;
	WorldScroll = nullptr;

	Super::NativeDestruct();
}

void UGameHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Poll OVERCLOCK meter every frame for smooth display
	// (Events only fire when change > 0.1, which causes stuttery updates)
	if (OverclockSystem)
	{
		UpdateOverclockDisplay(OverclockSystem->GetCurrentMeter(), 100.0f);
	}

	// Update OVERCLOCK meter color based on state
	UpdateOverclockMeterColor();

	// Poll speed display every frame for smooth updates
	// (Events only fire when change > 50 units, which causes stuttery updates)
	if (WorldScroll)
	{
		UpdateSpeedDisplay(WorldScroll->GetCurrentScrollSpeed());
	}

	// Update difficulty display when level changes
	if (DifficultyText)
	{
		// Check if we've hit max BASE speed (3500 units/sec, excluding overclock multiplier)
		const float MaxSpeedThreshold = 3500.0f;
		bool bAtMaxSpeed = false;
		
		if (WorldScroll)
		{
			// Calculate base speed by dividing out the overclock multiplier
			float CurrentSpeed = WorldScroll->GetCurrentScrollSpeed();
			float OverclockMultiplier = WorldScroll->GetOverclockMultiplier();
			float BaseSpeed = (OverclockMultiplier > 0.0f) ? (CurrentSpeed / OverclockMultiplier) : CurrentSpeed;
			bAtMaxSpeed = BaseSpeed >= MaxSpeedThreshold;
		}
		
		if (bAtMaxSpeed)
		{
			// Only update if not already showing MAX SPEED
			if (CachedDifficultyLevel != -999) // Use -999 as sentinel for "MAX SPEED" state
			{
				CachedDifficultyLevel = -999;
				DifficultyText->SetText(FText::FromString(TEXT("MAX SPEED")));
			}
		}
		else if (ObstacleSpawner)
		{
			int32 CurrentDifficulty = ObstacleSpawner->GetCurrentDifficultyLevel();
			if (CurrentDifficulty != CachedDifficultyLevel)
			{
				CachedDifficultyLevel = CurrentDifficulty;
				DifficultyText->SetText(FText::FromString(FString::Printf(TEXT("Difficulty %d"), CurrentDifficulty)));
			}
		}
	}

	// Process popup animations (grow-in and shrink-out)
	if (bTutorialPopupShrinking)
	{
		UpdateTutorialShrinkAnimation(InDeltaTime);
	}

	if (bTutorialPopupGrowing)
	{
		UpdateTutorialGrowAnimation(InDeltaTime);
	}

	if (bPickupPopupShrinking)
	{
		UpdatePickupShrinkAnimation(InDeltaTime);
	}

	if (bPickupPopupGrowing)
	{
		UpdatePickupGrowAnimation(InDeltaTime);
	}
}

// --- Initialization ---

void UGameHUDWidget::InitializeHUD()
{
	// Cache component references
	CacheComponentReferences();

	// Initialize life icons
	InitializeLifeIcons();

	// Bind to events
	BindToComponentEvents();

	// Set initial values
	if (ScoreSystem)
	{
		UpdateScoreDisplay(ScoreSystem->GetCurrentScore());
		if (HighScoreText)
		{
			HighScoreText->SetText(FText::FromString(FString::Printf(TEXT("HS: %d"), ScoreSystem->GetHighScore())));
		}
	}

	if (LivesSystem)
	{
		UpdateLivesDisplay(LivesSystem->GetCurrentLives(), LivesSystem->GetMaxLives());
	}

	if (OverclockSystem)
	{
		UpdateOverclockDisplay(OverclockSystem->GetCurrentMeter(), 100.0f);
		UpdateOverclockActiveState(OverclockSystem->IsOverclockActive());
	}

	if (WorldScroll)
	{
		UpdateSpeedDisplay(WorldScroll->GetCurrentScrollSpeed());
	}

	// Hide tutorial prompt initially
	HideTutorialPrompt();

	// Hide pickup popup initially
	HidePickupPopup();

	// Hide new high score indicator initially
	if (NewHighScoreText)
	{
		NewHighScoreText->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Initialize difficulty display
	if (DifficultyText)
	{
		CachedDifficultyLevel = 0;
		DifficultyText->SetText(FText::FromString(TEXT("Difficulty 0")));
	}

}

void UGameHUDWidget::CacheComponentReferences()
{
	// Get GameMode
	AStateRunner_ArcadeGameMode* GameMode = nullptr;
	if (UWorld* World = GetWorld())
	{
		GameMode = Cast<AStateRunner_ArcadeGameMode>(World->GetAuthGameMode());
	}

	if (!GameMode)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("GameHUDWidget: Could not find StateRunner_ArcadeGameMode"));
		return;
	}

	// Cache component references
	ScoreSystem = GameMode->GetScoreSystemComponent();
	LivesSystem = GameMode->GetLivesSystemComponent();
	OverclockSystem = GameMode->GetOverclockSystemComponent();
	ObstacleSpawner = GameMode->GetObstacleSpawnerComponent();
	WorldScroll = GameMode->GetWorldScrollComponent();

}

void UGameHUDWidget::BindToComponentEvents()
{
	// Bind to Score System events
	if (ScoreSystem)
	{
		ScoreSystem->OnScoreChanged.AddDynamic(this, &UGameHUDWidget::HandleScoreChanged);
		ScoreSystem->OnScoreRateChanged.AddDynamic(this, &UGameHUDWidget::HandleScoreRateChanged);
		ScoreSystem->OnHighScoreBeaten.AddDynamic(this, &UGameHUDWidget::HandleHighScoreBeaten);
		ScoreSystem->OnOneUpCollected.AddDynamic(this, &UGameHUDWidget::HandleOneUpCollected);
		ScoreSystem->OnEMPCollected.AddDynamic(this, &UGameHUDWidget::HandleEMPCollected);
		ScoreSystem->OnMagnetCollected.AddDynamic(this, &UGameHUDWidget::HandleMagnetCollected);
		ScoreSystem->OnNiceCombo.AddDynamic(this, &UGameHUDWidget::HandleNiceCombo);
		ScoreSystem->OnInsaneCombo.AddDynamic(this, &UGameHUDWidget::HandleInsaneCombo);
	}

	// Bind to Lives System events
	if (LivesSystem)
	{
		LivesSystem->OnLivesChanged.AddDynamic(this, &UGameHUDWidget::HandleLivesChanged);
		LivesSystem->OnDamageTaken.AddDynamic(this, &UGameHUDWidget::HandleDamageTaken);
		LivesSystem->OnInvulnerabilityChanged.AddDynamic(this, &UGameHUDWidget::HandleInvulnerabilityChanged);
	}

	// Bind to OVERCLOCK System events
	if (OverclockSystem)
	{
		OverclockSystem->OnOverclockStateChanged.AddDynamic(this, &UGameHUDWidget::HandleOverclockStateChanged);
		OverclockSystem->OnOverclockMeterChanged.AddDynamic(this, &UGameHUDWidget::HandleOverclockMeterChanged);
	}

	// Bind to Obstacle Spawner events (tutorial prompts)
	if (ObstacleSpawner)
	{
		ObstacleSpawner->OnTutorialPrompt.AddDynamic(this, &UGameHUDWidget::HandleTutorialPrompt);
		ObstacleSpawner->OnTutorialComplete.AddDynamic(this, &UGameHUDWidget::HandleTutorialComplete);
	}

	// Bind to World Scroll events (speed display)
	if (WorldScroll)
	{
		WorldScroll->OnScrollSpeedChanged.AddDynamic(this, &UGameHUDWidget::HandleScrollSpeedChanged);
	}

}

void UGameHUDWidget::InitializeLifeIcons()
{
	LifeIcons.Empty();

	if (LivesContainer)
	{
		// Get all Image children from the container
		for (int32 i = 0; i < LivesContainer->GetChildrenCount(); i++)
		{
			if (UImage* LifeIcon = Cast<UImage>(LivesContainer->GetChildAt(i)))
			{
				LifeIcons.Add(LifeIcon);
			}
		}

	}
}

// --- Public Display Functions ---

void UGameHUDWidget::ShowTutorialPrompt(const FString& PromptText, float Duration)
{
	// Cancel any in-progress animations and reset
	bTutorialPopupShrinking = false;
	TutorialShrinkProgress = 0.0f;
	bTutorialPopupGrowing = false;
	TutorialGrowProgress = 0.0f;

	// Start at scale 0 for grow-in animation
	SetTutorialPopupScale(0.0f);

	if (TutorialPromptText)
	{
		TutorialPromptText->SetText(FText::FromString(PromptText));
		TutorialPromptText->SetVisibility(ESlateVisibility::Visible);
	}

	if (TutorialPromptContainer)
	{
		TutorialPromptContainer->SetVisibility(ESlateVisibility::Visible);
	}

	if (TutorialBackground)
	{
		TutorialBackground->SetVisibility(ESlateVisibility::Visible);
		
		// Tutorial prompts (SWITCH LANES, JUMP, SLIDE, OVERCLOCK) use green background
		if (UImage* BackgroundImage = Cast<UImage>(TutorialBackground))
		{
			BackgroundImage->SetColorAndOpacity(FLinearColor(0.0f, 1.0f, 0.5f, 1.0f)); // Green
		}
	}

	// Start grow-in animation
	bTutorialPopupGrowing = true;
	TutorialGrowProgress = 0.0f;

	// Set timer to START SHRINK (not immediate hide)
	float ActualDuration = Duration > 0.0f ? Duration : TutorialPromptDuration;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TutorialPromptTimerHandle, this, &UGameHUDWidget::OnTutorialPopupStartShrink, ActualDuration, false);
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: Showing tutorial prompt: %s"), *PromptText);
}

void UGameHUDWidget::HideTutorialPrompt()
{
	// Stop any in-progress animations
	bTutorialPopupShrinking = false;
	TutorialShrinkProgress = 0.0f;
	bTutorialPopupGrowing = false;
	TutorialGrowProgress = 0.0f;

	if (TutorialPromptText)
	{
		TutorialPromptText->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (TutorialPromptContainer)
	{
		TutorialPromptContainer->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (TutorialBackground)
	{
		TutorialBackground->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Reset scale for next show
	ResetTutorialPopupScale();

	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TutorialPromptTimerHandle);
	}
}

// --- Countdown Display ---

void UGameHUDWidget::ShowCountdownText(const FString& CountdownText, float Duration)
{
	// If empty text, hide the display
	if (CountdownText.IsEmpty())
	{
		HideCountdownText();
		return;
	}

	// Cancel any in-progress animations and reset
	bTutorialPopupShrinking = false;
	TutorialShrinkProgress = 0.0f;
	bTutorialPopupGrowing = false;
	TutorialGrowProgress = 0.0f;

	// Start at scale 0 for grow-in animation
	SetTutorialPopupScale(0.0f);

	// Use the tutorial prompt text widget to display countdown
	if (TutorialPromptText)
	{
		TutorialPromptText->SetText(FText::FromString(CountdownText));
		TutorialPromptText->SetVisibility(ESlateVisibility::Visible);
	}

	if (TutorialPromptContainer)
	{
		TutorialPromptContainer->SetVisibility(ESlateVisibility::Visible);
	}

	if (TutorialBackground)
	{
		TutorialBackground->SetVisibility(ESlateVisibility::Visible);
		
		// Set green color for "GO" countdown to indicate game starting
		if (UImage* BackgroundImage = Cast<UImage>(TutorialBackground))
		{
			if (CountdownText.Contains(TEXT("GO")))
			{
				BackgroundImage->SetColorAndOpacity(FLinearColor(0.0f, 1.0f, 0.5f, 1.0f)); // Green
			}
			else
			{
				BackgroundImage->SetColorAndOpacity(FLinearColor(1.0f, 0.5f, 0.0f, 1.0f)); // Orange for countdown numbers
			}
		}
	}

	// Start grow-in animation
	bTutorialPopupGrowing = true;
	TutorialGrowProgress = 0.0f;

	// Set timer to START SHRINK (not immediate hide)
	float ActualDuration = Duration > 0.0f ? Duration : 0.9f;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TutorialPromptTimerHandle, this, &UGameHUDWidget::OnTutorialPopupStartShrink, ActualDuration, false);
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: Countdown: %s"), *CountdownText);
}

void UGameHUDWidget::HideCountdownText()
{
	// Same as HideTutorialPrompt - they share the same widget
	HideTutorialPrompt();
}

void UGameHUDWidget::NotifyCountdownFinished()
{
	// Call the Blueprint implementable event for post-countdown effects
	OnCountdownComplete();
	
	// Also broadcast the delegate for any external listeners
	OnCountdownFinished.Broadcast();
	
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: Countdown finished"));
}

void UGameHUDWidget::UpdateScoreDisplay(int32 Score)
{
	if (ScoreText)
	{
		ScoreText->SetText(FText::FromString(FString::Printf(TEXT("SCORE: %d"), Score)));
	}
}

void UGameHUDWidget::UpdateLivesDisplay(int32 CurrentLives, int32 MaxLives)
{
	// Update text display
	if (LivesText)
	{
		LivesText->SetText(FText::FromString(FString::Printf(TEXT("x%d"), CurrentLives)));
	}

	// Update icon display
	for (int32 i = 0; i < LifeIcons.Num(); i++)
	{
		if (LifeIcons[i])
		{
			if (i < CurrentLives)
			{
				// Active life
				LifeIcons[i]->SetColorAndOpacity(LifeActiveColor);
			}
			else
			{
				// Lost life (grayed out)
				LifeIcons[i]->SetColorAndOpacity(LifeLostColor);
			}
		}
	}
}

void UGameHUDWidget::UpdateOverclockDisplay(float CurrentMeter, float MaxMeter)
{
	float Percent = MaxMeter > 0.0f ? CurrentMeter / MaxMeter : 0.0f;

	if (OverclockMeterBar)
	{
		OverclockMeterBar->SetPercent(Percent);
	}

	if (OverclockMeterText)
	{
		int32 PercentInt = FMath::RoundToInt(Percent * 100.0f);
		OverclockMeterText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), PercentInt)));
	}

	// Update ready indicator
	if (OverclockReadyText && OverclockSystem)
	{
		bool bCanActivate = OverclockSystem->CanActivateOverclock();
		OverclockReadyText->SetVisibility(bCanActivate ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UGameHUDWidget::UpdateOverclockActiveState(bool bIsActive)
{
	if (OverclockActiveText)
	{
		OverclockActiveText->SetVisibility(bIsActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Hide ready text when active
	if (OverclockReadyText && bIsActive)
	{
		OverclockReadyText->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Update multiplier text
	if (MultiplierText)
	{
		if (bIsActive)
		{
			MultiplierText->SetText(FText::FromString(TEXT("2.5x")));
			MultiplierText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			MultiplierText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UGameHUDWidget::UpdateSpeedDisplay(float ScrollSpeed)
{
	if (SpeedText)
	{
		// Display speed in "MU/s" (Memory Units per second)
		// This is a thematic unit for a cyberpunk runner - represents data transfer rate
		// Convert to integer for cleaner display (no decimals needed at these scales)
		int32 SpeedInt = FMath::RoundToInt(ScrollSpeed);
		SpeedText->SetText(FText::FromString(FString::Printf(TEXT("%d"), SpeedInt)));
	}
}

// --- Event Handlers ---

void UGameHUDWidget::HandleScoreChanged(int32 NewScore)
{
	UpdateScoreDisplay(NewScore);
}

void UGameHUDWidget::HandleScoreRateChanged(int32 NewScoreRate)
{
	if (ScoreRateText)
	{
		ScoreRateText->SetText(FText::FromString(FString::Printf(TEXT("+%d/sec"), NewScoreRate)));
	}
}

void UGameHUDWidget::HandleHighScoreBeaten(int32 NewHighScore, int32 OldHighScore)
{
	bHasBeatenHighScore = true;

	// Update high score display
	if (HighScoreText)
	{
		HighScoreText->SetText(FText::FromString(FString::Printf(TEXT("HS: %d"), NewHighScore)));
	}

	// Show new high score indicator
	if (NewHighScoreText)
	{
		NewHighScoreText->SetVisibility(ESlateVisibility::Visible);
	}

	// Call Blueprint event
	OnHighScoreBeaten(NewHighScore, OldHighScore);

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: New high score! %d (was %d)"), NewHighScore, OldHighScore);
}

void UGameHUDWidget::HandleLivesChanged(int32 CurrentLives, int32 MaxLives)
{
	UpdateLivesDisplay(CurrentLives, MaxLives);
}

void UGameHUDWidget::HandleDamageTaken(int32 RemainingLives)
{
	// Call Blueprint event for visual effects
	OnDamageTaken(RemainingLives);
}

void UGameHUDWidget::HandleInvulnerabilityChanged(bool bIsInvulnerable)
{
	if (bIsInvulnerable)
	{
		OnInvulnerabilityStarted();
	}
	else
	{
		OnInvulnerabilityEnded();
	}
}

void UGameHUDWidget::HandleOverclockStateChanged(bool bIsActive)
{
	UpdateOverclockActiveState(bIsActive);

	if (bIsActive)
	{
		OnOverclockActivated();
	}
	else
	{
		OnOverclockDeactivated();
	}
}

void UGameHUDWidget::HandleOverclockMeterChanged(float CurrentMeter, float MaxMeter)
{
	UpdateOverclockDisplay(CurrentMeter, MaxMeter);
}

void UGameHUDWidget::HandleTutorialPrompt(ETutorialObstacleType PromptType)
{
	// If controls tutorial is still in progress, stop it immediately
	// (obstacle prompts take priority)
	if (IsControlsTutorialInProgress() || bIsControlsPopupVisible)
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: Stopping controls tutorial - obstacle prompt taking over"));
		StopControlsTutorial();
	}

	FString PromptText = GetTutorialPromptText(PromptType);
	ShowTutorialPrompt(PromptText);
	
	// After SLIDE tutorial, queue the OVERCLOCK tutorial to encourage boosting
	if (PromptType == ETutorialObstacleType::Slide && !bHasShownOverclockTutorial)
	{
		if (UWorld* World = GetWorld())
		{
			// Show OVERCLOCK! prompt 1.5 seconds after SLIDE prompt appears
			World->GetTimerManager().SetTimer(
				OverclockTutorialTimerHandle,
				this,
				&UGameHUDWidget::OnOverclockTutorialTimerExpired,
				1.5f,
				false
			);
		}
	}
}

void UGameHUDWidget::HandleTutorialComplete()
{
	HideTutorialPrompt();
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: Tutorial complete"));
}

void UGameHUDWidget::HandleOneUpCollected(bool bWasAtMaxLives, int32 BonusPoints)
{
	ShowOneUpCollectedPopup(bWasAtMaxLives, BonusPoints);
}

void UGameHUDWidget::HandleEMPCollected(int32 ObstaclesDestroyed, int32 PointsPerObstacle)
{
	ShowEMPCollectedPopup(ObstaclesDestroyed, PointsPerObstacle);
}

void UGameHUDWidget::HandleMagnetCollected()
{
	ShowMagnetCollectedPopup();
}

void UGameHUDWidget::HandleNiceCombo(int32 BonusPoints)
{
	ShowNiceComboPopup(BonusPoints);
}

void UGameHUDWidget::HandleInsaneCombo(int32 BonusPoints)
{
	ShowInsaneComboPopup(BonusPoints);
}

void UGameHUDWidget::HandleScrollSpeedChanged(float NewScrollSpeed)
{
	UpdateSpeedDisplay(NewScrollSpeed);
}

// --- Internal Functions ---

void UGameHUDWidget::OnTutorialPopupStartShrink()
{
	// Stop grow animation if still in progress
	bTutorialPopupGrowing = false;
	TutorialGrowProgress = 0.0f;
	
	// Ensure we're at full scale before shrinking
	SetTutorialPopupScale(1.0f);
	
	// Start the shrink animation instead of immediately hiding
	bTutorialPopupShrinking = true;
	TutorialShrinkProgress = 0.0f;
}

void UGameHUDWidget::OnPickupPopupStartShrink()
{
	// Stop grow animation if still in progress
	bPickupPopupGrowing = false;
	PickupGrowProgress = 0.0f;
	
	// Ensure we're at full scale before shrinking
	SetPickupPopupScale(1.0f);
	
	// Start the shrink animation instead of immediately hiding
	bPickupPopupShrinking = true;
	PickupShrinkProgress = 0.0f;
}

void UGameHUDWidget::OnOverclockTutorialTimerExpired()
{
	if (!bHasShownOverclockTutorial)
	{
		bHasShownOverclockTutorial = true;
		ShowTutorialPrompt(TEXT("OVERCLOCK!"), 2.5f);
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: Showing OVERCLOCK tutorial prompt"));
	}
}

FString UGameHUDWidget::GetTutorialPromptText(ETutorialObstacleType PromptType) const
{
	switch (PromptType)
	{
	case ETutorialObstacleType::LaneSwitch:
		return TEXT("SWITCH LANES!");  // ← SWITCH LANES! →
	case ETutorialObstacleType::Jump:
		return TEXT("JUMP!!");          // ↑ JUMP! ↑
	case ETutorialObstacleType::Slide:
		return TEXT("SLIDE!!!");         // ↓ SLIDE! ↓
	default:
		return TEXT("");
	}
}

void UGameHUDWidget::UpdateOverclockMeterColor()
{
	if (!OverclockMeterBar || !OverclockSystem) return;

	FLinearColor TargetColor;

	// Color states (all customizable in Blueprint):
	// - Active (depleting): OverclockActiveColor
	// - Ready (above threshold, not active): OverclockReadyColor
	// - Charging (below threshold): OverclockChargingColor

	if (OverclockSystem->IsOverclockActive())
	{
		TargetColor = OverclockActiveColor;
	}
	else if (OverclockSystem->CanActivateOverclock())
	{
		TargetColor = OverclockReadyColor;
	}
	else
	{
		TargetColor = OverclockChargingColor;
	}

	// Apply color to the fill
	OverclockMeterBar->SetFillColorAndOpacity(TargetColor);
}

// --- Popup Animations (Grow-in and Shrink-out) ---

void UGameHUDWidget::UpdateTutorialShrinkAnimation(float DeltaTime)
{
	if (!bTutorialPopupShrinking)
	{
		return;
	}

	// Advance animation progress
	TutorialShrinkProgress += DeltaTime / PopupShrinkDuration;

	if (TutorialShrinkProgress >= 1.0f)
	{
		// Animation complete - hide and reset
		bTutorialPopupShrinking = false;
		TutorialShrinkProgress = 0.0f;
		
		// Actually hide the widgets now
		if (TutorialPromptText)
		{
			TutorialPromptText->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (TutorialPromptContainer)
		{
			TutorialPromptContainer->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (TutorialBackground)
		{
			TutorialBackground->SetVisibility(ESlateVisibility::Collapsed);
		}
		
		// Reset scale for next use
		ResetTutorialPopupScale();
	}
	else
	{
		// Calculate scale with ease-in (starts slow, accelerates)
		// InterpEaseIn: Alpha^Exponent gives an ease-in curve
		float EasedProgress = FMath::Pow(TutorialShrinkProgress, PopupShrinkEaseExponent);
		float Scale = 1.0f - EasedProgress;
		
		SetTutorialPopupScale(Scale);
	}
}

void UGameHUDWidget::UpdateTutorialGrowAnimation(float DeltaTime)
{
	if (!bTutorialPopupGrowing)
	{
		return;
	}

	// Progress the animation
	TutorialGrowProgress += DeltaTime / PopupGrowDuration;

	if (TutorialGrowProgress >= 1.0f)
	{
		// Animation complete - set to full scale
		bTutorialPopupGrowing = false;
		TutorialGrowProgress = 0.0f;
		SetTutorialPopupScale(1.0f);
	}
	else
	{
		// Calculate scale with ease-out (starts fast, decelerates)
		// Ease-out: 1 - (1 - progress)^exponent
		float EasedProgress = 1.0f - FMath::Pow(1.0f - TutorialGrowProgress, PopupGrowEaseExponent);
		float Scale = EasedProgress;  // 0.0 → 1.0
		
		SetTutorialPopupScale(Scale);
	}
}

void UGameHUDWidget::UpdatePickupShrinkAnimation(float DeltaTime)
{
	if (!bPickupPopupShrinking)
	{
		return;
	}

	// Advance animation progress
	PickupShrinkProgress += DeltaTime / PopupShrinkDuration;

	if (PickupShrinkProgress >= 1.0f)
	{
		// Animation complete - hide and reset
		bPickupPopupShrinking = false;
		PickupShrinkProgress = 0.0f;
		
		// Actually hide the widgets now
		if (PickupPopupText)
		{
			PickupPopupText->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (PickupPopupContainer)
		{
			PickupPopupContainer->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (PickupPopupBackground)
		{
			PickupPopupBackground->SetVisibility(ESlateVisibility::Collapsed);
		}
		
		// Reset scale for next use
		ResetPickupPopupScale();

		// Popup is now fully done — process next queued popup
		bIsPickupPopupActive = false;
		ShowNextQueuedPopup();
	}
	else
	{
		// Calculate scale with ease-in (starts slow, accelerates)
		float EasedProgress = FMath::Pow(PickupShrinkProgress, PopupShrinkEaseExponent);
		float Scale = 1.0f - EasedProgress;
		
		SetPickupPopupScale(Scale);
	}
}

void UGameHUDWidget::SetTutorialPopupScale(float Scale)
{
	FVector2D ScaleVector(Scale, Scale);
	
	// Apply scale to text
	if (TutorialPromptText)
	{
		TutorialPromptText->SetRenderScale(ScaleVector);
	}
	
	// Apply scale to background
	if (TutorialBackground)
	{
		TutorialBackground->SetRenderScale(ScaleVector);
	}
	
	// Apply scale to container if present
	if (TutorialPromptContainer)
	{
		TutorialPromptContainer->SetRenderScale(ScaleVector);
	}
}

void UGameHUDWidget::SetPickupPopupScale(float Scale)
{
	FVector2D ScaleVector(Scale, Scale);
	
	// Apply scale to text
	if (PickupPopupText)
	{
		PickupPopupText->SetRenderScale(ScaleVector);
	}
	
	// Apply scale to background
	if (PickupPopupBackground)
	{
		PickupPopupBackground->SetRenderScale(ScaleVector);
	}
	
	// Apply scale to container if present
	if (PickupPopupContainer)
	{
		PickupPopupContainer->SetRenderScale(ScaleVector);
	}
}

void UGameHUDWidget::UpdatePickupGrowAnimation(float DeltaTime)
{
	if (!bPickupPopupGrowing)
	{
		return;
	}

	// Progress the animation
	PickupGrowProgress += DeltaTime / PopupGrowDuration;

	if (PickupGrowProgress >= 1.0f)
	{
		// Animation complete - set to full scale
		bPickupPopupGrowing = false;
		PickupGrowProgress = 0.0f;
		SetPickupPopupScale(1.0f);
	}
	else
	{
		// Calculate scale with ease-out (starts fast, decelerates)
		// Ease-out: 1 - (1 - progress)^exponent
		float EasedProgress = 1.0f - FMath::Pow(1.0f - PickupGrowProgress, PopupGrowEaseExponent);
		float Scale = EasedProgress;  // 0.0 → 1.0
		
		SetPickupPopupScale(Scale);
	}
}

void UGameHUDWidget::ResetTutorialPopupScale()
{
	FVector2D FullScale(1.0f, 1.0f);
	
	if (TutorialPromptText)
	{
		TutorialPromptText->SetRenderScale(FullScale);
	}
	
	if (TutorialBackground)
	{
		TutorialBackground->SetRenderScale(FullScale);
	}
	
	if (TutorialPromptContainer)
	{
		TutorialPromptContainer->SetRenderScale(FullScale);
	}
}

void UGameHUDWidget::ResetPickupPopupScale()
{
	FVector2D FullScale(1.0f, 1.0f);
	
	if (PickupPopupText)
	{
		PickupPopupText->SetRenderScale(FullScale);
	}
	
	if (PickupPopupBackground)
	{
		PickupPopupBackground->SetRenderScale(FullScale);
	}
	
	if (PickupPopupContainer)
	{
		PickupPopupContainer->SetRenderScale(FullScale);
	}
}

// --- Pickup Popups (separate from tutorial prompts for different font size) ---

void UGameHUDWidget::ShowPickupPopup(const FString& PopupText, float Duration)
{
	// If a popup is currently active, queue this one for later
	if (bIsPickupPopupActive)
	{
		FQueuedPickupPopup Queued;
		Queued.Text = PopupText;
		Queued.Duration = Duration;
		PickupPopupQueue.Add(Queued);
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: Popup queued: %s (queue size: %d)"),
			*PopupText.Left(30), PickupPopupQueue.Num());
		return;
	}

	bIsPickupPopupActive = true;

	// Cancel any in-progress animations and reset
	bPickupPopupShrinking = false;
	PickupShrinkProgress = 0.0f;
	bPickupPopupGrowing = false;
	PickupGrowProgress = 0.0f;

	// Start at scale 0 for grow-in animation
	SetPickupPopupScale(0.0f);

	// Use the dedicated pickup popup widgets (separate from tutorial text)
	if (PickupPopupText)
	{
		PickupPopupText->SetText(FText::FromString(PopupText));
		PickupPopupText->SetVisibility(ESlateVisibility::Visible);
	}

	if (PickupPopupContainer)
	{
		PickupPopupContainer->SetVisibility(ESlateVisibility::Visible);
	}

	if (PickupPopupBackground)
	{
		PickupPopupBackground->SetVisibility(ESlateVisibility::Visible);
	}

	// Start grow-in animation
	bPickupPopupGrowing = true;
	PickupGrowProgress = 0.0f;

	// Set timer to START SHRINK (not immediate hide)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PickupPopupTimerHandle,
			this,
			&UGameHUDWidget::OnPickupPopupStartShrink,
			Duration,
			false
		);
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: Pickup popup: %s"), *PopupText);
}

void UGameHUDWidget::HidePickupPopup()
{
	// Stop any in-progress animations
	bPickupPopupShrinking = false;
	PickupShrinkProgress = 0.0f;
	bPickupPopupGrowing = false;
	PickupGrowProgress = 0.0f;

	if (PickupPopupText)
	{
		PickupPopupText->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (PickupPopupContainer)
	{
		PickupPopupContainer->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (PickupPopupBackground)
	{
		PickupPopupBackground->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Reset scale for next show
	ResetPickupPopupScale();

	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PickupPopupTimerHandle);
	}

	// Mark popup as inactive and process next queued popup
	bIsPickupPopupActive = false;
	ShowNextQueuedPopup();
}

void UGameHUDWidget::ShowOneUpCollectedPopup(bool bWasAtMaxLives, int32 BonusPoints)
{
	FString PopupText;
	
	if (bWasAtMaxLives)
	{
		// At max lives - bonus points instead
		PopupText = FString::Printf(TEXT("1-UP BONUS!\n+%d"), BonusPoints);
	}
	else
	{
		// Gained a life
		PopupText = TEXT("1-UP!\nEXTRA LIFE!");
	}
	
	ShowPickupPopup(PopupText, OneUpPopupDuration);
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: 1-Up popup - %s"), bWasAtMaxLives ? TEXT("Bonus") : TEXT("Extra Life"));
}

void UGameHUDWidget::ShowEMPCollectedPopup(int32 ObstaclesDestroyed, int32 PointsPerObstacle)
{
	int32 TotalPoints = ObstaclesDestroyed * PointsPerObstacle;
	
	FString PopupText;
	if (ObstaclesDestroyed > 0)
	{
		PopupText = FString::Printf(TEXT("EMP!\n%d OBSTACLES\n+%d"), ObstaclesDestroyed, TotalPoints);
	}
	else
	{
		PopupText = TEXT("EMP!\n+OVERCLOCK BOOST");
	}
	
	ShowPickupPopup(PopupText, EMPPopupDuration);
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: EMP popup - %d obstacles destroyed"), ObstaclesDestroyed);
}

void UGameHUDWidget::ShowMagnetCollectedPopup()
{
	// Magnet popup takes priority — interrupt current popup if any
	if (bIsPickupPopupActive)
	{
		// Force-hide current popup without processing queue
		bIsPickupPopupActive = false;
		bPickupPopupShrinking = false;
		bPickupPopupGrowing = false;
		PickupShrinkProgress = 0.0f;
		PickupGrowProgress = 0.0f;

		if (PickupPopupText)
		{
			PickupPopupText->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (PickupPopupContainer)
		{
			PickupPopupContainer->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (PickupPopupBackground)
		{
			PickupPopupBackground->SetVisibility(ESlateVisibility::Collapsed);
		}

		ResetPickupPopupScale();

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(PickupPopupTimerHandle);
		}
	}

	FString PopupText = TEXT("MAGNET!\nPULLING PICKUPS!");
	ShowPickupPopup(PopupText, MagnetPopupDuration);
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: Magnet popup shown (priority)"));
}

void UGameHUDWidget::ShowNextQueuedPopup()
{
	if (PickupPopupQueue.Num() > 0)
	{
		FQueuedPickupPopup Next = PickupPopupQueue[0];
		PickupPopupQueue.RemoveAt(0);
		ShowPickupPopup(Next.Text, Next.Duration);
	}
}

void UGameHUDWidget::ShowNiceComboPopup(int32 BonusPoints)
{
	FString PopupText = FString::Printf(TEXT("NICE COMBO!\n+%d"), BonusPoints);
	ShowPickupPopup(PopupText, NiceComboPopupDuration);
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: NICE combo popup - +%d bonus"), BonusPoints);
}

void UGameHUDWidget::ShowInsaneComboPopup(int32 BonusPoints)
{
	FString PopupText = FString::Printf(TEXT("INSANE! 10+ COMBO!\n+%d"), BonusPoints);
	ShowPickupPopup(PopupText, InsaneComboPopupDuration);
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: INSANE combo popup - +%d bonus"), BonusPoints);
}

// --- Controls Tutorial (Intro Segment Timed Popups) ---

void UGameHUDWidget::StartControlsTutorial()
{
	// Don't start if already started
	if (bHasStartedControlsTutorial)
	{
		return;
	}

	// Check if we have the PickupPopupText widget (we use the pickup popup system for controls)
	if (!PickupPopupText)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("GameHUDWidget: Cannot start controls tutorial - PickupPopupText widget not bound"));
		return;
	}

	bHasStartedControlsTutorial = true;
	ControlsTutorialStep = 0;

	// Schedule first popup after start delay
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ControlsTutorialTimerHandle,
			this,
			&UGameHUDWidget::OnControlsTutorialTimer,
			ControlsTutorialStartDelay,
			false
		);
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: Controls tutorial started, first popup in %.1fs"), ControlsTutorialStartDelay);
}

void UGameHUDWidget::ShowNextControlsPopup()
{
	// Check if tutorial is done (5 steps: MOVE, JUMP, SLIDE, CAMERA, OVERCLOCK)
	if (ControlsTutorialStep >= 5)
	{
		HideControlsPopup();
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: Controls tutorial complete"));
		return;
	}

	// Get text for current step
	FString PopupText = GetControlsTutorialText(ControlsTutorialStep);

	// Use the PickupPopup system (same background as 1-Up, EMP, Combo popups)
	ShowPickupPopup(PopupText, ControlsPopupDuration);
	
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: Controls popup %d shown: %s"), ControlsTutorialStep, *PopupText.Left(50));

	bIsControlsPopupVisible = true;

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: Controls popup %d shown"), ControlsTutorialStep);

	// Schedule hide after duration, then next popup after gap
	if (UWorld* World = GetWorld())
	{
		// Timer to hide this popup and schedule next
		World->GetTimerManager().SetTimer(
			ControlsTutorialTimerHandle,
			this,
			&UGameHUDWidget::OnControlsTutorialTimer,
			ControlsPopupDuration,
			false
		);
	}
}

void UGameHUDWidget::HideControlsPopup()
{
	bIsControlsPopupVisible = false;

	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ControlsTutorialTimerHandle);
	}

	// Trigger shrink animation instead of immediate hide (consistent with all other popups)
	// Stop grow animation if still in progress
	bPickupPopupGrowing = false;
	PickupGrowProgress = 0.0f;
	
	// Ensure we're at full scale before shrinking
	SetPickupPopupScale(1.0f);
	
	// Start the shrink animation
	bPickupPopupShrinking = true;
	PickupShrinkProgress = 0.0f;
}

void UGameHUDWidget::StopControlsTutorial()
{
	// Mark tutorial as complete so no more popups show
	ControlsTutorialStep = 5;  // Set to max step to mark as done
	
	// Clear any pending timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ControlsTutorialTimerHandle);
	}
	
	// If a popup is currently visible, trigger shrink animation instead of immediate hide
	if (bIsControlsPopupVisible)
	{
		bIsControlsPopupVisible = false;
		
		// Trigger the shrink animation (same as pickup popup exit)
		// The pickup popup system already has this - just set the flags
		bPickupPopupShrinking = true;
		PickupShrinkProgress = 0.0f;
		bPickupPopupGrowing = false;  // Cancel any grow animation
		PickupGrowProgress = 0.0f;
		
		// Make sure we start from full scale for the shrink
		SetPickupPopupScale(1.0f);
		
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: Controls tutorial stopped with exit animation"));
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: Controls tutorial stopped (no popup visible)"));
	}
}

void UGameHUDWidget::OnControlsTutorialTimer()
{
	if (bIsControlsPopupVisible)
	{
		// Currently showing a popup - trigger shrink animation instead of immediate hide
		bIsControlsPopupVisible = false;

		// Stop grow animation if still in progress
		bPickupPopupGrowing = false;
		PickupGrowProgress = 0.0f;
		
		// Ensure we're at full scale before shrinking
		SetPickupPopupScale(1.0f);
		
		// Start the shrink animation (Tick will handle the actual animation)
		bPickupPopupShrinking = true;
		PickupShrinkProgress = 0.0f;

		// Advance to next step
		ControlsTutorialStep++;

		// Schedule next popup after gap + shrink duration (if not done)
		// The gap starts AFTER the shrink animation completes visually
		if (ControlsTutorialStep < 5)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					ControlsTutorialTimerHandle,
					this,
					&UGameHUDWidget::OnControlsTutorialTimer,
					PopupShrinkDuration + ControlsPopupGap,  // Wait for shrink + gap
					false
				);
			}
		}
		else
		{
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameHUDWidget: Controls tutorial sequence complete"));
		}
	}
	else
	{
		// Not showing - show next popup
		ShowNextControlsPopup();
	}
}

FString UGameHUDWidget::GetControlsTutorialText(int32 Step) const
{
	switch (Step)
	{
		case 0: return ControlsTextMove;
		case 1: return ControlsTextJump;
		case 2: return ControlsTextSlide;
		case 3: return ControlsTextCamera;
		case 4: return ControlsTextOverclock;
		default: return TEXT("");
	}
}
