#include "LeaderboardWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "StateRunner_ArcadeGameMode.h"
#include "ScoreSystemComponent.h"
#include "StateRunner_Arcade.h"

ULeaderboardWidget::ULeaderboardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Default focus on Back button
	DefaultFocusIndex = INDEX_BACK;
	
	// Enable Left/Right navigation for horizontal button layout
	bLeftRightNavigatesMenu = true;
}

void ULeaderboardWidget::NativeConstruct()
{
	// Register buttons BEFORE calling Super

	// Register Back button (required)
	if (BackToGameOverButton)
	{
		RegisterButton(BackToGameOverButton, BackToGameOverLabel);
		BackToGameOverButton->OnClicked.AddDynamic(this, &ULeaderboardWidget::OnBackClicked);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("LeaderboardWidget: BackToGameOverButton not bound!"));
	}

	// In view-only mode (opened from main menu), hide Play Again and Quit buttons
	if (bViewOnlyMode)
	{
		// Collapse buttons instead of registering them
		if (PlayAgainButton)
		{
			PlayAgainButton->SetVisibility(ESlateVisibility::Collapsed);
			PlayAgainButton->OnClicked.AddDynamic(this, &ULeaderboardWidget::OnPlayAgainClicked);
		}
		if (PlayAgainLabel)
		{
			PlayAgainLabel->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (QuitToMenuButton)
		{
			QuitToMenuButton->SetVisibility(ESlateVisibility::Collapsed);
			QuitToMenuButton->OnClicked.AddDynamic(this, &ULeaderboardWidget::OnQuitToMenuClicked);
		}
		if (QuitToMenuLabel)
		{
			QuitToMenuLabel->SetVisibility(ESlateVisibility::Collapsed);
		}

		UE_LOG(LogStateRunner_Arcade, Log, TEXT("LeaderboardWidget: View-only mode - hiding Play Again and Quit buttons"));
	}
	else
	{
		// Register Play Again button (optional)
		if (PlayAgainButton)
		{
			RegisterButton(PlayAgainButton, PlayAgainLabel);
			PlayAgainButton->OnClicked.AddDynamic(this, &ULeaderboardWidget::OnPlayAgainClicked);
		}

		// Register Quit to Menu button (optional)
		if (QuitToMenuButton)
		{
			RegisterButton(QuitToMenuButton, QuitToMenuLabel);
			QuitToMenuButton->OnClicked.AddDynamic(this, &ULeaderboardWidget::OnQuitToMenuClicked);
		}
	}

	// Cache component references
	CacheComponentReferences();

	// Populate leaderboard data
	PopulateLeaderboard();

	// Call parent (sets initial focus)
	Super::NativeConstruct();

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("LeaderboardWidget: Constructed (ViewOnlyMode: %s)"), bViewOnlyMode ? TEXT("true") : TEXT("false"));
}

void ULeaderboardWidget::NativeDestruct()
{
	// Unbind button delegates
	if (BackToGameOverButton)
	{
		BackToGameOverButton->OnClicked.RemoveDynamic(this, &ULeaderboardWidget::OnBackClicked);
	}
	if (PlayAgainButton)
	{
		PlayAgainButton->OnClicked.RemoveDynamic(this, &ULeaderboardWidget::OnPlayAgainClicked);
	}
	if (QuitToMenuButton)
	{
		QuitToMenuButton->OnClicked.RemoveDynamic(this, &ULeaderboardWidget::OnQuitToMenuClicked);
	}

	// Clear cached references
	ScoreSystem = nullptr;

	Super::NativeDestruct();
}

//=============================================================================
// COMPONENT REFERENCES
//=============================================================================

void ULeaderboardWidget::CacheComponentReferences()
{
	if (UWorld* World = GetWorld())
	{
		if (AStateRunner_ArcadeGameMode* GameMode = Cast<AStateRunner_ArcadeGameMode>(World->GetAuthGameMode()))
		{
			ScoreSystem = GameMode->GetScoreSystemComponent();
		}
	}
}

//=============================================================================
// LEADERBOARD POPULATION
//=============================================================================

