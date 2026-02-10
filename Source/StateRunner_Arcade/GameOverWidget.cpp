#include "GameOverWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "StateRunner_ArcadeGameMode.h"
#include "ScoreSystemComponent.h"
#include "LivesSystemComponent.h"
#include "WorldScrollComponent.h"
#include "ObstacleSpawnerComponent.h"
#include "PickupSpawnerComponent.h"
#include "OverclockSystemComponent.h"
#include "MusicPlayerWidget.h"
#include "LeaderboardWidget.h"
#include "StateRunner_Arcade.h"

UGameOverWidget::UGameOverWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Default focus will be set dynamically based on qualification
	DefaultFocusIndex = 0;
}

void UGameOverWidget::NativeConstruct()
{
	// Cache component references FIRST (before checking qualification)
	CacheComponentReferences();
	
	// Determine qualification status BEFORE registering buttons
	// This affects the menu order
	if (ScoreSystem)
	{
		FinalScore = ScoreSystem->GetCurrentScore();
		HighScore = ScoreSystem->GetHighScore();
		bIsNewHighScore = ScoreSystem->IsNewHighScore();
		
		if (bIsNewHighScore)
		{
			HighScore = FinalScore;
		}
		
		// Check if player qualified for leaderboard
		bQualifiedForLeaderboard = ScoreSystem->WouldQualifyForLeaderboard();
		if (bQualifiedForLeaderboard)
		{
			LeaderboardRank = ScoreSystem->GetCurrentLeaderboardRank();
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Qualified for leaderboard rank #%d"), LeaderboardRank);
		}
	}
	
	// Register buttons BEFORE calling Super - ORDER MATTERS for focus navigation
	
	// Optionally register confirm button if it exists (not required - Enter key works)
	if (ConfirmInitialsButton)
	{
		ConfirmInitialsButton->OnClicked.AddDynamic(this, &UGameOverWidget::OnConfirmInitialsClicked);
		
		// Only add to focusable list if qualified
		if (bQualifiedForLeaderboard)
		{
			RegisterButton(ConfirmInitialsButton, ConfirmInitialsLabel);
		}
	}
	
	// Show/hide initials entry based on qualification (NOT dependent on button existing)
	if (bQualifiedForLeaderboard)
	{
		// Show initials entry container
		if (InitialsEntryContainer)
		{
			InitialsEntryContainer->SetVisibility(ESlateVisibility::Visible);
		}
		
		// Hide high score text — it occupies the same space as the initials entry
		// and is redundant when the player qualified. Restored in HideInitialsEntry().
		if (HighScoreText)
		{
			HighScoreText->SetVisibility(ESlateVisibility::Collapsed);
		}
		
		// Start in initials entry mode automatically
		StartInitialsEntry();
		
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Qualified for leaderboard - initials entry started"));
	}
	else
	{
		// Hide initials entry if not qualified
		if (InitialsEntryContainer)
		{
			InitialsEntryContainer->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	
	// Register Restart button
	if (RestartButton)
	{
		RegisterButton(RestartButton, RestartLabel);
		RestartButton->OnClicked.AddDynamic(this, &UGameOverWidget::OnRestartClicked);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("GameOverWidget: RestartButton not bound!"));
	}

	// Register Leaderboard button (optional)
	if (LeaderboardButton)
	{
		RegisterButton(LeaderboardButton, LeaderboardLabel);
		LeaderboardButton->OnClicked.AddDynamic(this, &UGameOverWidget::OnLeaderboardClicked);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: LeaderboardButton not bound (optional)"));
	}

	// Register Quit to Menu button
	if (QuitToMenuButton)
	{
		RegisterButton(QuitToMenuButton, QuitToMenuLabel);
		QuitToMenuButton->OnClicked.AddDynamic(this, &UGameOverWidget::OnQuitToMenuClicked);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("GameOverWidget: QuitToMenuButton not bound!"));
	}

	// Set embedded music player for zone navigation (uses Left/Right instead of Up/Down)
	if (MusicPlayerWidget)
	{
		SetEmbeddedMusicPlayer(MusicPlayerWidget);
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Music player set for zone navigation (Left/Right)"));
	}

	// Update score displays
	UpdateScoreDisplays();
	
	if (bIsNewHighScore)
	{
		OnNewHighScoreSet(FinalScore, ScoreSystem ? ScoreSystem->GetHighScore() : 0);
	}

	// Call parent (sets initial focus to index 0, which is Enter Initials if qualified)
	Super::NativeConstruct();

	// Pause the game while Game Over is shown
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetPause(true);
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Game paused"));
		}
	}

	// Input delay - track start time and check in tick (timers don't work while paused)
	bInputEnabled = false;
	InputDelayStartTime = FPlatformTime::Seconds();
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Input disabled for %.1f seconds"), InputDelayDuration);

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Constructed - Score: %d, Qualified: %s, Initials Active: %s"),
		FinalScore, bQualifiedForLeaderboard ? TEXT("YES") : TEXT("NO"), bInitialsEntryActive ? TEXT("YES") : TEXT("NO"));
}

