#pragma once

#include "CoreMinimal.h"
#include "ArcadeMenuWidget.h"
#include "ScoreSystemComponent.h"
#include "LeaderboardWidget.generated.h"

class UTextBlock;
class UButton;
class UScoreSystemComponent;

/**
 * Leaderboard Widget for StateRunner Arcade
 * 
 * Displays top 10 scores with rank, score, time, and date.
 * Can be opened from the Game Over screen.
 * 
 * DISPLAYS:
 * - 10 leaderboard entries (Rank, Score, Time, Date)
 * - Highlights the current run's rank if it qualified
 * 
 * BUTTONS (keyboard navigable):
 * - Back: Returns to Game Over screen
 * - Play Again: Restarts the game
 * - Quit to Menu: Returns to main menu
 * 
 * ARCADE NAVIGATION:
 * - Arrow Up/Down: Navigate between buttons
 * - Enter: Select focused button
 * - Escape: Go back to Game Over screen
 */
UCLASS(Abstract, Blueprintable)
class STATERUNNER_ARCADE_API ULeaderboardWidget : public UArcadeMenuWidget
{
	GENERATED_BODY()

public:

	ULeaderboardWidget(const FObjectInitializer& ObjectInitializer);

protected:

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//=============================================================================
	// TITLE
	//=============================================================================

protected:

	/** Leaderboard title text */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Title")
	TObjectPtr<UTextBlock> TitleText;

	//=============================================================================
	// LEADERBOARD ENTRY WIDGETS
	// 10 rows, each with Rank, Name (initials), Score, Time, Date
	//=============================================================================

protected:

	// Entry 1
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Rank1Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Name1Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Score1Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Time1Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Date1Text;

	// Entry 2
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Rank2Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Name2Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Score2Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Time2Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Date2Text;

	// Entry 3
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Rank3Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Name3Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Score3Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Time3Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Date3Text;

	// Entry 4
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Rank4Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Name4Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Score4Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Time4Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Date4Text;

	// Entry 5
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Rank5Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Name5Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Score5Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Time5Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Date5Text;

	// Entry 6
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Rank6Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Name6Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Score6Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Time6Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Date6Text;

	// Entry 7
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Rank7Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Name7Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Score7Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Time7Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Date7Text;

	// Entry 8
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Rank8Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Name8Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Score8Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Time8Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Date8Text;

	// Entry 9
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Rank9Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Name9Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Score9Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Time9Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Date9Text;

	// Entry 10
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Rank10Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Name10Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Score10Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Time10Text;
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Entries")
	TObjectPtr<UTextBlock> Date10Text;

	//=============================================================================
	// BUTTON WIDGETS
	//=============================================================================

protected:

	/** Back button - returns to Game Over screen */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidget), Category="Buttons")
	TObjectPtr<UButton> BackToGameOverButton;

	/** Play Again button - restarts the game */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Buttons")
	TObjectPtr<UButton> PlayAgainButton;

	/** Quit to Menu button */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Buttons")
	TObjectPtr<UButton> QuitToMenuButton;

	//=============================================================================
	// OPTIONAL LABELS
	//=============================================================================

protected:

	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> BackToGameOverLabel;

	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> PlayAgainLabel;

	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> QuitToMenuLabel;

	//=============================================================================
	// MENU ITEM INDICES
	//=============================================================================

public:

	static const int32 INDEX_BACK = 0;
	static const int32 INDEX_PLAY_AGAIN = 1;
	static const int32 INDEX_QUIT_TO_MENU = 2;

	//=============================================================================
	// CONFIGURATION
	//=============================================================================

