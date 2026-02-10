#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/SaveGame.h"
#include "ScoreSystemComponent.generated.h"

/**
 * Simple save game class for high score persistence.
 */
UCLASS()
class STATERUNNER_ARCADE_API UHighScoreSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 HighScore = 0;
};

/**
 * Single leaderboard entry with score, run time, date, and player initials.
 */
USTRUCT(BlueprintType)
struct FLeaderboardEntry
{
	GENERATED_BODY()

	/** The score achieved */
	UPROPERTY(BlueprintReadOnly, Category="Leaderboard")
	int32 Score = 0;

	/** Run duration in seconds */
	UPROPERTY(BlueprintReadOnly, Category="Leaderboard")
	float RunTimeSeconds = 0.0f;

	/** Date and time when the score was achieved */
	UPROPERTY(BlueprintReadOnly, Category="Leaderboard")
	FDateTime DateAchieved;

	/** Player initials (3 characters max, e.g., "AAA") */
	UPROPERTY(BlueprintReadOnly, Category="Leaderboard")
	FString PlayerInitials = TEXT("---");

	/** Default constructor */
	FLeaderboardEntry()
		: Score(0)
		, RunTimeSeconds(0.0f)
		, DateAchieved(FDateTime::Now())
		, PlayerInitials(TEXT("---"))
	{
	}

	/** Constructor with values (no initials - legacy support) */
	FLeaderboardEntry(int32 InScore, float InRunTime, FDateTime InDate)
		: Score(InScore)
		, RunTimeSeconds(InRunTime)
		, DateAchieved(InDate)
		, PlayerInitials(TEXT("---"))
	{
	}

	/** Constructor with values including initials */
	FLeaderboardEntry(int32 InScore, float InRunTime, FDateTime InDate, const FString& InInitials)
		: Score(InScore)
		, RunTimeSeconds(InRunTime)
		, DateAchieved(InDate)
		, PlayerInitials(InInitials.Left(3).ToUpper())
	{
	}

	/** Get formatted run time string (MM:SS or HH:MM:SS) */
	FString GetFormattedRunTime() const
	{
		int32 TotalSeconds = FMath::FloorToInt(RunTimeSeconds);
		int32 Hours = TotalSeconds / 3600;
		int32 Minutes = (TotalSeconds % 3600) / 60;
		int32 Seconds = TotalSeconds % 60;

		if (Hours > 0)
		{
			return FString::Printf(TEXT("%d:%02d:%02d"), Hours, Minutes, Seconds);
		}
		return FString::Printf(TEXT("%d:%02d"), Minutes, Seconds);
	}

	/** Get formatted date string (MM/DD/YYYY) */
	FString GetFormattedDate() const
	{
		return FString::Printf(TEXT("%02d/%02d/%04d"), 
			DateAchieved.GetMonth(), 
			DateAchieved.GetDay(), 
			DateAchieved.GetYear());
	}

	/** Get formatted date and time string */
	FString GetFormattedDateTime() const
	{
		return FString::Printf(TEXT("%02d/%02d/%04d %02d:%02d"), 
			DateAchieved.GetMonth(), 
			DateAchieved.GetDay(), 
			DateAchieved.GetYear(),
			DateAchieved.GetHour(),
			DateAchieved.GetMinute());
	}
};

/**
 * Save game class for leaderboard persistence.
 * Stores top 10 scores with run time and date information.
 */
UCLASS()
class STATERUNNER_ARCADE_API ULeaderboardSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Array of leaderboard entries, sorted by score (highest first) */
	UPROPERTY()
	TArray<FLeaderboardEntry> Entries;

	/** Maximum number of entries to keep */
	static constexpr int32 MaxEntries = 10;

	/**
	 * Add a new entry to the leaderboard.
	 * Entries are automatically sorted and trimmed to MaxEntries.
	 * 
	 * @param NewEntry The entry to add
	 * @return The rank (1-10) if the entry made the leaderboard, 0 if it didn't qualify
	 */
	int32 AddEntry(const FLeaderboardEntry& NewEntry)
	{
		// Add the new entry
		Entries.Add(NewEntry);

		// Sort by score (highest first)
		Entries.Sort([](const FLeaderboardEntry& A, const FLeaderboardEntry& B)
		{
			return A.Score > B.Score;
		});

		// Find the rank of the new entry
		int32 Rank = 0;
		for (int32 i = 0; i < Entries.Num() && i < MaxEntries; i++)
		{
			if (Entries[i].Score == NewEntry.Score && 
				FMath::IsNearlyEqual(Entries[i].RunTimeSeconds, NewEntry.RunTimeSeconds, 0.1f))
			{
				Rank = i + 1;
				break;
			}
		}

		// Trim to max entries
		while (Entries.Num() > MaxEntries)
		{
			Entries.RemoveAt(Entries.Num() - 1);
		}

		// Return 0 if the entry didn't make the cut
		return (Rank > 0 && Rank <= MaxEntries) ? Rank : 0;
	}

	/**
	 * Check if a score would qualify for the leaderboard.
	 * 
	 * @param Score The score to check
	 * @return True if the score would make the top 10
	 */
	bool WouldQualify(int32 Score) const
	{
		if (Entries.Num() < MaxEntries)
		{
			return true;
		}
		return Score > Entries.Last().Score;
	}

	/**
	 * Get the minimum score needed to qualify for the leaderboard.
	 * 
	 * @return The minimum qualifying score, or 0 if leaderboard isn't full
	 */
	int32 GetMinimumQualifyingScore() const
	{
		if (Entries.Num() < MaxEntries)
		{
			return 0;
		}
		return Entries.Last().Score + 1;
	}
};

