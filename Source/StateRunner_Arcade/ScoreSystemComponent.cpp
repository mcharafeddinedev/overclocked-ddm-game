#include "ScoreSystemComponent.h"
#include "GameDebugSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "StateRunner_Arcade.h"

UScoreSystemComponent::UScoreSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UScoreSystemComponent::BeginPlay()
{
	Super::BeginPlay();

	// Initialize score rate
	CurrentScoreRate = BaseScoreRate;

	// Load high score and leaderboard
	LoadHighScore();
	LoadLeaderboard();
}

// --- Public Functions ---

void UScoreSystemComponent::StartScoring()
{
	if (bIsScoringActive)
	{
		return;
	}

	bIsScoringActive = true;
	TimeElapsed = 0.0f;
	FractionalScore = 0.0f;
	CurrentScoreRate = BaseScoreRate;

	// DEBUG: Apply initial score/time overrides if set
	if (DebugInitialScore > 0)
	{
		CurrentScore = DebugInitialScore;
		OnScoreChanged.Broadcast(CurrentScore);
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("Score initialized to %d"), DebugInitialScore);
	}
	if (DebugInitialTimeElapsed > 0.0f)
	{
		TimeElapsed = DebugInitialTimeElapsed;
		CurrentScoreRate = CalculateScoreRate();
		OnScoreRateChanged.Broadcast(CurrentScoreRate);
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("Time set to %.1fs, Score rate: %d pts/s"), DebugInitialTimeElapsed, CurrentScoreRate);
	}

	// Start score accumulation timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ScoreAccumulationTimer,
			this,
			&UScoreSystemComponent::AccumulateScore,
			ScoreAccumulationInterval,
			true
		);

		// Start score rate update timer
		World->GetTimerManager().SetTimer(
			ScoreRateUpdateTimer,
			this,
			&UScoreSystemComponent::UpdateScoreRate,
			ScoreRateStepInterval,
			true
		);
	}

}

void UScoreSystemComponent::StopScoring()
{
	if (!bIsScoringActive)
	{
		return;
	}

	bIsScoringActive = false;

	// Stop timers
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ScoreAccumulationTimer);
		World->GetTimerManager().ClearTimer(ScoreRateUpdateTimer);
	}
}

void UScoreSystemComponent::AddScore(int32 Points)
{
	if (Points <= 0)
	{
		return;
	}

	int32 OldScore = CurrentScore;
	CurrentScore += Points;

	OnScoreChanged.Broadcast(CurrentScore);

	// Check for high score
	if (!bHasBeatenHighScoreThisSession && CurrentScore > SessionStartHighScore && SessionStartHighScore > 0)
	{
		bHasBeatenHighScoreThisSession = true;
		OnHighScoreBeaten.Broadcast(CurrentScore, SessionStartHighScore);
		
		if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
		{
			Debug->LogEvent(EDebugCategory::Score, TEXT("NEW HIGH SCORE!"));
		}
	}

	// Update in-memory high score
	if (CurrentScore > HighScore)
	{
		HighScore = CurrentScore;
	}

	// Update debug subsystem stats
	if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
	{
		Debug->Stat_Score = CurrentScore;
	}
}