void ULeaderboardWidget::PopulateLeaderboard()
{
	TArray<FLeaderboardEntry> Entries;
	
	// Try to get from ScoreSystemComponent first (works during gameplay)
	if (!ScoreSystem)
	{
		CacheComponentReferences();
	}

	if (ScoreSystem)
	{
		// Get leaderboard data from the active score system
		Entries = ScoreSystem->GetLeaderboard();
	}
	else
	{
		// No ScoreSystem available (e.g., viewing from main menu)
		// Load directly from save file
		ULeaderboardSaveGame* SaveGame = Cast<ULeaderboardSaveGame>(
			UGameplayStatics::LoadGameFromSlot(TEXT("Leaderboard"), 0));
		
		if (SaveGame)
		{
			Entries = SaveGame->Entries;
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("LeaderboardWidget: Loaded %d entries directly from save file"), Entries.Num());
		}
		else
		{
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("LeaderboardWidget: No save file found - showing empty leaderboard"));
		}
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("LeaderboardWidget: Populating with %d entries"), Entries.Num());

	// Populate each row
	for (int32 i = 0; i < 10; i++)
	{
		if (i < Entries.Num())
		{
			const FLeaderboardEntry& Entry = Entries[i];
			
			// Determine if this entry needs special coloring
			// Only top 3 ranks and current run's rank get special colors
			// Ranks 4-10 are left alone (Blueprint default white)
			bool bNeedsSpecialColor = false;
			FLinearColor EntryColor = FLinearColor::White;
			
			// Current run's rank gets highlight color (overrides medal colors for emphasis)
			if (CurrentRunRank > 0 && CurrentRunRank == (i + 1))
			{
				EntryColor = HighlightColor;
				bNeedsSpecialColor = true;
			}
			// Top 3 get medal colors (HDR values for vibrancy)
			else if (i == 0)
			{
				EntryColor = GoldColor;   // #1 = Gold
				bNeedsSpecialColor = true;
			}
			else if (i == 1)
			{
				EntryColor = SilverColor; // #2 = Silver
				bNeedsSpecialColor = true;
			}
			else if (i == 2)
			{
				EntryColor = BronzeColor; // #3 = Bronze
				bNeedsSpecialColor = true;
			}
			// Ranks 4-10: Don't set color, leave Blueprint default (white)

			// Use player initials, or "---" if empty/legacy entry
			FString DisplayName = Entry.PlayerInitials.IsEmpty() ? TEXT("---") : Entry.PlayerInitials;

			if (bNeedsSpecialColor)
			{
				SetEntryText(
					i,
					FString::Printf(TEXT("#%d"), i + 1),
					DisplayName,
					FString::Printf(TEXT("%d"), Entry.Score),
					Entry.GetFormattedRunTime(),
					Entry.GetFormattedDate(),
					EntryColor
				);
			}
			else
			{
				// Ranks 4-10: Set text only, don't override color
				SetEntryTextOnly(
					i,
					FString::Printf(TEXT("#%d"), i + 1),
					DisplayName,
					FString::Printf(TEXT("%d"), Entry.Score),
					Entry.GetFormattedRunTime(),
					Entry.GetFormattedDate()
				);
			}
		}
		else
		{
			// Empty slot
			SetEntryText(i, FString::Printf(TEXT("#%d"), i + 1), TEXT("---"), TEXT("---"), TEXT("---"), TEXT("---"), EmptyColor);
		}
	}
}

void ULeaderboardWidget::SetHighlightedRank(int32 Rank)
{
	CurrentRunRank = Rank;
	// Re-populate to update colors
	PopulateLeaderboard();
}

void ULeaderboardWidget::SetEntryText(int32 Index, const FString& Rank, const FString& Name, const FString& Score, const FString& Time, const FString& Date, const FLinearColor& Color)
{
	UTextBlock* RankWidget = nullptr;
	UTextBlock* NameWidget = nullptr;
	UTextBlock* ScoreWidget = nullptr;
	UTextBlock* TimeWidget = nullptr;
	UTextBlock* DateWidget = nullptr;

	GetEntryWidgets(Index, RankWidget, NameWidget, ScoreWidget, TimeWidget, DateWidget);

	FSlateColor SlateColor(Color);

	if (RankWidget)
	{
		RankWidget->SetText(FText::FromString(Rank));
		RankWidget->SetColorAndOpacity(SlateColor);
	}
	if (NameWidget)
	{
		NameWidget->SetText(FText::FromString(Name));
		NameWidget->SetColorAndOpacity(SlateColor);
	}
	if (ScoreWidget)
	{
		ScoreWidget->SetText(FText::FromString(Score));
		ScoreWidget->SetColorAndOpacity(SlateColor);
	}
	if (TimeWidget)
	{
		TimeWidget->SetText(FText::FromString(Time));
		TimeWidget->SetColorAndOpacity(SlateColor);
	}
	if (DateWidget)
	{
		DateWidget->SetText(FText::FromString(Date));
		DateWidget->SetColorAndOpacity(SlateColor);
	}
}

