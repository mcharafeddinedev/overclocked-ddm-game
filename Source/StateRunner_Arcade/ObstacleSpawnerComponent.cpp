#include "ObstacleSpawnerComponent.h"
#include "BaseObstacle.h"
#include "StateRunner_ArcadeGameMode.h"
#include "GameDebugSubsystem.h"
#include "StateRunner_Arcade.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

UObstacleSpawnerComponent::UObstacleSpawnerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	// Set up default patterns so they can be edited in the Blueprint editor
	CreateDefaultPatterns();
}

void UObstacleSpawnerComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializePools();

	// Apply blockage test settings (skip tutorial, set late-game difficulty)
	if (bDebugForce3LaneBlockage)
	{
		bTutorialComplete = true;
		bHasSpawnedTutorialObstacles = true;

		// Difficulty 15 is past all functional thresholds
		SegmentsSpawned = 105;
		CurrentDifficultyLevel = SegmentsSpawned / SegmentsPerDifficultyIncrease;

		UE_LOG(LogStateRunner_Arcade, Warning,
			TEXT("Blockage test active — Tutorial: SKIPPED, Difficulty: %d, Segments: %d"),
			CurrentDifficultyLevel, SegmentsSpawned);
	}

	// Log init summary
	int32 TotalPoolSize = LowWallPool.Num() + HighBarrierPool.Num() + FullWallPool.Num();
	if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
	{
		FString InitInfo = FString::Printf(
			TEXT("Pools: %d total (LW:%d HB:%d FW:%d)\nPatterns: %d | Tutorial: %s\nDifficulty: %d-%d obs, +%d/level"),
			TotalPoolSize, LowWallPool.Num(), HighBarrierPool.Num(), FullWallPool.Num(),
			PredefinedPatterns.Num(), bEnableTutorial ? TEXT("ON") : TEXT("OFF"),
			MinObstaclesPerSegment, MaxObstaclesPerSegment, ObstaclesPerDifficultyLevel
		);
		Debug->LogInit(TEXT("ObstacleSpawner"), InitInfo);
	}
}

// --- Public Functions ---

void UObstacleSpawnerComponent::SpawnObstaclesForSegment(float SegmentStartX, float SegmentEndX, bool bIsTutorialSegment)
{
	if (bIsTutorialSegment)
	{
		return;
	}

	// Auto-spawn tutorial obstacles on first call
	if (bEnableTutorial && !bHasSpawnedTutorialObstacles)
	{
		SpawnTutorialObstacles();
		bHasSpawnedTutorialObstacles = true;
	}

	// No procedural obstacles until tutorial is done
	if (bEnableTutorial && !bTutorialComplete)
	{
		return;
	}

	SegmentsSpawned++;
	SegmentsSinceEmpty++;
	UpdateDifficulty();

	// Check for breather segment
	bool bIsBreatherSegment = ShouldBeBreatherSegment();
	float BreatherGapOffset = 0.0f;
	
	if (bIsBreatherSegment)
	{
		SegmentsSinceEmpty = 0;
		BreatherGapOffset = GetEffectiveBreatherGapFraction();
	}

	// Generate obstacle layout
	TArray<FObstacleSpawnData> ObstacleLayout;
	
	bool bUsePattern = FMath::FRand() < PredefinedPatternChance;
	
	if (bUsePattern && GenerateFromPattern(ObstacleLayout))
	{
		// Find pattern bounds and fill gaps with extra obstacles
		float PatternMinX = 1.0f;
		float PatternMaxX = 0.0f;
		for (const FObstacleSpawnData& Data : ObstacleLayout)
		{
			PatternMinX = FMath::Min(PatternMinX, Data.RelativeXOffset);
			PatternMaxX = FMath::Max(PatternMaxX, Data.RelativeXOffset);
		}
		
		AddFillerObstacles(ObstacleLayout, PatternMinX, PatternMaxX);
	}
	else
	{
		int32 ObstacleCount = CalculateObstacleCount();
		GenerateProcedural(ObstacleLayout, ObstacleCount);
	}

	// Inject 3-lane FullWall blockage for fairness testing
	if (bDebugForce3LaneBlockage)
	{
		const float BlockageX = 0.50f;
		for (ELane Lane : { ELane::Left, ELane::Center, ELane::Right })
		{
			FObstacleSpawnData BlockData;
			BlockData.Lane = Lane;
			BlockData.ObstacleType = EObstacleType::FullWall;
			BlockData.RelativeXOffset = BlockageX;
			BlockData.ZOffset = 0.0f;
			ObstacleLayout.Add(BlockData);
		}

		UE_LOG(LogStateRunner_Arcade, Warning,
			TEXT("Injected 3-lane FullWall blockage at X=0.50 — EnsureFairLayout should remove one"));
	}

	// Make sure at least one lane is passable
	EnsureFairLayout(ObstacleLayout);

	// For breather segments, shift obstacles forward to create a gap at the start
	if (bIsBreatherSegment && BreatherGapOffset > 0.0f)
	{
		float CompressionFactor = 1.0f - BreatherGapOffset;
		for (FObstacleSpawnData& Data : ObstacleLayout)
		{
			Data.RelativeXOffset = BreatherGapOffset + (Data.RelativeXOffset * CompressionFactor);
		}
	}

	float ActualSegmentLength = SegmentEndX - SegmentStartX;

	// Spawn all obstacles
	for (const FObstacleSpawnData& SpawnData : ObstacleLayout)
	{
		ABaseObstacle* Obstacle = GetObstacleFromPool(SpawnData.ObstacleType);
		if (!Obstacle)
		{
			if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
			{
				Debug->LogError(FString::Printf(TEXT("Obstacle pool exhausted for type %d!"), (int32)SpawnData.ObstacleType));
			}
			continue;
		}

		float WorldX = SegmentStartX + (SpawnData.RelativeXOffset * ActualSegmentLength);
		float WorldY = GetLaneYPosition(SpawnData.Lane);
		float WorldZ = ObstacleSpawnZ + SpawnData.ZOffset;

		Obstacle->Activate(FVector(WorldX, WorldY, WorldZ), SpawnData.Lane, SpawnData.ObstacleType);
		ActiveObstacles.Add(Obstacle);
		OnObstacleSpawned.Broadcast(Obstacle, SpawnData);
	}

	if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
	{
		Debug->Stat_ActiveObstacles = ActiveObstacles.Num();
		Debug->Stat_SegmentsSpawned = SegmentsSpawned;
		Debug->Stat_DifficultyLevel = CurrentDifficultyLevel;
	}
}

void UObstacleSpawnerComponent::ClearAllObstacles()
{
	// Used for game reset/restart, not EMP
	for (ABaseObstacle* Obstacle : ActiveObstacles)
	{
		if (Obstacle && Obstacle->IsActive())
		{
			Obstacle->Deactivate();
			OnObstacleDeactivated.Broadcast(Obstacle);
		}
	}

	ActiveObstacles.Empty();
}

int32 UObstacleSpawnerComponent::DeactivateAllActiveObstacles()
{
	// EMP: deactivate obstacles within range of the player
	const float ClearMinX = PlayerXPosition;
	const float ClearMaxX = PlayerXPosition + EMPClearRange;
	
	int32 DeactivatedCount = 0;
	TArray<ABaseObstacle*> ObstaclesToCheck = ActiveObstacles;
	
	for (ABaseObstacle* Obstacle : ObstaclesToCheck)
	{
		if (Obstacle && Obstacle->IsActive())
		{
			const float ObstacleX = Obstacle->GetActorLocation().X;
			if (ObstacleX >= ClearMinX && ObstacleX <= ClearMaxX)
			{
				Obstacle->Deactivate();
				DeactivatedCount++;
			}
		}
	}
	
	ActiveObstacles.RemoveAll([](ABaseObstacle* Obstacle) {
		return !Obstacle || !Obstacle->IsActive();
	});
	
	if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
	{
		Debug->LogEvent(EDebugCategory::Spawning, FString::Printf(TEXT("EMP cleared %d obstacles"), DeactivatedCount));
		Debug->Stat_ActiveObstacles = ActiveObstacles.Num();
	}
	
	return DeactivatedCount;
}

void UObstacleSpawnerComponent::ResetDifficulty()
{
	CurrentDifficultyLevel = 0;
	SegmentsSpawned = 0;
	SegmentsSinceEmpty = 0;
	TutorialSegmentsSkipped = 0;
	bHasSpawnedTutorialObstacles = false;
	RecentlyUsedPatternIndices.Empty();
	LastPatternName = TEXT("");
}

TArray<FString> UObstacleSpawnerComponent::GetAllPatternNames() const
{
	TArray<FString> Names;
	Names.Reserve(PredefinedPatterns.Num());
	
	for (const FObstaclePattern& Pattern : PredefinedPatterns)
	{
		Names.Add(Pattern.PatternName);
	}
	
	return Names;
}

void UObstacleSpawnerComponent::LogAllPatterns() const
{
	// Patterns are logged via the debug subsystem, not here
}

// --- Tutorial Functions ---

void UObstacleSpawnerComponent::SpawnTutorialObstacles()
{
	if (!bEnableTutorial || bTutorialComplete)
	{
		return;
	}

	// Clear any existing tutorial obstacles
	for (ABaseObstacle* Obstacle : TutorialObstacles)
	{
		if (Obstacle && Obstacle->IsActive())
		{
			Obstacle->Deactivate();
		}
	}
	TutorialObstacles.Empty();
	ShownTutorialPrompts.Empty();

	// Push tutorial obstacles forward to leave room for the intro pickup segment
	const float IntroOffset = GetIntroSegmentOffset();

	SpawnTutorialObstacleSet(TutorialObstacle1_X + IntroOffset, ETutorialObstacleType::LaneSwitch);
	SpawnTutorialObstacleSet(TutorialObstacle2_X + IntroOffset, ETutorialObstacleType::Jump);
	SpawnTutorialObstacleSet(TutorialObstacle3_X + IntroOffset, ETutorialObstacleType::Slide);

	if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
	{
		Debug->LogEvent(EDebugCategory::Spawning, FString::Printf(
			TEXT("Tutorial obstacles spawned with intro offset: %.0f units (%.1fs at %.0f u/s)"),
			IntroOffset, IntroSegmentDuration, BaseScrollSpeed));
	}
}