void UScoreSystemComponent::AddPickupBonus()
{
	AddScore(PickupScoreValue);
	DataPacketsCollected++;
	
	// Combo Tracking: Record timestamp and check for combos
	// NICE (6x) and INSANE (10x) now have SEPARATE time windows
	float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	
	// Add this pickup's timestamp
	RecentDataPacketTimestamps.Add(CurrentTime);
	
	// For INSANE combo: Remove timestamps outside the INSANE time window (longer)
	float InsaneWindowStart = CurrentTime - InsaneComboTimeWindow;
	RecentDataPacketTimestamps.RemoveAll([InsaneWindowStart](float Timestamp)
	{
		return Timestamp < InsaneWindowStart;
	});
	
	// Count pickups in each window separately
	int32 InsaneWindowCount = RecentDataPacketTimestamps.Num();
	
	// For NICE combo: Count only pickups within the NICE time window (shorter)
	float NiceWindowStart = CurrentTime - NiceComboTimeWindow;
	int32 NiceWindowCount = 0;
	for (float Timestamp : RecentDataPacketTimestamps)
	{
		if (Timestamp >= NiceWindowStart)
		{
			NiceWindowCount++;
		}
	}
	
	// Reset NICE combo flag if no pickups qualify for NICE window anymore
	if (NiceWindowCount < NiceComboRequiredPickups)
	{
		bNiceComboAwarded = false;
	}
	
	// Check for INSANE! combo (10x) - highest priority, uses longer window
	if (InsaneWindowCount >= InsaneComboRequiredPickups)
	{
		// INSANE! Combo achieved!
		// Cancel any pending NICE combo popup - INSANE takes priority
		if (bNiceComboDelayPending)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(NiceComboDelayTimer);
			}
			bNiceComboDelayPending = false;
			PendingNiceComboBonusValue = 0;
		}
		
		AddScore(InsaneComboBonusValue);
		OnInsaneCombo.Broadcast(InsaneComboBonusValue);
		
		// Clear the timestamps to require a fresh combo
		RecentDataPacketTimestamps.Empty();
		bNiceComboAwarded = false;
		
		if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
		{
			Debug->LogEvent(EDebugCategory::Score, FString::Printf(TEXT("INSANE 10x COMBO! +%d"), InsaneComboBonusValue));
		}
	}
	// Check for NICE! combo (6x) - uses shorter window, only if not already awarded this streak
	else if (NiceWindowCount >= NiceComboRequiredPickups && !bNiceComboAwarded)
	{
		// NICE! Combo achieved - but delay the popup to see if INSANE is coming
		// Award the score immediately (so player gets the points)
		AddScore(NiceComboBonusValue);
		bNiceComboAwarded = true;  // Don't award again until streak resets
		
		// Only start delay timer if one isn't already pending
		if (!bNiceComboDelayPending)
		{
			bNiceComboDelayPending = true;
			PendingNiceComboBonusValue = NiceComboBonusValue;
			
			// Start delay timer - if INSANE isn't achieved within this time, show NICE popup
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					NiceComboDelayTimer,
					this,
					&UScoreSystemComponent::OnNiceComboDelayExpired,
					ComboPopupDelaySeconds,
					false
				);
			}
			
			if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
			{
				Debug->LogEvent(EDebugCategory::Score, FString::Printf(TEXT("NICE 6x COMBO! +%d (popup delayed %.1fs)"), NiceComboBonusValue, ComboPopupDelaySeconds));
			}
		}
	}
}

void UScoreSystemComponent::OnNiceComboDelayExpired()
{
	// Delay expired without achieving INSANE combo - show the NICE popup now
	if (bNiceComboDelayPending)
	{
		OnNiceCombo.Broadcast(PendingNiceComboBonusValue);
		
		if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
		{
			Debug->LogEvent(EDebugCategory::Score, FString::Printf(TEXT("NICE 6x COMBO popup shown (delay expired)")));
		}
		
		bNiceComboDelayPending = false;
		PendingNiceComboBonusValue = 0;
	}
}

void UScoreSystemComponent::Add1UpBonus(bool bWasAtMaxLives)
{
	if (bWasAtMaxLives)
	{
		// At max lives - award bonus points instead
		AddScore(OneUpBonusValue);
		
		if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
		{
			Debug->LogEvent(EDebugCategory::Score, FString::Printf(TEXT("+%d 1-UP Bonus!"), OneUpBonusValue));
		}
	}
	
	OneUpsCollected++;
	
	// Broadcast for HUD popup (always, even if life was gained)
	OnOneUpCollected.Broadcast(bWasAtMaxLives, OneUpBonusValue);
}

void UScoreSystemComponent::AddEMPBonus(int32 ObstaclesDestroyed)
{
	// Award points per obstacle destroyed (using dedicated EMPScorePerObstacle)
	int32 TotalBonus = ObstaclesDestroyed * EMPScorePerObstacle;
	if (TotalBonus > 0)
	{
		AddScore(TotalBonus);
	}
	
	// Broadcast for HUD popup
	OnEMPCollected.Broadcast(ObstaclesDestroyed, EMPScorePerObstacle);
	
	if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
	{
		if (ObstaclesDestroyed > 0)
		{
			Debug->LogEvent(EDebugCategory::Score, FString::Printf(TEXT("EMP! %d obstacles +%d"), ObstaclesDestroyed, TotalBonus));
		}
		else
		{
			Debug->LogEvent(EDebugCategory::Score, TEXT("EMP! +OVERCLOCK Boost"));
		}
	}
}

void UScoreSystemComponent::NotifyMagnetCollected()
{
	// Magnet awards NO points — purely a utility pickup
	// Broadcast for HUD popup
	OnMagnetCollected.Broadcast();
	
	if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
	{
		Debug->LogEvent(EDebugCategory::Score, TEXT("MAGNET collected!"));
	}
}

void UScoreSystemComponent::ResetScore()
{
	StopScoring();
	
	CurrentScore = 0;
	CurrentScoreRate = BaseScoreRate;
	TimeElapsed = 0.0f;
	FractionalScore = 0.0f;
	bIsOverclockActive = false;
	bHasBeatenHighScoreThisSession = false;
	bWasNewHighScoreThisSession = false;
	DataPacketsCollected = 0;
	OneUpsCollected = 0;
	RecentDataPacketTimestamps.Empty();  // Clear combo tracking
	bNiceComboAwarded = false;  // Reset NICE combo flag
	LeaderboardRankThisRun = 0;  // Clear leaderboard rank

	OnScoreChanged.Broadcast(CurrentScore);
	OnScoreRateChanged.Broadcast(CurrentScoreRate);
}