void ULeaderboardWidget::SetEntryTextOnly(int32 Index, const FString& Rank, const FString& Name, const FString& Score, const FString& Time, const FString& Date)
{
	// Sets text WITHOUT modifying color - preserves Blueprint default colors for ranks 4-10
	UTextBlock* RankWidget = nullptr;
	UTextBlock* NameWidget = nullptr;
	UTextBlock* ScoreWidget = nullptr;
	UTextBlock* TimeWidget = nullptr;
	UTextBlock* DateWidget = nullptr;

	GetEntryWidgets(Index, RankWidget, NameWidget, ScoreWidget, TimeWidget, DateWidget);

	if (RankWidget)
	{
		RankWidget->SetText(FText::FromString(Rank));
	}
	if (NameWidget)
	{
		NameWidget->SetText(FText::FromString(Name));
	}
	if (ScoreWidget)
	{
		ScoreWidget->SetText(FText::FromString(Score));
	}
	if (TimeWidget)
	{
		TimeWidget->SetText(FText::FromString(Time));
	}
	if (DateWidget)
	{
		DateWidget->SetText(FText::FromString(Date));
	}
}

void ULeaderboardWidget::GetEntryWidgets(int32 Index, UTextBlock*& OutRank, UTextBlock*& OutName, UTextBlock*& OutScore, UTextBlock*& OutTime, UTextBlock*& OutDate)
{
	switch (Index)
	{
	case 0:
		OutRank = Rank1Text; OutName = Name1Text; OutScore = Score1Text; OutTime = Time1Text; OutDate = Date1Text;
		break;
	case 1:
		OutRank = Rank2Text; OutName = Name2Text; OutScore = Score2Text; OutTime = Time2Text; OutDate = Date2Text;
		break;
	case 2:
		OutRank = Rank3Text; OutName = Name3Text; OutScore = Score3Text; OutTime = Time3Text; OutDate = Date3Text;
		break;
	case 3:
		OutRank = Rank4Text; OutName = Name4Text; OutScore = Score4Text; OutTime = Time4Text; OutDate = Date4Text;
		break;
	case 4:
		OutRank = Rank5Text; OutName = Name5Text; OutScore = Score5Text; OutTime = Time5Text; OutDate = Date5Text;
		break;
	case 5:
		OutRank = Rank6Text; OutName = Name6Text; OutScore = Score6Text; OutTime = Time6Text; OutDate = Date6Text;
		break;
	case 6:
		OutRank = Rank7Text; OutName = Name7Text; OutScore = Score7Text; OutTime = Time7Text; OutDate = Date7Text;
		break;
	case 7:
		OutRank = Rank8Text; OutName = Name8Text; OutScore = Score8Text; OutTime = Time8Text; OutDate = Date8Text;
		break;
	case 8:
		OutRank = Rank9Text; OutName = Name9Text; OutScore = Score9Text; OutTime = Time9Text; OutDate = Date9Text;
		break;
	case 9:
		OutRank = Rank10Text; OutName = Name10Text; OutScore = Score10Text; OutTime = Time10Text; OutDate = Date10Text;
		break;
	default:
		OutRank = nullptr; OutName = nullptr; OutScore = nullptr; OutTime = nullptr; OutDate = nullptr;
		break;
	}
}

//=============================================================================
// ACTION FUNCTIONS
//=============================================================================

void ULeaderboardWidget::GoBackToGameOver()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("LeaderboardWidget: Going back to Game Over"));

	// Notify Blueprint
	OnBeforeGoBack();

	// Broadcast back pressed event (GameOverWidget will handle it)
	OnBackPressed.Broadcast();

	// Remove self from viewport
	RemoveFromParent();
}

void ULeaderboardWidget::PlayAgain()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("LeaderboardWidget: Play Again - Reloading level: %s"), *GameplayLevelName.ToString());

	// Notify Blueprint
	OnBeforePlayAgain();

	// Unpause (game is paused during Game Over)
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetPause(false);
		}
	}

	// Reload gameplay level
	if (!GameplayLevelName.IsNone())
	{
		RemoveFromParent();
		UGameplayStatics::OpenLevel(this, GameplayLevelName);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("LeaderboardWidget: GameplayLevelName not set!"));
	}
}

void ULeaderboardWidget::QuitToMainMenu()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("LeaderboardWidget: Quitting to main menu"));

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
		RemoveFromParent();
		UGameplayStatics::OpenLevel(this, MainMenuLevelName);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("LeaderboardWidget: MainMenuLevelName not set!"));
	}
}

//=============================================================================
// BUTTON HANDLERS
//=============================================================================

void ULeaderboardWidget::OnBackClicked()
{
	GoBackToGameOver();
}

void ULeaderboardWidget::OnPlayAgainClicked()
{
	PlayAgain();
}

void ULeaderboardWidget::OnQuitToMenuClicked()
{
	QuitToMainMenu();
}

//=============================================================================
// OVERRIDES
//=============================================================================

void ULeaderboardWidget::OnItemSelected_Implementation(int32 ItemIndex)
{
	// Button clicks are handled by OnClicked delegates
}

void ULeaderboardWidget::OnBackAction_Implementation()
{
	// Escape goes back to Game Over
	GoBackToGameOver();
}