void UGameOverWidget::NativeDestruct()
{
	// Clear timers
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InputDelayTimerHandle);
		World->GetTimerManager().ClearTimer(UnderscoreBlinkTimerHandle);
	}

	// Unbind button delegates to prevent dangling references
	if (RestartButton)
	{
		RestartButton->OnClicked.RemoveDynamic(this, &UGameOverWidget::OnRestartClicked);
	}
	if (LeaderboardButton)
	{
		LeaderboardButton->OnClicked.RemoveDynamic(this, &UGameOverWidget::OnLeaderboardClicked);
	}
	if (QuitToMenuButton)
	{
		QuitToMenuButton->OnClicked.RemoveDynamic(this, &UGameOverWidget::OnQuitToMenuClicked);
	}
	if (ConfirmInitialsButton)
	{
		ConfirmInitialsButton->OnClicked.RemoveDynamic(this, &UGameOverWidget::OnConfirmInitialsClicked);
	}

	// Clean up leaderboard widget if still open
	if (LeaderboardWidget)
	{
		LeaderboardWidget->OnBackPressed.RemoveDynamic(this, &UGameOverWidget::CloseLeaderboard);
		LeaderboardWidget->RemoveFromParent();
		LeaderboardWidget = nullptr;
	}

	// Clear cached references
	ScoreSystem = nullptr;

	Super::NativeDestruct();
}

// --- Setup Functions ---

void UGameOverWidget::SetupGameOver(int32 InFinalScore, int32 InHighScore, bool bNewHighScore)
{
	FinalScore = InFinalScore;
	HighScore = InHighScore;
	bIsNewHighScore = bNewHighScore;

	UpdateScoreDisplays();

	if (bIsNewHighScore)
	{
		OnNewHighScoreSet(FinalScore, InHighScore);
	}
}

void UGameOverWidget::InitializeFromGameMode()
{
	// This function is kept for Blueprint compatibility and manual re-initialization
	// Primary initialization now happens in NativeConstruct
	
	if (!ScoreSystem)
	{
		CacheComponentReferences();
	}

	if (ScoreSystem)
	{
		FinalScore = ScoreSystem->GetCurrentScore();
		HighScore = ScoreSystem->GetHighScore();
		bIsNewHighScore = ScoreSystem->IsNewHighScore();

		if (bIsNewHighScore)
		{
			HighScore = FinalScore;
		}

		bQualifiedForLeaderboard = ScoreSystem->WouldQualifyForLeaderboard();
		if (bQualifiedForLeaderboard)
		{
			LeaderboardRank = ScoreSystem->GetCurrentLeaderboardRank();
		}

		UpdateScoreDisplays();

		if (bIsNewHighScore)
		{
			OnNewHighScoreSet(FinalScore, ScoreSystem->GetHighScore());
		}
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("GameOverWidget: Could not get ScoreSystemComponent"));
	}
}