void UObstacleSpawnerComponent::SpawnTutorialObstacleSet(float WorldX, ETutorialObstacleType TutorialType)
{
	switch (TutorialType)
	{
		case ETutorialObstacleType::LaneSwitch:
		{
			ABaseObstacle* Obstacle = GetObstacleFromPool(EObstacleType::FullWall);
			if (Obstacle)
			{
				Obstacle->Activate(FVector(WorldX, CenterLaneY, ObstacleSpawnZ), ELane::Center, EObstacleType::FullWall);
				TutorialObstacles.Add(Obstacle);
				ActiveObstacles.Add(Obstacle);
			}
			break;
		}

		case ETutorialObstacleType::Jump:
		{
			for (ELane Lane : { ELane::Left, ELane::Center, ELane::Right })
			{
				ABaseObstacle* Obstacle = GetObstacleFromPool(EObstacleType::LowWall);
				if (Obstacle)
				{
					Obstacle->Activate(FVector(WorldX, GetLaneYPosition(Lane), ObstacleSpawnZ), Lane, EObstacleType::LowWall);
					TutorialObstacles.Add(Obstacle);
					ActiveObstacles.Add(Obstacle);
				}
			}
			break;
		}

		case ETutorialObstacleType::Slide:
		{
			// HighBarrier in all 3 lanes - forces slide
			for (ELane Lane : { ELane::Left, ELane::Center, ELane::Right })
			{
				ABaseObstacle* Obstacle = GetObstacleFromPool(EObstacleType::HighBarrier);
				if (Obstacle)
				{
					float LaneY = GetLaneYPosition(Lane);
					FVector SpawnLocation(WorldX, LaneY, ObstacleSpawnZ);
					Obstacle->Activate(SpawnLocation, Lane, EObstacleType::HighBarrier);
					TutorialObstacles.Add(Obstacle);
					ActiveObstacles.Add(Obstacle);
				}
			}
			break;
		}
	}
}

void UObstacleSpawnerComponent::CheckTutorialPrompts(float CurrentScrollSpeed, float PlayerX)
{
	if (!bEnableTutorial || bTutorialComplete || CurrentScrollSpeed <= 0.0f)
	{
		return;
	}

	// How far ahead to look based on lead time
	float PromptDistance = CurrentScrollSpeed * TutorialPromptLeadTime;

	// Obstacle 1: Lane Switch (FullWall, center)
	if (!ShownTutorialPrompts.Contains(ETutorialObstacleType::LaneSwitch))
	{
		for (ABaseObstacle* Obstacle : TutorialObstacles)
		{
			if (Obstacle && Obstacle->IsActive())
			{
				float ObstacleX = Obstacle->GetActorLocation().X;
				if (Obstacle->GetObstacleType() == EObstacleType::FullWall && 
				    Obstacle->GetCurrentLane() == ELane::Center)
				{
					float DistanceToPlayer = ObstacleX - PlayerX;
					if (DistanceToPlayer <= PromptDistance && DistanceToPlayer > 0)
					{
						ShownTutorialPrompts.Add(ETutorialObstacleType::LaneSwitch);
						OnTutorialPrompt.Broadcast(ETutorialObstacleType::LaneSwitch);
						break;
					}
				}
			}
		}
	}

	// Obstacle 2: Jump (LowWall)
	if (!ShownTutorialPrompts.Contains(ETutorialObstacleType::Jump))
	{
		for (ABaseObstacle* Obstacle : TutorialObstacles)
		{
			if (Obstacle && Obstacle->IsActive() && 
			    Obstacle->GetObstacleType() == EObstacleType::LowWall)
			{
				float ObstacleX = Obstacle->GetActorLocation().X;
				float DistanceToPlayer = ObstacleX - PlayerX;
				if (DistanceToPlayer <= PromptDistance && DistanceToPlayer > 0)
				{
					ShownTutorialPrompts.Add(ETutorialObstacleType::Jump);
					OnTutorialPrompt.Broadcast(ETutorialObstacleType::Jump);
					break;
				}
			}
		}
	}

	// Obstacle 3: Slide (HighBarrier)
	if (!ShownTutorialPrompts.Contains(ETutorialObstacleType::Slide))
	{
		for (ABaseObstacle* Obstacle : TutorialObstacles)
		{
			if (Obstacle && Obstacle->IsActive() && 
			    Obstacle->GetObstacleType() == EObstacleType::HighBarrier)
			{
				float ObstacleX = Obstacle->GetActorLocation().X;
				float DistanceToPlayer = ObstacleX - PlayerX;
				if (DistanceToPlayer <= PromptDistance && DistanceToPlayer > 0)
				{
					ShownTutorialPrompts.Add(ETutorialObstacleType::Slide);
					OnTutorialPrompt.Broadcast(ETutorialObstacleType::Slide);
					break;
				}
			}
		}
	}

	// Check if all tutorial obstacles have been passed
	if (ShownTutorialPrompts.Num() >= 3)
	{
		bool bAllPassed = true;
		int32 ActiveTutorialCount = 0;
		int32 PassedTutorialCount = 0;
		
		for (ABaseObstacle* Obstacle : TutorialObstacles)
		{
			if (Obstacle && Obstacle->IsActive())
			{
				ActiveTutorialCount++;
				float ObstacleX = Obstacle->GetActorLocation().X;
				if (ObstacleX > PlayerX - 500.0f)
				{
					bAllPassed = false;
				}
				else
				{
					PassedTutorialCount++;
				}
			}
		}

		if (bAllPassed)
		{
			CompleteTutorial();
		}
	}
}

void UObstacleSpawnerComponent::CompleteTutorial()
{
	if (bTutorialComplete)
	{
		return;
	}

	bTutorialComplete = true;
	OnTutorialComplete.Broadcast();
}

void UObstacleSpawnerComponent::ResetTutorial()
{
	bTutorialComplete = false;
	bHasSpawnedTutorialObstacles = false;
	TutorialSegmentsSkipped = 0;
	ShownTutorialPrompts.Empty();
	
	for (ABaseObstacle* Obstacle : TutorialObstacles)
	{
		if (Obstacle && Obstacle->IsActive())
		{
			Obstacle->Deactivate();
		}
	}
	TutorialObstacles.Empty();
}

// --- Pattern Functions ---

bool UObstacleSpawnerComponent::GenerateFromPattern(TArray<FObstacleSpawnData>& OutObstacles)
{
	int32 PatternIndex = INDEX_NONE;
	const FObstaclePattern* SelectedPattern = SelectPattern(PatternIndex);
	
	if (!SelectedPattern || SelectedPattern->Obstacles.Num() == 0)
	{
		return false;
	}

	OutObstacles = SelectedPattern->Obstacles;

	// Track pattern usage for variety
	if (bEnablePatternVariety && PatternIndex != INDEX_NONE)
	{
		RecentlyUsedPatternIndices.Add(PatternIndex);
		
		int32 VarietyCount = MinPatternVarietyCount;
		if (VarietyCount == 0)
		{
			int32 AvailableCount = 0;
			for (const FObstaclePattern& Pattern : PredefinedPatterns)
			{
				if (Pattern.MinDifficultyLevel <= CurrentDifficultyLevel && Pattern.Obstacles.Num() > 0)
				{
					AvailableCount++;
				}
			}
			VarietyCount = FMath::Max(1, AvailableCount - 1);
		}
		
		while (RecentlyUsedPatternIndices.Num() > VarietyCount)
		{
			RecentlyUsedPatternIndices.RemoveAt(0);
		}
	}

	LastPatternName = SelectedPattern->PatternName;

	if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
	{
		Debug->Stat_LastPattern = SelectedPattern->PatternName;
		Debug->LogEvent(EDebugCategory::Spawning, FString::Printf(TEXT("Pattern: %s"), *SelectedPattern->PatternName));
	}

	return true;
}

void UObstacleSpawnerComponent::GenerateProcedural(TArray<FObstacleSpawnData>& OutObstacles, int32 ObstacleCount)
{
	OutObstacles.Reserve(ObstacleCount);

	// Even distribution across the segment with a bit of jitter.
	// Difficulty ramps by adding more obstacles, not by tightening spacing.
	const float MinXSpacing = MinObstacleSpacing;
	
	float SpawnRange = MaxSpawnOffset - MinSpawnOffset;
	
	// Space obstacles evenly with gaps at start and end
	float EvenSpacing = SpawnRange / (ObstacleCount + 1);
	
	// Cap count if spacing gets too tight
	if (EvenSpacing < MinXSpacing)
	{
		int32 MaxObstaclesThatFit = FMath::FloorToInt(SpawnRange / MinXSpacing);
		ObstacleCount = FMath::Max(1, MaxObstaclesThatFit);
		EvenSpacing = SpawnRange / (ObstacleCount + 1);
	}
	
	// Track positions per lane to avoid same-lane stacking
	TMap<ELane, TArray<float>> UsedXOffsetsPerLane;
	UsedXOffsetsPerLane.Add(ELane::Left, TArray<float>());
	UsedXOffsetsPerLane.Add(ELane::Center, TArray<float>());
	UsedXOffsetsPerLane.Add(ELane::Right, TArray<float>());

	for (int32 i = 0; i < ObstacleCount; i++)
	{
		FObstacleSpawnData SpawnData;

		SpawnData.ObstacleType = GetRandomObstacleType();
		SpawnData.Lane = GetRandomLane();

		// Even base position + small jitter for variety
		float BaseOffset = MinSpawnOffset + (EvenSpacing * (i + 1));
		float Jitter = FMath::FRandRange(-EvenSpacing * 0.20f, EvenSpacing * 0.20f);
		float XOffset = FMath::Clamp(BaseOffset + Jitter, MinSpawnOffset, MaxSpawnOffset);
		
		// Resolve same-lane conflicts
		TArray<float>& LaneUsedOffsets = UsedXOffsetsPerLane[SpawnData.Lane];
		int32 Attempts = 0;
		const int32 MaxAttempts = 10;
		
		while (Attempts < MaxAttempts && LaneUsedOffsets.ContainsByPredicate(
			[XOffset, MinXSpacing](float Used) { return FMath::Abs(Used - XOffset) < MinXSpacing; }))
		{
			// Try a different lane
			SpawnData.Lane = GetRandomLane();
			TArray<float>& NewLaneOffsets = UsedXOffsetsPerLane[SpawnData.Lane];
			
			if (!NewLaneOffsets.ContainsByPredicate(
				[XOffset, MinXSpacing](float Used) { return FMath::Abs(Used - XOffset) < MinXSpacing; }))
			{
				break;
			}
			Attempts++;
		}

		SpawnData.RelativeXOffset = XOffset;
		UsedXOffsetsPerLane[SpawnData.Lane].Add(XOffset);
		SpawnData.ZOffset = 0.0f;

		OutObstacles.Add(SpawnData);
	}
}

int32 UObstacleSpawnerComponent::CalculateObstacleCount() const
{
	// Ramps with difficulty, capped at max
	int32 BaseCount = MinObstaclesPerSegment + (CurrentDifficultyLevel * ObstaclesPerDifficultyLevel);
	int32 RandomVariance = FMath::RandRange(-1, 1);
	int32 FinalCount = FMath::Clamp(BaseCount + RandomVariance, MinObstaclesPerSegment, MaxObstaclesPerSegment);

	return FinalCount;
}