/**
 * Delegate broadcast when score changes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScoreChanged, int32, NewScore);

/**
 * Delegate broadcast when score rate changes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScoreRateChanged, int32, NewScoreRate);

/**
 * Delegate broadcast when high score is beaten.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHighScoreBeaten, int32, NewHighScore, int32, OldHighScore);

/**
 * Delegate broadcast when 1-Up is collected (for HUD popup).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOneUpCollected, bool, bWasAtMaxLives, int32, BonusPoints);

/**
 * Delegate broadcast when EMP is collected (for HUD popup).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEMPCollected, int32, ObstaclesDestroyed, int32, PointsPerObstacle);

/**
 * Delegate broadcast when Magnet is collected (for HUD popup).
 * Magnet awards no points — this is purely for the popup notification.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMagnetCollected);

/**
 * Delegate broadcast when INSANE! combo is achieved (10 data packets in 2 seconds).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInsaneCombo, int32, BonusPoints);

/**
 * Delegate broadcast when NICE! combo is achieved (6 data packets in 2 seconds).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNiceCombo, int32, BonusPoints);

/**
 * Handles score tracking, time-based accumulation, combos, and high score persistence.
 * Score rate ramps up over time, and pickups/combos award bonus points.
 * High scores are saved locally via USaveGame.
 */
UCLASS(ClassGroup=(StateRunner), meta=(BlueprintSpawnableComponent))
class STATERUNNER_ARCADE_API UScoreSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UScoreSystemComponent();

protected:

	virtual void BeginPlay() override;

	// --- Score Configuration ---