void UGameOverWidget::CacheComponentReferences()
{
	if (UWorld* World = GetWorld())
	{
		if (AStateRunner_ArcadeGameMode* GameMode = Cast<AStateRunner_ArcadeGameMode>(World->GetAuthGameMode()))
		{
			ScoreSystem = GameMode->GetScoreSystemComponent();
		}
	}
}

void UGameOverWidget::UpdateScoreDisplays()
{
	// Update final score text
	if (FinalScoreText)
	{
		FinalScoreText->SetText(FText::FromString(FString::Printf(TEXT("%d"), FinalScore)));
	}

	// Update high score text
	if (HighScoreText)
	{
		if (bIsNewHighScore)
		{
			HighScoreText->SetText(FText::FromString(FString::Printf(TEXT("NEW HIGH: %d"), FinalScore)));
		}
		else
		{
			HighScoreText->SetText(FText::FromString(FString::Printf(TEXT("HIGH SCORE: %d"), HighScore)));
		}
	}

	// Show/hide new record indicator and backdrop (appears for ANY top 10 placement, not just #1)
	if (NewHighScoreText)
	{
		NewHighScoreText->SetVisibility(bQualifiedForLeaderboard ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (NewHighScoreBackdrop)
	{
		NewHighScoreBackdrop->SetVisibility(bQualifiedForLeaderboard ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Update score difference text
	if (ScoreDifferenceText)
	{
		if (bIsNewHighScore)
		{
			// HighScore was already updated to FinalScore if new record
			ScoreDifferenceText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			int32 Difference = HighScore - FinalScore;
			ScoreDifferenceText->SetText(FText::FromString(FString::Printf(TEXT("%d to beat high score"), Difference)));
			ScoreDifferenceText->SetVisibility(ESlateVisibility::Visible);
		}
	}
}

// --- Action Functions ---

void UGameOverWidget::RestartGame()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Restarting game - Reloading level: %s"), *GameplayLevelName.ToString());

	// Notify Blueprint
	OnBeforeRestart();

	// Unpause
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetPause(false);
		}
	}

	// Reload the gameplay level (clean restart - much simpler than resetting all systems)
	if (!GameplayLevelName.IsNone())
	{
		// Remove widget from viewport BEFORE level transition to prevent world leak
		RemoveFromParent();
		
		UGameplayStatics::OpenLevel(this, GameplayLevelName);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("GameOverWidget: GameplayLevelName not set! Cannot restart."));
	}
}

void UGameOverWidget::QuitToMainMenu()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Quitting to main menu"));

	// Notify Blueprint
	OnBeforeQuitToMenu();

	// Unpause
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetPause(false);
		}
	}

	// Load main menu level
	if (!MainMenuLevelName.IsNone())
	{
		// Remove widget from viewport BEFORE level transition to prevent world leak
		RemoveFromParent();
		
		UGameplayStatics::OpenLevel(this, MainMenuLevelName);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("GameOverWidget: MainMenuLevelName not set!"));
	}
}