bool UScoreSystemComponent::CheckAndSaveHighScore()
{
	if (CurrentScore > SessionStartHighScore)
	{
		HighScore = CurrentScore;
		bWasNewHighScoreThisSession = true;
		SaveHighScore();
		return true;
	}
	else
	{
		bWasNewHighScoreThisSession = false;
	}

	return false;
}

void UScoreSystemComponent::SetOverclockActive(bool bActive)
{
	if (bIsOverclockActive != bActive)
	{
		bIsOverclockActive = bActive;
		
		if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
		{
			Debug->Stat_OverclockActive = bActive;
			if (bActive)
			{
				Debug->LogEvent(EDebugCategory::Overclock, TEXT("OVERCLOCK Bonus Active!"));
			}
		}
	}
}

// --- Pickup Pitch Scaling ---

float UScoreSystemComponent::GetCurrentPickupPitchMultiplier() const
{
	// Count pickups within the NICE combo time window for streak determination.
	// Uses RecentDataPacketTimestamps from AddPickupBonus().
	// Must be called AFTER AddPickupBonus() so the current pickup is counted.
	
	float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	float WindowStart = CurrentTime - NiceComboTimeWindow;
	
	int32 StreakCount = 0;
	for (float Timestamp : RecentDataPacketTimestamps)
	{
		if (Timestamp >= WindowStart)
		{
			StreakCount++;
		}
	}
	
	// If no pickups in window (e.g., timestamps cleared after INSANE combo), use baseline
	if (StreakCount <= 0)
	{
		return PickupPitchMin;
	}
	
	// Linear ramp: Min + (StreakCount - 1) * Step, clamped to [Min, Max]
	float Pitch = PickupPitchMin + (StreakCount - 1) * PickupPitchStepPerPickup;
	Pitch = FMath::Clamp(Pitch, PickupPitchMin, PickupPitchMax);
	
	return Pitch;
}

// --- Protected Functions ---

void UScoreSystemComponent::AccumulateScore()
{
	if (!bIsScoringActive)
	{
		return;
	}

	// Track previous time for periodic display check
	float PreviousTime = TimeElapsed;
	
	// Update time elapsed
	TimeElapsed += ScoreAccumulationInterval;

	// Calculate score to add this tick
	float ScoreToAdd = CurrentScoreRate * ScoreAccumulationInterval;

	// Add OVERCLOCK bonus if active
	if (bIsOverclockActive)
	{
		ScoreToAdd += OverclockBonusRate * ScoreAccumulationInterval;
	}

	// Accumulate fractional score
	FractionalScore += ScoreToAdd;

	// Add whole points
	int32 WholePoints = FMath::FloorToInt(FractionalScore);
	if (WholePoints > 0)
	{
		FractionalScore -= WholePoints;
		AddScore(WholePoints);
	}
	
	// Update debug subsystem with current stats (for the compact display)
	if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
	{
		Debug->Stat_Score = CurrentScore;
		Debug->UpdateDisplay();
	}
}

void UScoreSystemComponent::UpdateScoreRate()
{
	if (!bIsScoringActive)
	{
		return;
	}

	int32 NewRate = CalculateScoreRate();
	
	if (NewRate != CurrentScoreRate)
	{
		CurrentScoreRate = NewRate;
		OnScoreRateChanged.Broadcast(CurrentScoreRate);
	}
}

void UScoreSystemComponent::LoadHighScore()
{
	if (UHighScoreSaveGame* SaveGame = Cast<UHighScoreSaveGame>(
		UGameplayStatics::LoadGameFromSlot(TEXT("HighScore"), 0)))
	{
		HighScore = SaveGame->HighScore;
		SessionStartHighScore = HighScore;
	}
	else
	{
		HighScore = 0;
		SessionStartHighScore = 0;
	}
	
	bHasBeatenHighScoreThisSession = false;
}

void UScoreSystemComponent::SaveHighScore()
{
	UHighScoreSaveGame* SaveGame = Cast<UHighScoreSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UHighScoreSaveGame::StaticClass()));
	
	if (SaveGame)
	{
		SaveGame->HighScore = HighScore;
		UGameplayStatics::SaveGameToSlot(SaveGame, TEXT("HighScore"), 0);
	}
}

int32 UScoreSystemComponent::CalculateScoreRate() const
{
	// Stepped increase: +5 every 5 seconds
	// Formula: BaseRate + (floor(TimeElapsed / StepInterval) * Increase)
	int32 Steps = FMath::FloorToInt(TimeElapsed / ScoreRateStepInterval);
	return BaseScoreRate + (Steps * ScoreRateIncrease);
}