protected:

	/**
	 * Base score rate (points per second).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Score Config", meta=(ClampMin="50", ClampMax="500"))
	int32 BaseScoreRate = 125;

	/** Score rate increase per step (points per second) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Score Config", meta=(ClampMin="1", ClampMax="25"))
	int32 ScoreRateIncrease = 8;

	/** Time between score rate increases (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Score Config", meta=(ClampMin="1.0", ClampMax="15.0"))
	float ScoreRateStepInterval = 4.0f;

	/**
	 * Score accumulation interval (seconds).
	 * Score is added this often for smooth display.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Score Config", meta=(ClampMin="0.05", ClampMax="1.0"))
	float ScoreAccumulationInterval = 0.1f;

	/**
	 * Points awarded per pickup collected.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Score Config", meta=(ClampMin="100", ClampMax="2000"))
	int32 PickupScoreValue = 500;

	/** Bonus points when collecting 1-Up at max lives (risk/reward) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Score Config", meta=(ClampMin="1000", ClampMax="10000"))
	int32 OneUpBonusValue = 7500;

	/** Points per obstacle destroyed by EMP */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Score Config", meta=(ClampMin="100", ClampMax="2500"))
	int32 EMPScorePerObstacle = 1000;

	/**
	 * OVERCLOCK bonus points per second.
	 * This is ADDED to base score rate while OVERCLOCK is active.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Score Config|OVERCLOCK", meta=(ClampMin="50", ClampMax="2000"))
	int32 OverclockBonusRate = 300;

	// --- Combo Configuration ---

	/**
	 * Number of data packets required within time window for NICE! combo.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Score Config|Combo", meta=(ClampMin="3", ClampMax="10"))
	int32 NiceComboRequiredPickups = 6;

	/**
	 * Bonus points awarded for NICE! combo (6+ pickups in time window).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Score Config|Combo", meta=(ClampMin="500", ClampMax="10000"))
	int32 NiceComboBonusValue = 3000;

	/**
	 * Number of data packets required within time window for INSANE! combo.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Score Config|Combo", meta=(ClampMin="5", ClampMax="20"))
	int32 InsaneComboRequiredPickups = 10;

	/** Time window for NICE combo - collect 6+ data packets within this */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Score Config|Combo", meta=(ClampMin="0.5", ClampMax="3.0"))
	float NiceComboTimeWindow = 1.5f;

	/**
	 * Time window (seconds) for INSANE! combo (10+ pickups).
	 * Player must collect 10+ data packets within this window for INSANE combo.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Score Config|Combo", meta=(ClampMin="1.0", ClampMax="5.0"))
	float InsaneComboTimeWindow = 2.0f;

	/**
	 * Bonus points awarded for INSANE! combo (10+ pickups in time window).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Score Config|Combo", meta=(ClampMin="1000", ClampMax="20000"))
	int32 InsaneComboBonusValue = 6000;

	// --- Pickup Pitch Scaling ---

	/**
	 * Minimum pitch multiplier (used when streak count is 1 or 0).
	 * Slightly below normal pitch for a subtle "starting low" feel.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Score Config|Pickup Pitch", meta=(ClampMin="0.5", ClampMax="1.5"))
	float PickupPitchMin = 0.95f;

	/**
	 * Maximum pitch multiplier (cap for long streaks, hit around INSANE territory).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Score Config|Pickup Pitch", meta=(ClampMin="0.5", ClampMax="2.0"))
	float PickupPitchMax = 1.20f;

	/**
	 * Pitch increase per successive pickup in a streak.
	 * Pitch = Clamp(Min + (StreakCount - 1) * Step, Min, Max)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Score Config|Pickup Pitch", meta=(ClampMin="0.005", ClampMax="0.1"))
	float PickupPitchStepPerPickup = 0.028f;

	// --- Runtime State ---

protected:

	/**
	 * Current score.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Score")
	int32 CurrentScore = 0;

	/**
	 * Current score rate (points per second).
	 */
	UPROPERTY(BlueprintReadOnly, Category="Score")
	int32 CurrentScoreRate = 0;

	/**
	 * High score (loaded from save).
	 */
	UPROPERTY(BlueprintReadOnly, Category="Score")
	int32 HighScore = 0;

	/**
	 * Time elapsed since scoring started.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Score")
	float TimeElapsed = 0.0f;

	/**
	 * Whether scoring is currently active.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Score")
	bool bIsScoringActive = false;

	/**
	 * Whether OVERCLOCK bonus is active.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Score")
	bool bIsOverclockActive = false;

	/**
	 * Whether high score was beaten this session (prevents log spam).
	 */
	bool bHasBeatenHighScoreThisSession = false;

	/**
	 * Whether this session resulted in a new high score (set at game end).
	 * This is set by CheckAndSaveHighScore() and persists until reset.
	 */
	bool bWasNewHighScoreThisSession = false;

	/**
	 * The high score value when this session started (for comparison).
	 */
	int32 SessionStartHighScore = 0;

	/**
	 * Timer handles.
	 */
	FTimerHandle ScoreAccumulationTimer;
	FTimerHandle ScoreRateUpdateTimer;

	/**
	 * Fractional score accumulator (for smooth scoring).
	 */
	float FractionalScore = 0.0f;

	/**
	 * Count of Data Packets collected this session.
	 */
	int32 DataPacketsCollected = 0;

	/**
	 * Count of 1-Ups collected this session.
	 */
	int32 OneUpsCollected = 0;

	/**
	 * Timestamps of recent data packet collections for combo tracking.
	 * Only stores the most recent N timestamps within the time window.
	 */
	TArray<float> RecentDataPacketTimestamps;

	/**
	 * Whether NICE! combo was already awarded for current combo streak.
	 * Reset when timestamps are cleared (after INSANE! or time window expires).
	 */
	bool bNiceComboAwarded = false;

	// --- Combo Popup Delay ---

	/**
	 * Delay (seconds) before showing NICE combo popup.
	 * Allows time to see if player will achieve INSANE combo instead.
	 * If INSANE is achieved during this delay, only INSANE popup shows.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Score Config|Combo")
	float ComboPopupDelaySeconds = 1.0f;

	/** Timer handle for delayed NICE combo popup */
	FTimerHandle NiceComboDelayTimer;

	/** Bonus value stored when NICE combo is pending (in case we need to show it after delay) */
	int32 PendingNiceComboBonusValue = 0;

	/** Whether a NICE combo popup is pending (waiting to see if INSANE will be achieved) */
	bool bNiceComboDelayPending = false;

	// --- Debug Configuration ---