void UGameOverWidget::ResetAllSystems()
{
	AStateRunner_ArcadeGameMode* GameMode = nullptr;
	if (UWorld* World = GetWorld())
	{
		GameMode = Cast<AStateRunner_ArcadeGameMode>(World->GetAuthGameMode());
	}

	if (!GameMode)
	{
		UE_LOG(LogStateRunner_Arcade, Error, TEXT("GameOverWidget: Could not get GameMode for reset"));
		return;
	}

	// Reset World Scroll
	if (UWorldScrollComponent* WorldScroll = GameMode->GetWorldScrollComponent())
	{
		WorldScroll->ResetScrollSpeed();
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Reset WorldScrollComponent"));
	}

	// Reset Score
	if (UScoreSystemComponent* Score = GameMode->GetScoreSystemComponent())
	{
		Score->ResetScore();
		Score->StartScoring(); // Restart scoring
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Reset ScoreSystemComponent"));
	}

	// Reset Lives
	if (ULivesSystemComponent* Lives = GameMode->GetLivesSystemComponent())
	{
		Lives->ResetLives();
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Reset LivesSystemComponent"));
	}

	// Reset Obstacles
	if (UObstacleSpawnerComponent* Obstacles = GameMode->GetObstacleSpawnerComponent())
	{
		Obstacles->ClearAllObstacles();
		Obstacles->ResetDifficulty();
		Obstacles->ResetTutorial();
		Obstacles->SpawnTutorialObstacles(); // Respawn tutorial obstacles
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Reset ObstacleSpawnerComponent"));
	}

	// Reset Pickups
	if (UPickupSpawnerComponent* Pickups = GameMode->GetPickupSpawnerComponent())
	{
		Pickups->ClearAllPickups();
		Pickups->ResetSpawner();
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Reset PickupSpawnerComponent"));
	}

	// Reset OVERCLOCK
	if (UOverclockSystemComponent* Overclock = GameMode->GetOverclockSystemComponent())
	{
		Overclock->ResetOverclock();
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Reset OverclockSystemComponent"));
	}

	// Reset player character position/state
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		// Reset character to starting position
		// The character's position is typically managed by the character itself
		// Handled by character Blueprint or separate reset function
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: All systems reset"));
}

// --- Input Delay ---

void UGameOverWidget::OnInputDelayExpired()
{
	bInputEnabled = true;
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Input now enabled"));
}

// --- Button Handlers ---

void UGameOverWidget::OnRestartClicked()
{
	if (!bInputEnabled) return;
	RestartGame();
}

void UGameOverWidget::OnLeaderboardClicked()
{
	if (!bInputEnabled) return;
	OpenLeaderboard();
}

void UGameOverWidget::OnQuitToMenuClicked()
{
	if (!bInputEnabled) return;
	QuitToMainMenu();
}

void UGameOverWidget::OnConfirmInitialsClicked()
{
	if (!bInputEnabled) return;
	
	// If initials entry is active, confirm them
	// If not active (user previously canceled), restart initials entry
	if (bInitialsEntryActive)
	{
		ConfirmInitials();
	}
	else if (bQualifiedForLeaderboard && !bInitialsSubmitted)
	{
		StartInitialsEntry();
	}
}

// --- Initials Entry ---

void UGameOverWidget::StartInitialsEntry()
{
	if (bInitialsSubmitted) return; // Already submitted, can't re-enter
	
	bInitialsEntryActive = true;
	CurrentInitials = TEXT("AAA");
	CurrentInitialSlot = 0;

	// Show initials entry container
	if (InitialsEntryContainer)
	{
		InitialsEntryContainer->SetVisibility(ESlateVisibility::Visible);
	}

	UpdateInitialsDisplay();
	UpdateUnderscoreDisplay();
	StartUnderscoreBlink();
	UpdateInitialsButtonText();

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Initials entry started"));
}

void UGameOverWidget::CancelInitialsEntry()
{
	if (!bInitialsEntryActive) return;
	
	bInitialsEntryActive = false;
	StopUnderscoreBlink();
	
	// Hide all underscores when not editing
	UpdateUnderscoreDisplay();
	
	// Keep the container visible so the button is still accessible
	// Just update the text to indicate they can re-enter
	UpdateInitialsButtonText();
	
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Initials entry canceled - can re-enter by clicking button"));
}

void UGameOverWidget::HideInitialsEntry()
{
	bInitialsEntryActive = false;
	StopUnderscoreBlink();

	if (InitialsEntryContainer)
	{
		InitialsEntryContainer->SetVisibility(ESlateVisibility::Collapsed);
	}
	
	// Restore high score text now that initials entry is gone
	if (HighScoreText)
	{
		HighScoreText->SetVisibility(ESlateVisibility::Visible);
	}
	
	UpdateInitialsButtonText();
}