void UObstacleSpawnerComponent::EnsureFairLayout(TArray<FObstacleSpawnData>& Obstacles)
{
	if (Obstacles.Num() < 3)
	{
		if (bEnableTypeAwareSpacing)
		{
			FixTypeSpacingViolations(Obstacles);
		}
		return;
	}

	// Sort by X for consistent processing
	Obstacles.Sort([](const FObstacleSpawnData& A, const FObstacleSpawnData& B)
	{
		return A.RelativeXOffset < B.RelativeXOffset;
	});

	// --- Pass 1: Remove impassable FullWall clusters ---
	//
	// A FullWall blocks a lane completely. If FullWalls cover all 3 lanes
	// within close proximity, the player can't pass without taking a hit.
	//
	// We use BFS to find connected components: two FullWalls are "connected"
	// if they're within RowThreshold X distance (transitively). This catches
	// chained clusters the old fixed-anchor approach missed.

	const float RowThreshold = 0.12f; // ~750 units at 6250 segment length

	TArray<int32> IndicesToRemove;

	// Collect all FullWall indices
	TArray<int32> FullWallIndices;
	for (int32 i = 0; i < Obstacles.Num(); i++)
	{
		if (Obstacles[i].ObstacleType == EObstacleType::FullWall)
		{
			FullWallIndices.Add(i);
		}
	}

	// BFS to find connected FullWall clusters
	TSet<int32> Visited;

	for (int32 StartIdx : FullWallIndices)
	{
		if (Visited.Contains(StartIdx))
		{
			continue;
		}

		TArray<int32> Cluster;
		TArray<int32> Queue;
		Queue.Add(StartIdx);

		while (Queue.Num() > 0)
		{
			int32 Current = Queue[0];
			Queue.RemoveAt(0);

			if (Visited.Contains(Current))
			{
				continue;
			}
			Visited.Add(Current);
			Cluster.Add(Current);

			for (int32 OtherIdx : FullWallIndices)
			{
				if (!Visited.Contains(OtherIdx))
				{
					float XDiff = FMath::Abs(Obstacles[OtherIdx].RelativeXOffset - Obstacles[Current].RelativeXOffset);
					if (XDiff <= RowThreshold)
					{
						Queue.Add(OtherIdx);
					}
				}
			}
		}

		// Check if this cluster covers all 3 lanes
		bool bHasLeft = false;
		bool bHasCenter = false;
		bool bHasRight = false;

		for (int32 Idx : Cluster)
		{
			switch (Obstacles[Idx].Lane)
			{
				case ELane::Left:   bHasLeft = true; break;
				case ELane::Center: bHasCenter = true; break;
				case ELane::Right:  bHasRight = true; break;
			}
		}

		if (bHasLeft && bHasCenter && bHasRight)
		{
			// All 3 lanes blocked -- remove one to open a path
			int32 RemoveIdx = Cluster[FMath::RandRange(0, Cluster.Num() - 1)];
			IndicesToRemove.AddUnique(RemoveIdx);

			UE_LOG(LogStateRunner_Arcade, Warning,
				TEXT("EnsureFairLayout: FullWall cluster blocks ALL 3 lanes! Removing %s lane FullWall at X=%.2f (cluster size: %d)"),
				Obstacles[RemoveIdx].Lane == ELane::Left ? TEXT("Left") :
					(Obstacles[RemoveIdx].Lane == ELane::Center ? TEXT("Center") : TEXT("Right")),
				Obstacles[RemoveIdx].RelativeXOffset,
				Cluster.Num());
		}
	}

	// Remove in reverse order to preserve indices
	IndicesToRemove.Sort([](int32 A, int32 B) { return A > B; });
	for (int32 Idx : IndicesToRemove)
	{
		Obstacles.RemoveAt(Idx);
	}

	// --- Pass 2: Type-aware spacing (prevents unfair sequences) ---
	if (bEnableTypeAwareSpacing)
	{
		FixTypeSpacingViolations(Obstacles);
	}
}

float UObstacleSpawnerComponent::GetRequiredSpacingForTypes(EObstacleType FirstType, EObstacleType SecondType) const
{
	float RequiredSpacing = MinObstacleSpacing;
	
	// Slide then jump: player is in slide animation and can't immediately jump
	if (FirstType == EObstacleType::HighBarrier && SecondType == EObstacleType::LowWall)
	{
		RequiredSpacing += SlideToJumpExtraSpacing;
	}
	// Jump then slide: player needs time to land and initiate slide
	else if (FirstType == EObstacleType::LowWall && SecondType == EObstacleType::HighBarrier)
	{
		RequiredSpacing += JumpToSlideExtraSpacing;
	}
	
	return RequiredSpacing;
}

void UObstacleSpawnerComponent::FindTypeSpacingViolations(const TArray<FObstacleSpawnData>& Obstacles, TArray<TPair<int32, int32>>& OutViolations) const
{
	OutViolations.Empty();
	
	// Check same-lane pairs for type-combo spacing issues
	for (int32 i = 0; i < Obstacles.Num(); i++)
	{
		for (int32 j = i + 1; j < Obstacles.Num(); j++)
		{
			if (Obstacles[i].Lane != Obstacles[j].Lane)
			{
				continue;
			}
			
			int32 FirstIdx = (Obstacles[i].RelativeXOffset <= Obstacles[j].RelativeXOffset) ? i : j;
			int32 SecondIdx = (FirstIdx == i) ? j : i;
			
			const FObstacleSpawnData& First = Obstacles[FirstIdx];
			const FObstacleSpawnData& Second = Obstacles[SecondIdx];
			
			float RequiredSpacing = GetRequiredSpacingForTypes(First.ObstacleType, Second.ObstacleType);
			float ActualSpacing = Second.RelativeXOffset - First.RelativeXOffset;
			
			if (ActualSpacing < RequiredSpacing)
			{
				OutViolations.Add(TPair<int32, int32>(FirstIdx, SecondIdx));
			}
		}
	}
}

void UObstacleSpawnerComponent::FixTypeSpacingViolations(TArray<FObstacleSpawnData>& Obstacles)
{
	TArray<TPair<int32, int32>> Violations;
	FindTypeSpacingViolations(Obstacles, Violations);
	
	if (Violations.Num() == 0)
	{
		return;
	}
	
	TSet<int32> ModifiedIndices;
	TArray<int32> IndicesToRemove;
	
	for (const TPair<int32, int32>& Violation : Violations)
	{
		int32 FirstIdx = Violation.Key;
		int32 SecondIdx = Violation.Value;
		
		if (ModifiedIndices.Contains(FirstIdx) || ModifiedIndices.Contains(SecondIdx) ||
			IndicesToRemove.Contains(FirstIdx) || IndicesToRemove.Contains(SecondIdx))
		{
			continue;
		}
		
		FObstacleSpawnData& First = Obstacles[FirstIdx];
		FObstacleSpawnData& Second = Obstacles[SecondIdx];
		
		float RequiredSpacing = GetRequiredSpacingForTypes(First.ObstacleType, Second.ObstacleType);
		float ActualSpacing = Second.RelativeXOffset - First.RelativeXOffset;
		float SpacingDeficit = RequiredSpacing - ActualSpacing;
		
		// Try pushing the second obstacle forward
		float NewSecondX = Second.RelativeXOffset + SpacingDeficit + 0.02f;
		
		if (NewSecondX <= MaxSpawnOffset)
		{
			Second.RelativeXOffset = NewSecondX;
			ModifiedIndices.Add(SecondIdx);
			continue;
		}
		
		// Try pulling the first obstacle backward
		float NewFirstX = First.RelativeXOffset - SpacingDeficit - 0.02f;
		
		if (NewFirstX >= MinSpawnOffset)
		{
			First.RelativeXOffset = NewFirstX;
			ModifiedIndices.Add(FirstIdx);
			continue;
		}
		
		// Change the second obstacle's type to break the bad combo
		// slide->jump becomes slide->slide, jump->slide becomes jump->jump
		if (First.ObstacleType == EObstacleType::HighBarrier && Second.ObstacleType == EObstacleType::LowWall)
		{
			Second.ObstacleType = EObstacleType::HighBarrier;
			ModifiedIndices.Add(SecondIdx);
			continue;
		}
		else if (First.ObstacleType == EObstacleType::LowWall && Second.ObstacleType == EObstacleType::HighBarrier)
		{
			Second.ObstacleType = EObstacleType::LowWall;
			ModifiedIndices.Add(SecondIdx);
			continue;
		}
		
		// Last resort: remove the second obstacle
		IndicesToRemove.AddUnique(SecondIdx);
	}
	
	if (IndicesToRemove.Num() > 0)
	{
		IndicesToRemove.Sort([](int32 A, int32 B) { return A > B; });
		for (int32 Idx : IndicesToRemove)
		{
			Obstacles.RemoveAt(Idx);
		}
	}
}

const FObstaclePattern* UObstacleSpawnerComponent::SelectPattern(int32& OutPatternIndex)
{
	struct FPatternCandidate
	{
		const FObstaclePattern* Pattern;
		int32 Index;
		float Weight;
	};
	
	TArray<FPatternCandidate> ValidPatterns;
	float TotalWeight = 0.0f;

	for (int32 i = 0; i < PredefinedPatterns.Num(); i++)
	{
		const FObstaclePattern& Pattern = PredefinedPatterns[i];
		
		if (Pattern.MinDifficultyLevel <= CurrentDifficultyLevel && Pattern.Obstacles.Num() > 0)
		{
			bool bRecentlyUsed = bEnablePatternVariety && RecentlyUsedPatternIndices.Contains(i);
			
			if (!bRecentlyUsed)
			{
				FPatternCandidate Candidate;
				Candidate.Pattern = &Pattern;
				Candidate.Index = i;
				Candidate.Weight = Pattern.SelectionWeight;
				
				ValidPatterns.Add(Candidate);
				TotalWeight += Pattern.SelectionWeight;
			}
		}
	}

	// If variety system filtered everything, reset and try again
	if (ValidPatterns.Num() == 0)
	{
		UObstacleSpawnerComponent* MutableThis = const_cast<UObstacleSpawnerComponent*>(this);
		MutableThis->RecentlyUsedPatternIndices.Empty();
		
		for (int32 i = 0; i < PredefinedPatterns.Num(); i++)
		{
			const FObstaclePattern& Pattern = PredefinedPatterns[i];
			
			if (Pattern.MinDifficultyLevel <= CurrentDifficultyLevel && Pattern.Obstacles.Num() > 0)
			{
				FPatternCandidate Candidate;
				Candidate.Pattern = &Pattern;
				Candidate.Index = i;
				Candidate.Weight = Pattern.SelectionWeight;
				
				ValidPatterns.Add(Candidate);
				TotalWeight += Pattern.SelectionWeight;
			}
		}
	}

	if (ValidPatterns.Num() == 0 || TotalWeight <= 0.0f)
	{
		OutPatternIndex = INDEX_NONE;
		return nullptr;
	}

	// Weighted random selection
	float RandomValue = FMath::FRand() * TotalWeight;
	float AccumulatedWeight = 0.0f;

	for (const FPatternCandidate& Candidate : ValidPatterns)
	{
		AccumulatedWeight += Candidate.Weight;
		if (RandomValue <= AccumulatedWeight)
		{
			OutPatternIndex = Candidate.Index;
			return Candidate.Pattern;
		}
	}

	OutPatternIndex = ValidPatterns.Last().Index;
	return ValidPatterns.Last().Pattern;
}

// --- Pooling Functions ---