public:

	/**
	 * DEBUG: Initial score when scoring starts. 0 = normal start.
	 * Set to 1000000 to simulate a late-game score state.
	 * 
	 * Can be set directly on this component, or automatically via the GameMode's
	 * bDebugEndgameBlockageTest master checkbox.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Debug")
	int32 DebugInitialScore = 0;

	/**
	 * DEBUG: Initial time elapsed when scoring starts. 0 = normal start.
	 * Affects score rate calculation (rate = BaseRate + floor(Time/StepInterval) * Increase).
	 * 
	 * At 936 seconds: rate = 125 + floor(936/4)*8 = 125 + 1872 = 1997 pts/s.
	 * This approximates the score rate a player would have at ~1,000,000 score.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Debug")
	float DebugInitialTimeElapsed = 0.0f;

	// --- Events ---

public:

	/** Broadcast when score changes */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnScoreChanged OnScoreChanged;

	/** Broadcast when score rate changes */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnScoreRateChanged OnScoreRateChanged;

	/** Broadcast when high score is beaten */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnHighScoreBeaten OnHighScoreBeaten;

	/** Broadcast when 1-Up is collected (for HUD popup) */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnOneUpCollected OnOneUpCollected;

	/** Broadcast when EMP is collected (for HUD popup) */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnEMPCollected OnEMPCollected;

	/** Broadcast when Magnet is collected (for HUD popup, no points awarded) */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnMagnetCollected OnMagnetCollected;

	/** Broadcast when NICE! combo is achieved */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnNiceCombo OnNiceCombo;

	/** Broadcast when INSANE! combo is achieved */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnInsaneCombo OnInsaneCombo;

	// --- Public Functions ---