// --- Leaderboard Functions ---

int32 UScoreSystemComponent::SubmitToLeaderboard()
{
	// Create entry for this run
	FLeaderboardEntry NewEntry(CurrentScore, TimeElapsed, FDateTime::Now());
	
	// Ensure leaderboard is loaded
	if (!CachedLeaderboard)
	{
		LoadLeaderboard();
	}
	
	if (CachedLeaderboard)
	{
		// Add to leaderboard and get rank
		LeaderboardRankThisRun = CachedLeaderboard->AddEntry(NewEntry);
		
		if (LeaderboardRankThisRun > 0)
		{
			// Made the leaderboard! Save it.
			SaveLeaderboard();
			
			if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
			{
				Debug->LogEvent(EDebugCategory::Score, 
					FString::Printf(TEXT("LEADERBOARD #%d! Score: %d, Time: %s"), 
						LeaderboardRankThisRun, CurrentScore, *NewEntry.GetFormattedRunTime()));
			}
		}
	}
	
	return LeaderboardRankThisRun;
}

int32 UScoreSystemComponent::SubmitToLeaderboardWithInitials(const FString& Initials)
{
	// Create entry for this run with initials
	FLeaderboardEntry NewEntry(CurrentScore, TimeElapsed, FDateTime::Now(), Initials);
	
	// Ensure leaderboard is loaded
	if (!CachedLeaderboard)
	{
		LoadLeaderboard();
	}
	
	if (CachedLeaderboard)
	{
		// Add to leaderboard and get rank
		LeaderboardRankThisRun = CachedLeaderboard->AddEntry(NewEntry);
		
		if (LeaderboardRankThisRun > 0)
		{
			// Made the leaderboard! Save it.
			SaveLeaderboard();
			
			if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
			{
				Debug->LogEvent(EDebugCategory::Score, 
					FString::Printf(TEXT("LEADERBOARD #%d! %s - Score: %d, Time: %s"), 
						LeaderboardRankThisRun, *NewEntry.PlayerInitials, CurrentScore, *NewEntry.GetFormattedRunTime()));
			}
		}
	}
	
	return LeaderboardRankThisRun;
}

TArray<FLeaderboardEntry> UScoreSystemComponent::GetLeaderboard() const
{
	if (CachedLeaderboard)
	{
		return CachedLeaderboard->Entries;
	}
	return TArray<FLeaderboardEntry>();
}

bool UScoreSystemComponent::WouldQualifyForLeaderboard() const
{
	if (CachedLeaderboard)
	{
		return CachedLeaderboard->WouldQualify(CurrentScore);
	}
	return true; // If no leaderboard loaded, assume it would qualify
}

int32 UScoreSystemComponent::GetCurrentLeaderboardRank() const
{
	if (!CachedLeaderboard || CurrentScore <= 0)
	{
		return 0;
	}
	
	// Find where current score would rank
	int32 Rank = 1;
	for (const FLeaderboardEntry& Entry : CachedLeaderboard->Entries)
	{
		if (CurrentScore > Entry.Score)
		{
			return Rank;
		}
		Rank++;
	}
	
	// Would be at the end
	if (Rank <= ULeaderboardSaveGame::MaxEntries)
	{
		return Rank;
	}
	
	return 0; // Wouldn't qualify
}

int32 UScoreSystemComponent::GetMinimumLeaderboardScore() const
{
	if (CachedLeaderboard)
	{
		return CachedLeaderboard->GetMinimumQualifyingScore();
	}
	return 0;
}

void UScoreSystemComponent::ClearLeaderboard()
{
	if (CachedLeaderboard)
	{
		CachedLeaderboard->Entries.Empty();
		SaveLeaderboard();
		
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("Leaderboard cleared"));
	}
}

void UScoreSystemComponent::LoadLeaderboard()
{
	CachedLeaderboard = Cast<ULeaderboardSaveGame>(
		UGameplayStatics::LoadGameFromSlot(TEXT("Leaderboard"), 0));
	
	if (!CachedLeaderboard)
	{
		// Create new leaderboard if none exists
		CachedLeaderboard = Cast<ULeaderboardSaveGame>(
			UGameplayStatics::CreateSaveGameObject(ULeaderboardSaveGame::StaticClass()));
		
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("Created new leaderboard save"));
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("Loaded leaderboard with %d entries"), CachedLeaderboard->Entries.Num());
	}
}

void UScoreSystemComponent::SaveLeaderboard()
{
	if (CachedLeaderboard)
	{
		UGameplayStatics::SaveGameToSlot(CachedLeaderboard, TEXT("Leaderboard"), 0);
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("Saved leaderboard with %d entries"), CachedLeaderboard->Entries.Num());
	}
}