void UObstacleSpawnerComponent::InitializePools()
{
	if (!LowWallClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("ObstacleSpawnerComponent: No LowWallClass set! Assign BP_JumpHurdle."));
	}
	if (!HighBarrierClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("ObstacleSpawnerComponent: No HighBarrierClass set! Assign BP_SlideHurdle."));
	}
	if (!FullWallClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("ObstacleSpawnerComponent: No FullWallClass set! Assign BP_SlabObstacle."));
	}

	InitializePoolForType(EObstacleType::LowWall);
	InitializePoolForType(EObstacleType::HighBarrier);
	InitializePoolForType(EObstacleType::FullWall);
}

void UObstacleSpawnerComponent::InitializePoolForType(EObstacleType Type)
{
	TSubclassOf<ABaseObstacle> ClassToSpawn = GetClassForType(Type);
	if (!ClassToSpawn)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("Cannot initialize pool for type %d - no class assigned!"), (int32)Type);
		return;
	}

	TArray<TObjectPtr<ABaseObstacle>>& Pool = GetPoolForType(Type);
	Pool.Reserve(InitialPoolSizePerType);

	for (int32 i = 0; i < InitialPoolSizePerType; i++)
	{
		ABaseObstacle* Obstacle = SpawnObstacleActor(Type);
		if (Obstacle)
		{
			Pool.Add(Obstacle);
		}
	}
}

ABaseObstacle* UObstacleSpawnerComponent::GetObstacleFromPool(EObstacleType Type)
{
	TArray<TObjectPtr<ABaseObstacle>>& Pool = GetPoolForType(Type);

	for (ABaseObstacle* Obstacle : Pool)
	{
		if (Obstacle && !Obstacle->IsActive())
		{
			return Obstacle;
		}
	}

	// Pool exhausted, expand
	ExpandPool(Type);

	for (ABaseObstacle* Obstacle : Pool)
	{
		if (Obstacle && !Obstacle->IsActive())
		{
			return Obstacle;
		}
	}

	UE_LOG(LogStateRunner_Arcade, Warning, TEXT("Failed to get obstacle from pool even after expansion!"));
	return nullptr;
}

TArray<TObjectPtr<ABaseObstacle>>& UObstacleSpawnerComponent::GetPoolForType(EObstacleType Type)
{
	switch (Type)
	{
		case EObstacleType::LowWall:
			return LowWallPool;
		case EObstacleType::HighBarrier:
			return HighBarrierPool;
		case EObstacleType::FullWall:
		default:
			return FullWallPool;
	}
}

TSubclassOf<ABaseObstacle> UObstacleSpawnerComponent::GetClassForType(EObstacleType Type) const
{
	switch (Type)
	{
		case EObstacleType::LowWall:
			return LowWallClass;
		case EObstacleType::HighBarrier:
			return HighBarrierClass;
		case EObstacleType::FullWall:
		default:
			return FullWallClass;
	}
}

void UObstacleSpawnerComponent::ExpandPool(EObstacleType Type)
{
	TSubclassOf<ABaseObstacle> ClassToSpawn = GetClassForType(Type);
	if (!ClassToSpawn)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("Cannot expand pool for type %d - no class assigned!"), (int32)Type);
		return;
	}

	TArray<TObjectPtr<ABaseObstacle>>& Pool = GetPoolForType(Type);
	int32 OldSize = Pool.Num();
	Pool.Reserve(OldSize + PoolExpansionSize);

	for (int32 i = 0; i < PoolExpansionSize; i++)
	{
		ABaseObstacle* Obstacle = SpawnObstacleActor(Type);
		if (Obstacle)
		{
			Pool.Add(Obstacle);
		}
	}
}

ABaseObstacle* UObstacleSpawnerComponent::SpawnObstacleActor(EObstacleType Type)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TSubclassOf<ABaseObstacle> ClassToSpawn = GetClassForType(Type);
	if (!ClassToSpawn)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("Cannot spawn obstacle - no class for type %d!"), (int32)Type);
		return nullptr;
	}

	FVector SpawnLocation(0.0f, 0.0f, -10000.0f);
	FRotator SpawnRotation = FRotator::ZeroRotator;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABaseObstacle* Obstacle = World->SpawnActor<ABaseObstacle>(ClassToSpawn, SpawnLocation, SpawnRotation, SpawnParams);
	
	if (Obstacle)
	{
		Obstacle->Deactivate();
	}

	return Obstacle;
}

// --- Helper Functions ---

float UObstacleSpawnerComponent::GetLaneYPosition(ELane Lane) const
{
	switch (Lane)
	{
		case ELane::Left:	return LeftLaneY;
		case ELane::Center:	return CenterLaneY;
		case ELane::Right:	return RightLaneY;
		default:			return CenterLaneY;
	}
}

ELane UObstacleSpawnerComponent::GetRandomLane() const
{
	int32 RandomValue = FMath::RandRange(0, 2);
	switch (RandomValue)
	{
		case 0:		return ELane::Left;
		case 1:		return ELane::Center;
		default:	return ELane::Right;
	}
}

EObstacleType UObstacleSpawnerComponent::GetRandomObstacleType() const
{
	// 40% LowWall (jump), 40% HighBarrier (slide), 20% FullWall (lane change)
	int32 RandomValue = FMath::RandRange(0, 9);
	
	if (RandomValue < 4)
	{
		return EObstacleType::LowWall;
	}
	else if (RandomValue < 8)
	{
		return EObstacleType::HighBarrier;
	}
	else
	{
		return EObstacleType::FullWall;
	}
}

void UObstacleSpawnerComponent::UpdateDifficulty()
{
	int32 NewDifficulty = SegmentsSpawned / SegmentsPerDifficultyIncrease;
	
	if (NewDifficulty != CurrentDifficultyLevel)
	{
		int32 OldDifficulty = CurrentDifficultyLevel;
		CurrentDifficultyLevel = NewDifficulty;
		
		if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
		{
			Debug->Stat_DifficultyLevel = CurrentDifficultyLevel;
			Debug->LogEvent(EDebugCategory::Spawning, FString::Printf(TEXT("Difficulty: %d"), CurrentDifficultyLevel));
			
			if (OldDifficulty < DifficultyToDisableBreathers && CurrentDifficultyLevel >= DifficultyToDisableBreathers)
			{
				Debug->LogEvent(EDebugCategory::Spawning, TEXT("ENDGAME - No more breathers!"));
			}
		}
	}
}

bool UObstacleSpawnerComponent::ShouldBeBreatherSegment()
{
	if (EmptySegmentInterval <= 0)
	{
		return false;
	}
	
	// No breathers at high difficulty -- constant action
	if (CurrentDifficultyLevel >= DifficultyToDisableBreathers)
	{
		return false;
	}
	
	return SegmentsSinceEmpty >= EmptySegmentInterval;
}

float UObstacleSpawnerComponent::GetEffectiveBreatherGapFraction() const
{
	// Gap shrinks as difficulty increases
	float EffectiveGap = BreatherGapFraction - (CurrentDifficultyLevel * BreatherGapShrinkPerLevel);
	EffectiveGap = FMath::Max(EffectiveGap, MinBreatherGapFraction);
	
	return EffectiveGap;
}

void UObstacleSpawnerComponent::AddFillerObstacles(TArray<FObstacleSpawnData>& OutObstacles, float PatternMinX, float PatternMaxX)
{
	// More filler at higher difficulty
	int32 TotalFillerCount = FillerObstacleCount;
	
	if (CurrentDifficultyLevel >= DifficultyToDisableBreathers)
	{
		int32 ExtraDifficulty = CurrentDifficultyLevel - DifficultyToDisableBreathers;
		TotalFillerCount += ExtraDifficulty * FillerPerDifficulty;
	}
	
	if (TotalFillerCount <= 0)
	{
		return;
	}

	// Filler zones: before and after the pattern
	const float GapBuffer = 0.08f;
	
	float BeforeGapStart = MinSpawnOffset;
	float BeforeGapEnd = FMath::Max(MinSpawnOffset, PatternMinX - GapBuffer);
	float BeforeGapSize = BeforeGapEnd - BeforeGapStart;
	
	float AfterGapStart = FMath::Min(MaxSpawnOffset, PatternMaxX + GapBuffer);
	float AfterGapEnd = MaxSpawnOffset;
	float AfterGapSize = AfterGapEnd - AfterGapStart;
	
	float TotalGapSize = BeforeGapSize + AfterGapSize;
	
	if (TotalGapSize < 0.1f)
	{
		return;
	}
	
	// Distribute proportionally
	int32 BeforeCount = (BeforeGapSize > 0.05f) ? FMath::RoundToInt(TotalFillerCount * (BeforeGapSize / TotalGapSize)) : 0;
	int32 AfterCount = TotalFillerCount - BeforeCount;
	
	if (BeforeGapSize > 0.1f && BeforeCount == 0 && AfterCount > 1)
	{
		BeforeCount = 1;
		AfterCount--;
	}
	if (AfterGapSize > 0.1f && AfterCount == 0 && BeforeCount > 1)
	{
		AfterCount = 1;
		BeforeCount--;
	}
	
	if (BeforeCount > 0 && BeforeGapSize > 0.05f)
	{
		float Spacing = BeforeGapSize / (BeforeCount + 1);
		for (int32 i = 0; i < BeforeCount; i++)
		{
			FObstacleSpawnData Filler;
			Filler.RelativeXOffset = BeforeGapStart + Spacing * (i + 1);
			Filler.Lane = static_cast<ELane>(FMath::RandRange(0, 2));
			Filler.ObstacleType = GetRandomObstacleType();
			Filler.ZOffset = 0.0f;
			OutObstacles.Add(Filler);
		}
	}
	
	if (AfterCount > 0 && AfterGapSize > 0.05f)
	{
		float Spacing = AfterGapSize / (AfterCount + 1);
		for (int32 i = 0; i < AfterCount; i++)
		{
			FObstacleSpawnData Filler;
			Filler.RelativeXOffset = AfterGapStart + Spacing * (i + 1);
			Filler.Lane = static_cast<ELane>(FMath::RandRange(0, 2));
			Filler.ObstacleType = GetRandomObstacleType();
			Filler.ZOffset = 0.0f;
			OutObstacles.Add(Filler);
		}
	}
}