public:

	/**
	 * Start scoring (call when gameplay begins).
	 */
	UFUNCTION(BlueprintCallable, Category="Score")
	void StartScoring();

	/**
	 * Stop scoring (call when game ends).
	 */
	UFUNCTION(BlueprintCallable, Category="Score")
	void StopScoring();

	/**
	 * Add bonus score (pickup collection, etc.).
	 * 
	 * @param Points Points to add
	 */
	UFUNCTION(BlueprintCallable, Category="Score")
	void AddScore(int32 Points);

	/**
	 * Add pickup bonus score.
	 */
	UFUNCTION(BlueprintCallable, Category="Score")
	void AddPickupBonus();

	/**
	 * Add 1-Up bonus score (when collected at max lives).
	 * Awards OneUpBonusValue (5000 points by default).
	 * Also broadcasts OnOneUpCollected for HUD popup.
	 * 
	 * @param bWasAtMaxLives True if player was at max lives (bonus instead of life)
	 */
	UFUNCTION(BlueprintCallable, Category="Score")
	void Add1UpBonus(bool bWasAtMaxLives = true);

	/**
	 * Notify the score system that an EMP was collected.
	 * Awards points per obstacle destroyed and broadcasts OnEMPCollected for HUD popup.
	 * 
	 * @param ObstaclesDestroyed Number of obstacles destroyed by the EMP
	 */
	UFUNCTION(BlueprintCallable, Category="Score")
	void AddEMPBonus(int32 ObstaclesDestroyed);

	/**
	 * Notify the score system that a Magnet was collected.
	 * Awards NO points — purely broadcasts OnMagnetCollected for HUD popup.
	 */
	UFUNCTION(BlueprintCallable, Category="Score")
	void NotifyMagnetCollected();

	/**
	 * Get the current pickup pitch multiplier based on the active streak.
	 * 
	 * Counts data packets collected within NiceComboTimeWindow and maps
	 * the streak count to a pitch value between PickupPitchMin and PickupPitchMax.
	 * 
	 * Call this after AddPickupBonus() so the current pickup's
	 * timestamp is included in the streak count.
	 * 
	 * @return Pitch multiplier (0.95 baseline → 1.20 at max streak)
	 */
	UFUNCTION(BlueprintPure, Category="Score|Pickup Pitch")
	float GetCurrentPickupPitchMultiplier() const;

	/**
	 * Get current score.
	 */
	UFUNCTION(BlueprintPure, Category="Score")
	int32 GetCurrentScore() const { return CurrentScore; }

	/**
	 * Get current score rate.
	 */
	UFUNCTION(BlueprintPure, Category="Score")
	int32 GetCurrentScoreRate() const { return CurrentScoreRate; }

	/**
	 * Get high score.
	 */
	UFUNCTION(BlueprintPure, Category="Score")
	int32 GetHighScore() const { return HighScore; }

	/**
	 * Check if current score beats high score.
	 * During gameplay, compares current score to high score.
	 * After game end (CheckAndSaveHighScore called), returns the persisted result.
	 */
	UFUNCTION(BlueprintPure, Category="Score")
	bool IsNewHighScore() const { return bWasNewHighScoreThisSession || CurrentScore > HighScore; }

	/**
	 * Reset score for new game.
	 */
	UFUNCTION(BlueprintCallable, Category="Score")
	void ResetScore();

	/**
	 * Check and save high score if beaten.
	 * Call at game end.
	 * 
	 * @return True if new high score was saved
	 */
	UFUNCTION(BlueprintCallable, Category="Score")
	bool CheckAndSaveHighScore();

	// --- Leaderboard Functions ---

	/**
	 * Submit the current run to the leaderboard (without initials).
	 * Should be called at game end (after CheckAndSaveHighScore).
	 * 
	 * @return The rank achieved (1-10), or 0 if didn't qualify for leaderboard
	 */
	UFUNCTION(BlueprintCallable, Category="Leaderboard")
	int32 SubmitToLeaderboard();

	/**
	 * Submit the current run to the leaderboard with player initials.
	 * Should be called after player enters their initials on game over screen.
	 * 
	 * @param Initials Player's 3-letter initials (will be uppercased and truncated to 3 chars)
	 * @return The rank achieved (1-10), or 0 if didn't qualify for leaderboard
	 */
	UFUNCTION(BlueprintCallable, Category="Leaderboard")
	int32 SubmitToLeaderboardWithInitials(const FString& Initials);

	/**
	 * Get the current leaderboard entries.
	 * 
	 * @return Array of leaderboard entries (sorted by score, highest first)
	 */
	UFUNCTION(BlueprintCallable, Category="Leaderboard")
	TArray<FLeaderboardEntry> GetLeaderboard() const;

	/**
	 * Check if the current score would qualify for the leaderboard.
	 * Can be called during gameplay to show "NEW RECORD!" etc.
	 * 
	 * @return True if current score would make top 10
	 */
	UFUNCTION(BlueprintPure, Category="Leaderboard")
	bool WouldQualifyForLeaderboard() const;

	/**
	 * Get the rank the current score would achieve on the leaderboard.
	 * 
	 * @return Rank (1-10) or 0 if wouldn't qualify
	 */
	UFUNCTION(BlueprintPure, Category="Leaderboard")
	int32 GetCurrentLeaderboardRank() const;

	/**
	 * Get the minimum score needed to make the leaderboard.
	 * 
	 * @return Minimum qualifying score, or 0 if leaderboard isn't full
	 */
	UFUNCTION(BlueprintPure, Category="Leaderboard")
	int32 GetMinimumLeaderboardScore() const;

	/**
	 * Clear all leaderboard entries (for testing/reset).
	 */
	UFUNCTION(BlueprintCallable, Category="Leaderboard")
	void ClearLeaderboard();

	/**
	 * Set OVERCLOCK bonus active state.
	 * 
	 * @param bActive Whether OVERCLOCK is active
	 */
	UFUNCTION(BlueprintCallable, Category="Score|OVERCLOCK")
	void SetOverclockActive(bool bActive);

	/**
	 * Check if OVERCLOCK bonus is active.
	 */
	UFUNCTION(BlueprintPure, Category="Score|OVERCLOCK")
	bool IsOverclockActive() const { return bIsOverclockActive; }

	// --- Protected Functions ---

protected:

	/** Called by timer to accumulate score */
	UFUNCTION()
	void AccumulateScore();

	/** Called by timer to update score rate */
	UFUNCTION()
	void UpdateScoreRate();

	/** Called when NICE combo delay expires - shows popup if INSANE wasn't achieved */
	UFUNCTION()
	void OnNiceComboDelayExpired();

	/** Load high score from save */
	void LoadHighScore();

	/** Save high score to save */
	void SaveHighScore();

	/** Load leaderboard from save */
	void LoadLeaderboard();

	/** Save leaderboard to save */
	void SaveLeaderboard();

	/** Calculate current score rate based on time elapsed */
	int32 CalculateScoreRate() const;

	// --- Leaderboard State ---

protected:

	/** Cached leaderboard data (loaded on BeginPlay) */
	UPROPERTY()
	TObjectPtr<ULeaderboardSaveGame> CachedLeaderboard;

	/** Rank achieved this run (set after SubmitToLeaderboard) */
	int32 LeaderboardRankThisRun = 0;
};