void UGameOverWidget::CycleInitialUp()
{
	if (!bInitialsEntryActive) return;

	// Up cycles backward through alphabet (Z→Y→X...→A→Z) - visually "up" toward earlier letters
	TCHAR Current = GetInitialAtSlot(CurrentInitialSlot);
	TCHAR Prev = (Current == TEXT('A')) ? TEXT('Z') : (Current - 1);
	SetInitialAtSlot(CurrentInitialSlot, Prev);
	UpdateInitialsDisplay();
}

void UGameOverWidget::CycleInitialDown()
{
	if (!bInitialsEntryActive) return;

	// Down cycles forward through alphabet (A→B→C...→Z→A) - visually "down" toward later letters
	TCHAR Current = GetInitialAtSlot(CurrentInitialSlot);
	TCHAR Next = (Current == TEXT('Z')) ? TEXT('A') : (Current + 1);
	SetInitialAtSlot(CurrentInitialSlot, Next);
	UpdateInitialsDisplay();
}

void UGameOverWidget::MoveToNextSlot()
{
	if (!bInitialsEntryActive) return;

	CurrentInitialSlot = (CurrentInitialSlot + 1) % 3;
	UpdateInitialsDisplay();
	UpdateUnderscoreDisplay();
	
	// Reset blink to visible when changing slots
	bUnderscoreVisible = true;
}

void UGameOverWidget::MoveToPreviousSlot()
{
	if (!bInitialsEntryActive) return;

	CurrentInitialSlot = (CurrentInitialSlot + 2) % 3; // +2 is same as -1 mod 3
	UpdateInitialsDisplay();
	UpdateUnderscoreDisplay();
	
	// Reset blink to visible when changing slots
	bUnderscoreVisible = true;
}

void UGameOverWidget::ConfirmInitials()
{
	if (!bInitialsEntryActive || bInitialsSubmitted) return;

	bInitialsSubmitted = true;
	bInitialsEntryActive = false;

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Confirming initials: %s"), *CurrentInitials);

	// Submit to leaderboard with initials
	if (ScoreSystem)
	{
		LeaderboardRank = ScoreSystem->SubmitToLeaderboardWithInitials(CurrentInitials);
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Submitted to leaderboard with rank #%d"), LeaderboardRank);
	}

	// Hide initials entry
	HideInitialsEntry();

	// Automatically open leaderboard to show their entry
	OpenLeaderboard();
}

void UGameOverWidget::UpdateInitialsDisplay()
{
	// Update each initial text block
	for (int32 SlotIndex = 0; SlotIndex < 3; SlotIndex++)
	{
		UTextBlock* TextBlock = GetInitialTextBlock(SlotIndex);
		if (TextBlock)
		{
			// Set the character
			FString CharStr;
			CharStr.AppendChar(GetInitialAtSlot(SlotIndex));
			TextBlock->SetText(FText::FromString(CharStr));

			// Set color based on selection
			FLinearColor Color = (SlotIndex == CurrentInitialSlot) ? SelectedInitialColor : UnselectedInitialColor;
			TextBlock->SetColorAndOpacity(FSlateColor(Color));
		}
	}
}

void UGameOverWidget::SetInitialAtSlot(int32 SlotIndex, TCHAR Character)
{
	if (SlotIndex >= 0 && SlotIndex < 3 && CurrentInitials.Len() >= 3)
	{
		CurrentInitials[SlotIndex] = FChar::ToUpper(Character);
	}
}

TCHAR UGameOverWidget::GetInitialAtSlot(int32 SlotIndex) const
{
	if (SlotIndex >= 0 && SlotIndex < 3 && CurrentInitials.Len() >= 3)
	{
		return CurrentInitials[SlotIndex];
	}
	return TEXT('A');
}

UTextBlock* UGameOverWidget::GetInitialTextBlock(int32 SlotIndex) const
{
	switch (SlotIndex)
	{
		case 0: return Initial1Text.Get();
		case 1: return Initial2Text.Get();
		case 2: return Initial3Text.Get();
		default: return nullptr;
	}
}