void UObstacleSpawnerComponent::CreateDefaultPatterns()
{
	// --- Difficulty 0 Patterns (Early game) ---

	// Simple Jump Center
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Jump Center");
		Pattern.MinDifficultyLevel = 0;
		Pattern.SelectionWeight = 2.0f;

		FObstacleSpawnData Obs;
		Obs.Lane = ELane::Center;
		Obs.ObstacleType = EObstacleType::LowWall;
		Obs.RelativeXOffset = 0.5f;
		Pattern.Obstacles.Add(Obs);

		PredefinedPatterns.Add(Pattern);
	}

	// Slide Left
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Slide Left");
		Pattern.MinDifficultyLevel = 0;
		Pattern.SelectionWeight = 1.5f;

		FObstacleSpawnData Obs;
		Obs.Lane = ELane::Left;
		Obs.ObstacleType = EObstacleType::HighBarrier;
		Obs.RelativeXOffset = 0.5f;
		Pattern.Obstacles.Add(Obs);

		PredefinedPatterns.Add(Pattern);
	}

	// Lane Choice - Two blocked, one free
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Lane Choice");
		Pattern.MinDifficultyLevel = 0;
		Pattern.SelectionWeight = 1.2f;

		FObstacleSpawnData Obs1;
		Obs1.Lane = ELane::Center;
		Obs1.ObstacleType = EObstacleType::FullWall;
		Obs1.RelativeXOffset = 0.5f;
		Pattern.Obstacles.Add(Obs1);

		FObstacleSpawnData Obs2;
		Obs2.Lane = ELane::Right;
		Obs2.ObstacleType = EObstacleType::FullWall;
		Obs2.RelativeXOffset = 0.5f;
		Pattern.Obstacles.Add(Obs2);

		PredefinedPatterns.Add(Pattern);
	}

	// Staggered Pair
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Staggered Pair");
		Pattern.MinDifficultyLevel = 0;
		Pattern.SelectionWeight = 1.0f;

		FObstacleSpawnData Obs1;
		Obs1.Lane = ELane::Left;
		Obs1.ObstacleType = EObstacleType::LowWall;
		Obs1.RelativeXOffset = 0.3f;
		Pattern.Obstacles.Add(Obs1);

		FObstacleSpawnData Obs2;
		Obs2.Lane = ELane::Right;
		Obs2.ObstacleType = EObstacleType::HighBarrier;
		Obs2.RelativeXOffset = 0.6f;
		Pattern.Obstacles.Add(Obs2);

		PredefinedPatterns.Add(Pattern);
	}

	// --- Difficulty 1 Patterns (Warming up) ---

	// Zigzag Classic
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Zigzag Classic");
		Pattern.MinDifficultyLevel = 1;
		Pattern.SelectionWeight = 1.2f;

		FObstacleSpawnData Obs1;
		Obs1.Lane = ELane::Left;
		Obs1.ObstacleType = EObstacleType::LowWall;
		Obs1.RelativeXOffset = 0.2f;
		Pattern.Obstacles.Add(Obs1);

		FObstacleSpawnData Obs2;
		Obs2.Lane = ELane::Center;
		Obs2.ObstacleType = EObstacleType::HighBarrier;
		Obs2.RelativeXOffset = 0.4f;
		Pattern.Obstacles.Add(Obs2);

		FObstacleSpawnData Obs3;
		Obs3.Lane = ELane::Right;
		Obs3.ObstacleType = EObstacleType::LowWall;
		Obs3.RelativeXOffset = 0.6f;
		Pattern.Obstacles.Add(Obs3);

		FObstacleSpawnData Obs4;
		Obs4.Lane = ELane::Center;
		Obs4.ObstacleType = EObstacleType::HighBarrier;
		Obs4.RelativeXOffset = 0.8f;
		Pattern.Obstacles.Add(Obs4);

		PredefinedPatterns.Add(Pattern);
	}

	// Diagonal Left-to-Right
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Diagonal LR");
		Pattern.MinDifficultyLevel = 1;
		Pattern.SelectionWeight = 1.0f;

		const ELane Lanes[] = { ELane::Left, ELane::Center, ELane::Right };
		const float XOffsets[] = { 0.2f, 0.4f, 0.6f };
		
		for (int32 i = 0; i < 3; i++)
		{
			FObstacleSpawnData Obs;
			Obs.Lane = Lanes[i];
			Obs.ObstacleType = (i % 2 == 0) ? EObstacleType::LowWall : EObstacleType::HighBarrier;
			Obs.RelativeXOffset = XOffsets[i];
			Pattern.Obstacles.Add(Obs);
		}

		PredefinedPatterns.Add(Pattern);
	}

	// Diagonal Right-to-Left
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Diagonal RL");
		Pattern.MinDifficultyLevel = 1;
		Pattern.SelectionWeight = 1.0f;

		const ELane Lanes[] = { ELane::Right, ELane::Center, ELane::Left };
		const float XOffsets[] = { 0.2f, 0.4f, 0.6f };
		
		for (int32 i = 0; i < 3; i++)
		{
			FObstacleSpawnData Obs;
			Obs.Lane = Lanes[i];
			Obs.ObstacleType = (i % 2 == 0) ? EObstacleType::HighBarrier : EObstacleType::LowWall;
			Obs.RelativeXOffset = XOffsets[i];
			Pattern.Obstacles.Add(Obs);
		}

		PredefinedPatterns.Add(Pattern);
	}

	// Jump Line - all three lanes
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Jump Line");
		Pattern.MinDifficultyLevel = 1;
		Pattern.SelectionWeight = 1.4f;

		for (ELane Lane : { ELane::Left, ELane::Center, ELane::Right })
		{
			FObstacleSpawnData Obs;
			Obs.Lane = Lane;
			Obs.ObstacleType = EObstacleType::LowWall;
			Obs.RelativeXOffset = 0.5f;
			Pattern.Obstacles.Add(Obs);
		}

		PredefinedPatterns.Add(Pattern);
	}

	// Slide Line - all three lanes
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Slide Line");
		Pattern.MinDifficultyLevel = 1;
		Pattern.SelectionWeight = 0.9f;

		for (ELane Lane : { ELane::Left, ELane::Center, ELane::Right })
		{
			FObstacleSpawnData Obs;
			Obs.Lane = Lane;
			Obs.ObstacleType = EObstacleType::HighBarrier;
			Obs.RelativeXOffset = 0.5f;
			Pattern.Obstacles.Add(Obs);
		}

		PredefinedPatterns.Add(Pattern);
	}

	// --- Difficulty 2 Patterns (Intermediate) ---

	// Wave
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Wave");
		Pattern.MinDifficultyLevel = 2;
		Pattern.SelectionWeight = 1.1f;

		const ELane WaveLanes[] = { ELane::Center, ELane::Right, ELane::Center, ELane::Left, ELane::Center };
		const EObstacleType WaveTypes[] = { EObstacleType::LowWall, EObstacleType::HighBarrier, EObstacleType::LowWall, EObstacleType::HighBarrier, EObstacleType::LowWall };
		
		for (int32 i = 0; i < 5; i++)
		{
			FObstacleSpawnData Obs;
			Obs.Lane = WaveLanes[i];
			Obs.ObstacleType = WaveTypes[i];
			Obs.RelativeXOffset = 0.15f + (i * 0.15f);
			Pattern.Obstacles.Add(Obs);
		}

		PredefinedPatterns.Add(Pattern);
	}

	// Funnel Left
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Funnel Left");
		Pattern.MinDifficultyLevel = 2;
		Pattern.SelectionWeight = 0.9f;

		FObstacleSpawnData Obs1; Obs1.Lane = ELane::Right; Obs1.ObstacleType = EObstacleType::FullWall; Obs1.RelativeXOffset = 0.2f; Pattern.Obstacles.Add(Obs1);
		FObstacleSpawnData Obs2; Obs2.Lane = ELane::Right; Obs2.ObstacleType = EObstacleType::FullWall; Obs2.RelativeXOffset = 0.4f; Pattern.Obstacles.Add(Obs2);
		FObstacleSpawnData Obs3; Obs3.Lane = ELane::Center; Obs3.ObstacleType = EObstacleType::FullWall; Obs3.RelativeXOffset = 0.4f; Pattern.Obstacles.Add(Obs3);
		FObstacleSpawnData Obs4; Obs4.Lane = ELane::Center; Obs4.ObstacleType = EObstacleType::LowWall; Obs4.RelativeXOffset = 0.7f; Pattern.Obstacles.Add(Obs4);
		FObstacleSpawnData Obs5; Obs5.Lane = ELane::Right; Obs5.ObstacleType = EObstacleType::FullWall; Obs5.RelativeXOffset = 0.7f; Pattern.Obstacles.Add(Obs5);

		PredefinedPatterns.Add(Pattern);
	}

	// Funnel Right
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Funnel Right");
		Pattern.MinDifficultyLevel = 2;
		Pattern.SelectionWeight = 0.9f;

		FObstacleSpawnData Obs1; Obs1.Lane = ELane::Left; Obs1.ObstacleType = EObstacleType::FullWall; Obs1.RelativeXOffset = 0.2f; Pattern.Obstacles.Add(Obs1);
		FObstacleSpawnData Obs2; Obs2.Lane = ELane::Left; Obs2.ObstacleType = EObstacleType::FullWall; Obs2.RelativeXOffset = 0.4f; Pattern.Obstacles.Add(Obs2);
		FObstacleSpawnData Obs3; Obs3.Lane = ELane::Center; Obs3.ObstacleType = EObstacleType::FullWall; Obs3.RelativeXOffset = 0.4f; Pattern.Obstacles.Add(Obs3);
		FObstacleSpawnData Obs4; Obs4.Lane = ELane::Center; Obs4.ObstacleType = EObstacleType::HighBarrier; Obs4.RelativeXOffset = 0.7f; Pattern.Obstacles.Add(Obs4);
		FObstacleSpawnData Obs5; Obs5.Lane = ELane::Left; Obs5.ObstacleType = EObstacleType::FullWall; Obs5.RelativeXOffset = 0.7f; Pattern.Obstacles.Add(Obs5);

		PredefinedPatterns.Add(Pattern);
	}

	// Alternating Rhythm
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Alternating Rhythm");
		Pattern.MinDifficultyLevel = 2;
		Pattern.SelectionWeight = 1.0f;

		const float XPositions[] = { 0.15f, 0.35f, 0.55f, 0.75f };
		const ELane AlternatingLanes[] = { ELane::Center, ELane::Left, ELane::Center, ELane::Right };
		
		for (int32 i = 0; i < 4; i++)
		{
			FObstacleSpawnData Obs;
			Obs.Lane = AlternatingLanes[i];
			Obs.ObstacleType = (i % 2 == 0) ? EObstacleType::LowWall : EObstacleType::HighBarrier;
			Obs.RelativeXOffset = XPositions[i];
			Pattern.Obstacles.Add(Obs);
		}

		PredefinedPatterns.Add(Pattern);
	}

	// Split Choice
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Split Choice");
		Pattern.MinDifficultyLevel = 2;
		Pattern.SelectionWeight = 0.85f;

		FObstacleSpawnData Obs1; Obs1.Lane = ELane::Center; Obs1.ObstacleType = EObstacleType::FullWall; Obs1.RelativeXOffset = 0.3f; Pattern.Obstacles.Add(Obs1);
		FObstacleSpawnData Obs2; Obs2.Lane = ELane::Left; Obs2.ObstacleType = EObstacleType::LowWall; Obs2.RelativeXOffset = 0.5f; Pattern.Obstacles.Add(Obs2);
		FObstacleSpawnData Obs3; Obs3.Lane = ELane::Right; Obs3.ObstacleType = EObstacleType::HighBarrier; Obs3.RelativeXOffset = 0.5f; Pattern.Obstacles.Add(Obs3);
		FObstacleSpawnData Obs4; Obs4.Lane = ELane::Center; Obs4.ObstacleType = EObstacleType::FullWall; Obs4.RelativeXOffset = 0.7f; Pattern.Obstacles.Add(Obs4);

		PredefinedPatterns.Add(Pattern);
	}

	// --- Difficulty 3 Patterns (Advanced) ---

	// Dense Wave
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Dense Wave");
		Pattern.MinDifficultyLevel = 3;
		Pattern.SelectionWeight = 0.9f;

		const ELane DenseWaveLanes[] = { ELane::Left, ELane::Center, ELane::Right, ELane::Center, ELane::Left, ELane::Center, ELane::Right };
		const float DenseXOffsets[] = { 0.1f, 0.2f, 0.3f, 0.45f, 0.55f, 0.7f, 0.85f };
		
		for (int32 i = 0; i < 7; i++)
		{
			FObstacleSpawnData Obs;
			Obs.Lane = DenseWaveLanes[i];
			Obs.ObstacleType = (i % 3 == 0) ? EObstacleType::FullWall : ((i % 2 == 0) ? EObstacleType::LowWall : EObstacleType::HighBarrier);
			Obs.RelativeXOffset = DenseXOffsets[i];
			Pattern.Obstacles.Add(Obs);
		}

		PredefinedPatterns.Add(Pattern);
	}

	// Gauntlet
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Gauntlet");
		Pattern.MinDifficultyLevel = 3;
		Pattern.SelectionWeight = 0.8f;

		FObstacleSpawnData Obs1; Obs1.Lane = ELane::Left; Obs1.ObstacleType = EObstacleType::FullWall; Obs1.RelativeXOffset = 0.15f; Pattern.Obstacles.Add(Obs1);
		FObstacleSpawnData Obs2; Obs2.Lane = ELane::Center; Obs2.ObstacleType = EObstacleType::FullWall; Obs2.RelativeXOffset = 0.15f; Pattern.Obstacles.Add(Obs2);
		FObstacleSpawnData Obs3; Obs3.Lane = ELane::Right; Obs3.ObstacleType = EObstacleType::LowWall; Obs3.RelativeXOffset = 0.3f; Pattern.Obstacles.Add(Obs3);
		FObstacleSpawnData Obs4; Obs4.Lane = ELane::Center; Obs4.ObstacleType = EObstacleType::HighBarrier; Obs4.RelativeXOffset = 0.45f; Pattern.Obstacles.Add(Obs4);
		FObstacleSpawnData Obs5; Obs5.Lane = ELane::Left; Obs5.ObstacleType = EObstacleType::LowWall; Obs5.RelativeXOffset = 0.6f; Pattern.Obstacles.Add(Obs5);
		FObstacleSpawnData Obs6; Obs6.Lane = ELane::Right; Obs6.ObstacleType = EObstacleType::FullWall; Obs6.RelativeXOffset = 0.6f; Pattern.Obstacles.Add(Obs6);
		FObstacleSpawnData Obs7; Obs7.Lane = ELane::Center; Obs7.ObstacleType = EObstacleType::LowWall; Obs7.RelativeXOffset = 0.75f; Pattern.Obstacles.Add(Obs7);
		FObstacleSpawnData Obs8; Obs8.Lane = ELane::Left; Obs8.ObstacleType = EObstacleType::HighBarrier; Obs8.RelativeXOffset = 0.85f; Pattern.Obstacles.Add(Obs8);

		PredefinedPatterns.Add(Pattern);
	}

	// Weave - forces lane changes (always 1-lane switches)
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Weave");
		Pattern.MinDifficultyLevel = 3;
		Pattern.SelectionWeight = 0.85f;

		FObstacleSpawnData Obs1; Obs1.Lane = ELane::Center; Obs1.ObstacleType = EObstacleType::FullWall; Obs1.RelativeXOffset = 0.15f; Pattern.Obstacles.Add(Obs1);
		FObstacleSpawnData Obs2; Obs2.Lane = ELane::Right; Obs2.ObstacleType = EObstacleType::FullWall; Obs2.RelativeXOffset = 0.15f; Pattern.Obstacles.Add(Obs2);
		FObstacleSpawnData Obs3; Obs3.Lane = ELane::Left; Obs3.ObstacleType = EObstacleType::FullWall; Obs3.RelativeXOffset = 0.35f; Pattern.Obstacles.Add(Obs3);
		FObstacleSpawnData Obs4; Obs4.Lane = ELane::Right; Obs4.ObstacleType = EObstacleType::FullWall; Obs4.RelativeXOffset = 0.35f; Pattern.Obstacles.Add(Obs4);
		FObstacleSpawnData Obs5; Obs5.Lane = ELane::Left; Obs5.ObstacleType = EObstacleType::FullWall; Obs5.RelativeXOffset = 0.55f; Pattern.Obstacles.Add(Obs5);
		FObstacleSpawnData Obs6; Obs6.Lane = ELane::Center; Obs6.ObstacleType = EObstacleType::FullWall; Obs6.RelativeXOffset = 0.55f; Pattern.Obstacles.Add(Obs6);
		FObstacleSpawnData Obs7; Obs7.Lane = ELane::Center; Obs7.ObstacleType = EObstacleType::FullWall; Obs7.RelativeXOffset = 0.75f; Pattern.Obstacles.Add(Obs7);
		FObstacleSpawnData Obs8; Obs8.Lane = ELane::Left; Obs8.ObstacleType = EObstacleType::LowWall; Obs8.RelativeXOffset = 0.75f; Pattern.Obstacles.Add(Obs8);

		PredefinedPatterns.Add(Pattern);
	}

	// --- Difficulty 4+ Patterns (Expert) ---

	// Chaos Run
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Chaos Run");
		Pattern.MinDifficultyLevel = 4;
		Pattern.SelectionWeight = 0.75f;

		const ELane ChaosLanes[] = { ELane::Left, ELane::Right, ELane::Center, ELane::Left, ELane::Center, 
		                              ELane::Right, ELane::Left, ELane::Center, ELane::Right, ELane::Center };
		const EObstacleType ChaosTypes[] = { EObstacleType::LowWall, EObstacleType::HighBarrier, EObstacleType::FullWall,
		                                      EObstacleType::HighBarrier, EObstacleType::LowWall, EObstacleType::FullWall,
		                                      EObstacleType::HighBarrier, EObstacleType::LowWall, EObstacleType::HighBarrier,
		                                      EObstacleType::LowWall };
		
		for (int32 i = 0; i < 10; i++)
		{
			FObstacleSpawnData Obs;
			Obs.Lane = ChaosLanes[i];
			Obs.ObstacleType = ChaosTypes[i];
			Obs.RelativeXOffset = 0.1f + (i * 0.08f);
			Pattern.Obstacles.Add(Obs);
		}

		PredefinedPatterns.Add(Pattern);
	}

	// Double Helix
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Double Helix");
		Pattern.MinDifficultyLevel = 4;
		Pattern.SelectionWeight = 0.7f;

		const ELane Helix1[] = { ELane::Left, ELane::Center, ELane::Right, ELane::Center, ELane::Left };
		const ELane Helix2[] = { ELane::Right, ELane::Center, ELane::Left, ELane::Center, ELane::Right };
		
		for (int32 i = 0; i < 5; i++)
		{
			FObstacleSpawnData Obs1;
			Obs1.Lane = Helix1[i];
			Obs1.ObstacleType = EObstacleType::LowWall;
			Obs1.RelativeXOffset = 0.1f + (i * 0.18f);
			Pattern.Obstacles.Add(Obs1);

			if (i < 4)
			{
				FObstacleSpawnData Obs2;
				Obs2.Lane = Helix2[i];
				Obs2.ObstacleType = EObstacleType::HighBarrier;
				Obs2.RelativeXOffset = 0.19f + (i * 0.18f);
				Pattern.Obstacles.Add(Obs2);
			}
		}

		PredefinedPatterns.Add(Pattern);
	}

	// The Crusher
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("The Crusher");
		Pattern.MinDifficultyLevel = 4;
		Pattern.SelectionWeight = 0.65f;

		FObstacleSpawnData Obs1; Obs1.Lane = ELane::Left; Obs1.ObstacleType = EObstacleType::FullWall; Obs1.RelativeXOffset = 0.15f; Pattern.Obstacles.Add(Obs1);
		FObstacleSpawnData Obs2; Obs2.Lane = ELane::Right; Obs2.ObstacleType = EObstacleType::FullWall; Obs2.RelativeXOffset = 0.15f; Pattern.Obstacles.Add(Obs2);
		FObstacleSpawnData Obs3; Obs3.Lane = ELane::Center; Obs3.ObstacleType = EObstacleType::LowWall; Obs3.RelativeXOffset = 0.3f; Pattern.Obstacles.Add(Obs3);
		FObstacleSpawnData Obs4; Obs4.Lane = ELane::Left; Obs4.ObstacleType = EObstacleType::FullWall; Obs4.RelativeXOffset = 0.4f; Pattern.Obstacles.Add(Obs4);
		FObstacleSpawnData Obs5; Obs5.Lane = ELane::Right; Obs5.ObstacleType = EObstacleType::FullWall; Obs5.RelativeXOffset = 0.4f; Pattern.Obstacles.Add(Obs5);
		FObstacleSpawnData Obs6; Obs6.Lane = ELane::Center; Obs6.ObstacleType = EObstacleType::HighBarrier; Obs6.RelativeXOffset = 0.55f; Pattern.Obstacles.Add(Obs6);
		FObstacleSpawnData Obs7; Obs7.Lane = ELane::Left; Obs7.ObstacleType = EObstacleType::FullWall; Obs7.RelativeXOffset = 0.65f; Pattern.Obstacles.Add(Obs7);
		FObstacleSpawnData Obs8; Obs8.Lane = ELane::Right; Obs8.ObstacleType = EObstacleType::FullWall; Obs8.RelativeXOffset = 0.65f; Pattern.Obstacles.Add(Obs8);
		FObstacleSpawnData Obs9; Obs9.Lane = ELane::Center; Obs9.ObstacleType = EObstacleType::LowWall; Obs9.RelativeXOffset = 0.8f; Pattern.Obstacles.Add(Obs9);

		PredefinedPatterns.Add(Pattern);
	}

	// --- Additional Patterns for Variety ---

	// Slide Right (difficulty 0)
	{ FObstaclePattern P; P.PatternName = TEXT("Slide Right"); P.MinDifficultyLevel = 0; P.SelectionWeight = 1.5f;
	  FObstacleSpawnData O; O.Lane = ELane::Right; O.ObstacleType = EObstacleType::HighBarrier; O.RelativeXOffset = 0.5f; P.Obstacles.Add(O); PredefinedPatterns.Add(P); }

	// Jump Left (difficulty 0)
	{ FObstaclePattern P; P.PatternName = TEXT("Jump Left"); P.MinDifficultyLevel = 0; P.SelectionWeight = 2.0f;
	  FObstacleSpawnData O; O.Lane = ELane::Left; O.ObstacleType = EObstacleType::LowWall; O.RelativeXOffset = 0.5f; P.Obstacles.Add(O); PredefinedPatterns.Add(P); }

	// Jump Right (difficulty 0)
	{ FObstaclePattern P; P.PatternName = TEXT("Jump Right"); P.MinDifficultyLevel = 0; P.SelectionWeight = 2.0f;
	  FObstacleSpawnData O; O.Lane = ELane::Right; O.ObstacleType = EObstacleType::LowWall; O.RelativeXOffset = 0.5f; P.Obstacles.Add(O); PredefinedPatterns.Add(P); }

	// Slide Center (difficulty 0)
	{ FObstaclePattern P; P.PatternName = TEXT("Slide Center"); P.MinDifficultyLevel = 0; P.SelectionWeight = 1.5f;
	  FObstacleSpawnData O; O.Lane = ELane::Center; O.ObstacleType = EObstacleType::HighBarrier; O.RelativeXOffset = 0.5f; P.Obstacles.Add(O); PredefinedPatterns.Add(P); }

	// Double Jump (difficulty 1)
	{ FObstaclePattern P; P.PatternName = TEXT("Double Jump"); P.MinDifficultyLevel = 1; P.SelectionWeight = 1.6f;
	  FObstacleSpawnData O1; O1.Lane = ELane::Center; O1.ObstacleType = EObstacleType::LowWall; O1.RelativeXOffset = 0.3f; P.Obstacles.Add(O1);
	  FObstacleSpawnData O2; O2.Lane = ELane::Center; O2.ObstacleType = EObstacleType::LowWall; O2.RelativeXOffset = 0.6f; P.Obstacles.Add(O2); PredefinedPatterns.Add(P); }

	// Double Slide (difficulty 1)
	{ FObstaclePattern P; P.PatternName = TEXT("Double Slide"); P.MinDifficultyLevel = 1; P.SelectionWeight = 1.1f;
	  FObstacleSpawnData O1; O1.Lane = ELane::Center; O1.ObstacleType = EObstacleType::HighBarrier; O1.RelativeXOffset = 0.3f; P.Obstacles.Add(O1);
	  FObstacleSpawnData O2; O2.Lane = ELane::Center; O2.ObstacleType = EObstacleType::HighBarrier; O2.RelativeXOffset = 0.6f; P.Obstacles.Add(O2); PredefinedPatterns.Add(P); }

	// Jump Slide Combo (difficulty 1)
	{ FObstaclePattern P; P.PatternName = TEXT("Jump Slide Combo"); P.MinDifficultyLevel = 1; P.SelectionWeight = 1.5f;
	  FObstacleSpawnData O1; O1.Lane = ELane::Center; O1.ObstacleType = EObstacleType::LowWall; O1.RelativeXOffset = 0.35f; P.Obstacles.Add(O1);
	  FObstacleSpawnData O2; O2.Lane = ELane::Center; O2.ObstacleType = EObstacleType::HighBarrier; O2.RelativeXOffset = 0.65f; P.Obstacles.Add(O2); PredefinedPatterns.Add(P); }

	// Slide Jump Combo (difficulty 1)
	{ FObstaclePattern P; P.PatternName = TEXT("Slide Jump Combo"); P.MinDifficultyLevel = 1; P.SelectionWeight = 1.2f;
	  FObstacleSpawnData O1; O1.Lane = ELane::Center; O1.ObstacleType = EObstacleType::HighBarrier; O1.RelativeXOffset = 0.35f; P.Obstacles.Add(O1);
	  FObstacleSpawnData O2; O2.Lane = ELane::Center; O2.ObstacleType = EObstacleType::LowWall; O2.RelativeXOffset = 0.65f; P.Obstacles.Add(O2); PredefinedPatterns.Add(P); }

	// Side Wall Left (difficulty 0)
	{ FObstaclePattern P; P.PatternName = TEXT("Side Wall Left"); P.MinDifficultyLevel = 0; P.SelectionWeight = 1.0f;
	  FObstacleSpawnData O; O.Lane = ELane::Left; O.ObstacleType = EObstacleType::FullWall; O.RelativeXOffset = 0.5f; P.Obstacles.Add(O); PredefinedPatterns.Add(P); }

	// Side Wall Right (difficulty 0)
	{ FObstaclePattern P; P.PatternName = TEXT("Side Wall Right"); P.MinDifficultyLevel = 0; P.SelectionWeight = 1.0f;
	  FObstacleSpawnData O; O.Lane = ELane::Right; O.ObstacleType = EObstacleType::FullWall; O.RelativeXOffset = 0.5f; P.Obstacles.Add(O); PredefinedPatterns.Add(P); }

	// Corridor Left (difficulty 2)
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Corridor Left");
		Pattern.MinDifficultyLevel = 2;
		Pattern.SelectionWeight = 0.9f;

		FObstacleSpawnData O1; O1.Lane = ELane::Center; O1.ObstacleType = EObstacleType::FullWall; O1.RelativeXOffset = 0.2f; Pattern.Obstacles.Add(O1);
		FObstacleSpawnData O2; O2.Lane = ELane::Right; O2.ObstacleType = EObstacleType::FullWall; O2.RelativeXOffset = 0.2f; Pattern.Obstacles.Add(O2);
		FObstacleSpawnData O3; O3.Lane = ELane::Left; O3.ObstacleType = EObstacleType::LowWall; O3.RelativeXOffset = 0.45f; Pattern.Obstacles.Add(O3);
		FObstacleSpawnData O4; O4.Lane = ELane::Center; O4.ObstacleType = EObstacleType::FullWall; O4.RelativeXOffset = 0.7f; Pattern.Obstacles.Add(O4);
		FObstacleSpawnData O5; O5.Lane = ELane::Right; O5.ObstacleType = EObstacleType::FullWall; O5.RelativeXOffset = 0.7f; Pattern.Obstacles.Add(O5);
		PredefinedPatterns.Add(Pattern);
	}

	// Corridor Right (difficulty 2)
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Corridor Right");
		Pattern.MinDifficultyLevel = 2;
		Pattern.SelectionWeight = 0.9f;

		FObstacleSpawnData O1; O1.Lane = ELane::Center; O1.ObstacleType = EObstacleType::FullWall; O1.RelativeXOffset = 0.2f; Pattern.Obstacles.Add(O1);
		FObstacleSpawnData O2; O2.Lane = ELane::Left; O2.ObstacleType = EObstacleType::FullWall; O2.RelativeXOffset = 0.2f; Pattern.Obstacles.Add(O2);
		FObstacleSpawnData O3; O3.Lane = ELane::Right; O3.ObstacleType = EObstacleType::HighBarrier; O3.RelativeXOffset = 0.45f; Pattern.Obstacles.Add(O3);
		FObstacleSpawnData O4; O4.Lane = ELane::Center; O4.ObstacleType = EObstacleType::FullWall; O4.RelativeXOffset = 0.7f; Pattern.Obstacles.Add(O4);
		FObstacleSpawnData O5; O5.Lane = ELane::Left; O5.ObstacleType = EObstacleType::FullWall; O5.RelativeXOffset = 0.7f; Pattern.Obstacles.Add(O5);
		PredefinedPatterns.Add(Pattern);
	}

	// Corridor Center (difficulty 2)
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Corridor Center");
		Pattern.MinDifficultyLevel = 2;
		Pattern.SelectionWeight = 0.9f;

		FObstacleSpawnData O1; O1.Lane = ELane::Left; O1.ObstacleType = EObstacleType::FullWall; O1.RelativeXOffset = 0.2f; Pattern.Obstacles.Add(O1);
		FObstacleSpawnData O2; O2.Lane = ELane::Right; O2.ObstacleType = EObstacleType::FullWall; O2.RelativeXOffset = 0.2f; Pattern.Obstacles.Add(O2);
		FObstacleSpawnData O3; O3.Lane = ELane::Center; O3.ObstacleType = EObstacleType::LowWall; O3.RelativeXOffset = 0.45f; Pattern.Obstacles.Add(O3);
		FObstacleSpawnData O4; O4.Lane = ELane::Left; O4.ObstacleType = EObstacleType::FullWall; O4.RelativeXOffset = 0.7f; Pattern.Obstacles.Add(O4);
		FObstacleSpawnData O5; O5.Lane = ELane::Right; O5.ObstacleType = EObstacleType::FullWall; O5.RelativeXOffset = 0.7f; Pattern.Obstacles.Add(O5);
		PredefinedPatterns.Add(Pattern);
	}

	// Triple Jump (difficulty 2)
	{
		FObstaclePattern P; P.PatternName = TEXT("Triple Jump"); P.MinDifficultyLevel = 2; P.SelectionWeight = 1.5f;
		FObstacleSpawnData O1; O1.Lane = ELane::Left; O1.ObstacleType = EObstacleType::LowWall; O1.RelativeXOffset = 0.25f; P.Obstacles.Add(O1);
		FObstacleSpawnData O2; O2.Lane = ELane::Center; O2.ObstacleType = EObstacleType::LowWall; O2.RelativeXOffset = 0.5f; P.Obstacles.Add(O2);
		FObstacleSpawnData O3; O3.Lane = ELane::Right; O3.ObstacleType = EObstacleType::LowWall; O3.RelativeXOffset = 0.75f; P.Obstacles.Add(O3);
		PredefinedPatterns.Add(P);
	}

	// Triple Slide (difficulty 2)
	{
		FObstaclePattern P; P.PatternName = TEXT("Triple Slide"); P.MinDifficultyLevel = 2; P.SelectionWeight = 1.0f;
		FObstacleSpawnData O1; O1.Lane = ELane::Right; O1.ObstacleType = EObstacleType::HighBarrier; O1.RelativeXOffset = 0.25f; P.Obstacles.Add(O1);
		FObstacleSpawnData O2; O2.Lane = ELane::Center; O2.ObstacleType = EObstacleType::HighBarrier; O2.RelativeXOffset = 0.5f; P.Obstacles.Add(O2);
		FObstacleSpawnData O3; O3.Lane = ELane::Left; O3.ObstacleType = EObstacleType::HighBarrier; O3.RelativeXOffset = 0.75f; P.Obstacles.Add(O3);
		PredefinedPatterns.Add(P);
	}

	// Pinch Left (difficulty 2)
	{
		FObstaclePattern P; P.PatternName = TEXT("Pinch Left"); P.MinDifficultyLevel = 2; P.SelectionWeight = 0.85f;
		FObstacleSpawnData O1; O1.Lane = ELane::Center; O1.ObstacleType = EObstacleType::FullWall; O1.RelativeXOffset = 0.25f; P.Obstacles.Add(O1);
		FObstacleSpawnData O2; O2.Lane = ELane::Right; O2.ObstacleType = EObstacleType::FullWall; O2.RelativeXOffset = 0.25f; P.Obstacles.Add(O2);
		FObstacleSpawnData O3; O3.Lane = ELane::Left; O3.ObstacleType = EObstacleType::LowWall; O3.RelativeXOffset = 0.55f; P.Obstacles.Add(O3);
		FObstacleSpawnData O4; O4.Lane = ELane::Center; O4.ObstacleType = EObstacleType::HighBarrier; O4.RelativeXOffset = 0.75f; P.Obstacles.Add(O4);
		PredefinedPatterns.Add(P);
	}

	// Pinch Right (difficulty 2)
	{
		FObstaclePattern P; P.PatternName = TEXT("Pinch Right"); P.MinDifficultyLevel = 2; P.SelectionWeight = 0.85f;
		FObstacleSpawnData O1; O1.Lane = ELane::Center; O1.ObstacleType = EObstacleType::FullWall; O1.RelativeXOffset = 0.25f; P.Obstacles.Add(O1);
		FObstacleSpawnData O2; O2.Lane = ELane::Left; O2.ObstacleType = EObstacleType::FullWall; O2.RelativeXOffset = 0.25f; P.Obstacles.Add(O2);
		FObstacleSpawnData O3; O3.Lane = ELane::Right; O3.ObstacleType = EObstacleType::HighBarrier; O3.RelativeXOffset = 0.55f; P.Obstacles.Add(O3);
		FObstacleSpawnData O4; O4.Lane = ELane::Center; O4.ObstacleType = EObstacleType::LowWall; O4.RelativeXOffset = 0.75f; P.Obstacles.Add(O4);
		PredefinedPatterns.Add(P);
	}

	// Scatter Shot (difficulty 3)
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Scatter Shot");
		Pattern.MinDifficultyLevel = 3;
		Pattern.SelectionWeight = 0.9f;

		const ELane ScatterLanes[] = { ELane::Left, ELane::Right, ELane::Center, ELane::Left, ELane::Right, ELane::Center };
		const EObstacleType ScatterTypes[] = { EObstacleType::LowWall, EObstacleType::HighBarrier, EObstacleType::LowWall, 
		                                        EObstacleType::HighBarrier, EObstacleType::LowWall, EObstacleType::HighBarrier };
		const float ScatterX[] = { 0.12f, 0.28f, 0.42f, 0.58f, 0.72f, 0.88f };

		for (int32 i = 0; i < 6; i++)
		{
			FObstacleSpawnData Obs;
			Obs.Lane = ScatterLanes[i];
			Obs.ObstacleType = ScatterTypes[i];
			Obs.RelativeXOffset = ScatterX[i];
			Pattern.Obstacles.Add(Obs);
		}

		PredefinedPatterns.Add(Pattern);
	}

	// The Maze (difficulty 4)
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("The Maze");
		Pattern.MinDifficultyLevel = 4;
		Pattern.SelectionWeight = 0.7f;

		FObstacleSpawnData O1; O1.Lane = ELane::Left; O1.ObstacleType = EObstacleType::FullWall; O1.RelativeXOffset = 0.1f; Pattern.Obstacles.Add(O1);
		FObstacleSpawnData O2; O2.Lane = ELane::Center; O2.ObstacleType = EObstacleType::FullWall; O2.RelativeXOffset = 0.1f; Pattern.Obstacles.Add(O2);
		FObstacleSpawnData O3; O3.Lane = ELane::Right; O3.ObstacleType = EObstacleType::LowWall; O3.RelativeXOffset = 0.25f; Pattern.Obstacles.Add(O3);
		FObstacleSpawnData O4; O4.Lane = ELane::Center; O4.ObstacleType = EObstacleType::FullWall; O4.RelativeXOffset = 0.4f; Pattern.Obstacles.Add(O4);
		FObstacleSpawnData O5; O5.Lane = ELane::Right; O5.ObstacleType = EObstacleType::FullWall; O5.RelativeXOffset = 0.4f; Pattern.Obstacles.Add(O5);
		FObstacleSpawnData O6; O6.Lane = ELane::Left; O6.ObstacleType = EObstacleType::HighBarrier; O6.RelativeXOffset = 0.55f; Pattern.Obstacles.Add(O6);
		FObstacleSpawnData O7; O7.Lane = ELane::Left; O7.ObstacleType = EObstacleType::FullWall; O7.RelativeXOffset = 0.7f; Pattern.Obstacles.Add(O7);
		FObstacleSpawnData O8; O8.Lane = ELane::Right; O8.ObstacleType = EObstacleType::FullWall; O8.RelativeXOffset = 0.7f; Pattern.Obstacles.Add(O8);
		FObstacleSpawnData O9; O9.Lane = ELane::Center; O9.ObstacleType = EObstacleType::LowWall; O9.RelativeXOffset = 0.85f; Pattern.Obstacles.Add(O9);

		PredefinedPatterns.Add(Pattern);
	}

	// Speed Bumps (difficulty 3)
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Speed Bumps");
		Pattern.MinDifficultyLevel = 3;
		Pattern.SelectionWeight = 1.3f;

		const float BumpX[] = { 0.15f, 0.3f, 0.45f, 0.6f, 0.75f, 0.9f };
		const ELane BumpLanes[] = { ELane::Center, ELane::Left, ELane::Right, ELane::Center, ELane::Left, ELane::Right };

		for (int32 i = 0; i < 6; i++)
		{
			FObstacleSpawnData Obs;
			Obs.Lane = BumpLanes[i];
			Obs.ObstacleType = EObstacleType::LowWall;
			Obs.RelativeXOffset = BumpX[i];
			Pattern.Obstacles.Add(Obs);
		}

		PredefinedPatterns.Add(Pattern);
	}

	// Low Bridge (difficulty 3)
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Low Bridge");
		Pattern.MinDifficultyLevel = 3;
		Pattern.SelectionWeight = 0.85f;

		const float BridgeX[] = { 0.15f, 0.3f, 0.45f, 0.6f, 0.75f, 0.9f };
		const ELane BridgeLanes[] = { ELane::Center, ELane::Right, ELane::Left, ELane::Center, ELane::Right, ELane::Left };

		for (int32 i = 0; i < 6; i++)
		{
			FObstacleSpawnData Obs;
			Obs.Lane = BridgeLanes[i];
			Obs.ObstacleType = EObstacleType::HighBarrier;
			Obs.RelativeXOffset = BridgeX[i];
			Pattern.Obstacles.Add(Obs);
		}

		PredefinedPatterns.Add(Pattern);
	}

	// Staircase Up (difficulty 3)
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Staircase Up");
		Pattern.MinDifficultyLevel = 3;
		Pattern.SelectionWeight = 0.8f;

		FObstacleSpawnData O1; O1.Lane = ELane::Center; O1.ObstacleType = EObstacleType::LowWall; O1.RelativeXOffset = 0.15f; Pattern.Obstacles.Add(O1);
		FObstacleSpawnData O2; O2.Lane = ELane::Left; O2.ObstacleType = EObstacleType::FullWall; O2.RelativeXOffset = 0.35f; Pattern.Obstacles.Add(O2);
		FObstacleSpawnData O3; O3.Lane = ELane::Center; O3.ObstacleType = EObstacleType::HighBarrier; O3.RelativeXOffset = 0.35f; Pattern.Obstacles.Add(O3);
		FObstacleSpawnData O4; O4.Lane = ELane::Left; O4.ObstacleType = EObstacleType::FullWall; O4.RelativeXOffset = 0.55f; Pattern.Obstacles.Add(O4);
		FObstacleSpawnData O5; O5.Lane = ELane::Center; O5.ObstacleType = EObstacleType::FullWall; O5.RelativeXOffset = 0.55f; Pattern.Obstacles.Add(O5);
		FObstacleSpawnData O6; O6.Lane = ELane::Right; O6.ObstacleType = EObstacleType::LowWall; O6.RelativeXOffset = 0.75f; Pattern.Obstacles.Add(O6);

		PredefinedPatterns.Add(Pattern);
	}

	// The Serpent (difficulty 4)
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("The Serpent");
		Pattern.MinDifficultyLevel = 4;
		Pattern.SelectionWeight = 0.75f;

		const float SnakeX[] = { 0.1f, 0.25f, 0.4f, 0.55f, 0.7f, 0.85f };
		const ELane SnakeLanes[] = { ELane::Center, ELane::Right, ELane::Center, ELane::Left, ELane::Right, ELane::Center };

		for (int32 i = 0; i < 6; i++)
		{
			FObstacleSpawnData Obs;
			Obs.Lane = SnakeLanes[i];
			Obs.ObstacleType = EObstacleType::FullWall;
			Obs.RelativeXOffset = SnakeX[i];
			Pattern.Obstacles.Add(Obs);
		}

		PredefinedPatterns.Add(Pattern);
	}

	// Quick Reflexes (difficulty 5)
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("Quick Reflexes");
		Pattern.MinDifficultyLevel = 5;
		Pattern.SelectionWeight = 0.6f;

		const float QuickX[] = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f };
		const ELane QuickLanes[] = { ELane::Left, ELane::Center, ELane::Right, ELane::Center, 
		                              ELane::Left, ELane::Right, ELane::Center, ELane::Left, ELane::Right };
		const EObstacleType QuickTypes[] = { EObstacleType::LowWall, EObstacleType::HighBarrier, EObstacleType::LowWall,
		                                      EObstacleType::HighBarrier, EObstacleType::LowWall, EObstacleType::HighBarrier,
		                                      EObstacleType::LowWall, EObstacleType::HighBarrier, EObstacleType::LowWall };

		for (int32 i = 0; i < 9; i++)
		{
			FObstacleSpawnData Obs;
			Obs.Lane = QuickLanes[i];
			Obs.ObstacleType = QuickTypes[i];
			Obs.RelativeXOffset = QuickX[i];
			Pattern.Obstacles.Add(Obs);
		}

		PredefinedPatterns.Add(Pattern);
	}

	// The Finale (difficulty 6)
	{
		FObstaclePattern Pattern;
		Pattern.PatternName = TEXT("The Finale");
		Pattern.MinDifficultyLevel = 6;
		Pattern.SelectionWeight = 0.5f;

		const float FinaleX[] = { 0.08f, 0.18f, 0.28f, 0.38f, 0.48f, 0.58f, 0.68f, 0.78f, 0.88f };
		const ELane FinaleLanes[] = { ELane::Center, ELane::Left, ELane::Right, ELane::Center, 
		                               ELane::Left, ELane::Right, ELane::Center, ELane::Left, ELane::Right };
		const EObstacleType FinaleTypes[] = { EObstacleType::FullWall, EObstacleType::LowWall, EObstacleType::HighBarrier,
		                                       EObstacleType::FullWall, EObstacleType::HighBarrier, EObstacleType::LowWall,
		                                       EObstacleType::FullWall, EObstacleType::LowWall, EObstacleType::HighBarrier };

		for (int32 i = 0; i < 9; i++)
		{
			FObstacleSpawnData Obs;
			Obs.Lane = FinaleLanes[i];
			Obs.ObstacleType = FinaleTypes[i];
			Obs.RelativeXOffset = FinaleX[i];
			Pattern.Obstacles.Add(Obs);
		}

		PredefinedPatterns.Add(Pattern);
	}
}