protected:

	/** Main menu level name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
	FName MainMenuLevelName = TEXT("SR_MainMenu");

	/** Gameplay level name for restart */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration")
	FName GameplayLevelName = TEXT("SR_OfficialTrack");

	/** Color for #1 rank (Gold) - warm orange-gold */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Colors")
	FLinearColor GoldColor = FLinearColor(1.0f, 0.7f, 0.0f, 1.0f);

	/** Color for #2 rank (Silver) - cool blue-white silver */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Colors")
	FLinearColor SilverColor = FLinearColor(0.7f, 0.8f, 1.0f, 1.0f);

	/** Color for #3 rank (Bronze) - distinct orange-brown bronze */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Colors")
	FLinearColor BronzeColor = FLinearColor(0.8f, 0.4f, 0.1f, 1.0f);

	/** Color for highlighted entry (current run's rank) - bright green */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Colors")
	FLinearColor HighlightColor = FLinearColor(0.2f, 1.0f, 0.2f, 1.0f);

	/** Color for normal entries (ranks 4-10) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Colors")
	FLinearColor NormalColor = FLinearColor::White;

	/** Color for empty entries */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Configuration|Colors")
	FLinearColor EmptyColor = FLinearColor(0.5f, 0.5f, 0.5f, 0.5f); // Gray

public:

	/**
	 * View-only mode hides Play Again and Quit to Menu buttons.
	 * Used when opened from main menu (only Back button shows).
	 * Set this BEFORE adding widget to viewport.
	 */
	UPROPERTY(BlueprintReadWrite, Category="Configuration")
	bool bViewOnlyMode = false;

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

	/** Rank achieved by current run (0 = didn't qualify, 1-10 = rank) */
	UPROPERTY(BlueprintReadOnly, Category="State")
	int32 CurrentRunRank = 0;

	//=============================================================================
	// PUBLIC FUNCTIONS
	//=============================================================================

public:

	/**
	 * Populate the leaderboard display with data from ScoreSystemComponent.
	 * Automatically called on construct, but can be called manually to refresh.
	 */
	UFUNCTION(BlueprintCallable, Category="Leaderboard")
	void PopulateLeaderboard();

	/**
	 * Set the rank to highlight (current run's placement).
	 * Call this before showing the widget if you want to highlight the player's entry.
	 * 
	 * @param Rank Rank to highlight (1-10), or 0 for no highlight
	 */
	UFUNCTION(BlueprintCallable, Category="Leaderboard")
	void SetHighlightedRank(int32 Rank);

	/**
	 * Go back to the Game Over screen.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void GoBackToGameOver();

	/**
	 * Restart the game.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void PlayAgain();

	/**
	 * Quit to main menu.
	 */
	UFUNCTION(BlueprintCallable, Category="Actions")
	void QuitToMainMenu();

	//=============================================================================
	// BLUEPRINT EVENTS
	//=============================================================================

public:

	/** Called before going back to Game Over */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnBeforeGoBack();

	/** Called before restarting */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnBeforePlayAgain();

	/** Called before quitting to menu */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnBeforeQuitToMenu();

protected:

	//=============================================================================
	// BUTTON HANDLERS
	//=============================================================================

	UFUNCTION()
	void OnBackClicked();

	UFUNCTION()
	void OnPlayAgainClicked();

	UFUNCTION()
	void OnQuitToMenuClicked();

	//=============================================================================
	// INTERNAL FUNCTIONS
	//=============================================================================

	/** Cache component references */
	void CacheComponentReferences();

	/** Set entry text for a specific row with color override */
	void SetEntryText(int32 Index, const FString& Rank, const FString& Name, const FString& Score, const FString& Time, const FString& Date, const FLinearColor& Color);

	/** Set entry text for a specific row WITHOUT changing color (preserves Blueprint default) */
	void SetEntryTextOnly(int32 Index, const FString& Rank, const FString& Name, const FString& Score, const FString& Time, const FString& Date);

	/** Get text widgets for a specific entry index (0-9) */
	void GetEntryWidgets(int32 Index, UTextBlock*& OutRank, UTextBlock*& OutName, UTextBlock*& OutScore, UTextBlock*& OutTime, UTextBlock*& OutDate);

	//=============================================================================
	// OVERRIDES
	//=============================================================================

	virtual void OnItemSelected_Implementation(int32 ItemIndex) override;
	virtual void OnBackAction_Implementation() override;
};