void UGameOverWidget::UpdateInitialsButtonText()
{
	if (!ConfirmInitialsLabel) return;
	
	if (bInitialsSubmitted)
	{
		// Already submitted - hide or disable
		ConfirmInitialsLabel->SetText(FText::FromString(TEXT("SUBMITTED")));
	}
	else if (bInitialsEntryActive)
	{
		// Currently editing - show CONFIRM
		ConfirmInitialsLabel->SetText(FText::FromString(TEXT("CONFIRM")));
	}
	else
	{
		// Not editing - show ENTER INITIALS
		ConfirmInitialsLabel->SetText(FText::FromString(TEXT("ENTER INITIALS")));
	}
}

void UGameOverWidget::StartUnderscoreBlink()
{
	bUnderscoreVisible = true;
	LastUnderscoreToggleTime = FPlatformTime::Seconds();
	UpdateUnderscoreDisplay();
}

void UGameOverWidget::StopUnderscoreBlink()
{
	LastUnderscoreToggleTime = 0.0;
	bUnderscoreVisible = false;
	UpdateUnderscoreDisplay();
}

void UGameOverWidget::ToggleUnderscoreBlink()
{
	bUnderscoreVisible = !bUnderscoreVisible;
	UpdateUnderscoreDisplay();
}

void UGameOverWidget::UpdateUnderscoreDisplay()
{
	// Get all underscore text blocks
	UTextBlock* Underscores[3] = { Underscore1Text.Get(), Underscore2Text.Get(), Underscore3Text.Get() };
	
	for (int32 i = 0; i < 3; i++)
	{
		if (Underscores[i])
		{
			if (bInitialsEntryActive && i == CurrentInitialSlot && bUnderscoreVisible)
			{
				// Show underscore for current slot (blinking)
				Underscores[i]->SetVisibility(ESlateVisibility::Visible);
				Underscores[i]->SetColorAndOpacity(FSlateColor(SelectedInitialColor));
			}
			else
			{
				// Hide underscore for other slots or when blink is off
				Underscores[i]->SetVisibility(ESlateVisibility::Hidden);
			}
		}
	}
}

// --- Leaderboard ---

void UGameOverWidget::OpenLeaderboard()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Opening Leaderboard"));

	if (!LeaderboardWidgetClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("GameOverWidget: LeaderboardWidgetClass is not set!"));
		return;
	}

	// Create and show leaderboard widget
	LeaderboardWidget = CreateWidget<ULeaderboardWidget>(GetOwningPlayer(), LeaderboardWidgetClass);
	if (LeaderboardWidget)
	{
		// Set the highlighted rank before adding to viewport
		LeaderboardWidget->SetHighlightedRank(LeaderboardRank);

		LeaderboardWidget->AddToViewport(10); // Above game over screen
		LeaderboardWidget->TakeKeyboardFocus();

		// Bind to leaderboard back action to close it
		LeaderboardWidget->OnBackPressed.AddDynamic(this, &UGameOverWidget::CloseLeaderboard);

		// Hide game over screen while leaderboard is open
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UGameOverWidget::CloseLeaderboard()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Closing Leaderboard"));

	if (LeaderboardWidget)
	{
		LeaderboardWidget->OnBackPressed.RemoveDynamic(this, &UGameOverWidget::CloseLeaderboard);
		LeaderboardWidget->RemoveFromParent();
		LeaderboardWidget = nullptr;
	}

	// Show game over screen again
	SetVisibility(ESlateVisibility::Visible);

	// Refocus game over menu
	TakeKeyboardFocus();
}

// --- Overrides ---

void UGameOverWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	// Check input delay (timers don't work while paused, so we use real time)
	if (!bInputEnabled && InputDelayStartTime > 0.0)
	{
		double ElapsedTime = FPlatformTime::Seconds() - InputDelayStartTime;
		if (ElapsedTime >= InputDelayDuration)
		{
			bInputEnabled = true;
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameOverWidget: Input now enabled (after %.1f seconds)"), ElapsedTime);
		}
	}
	
	// Underscore blink animation (using real time since timers don't work while paused)
	if (bInitialsEntryActive && LastUnderscoreToggleTime > 0.0)
	{
		double CurrentTime = FPlatformTime::Seconds();
		double BlinkInterval = UnderscoreBlinkRate / 2.0; // Half rate for on/off cycle
		
		if (CurrentTime - LastUnderscoreToggleTime >= BlinkInterval)
		{
			bUnderscoreVisible = !bUnderscoreVisible;
			LastUnderscoreToggleTime = CurrentTime;
			UpdateUnderscoreDisplay();
		}
	}
}

void UGameOverWidget::OnItemSelected_Implementation(int32 ItemIndex)
{
	// Button clicks are handled by OnClicked delegates
}

void UGameOverWidget::OnBackAction_Implementation()
{
	// If in initials entry mode, cancel it
	if (bInitialsEntryActive)
	{
		CancelInitialsEntry();
		return;
	}
	
	// Otherwise, focus the Quit button
	SetFocusIndex(GetQuitIndex());
}

FReply UGameOverWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	FKey Key = InKeyEvent.GetKey();
	
	// Block all input except Escape during input delay
	// Escape is always allowed (for canceling initials entry)
	bool bIsEscapeKey = (Key == EKeys::Escape);
	if (!bInputEnabled && !bIsEscapeKey)
	{
		return FReply::Handled(); // Consume input but do nothing
	}
	
	// Handle Escape - cancel initials entry if active
	// Only Escape can cancel initials entry (not B button or anything else)
	if (bIsEscapeKey)
	{
		if (bInitialsEntryActive)
		{
			CancelInitialsEntry();
			return FReply::Handled();
		}
		// Let parent handle Escape for normal menu
	}
	
	// While initials are active, consume all input not handled below
	// so B button or other keys don't accidentally exit
	if (bInitialsEntryActive)
	{
		// B button should do nothing during initials entry (not cancel)
		if (Key == EKeys::Gamepad_FaceButton_Right)
		{
			return FReply::Handled(); // Consume but do nothing
		}
	}
	
	// Handle initials entry input (keyboard + controller)
	if (bInitialsEntryActive)
	{
		// Up - cycle letter backward (Z→Y→X...) (keyboard: Up/W, controller: DPad Up/LS Up)
		if (Key == EKeys::Up || Key == EKeys::W || 
			Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up)
		{
			CycleInitialUp();
			return FReply::Handled();
		}
		// Down - cycle letter forward (A→B→C...) (keyboard: Down/S, controller: DPad Down/LS Down)
		else if (Key == EKeys::Down || Key == EKeys::S || 
				 Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
		{
			CycleInitialDown();
			return FReply::Handled();
		}
		// Left - move to previous slot (keyboard: Left/A, controller: DPad Left/LS Left)
		else if (Key == EKeys::Left || Key == EKeys::A || 
				 Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left)
		{
			MoveToPreviousSlot();
			return FReply::Handled();
		}
		// Right - move to next slot (keyboard: Right/D, controller: DPad Right/LS Right)
		else if (Key == EKeys::Right || Key == EKeys::D || 
				 Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right)
		{
			MoveToNextSlot();
			return FReply::Handled();
		}
		// Confirm - Enter/Space/A button
		else if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
		{
			// Enter confirms current letter and moves to next
			// On the last slot (index 2), it confirms all initials
			if (CurrentInitialSlot < 2)
			{
				MoveToNextSlot();
			}
			else
			{
				ConfirmInitials();
			}
			return FReply::Handled();
		}
		
		// Consume all other input while initials entry is active
		// The only way out is Escape (handled above) or completing the initials
		return FReply::Handled();
	}

	// Pass to parent for normal menu navigation
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
