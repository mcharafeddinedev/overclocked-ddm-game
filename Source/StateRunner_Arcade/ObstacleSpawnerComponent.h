#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BaseObstacle.h"
#include "ObstacleSpawnerComponent.generated.h"

class ABaseObstacle;

/**
 * Spawn data for a single obstacle within a segment.
 * Used by patterns and procedural generation.
 */
USTRUCT(BlueprintType)
struct FObstacleSpawnData
{
	GENERATED_BODY()

	/** Type of obstacle to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Obstacle")
	EObstacleType ObstacleType = EObstacleType::LowWall;

	/** Which lane to spawn in */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Obstacle")
	ELane Lane = ELane::Center;

	/** 
	 * X offset within segment (0.0 = segment start, 1.0 = segment end).
	 * Actual position: SegmentStartX + (RelativeXOffset * SegmentLength)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Obstacle", meta=(ClampMin="0.0", ClampMax="1.0"))
	float RelativeXOffset = 0.5f;

	/** Optional Z position offset (for elevated obstacles) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Obstacle")
	float ZOffset = 0.0f;
};

/**
 * Predefined obstacle pattern for a track segment.
 * Stores a set of obstacles that form a fair, tested configuration.
 */
USTRUCT(BlueprintType)
struct FObstaclePattern
{
	GENERATED_BODY()

	/** Display name for debugging */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern")
	FString PatternName = TEXT("Unnamed Pattern");

	/** Array of obstacles in this pattern */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern")
	TArray<FObstacleSpawnData> Obstacles;

	/** Minimum difficulty level to use this pattern (0 = always available) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="0"))
	int32 MinDifficultyLevel = 0;

	/** Weight for random selection (higher = more likely) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="0.1"))
	float SelectionWeight = 1.0f;
};

/**
 * Tutorial obstacle type enum for clarity.
 */
UENUM(BlueprintType)
enum class ETutorialObstacleType : uint8
{
	/** First: FullWall in center - forces lane switch (left or right) */
	LaneSwitch		UMETA(DisplayName = "Lane Switch (Center FullWall)"),
	
	/** Second: LowWall in all 3 lanes - forces jump */
	Jump			UMETA(DisplayName = "Jump (All Lanes LowWall)"),
	
	/** Third: HighBarrier in all 3 lanes - forces slide */
	Slide			UMETA(DisplayName = "Slide (All Lanes HighBarrier)")
};

/**
 * Delegate broadcast when an obstacle is spawned.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObstacleSpawned, ABaseObstacle*, Obstacle, const FObstacleSpawnData&, SpawnData);

/**
 * Delegate broadcast when an obstacle is deactivated (returned to pool).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObstacleDeactivated, ABaseObstacle*, Obstacle);

/**
 * Delegate broadcast when a tutorial obstacle is about to be reached.
 * UI can bind to this to show prompts (SWITCH LANES!, JUMP!, SLIDE!)
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTutorialPrompt, ETutorialObstacleType, PromptType);

/**
 * Delegate broadcast when tutorial is complete.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTutorialComplete);

/**
 * Handles obstacle spawning, pooling, and pattern generation.
 * Lives on the GameMode. Supports both predefined patterns and
 * procedural generation, always keeping at least one clear path.
 */
UCLASS(ClassGroup=(StateRunner), meta=(BlueprintSpawnableComponent))
class STATERUNNER_ARCADE_API UObstacleSpawnerComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Constructor */
	UObstacleSpawnerComponent();

protected:

	/** Called when the game starts */
	virtual void BeginPlay() override;

	// --- Tutorial Configuration ---

protected:

	/**
	 * Whether to spawn tutorial obstacles at game start.
	 * If true, 3 tutorial obstacles spawn before procedural generation begins.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tutorial")
	bool bEnableTutorial = true;

	/**
	 * Duration of the intro segment before tutorial obstacles appear (in seconds).
	 * During this time, only pickups spawn (no obstacles) to let players learn movement.
	 * 
	 * This needs to be long enough for:
	 * - Controls tutorial popups (4 popups x ~3s each + gaps = ~16s)
	 * - Pickup patterns for learning movement
	 * 
	 * At base speed 1000 u/s: 18 seconds = 18000 units of distance traveled.
	 * Tutorial obstacles are pushed back by this distance.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tutorial|Intro", meta=(ClampMin="0.0", ClampMax="45.0"))
	float IntroSegmentDuration = 30.0f;  // ~30 seconds intro for controls tutorial (2s delay + 4x3s popups + 3x1s gaps = ~17s)

	/**
	 * Base scroll speed used for calculating intro segment distance.
	 * Should match WorldScrollComponent's BaseScrollSpeed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tutorial|Intro")
	float BaseScrollSpeed = 1000.0f;

	/**
	 * X position for the first tutorial obstacle (Lane Switch - center FullWall).
	 * This is the world X position where the obstacle spawns.
	 * 
	 * TIMING CALCULATION (Player at X=-5000, Base Speed 1000 u/s):
	 * - Distance to player = ObstacleX - (-5000)
	 * - Time = Distance / Speed
	 * 
	 * With IntroSegmentDuration of 10s (10000 units offset):
	 * - Effective X = TutorialObstacle1_X + (IntroSegmentDuration * BaseScrollSpeed)
	 * - X=7500 → 12500 units from player → 12.5 seconds after GO (10s intro + 2.5s to obstacle)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tutorial", meta=(EditCondition="bEnableTutorial"))
	float TutorialObstacle1_X = -2500.0f;

	/**
	 * X position for the second tutorial obstacle (Jump - all lanes LowWall).
	 * 
	 * With IntroSegmentDuration of 10s:
	 * - Effective X = TutorialObstacle2_X + (10000)
	 * - Appears ~14.5 seconds after GO (10s intro + 4.5s)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tutorial", meta=(EditCondition="bEnableTutorial"))
	float TutorialObstacle2_X = -500.0f;

	/**
	 * X position for the third tutorial obstacle (Slide - all lanes HighBarrier).
	 * 
	 * With IntroSegmentDuration of 10s:
	 * - Effective X = TutorialObstacle3_X + (10000)
	 * - Appears ~16.5 seconds after GO (10s intro + 6.5s)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tutorial", meta=(EditCondition="bEnableTutorial"))
	float TutorialObstacle3_X = 1500.0f;

	/**
	 * Time (in seconds) before obstacle reaches player to show UI prompt.
	 * Prompt triggers when obstacle X position is this many seconds away.
	 * Lower = prompts appear closer to obstacle (less warning time).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tutorial", meta=(EditCondition="bEnableTutorial", ClampMin="0.25", ClampMax="3.0"))
	float TutorialPromptLeadTime = 1.0f;

	/**
	 * Whether tutorial has been completed this session.
	 * Set to true after all 3 tutorial obstacles are passed.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Tutorial")
	bool bTutorialComplete = false;

	/**
	 * Track which tutorial prompts have been shown.
	 * Prevents duplicate prompts.
	 */
	UPROPERTY()
	TArray<ETutorialObstacleType> ShownTutorialPrompts;

	/**
	 * Tutorial obstacles that have been spawned (for tracking).
	 */
	UPROPERTY()
	TArray<TObjectPtr<ABaseObstacle>> TutorialObstacles;

	// --- Obstacle Configuration ---

protected:

	/**
	 * Blueprint class for LowWall obstacles (jump over).
	 * Assign BP_JumpHurdle or similar.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Obstacle Config|Classes")
	TSubclassOf<ABaseObstacle> LowWallClass;

	/**
	 * Blueprint class for HighBarrier obstacles (slide under).
	 * Assign BP_SlideHurdle or similar.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Obstacle Config|Classes")
	TSubclassOf<ABaseObstacle> HighBarrierClass;

	/**
	 * Blueprint class for FullWall obstacles (change lanes).
	 * Assign BP_SlabObstacle or similar.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Obstacle Config|Classes")
	TSubclassOf<ABaseObstacle> FullWallClass;

	/**
	 * Initial pool size per obstacle type.
	 * Pools expand automatically if needed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Obstacle Config|Pooling", meta=(ClampMin="10", ClampMax="200"))
	int32 InitialPoolSizePerType = 50;

	/**
	 * Number of obstacles to add when pool expands.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Obstacle Config|Pooling", meta=(ClampMin="5", ClampMax="50"))
	int32 PoolExpansionSize = 10;

	// --- Lane Configuration ---

protected:

	/**
	 * Y-axis positions for each lane.
	 * Based on track geometry: 950 units wide, 3 equal lanes.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lane Config")
	float LeftLaneY = -316.67f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lane Config")
	float CenterLaneY = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lane Config")
	float RightLaneY = 316.67f;

	/**
	 * Z position for obstacles (floor level).
	 * Obstacles spawn at this Z, with their collision boxes adjusted appropriately.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lane Config")
	float ObstacleSpawnZ = 20.0f;

	// --- Segment Configuration ---

protected:

	/**
	 * Length of a track segment in units.
	 * Used to calculate obstacle X positions from relative offsets.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Segment Config")
	float SegmentLength = 6250.0f;

	/** Min X offset from segment start, prevents obstacles right at the boundary */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Segment Config", meta=(ClampMin="0.0", ClampMax="0.2"))
	float MinSpawnOffset = 0.02f;

	/** Max X offset, prevents obstacles right at the next segment boundary */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Segment Config", meta=(ClampMin="0.8", ClampMax="1.0"))
	float MaxSpawnOffset = 0.98f;

	/**
	 * Minimum X spacing between obstacles as a fraction of segment length (0.0 to 1.0).
	 * Obstacles will NEVER spawn closer than this to each other.
	 * 
	 * At 6250 unit segments:
	 * - 0.10 = 625 units (tight, may feel unfair for jump/slide combos)
	 * - 0.20 = 1250 units (comfortable, gives time to react between different actions)
	 * - 0.25 = 1562 units (generous spacing)
	 * 
	 * When a jump obstacle is followed by a slide (or vice versa), the player
	 * needs extra time to switch actions, so keep this high enough to be fair.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Segment Config", meta=(ClampMin="0.05", ClampMax="0.4"))
	float MinObstacleSpacing = 0.20f;

	// --- Type-Aware Spacing ---

	/**
	 * Extra spacing when a HighBarrier is followed by a LowWall in the same lane.
	 * Gives the player time to recover from sliding before needing to jump.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Segment Config|Type Spacing", meta=(ClampMin="0.0", ClampMax="0.3"))
	float SlideToJumpExtraSpacing = 0.15f;

	/**
	 * Extra spacing required when a LowWall is followed by a HighBarrier in the same lane.
	 * Player lands from jump and needs time to initiate slide.
	 * 
	 * At 6250 unit segments: 0.12 = 750 units extra spacing
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Segment Config|Type Spacing", meta=(ClampMin="0.0", ClampMax="0.3"))
	float JumpToSlideExtraSpacing = 0.12f;

	/**
	 * Enable type-aware spacing validation.
	 * When true, EnsureFairLayout will check for and fix unfair obstacle combinations.
	 * When false, only basic same-position checks are performed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Segment Config|Type Spacing")
	bool bEnableTypeAwareSpacing = true;

	// --- EMP Configuration ---

protected:

	/** How far ahead of the player (in world units) the EMP clears obstacles */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EMP Config", meta=(ClampMin="1000", ClampMax="50000"))
	float EMPClearRange = 15000.0f;

	/**
	 * The player's fixed X position in world space.
	 * Used to calculate EMP clear range relative to player.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EMP Config")
	float PlayerXPosition = -5000.0f;

	// --- Difficulty Configuration ---

protected:

	/** Min obstacles per segment at the start. Increases with difficulty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="1", ClampMax="15"))
	int32 MinObstaclesPerSegment = 3;

	/** Max obstacles per segment. Caps endgame density. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="5", ClampMax="35"))
	int32 MaxObstaclesPerSegment = 16;

	/**
	 * Current difficulty level (increases over time).
	 * Affects obstacle count and pattern selection.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Difficulty")
	int32 CurrentDifficultyLevel = 0;

	/**
	 * Number of segments spawned since game start.
	 * Used to track difficulty progression.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Difficulty")
	int32 SegmentsSpawned = 0;

	/**
	 * Counter for empty segment spacing.
	 * When this reaches EmptySegmentInterval, spawn an empty "breather" segment.
	 */
	int32 SegmentsSinceEmpty = 0;

	/**
	 * How often to spawn a "breather" segment (partial gap before obstacles).
	 * Every N segments, obstacles start later in the segment, creating a short gap.
	 * Set to 0 to disable breather segments entirely.
	 * 
	 * Breather segments are disabled at DifficultyToDisableBreathers and above.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0", ClampMax="15"))
	int32 EmptySegmentInterval = 5;

	/** Difficulty level at which breather segments stop appearing entirely */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="1", ClampMax="8"))
	int32 DifficultyToDisableBreathers = 6;

	/**
	 * Fraction of segment that's empty during breather segments at Difficulty 0.
	 * Shrinks as difficulty increases (see BreatherGapShrinkPerLevel).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0.1", ClampMax="0.6"))
	float BreatherGapFraction = 0.25f;

	/**
	 * How much the breather gap shrinks per difficulty level.
	 * Gap = BreatherGapFraction - (DifficultyLevel * this value).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0.0", ClampMax="0.1"))
	float BreatherGapShrinkPerLevel = 0.04f;

	/** Floor for breather gap - won't shrink below this regardless of difficulty */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0.05", ClampMax="0.3"))
	float MinBreatherGapFraction = 0.1f;

	/**
	 * Base number of filler obstacles to add in gaps around patterns.
	 * These obstacles fill the empty space before/after a pattern cluster.
	 * Ensures the track feels populated even when using compact patterns.
	 * 
	 * At high difficulty, additional filler is added based on FillerPerDifficulty.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0", ClampMax="10"))
	int32 FillerObstacleCount = 3;

	/** Extra filler obstacles per difficulty level above DifficultyToDisableBreathers */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0", ClampMax="3"))
	int32 FillerPerDifficulty = 1;

	/** Segments between difficulty increases. Should match PickupSpawner's value. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="1", ClampMax="15"))
	int32 SegmentsPerDifficultyIncrease = 7;

	/**
	 * Obstacles added per difficulty level increase.
	 * Controls how aggressive the difficulty ramp is.
	 * 
	 * 1 = gentle ramp, 2 = moderate, 3+ = aggressive.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="1", ClampMax="5"))
	int32 ObstaclesPerDifficultyLevel = 1;

	/** Chance to use a predefined pattern vs procedural generation (0.0 to 1.0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0.0", ClampMax="1.0"))
	float PredefinedPatternChance = 0.5f;

	/**
	 * Enable maximum pattern variety mode.
	 * When true, the system tries to avoid repeating the same pattern back-to-back,
	 * cycling through all available patterns before repeating.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty")
	bool bEnablePatternVariety = true;

	/**
	 * Minimum number of different patterns to use before allowing repeats.
	 * Only applies when bEnablePatternVariety is true.
	 * Set to 0 to use all available patterns before repeating.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0", ClampMax="20"))
	int32 MinPatternVarietyCount = 0;

	// --- Predefined Patterns ---

protected:

	/**
	 * Array of predefined obstacle patterns.
	 * These are manually designed, tested patterns that guarantee fairness.
	 * Can be edited in Blueprint for easy pattern creation.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Patterns")
	TArray<FObstaclePattern> PredefinedPatterns;

	// --- Runtime State ---

protected:

	/**
	 * Object pool for LowWall obstacles (jump).
	 */
	UPROPERTY()
	TArray<TObjectPtr<ABaseObstacle>> LowWallPool;

	/**
	 * Object pool for HighBarrier obstacles (slide).
	 */
	UPROPERTY()
	TArray<TObjectPtr<ABaseObstacle>> HighBarrierPool;

	/**
	 * Object pool for FullWall obstacles (lane change).
	 */
	UPROPERTY()
	TArray<TObjectPtr<ABaseObstacle>> FullWallPool;

	/**
	 * Track currently active obstacles (for debugging/management).
	 */
	UPROPERTY()
	TArray<TObjectPtr<ABaseObstacle>> ActiveObstacles;

	/**
	 * Number of segments to skip at the start (for tutorial area).
	 * Tutorial obstacles spawn in this area instead of regular obstacles.
	 * 
	 * Set to 0 to disable tutorial segment skipping.
	 * Set to 1-2 to give the tutorial obstacles space.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tutorial")
	int32 TutorialSegmentsToSkip = 1;

	/**
	 * Counter for how many segments have been skipped for tutorial.
	 */
	int32 TutorialSegmentsSkipped = 0;

	/**
	 * Whether tutorial obstacles have been spawned this session.
	 */
	bool bHasSpawnedTutorialObstacles = false;

	/**
	 * Indices of recently used patterns (for variety tracking).
	 * Used to avoid repeating patterns when bEnablePatternVariety is true.
	 */
	UPROPERTY()
	TArray<int32> RecentlyUsedPatternIndices;

	/** Last pattern name used (for debug logging). */
	FString LastPatternName;

	// --- Debug Configuration ---

public:

	/**
	 * DEBUG: Force a 3-lane FullWall blockage into every spawned segment.
	 * Tests that EnsureFairLayout correctly detects and removes impassable obstacles.
	 * Also skips tutorial and sets difficulty to late-game levels.
	 * 
	 * When enabled, every segment will have 3 FullWalls injected across all lanes
	 * BEFORE the fairness check runs. The fairness check should then remove one,
	 * logging a Warning: "EnsureFairLayout: FullWall cluster blocks ALL 3 lanes!"
	 * 
	 * Can be set directly on this component, or automatically via the GameMode's
	 * bDebugEndgameBlockageTest master checkbox.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Debug")
	bool bDebugForce3LaneBlockage = false;

	// --- Events ---

public:

	/** Broadcast when an obstacle is spawned */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnObstacleSpawned OnObstacleSpawned;

	/** Broadcast when an obstacle is deactivated */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnObstacleDeactivated OnObstacleDeactivated;

	/** Broadcast when a tutorial prompt should be shown (SWITCH LANES!, JUMP!, SLIDE!) */
	UPROPERTY(BlueprintAssignable, Category="Events|Tutorial")
	FOnTutorialPrompt OnTutorialPrompt;

	/** Broadcast when all tutorial obstacles have been passed */
	UPROPERTY(BlueprintAssignable, Category="Events|Tutorial")
	FOnTutorialComplete OnTutorialComplete;

	// --- Public Functions ---

public:

	/**
	 * Spawn all obstacles for a track segment.
	 * Called by track spawning system when a new segment is created.
	 * 
	 * @param SegmentStartX World X position where segment starts
	 * @param SegmentEndX World X position where segment ends
	 * @param bIsTutorialSegment If true, skip spawning (tutorial has manual placement)
	 */
	UFUNCTION(BlueprintCallable, Category="Spawning")
	void SpawnObstaclesForSegment(float SegmentStartX, float SegmentEndX, bool bIsTutorialSegment = false);

	/**
	 * Clear all active obstacles (return to pool).
	 * Use when restarting the game.
	 */
	UFUNCTION(BlueprintCallable, Category="Spawning")
	void ClearAllObstacles();

	/**
	 * Deactivate all currently active obstacles (EMP functionality).
	 * Returns obstacles to pool - they can be reused for future spawning.
	 * Does NOT affect any other systems (spawning, tutorial, difficulty, etc.)
	 * 
	 * @return Number of obstacles that were deactivated
	 */
	UFUNCTION(BlueprintCallable, Category="Spawning")
	int32 DeactivateAllActiveObstacles();

	/**
	 * Get number of active obstacles.
	 */
	UFUNCTION(BlueprintPure, Category="Spawning")
	int32 GetActiveObstacleCount() const { return ActiveObstacles.Num(); }

	/**
	 * Get current difficulty level.
	 */
	UFUNCTION(BlueprintPure, Category="Difficulty")
	int32 GetCurrentDifficultyLevel() const { return CurrentDifficultyLevel; }

	/**
	 * Reset difficulty to starting values.
	 * Use when restarting the game.
	 */
	UFUNCTION(BlueprintCallable, Category="Difficulty")
	void ResetDifficulty();

	/**
	 * Get total number of predefined patterns.
	 */
	UFUNCTION(BlueprintPure, Category="Patterns")
	int32 GetPatternCount() const { return PredefinedPatterns.Num(); }

	/**
	 * Get names of all predefined patterns (for debug/display).
	 */
	UFUNCTION(BlueprintPure, Category="Patterns")
	TArray<FString> GetAllPatternNames() const;

	/**
	 * Get the last pattern name used.
	 */
	UFUNCTION(BlueprintPure, Category="Patterns")
	FString GetLastPatternName() const { return LastPatternName; }

	/**
	 * Log all pattern information to the output log.
	 * Useful for debugging and verifying pattern variety.
	 */
	UFUNCTION(BlueprintCallable, Category="Patterns")
	void LogAllPatterns() const;

	// --- Tutorial Functions ---

public:

	/**
	 * Spawns tutorial obstacles at game start (after countdown).
	 * Creates: Center FullWall, All LowWalls, All HighBarriers
	 * 
	 * Tutorial obstacles are offset by IntroSegmentDuration * BaseScrollSpeed
	 * to allow for an intro segment with only pickups.
	 */
	UFUNCTION(BlueprintCallable, Category="Tutorial")
	void SpawnTutorialObstacles();

	/**
	 * Check if tutorial is enabled.
	 */
	UFUNCTION(BlueprintPure, Category="Tutorial")
	bool IsTutorialEnabled() const { return bEnableTutorial; }

	/**
	 * Check if tutorial is complete.
	 */
	UFUNCTION(BlueprintPure, Category="Tutorial")
	bool IsTutorialComplete() const { return bTutorialComplete; }

	/**
	 * Check if we're still in the intro segment (before tutorial obstacles appear).
	 * Intro segment is pure pickups only - no obstacles.
	 */
	UFUNCTION(BlueprintPure, Category="Tutorial|Intro")
	bool IsInIntroSegment() const { return bEnableTutorial && !bTutorialComplete && !bHasSpawnedTutorialObstacles; }

	/**
	 * Get the intro segment offset distance (IntroSegmentDuration * BaseScrollSpeed).
	 * This is how far tutorial obstacles are pushed back to make room for the intro.
	 */
	UFUNCTION(BlueprintPure, Category="Tutorial|Intro")
	float GetIntroSegmentOffset() const { return IntroSegmentDuration * BaseScrollSpeed; }

	/**
	 * Get the intro segment duration in seconds.
	 */
	UFUNCTION(BlueprintPure, Category="Tutorial|Intro")
	float GetIntroSegmentDuration() const { return IntroSegmentDuration; }

	/**
	 * Mark tutorial as complete.
	 * Called when player passes all tutorial obstacles.
	 */
	UFUNCTION(BlueprintCallable, Category="Tutorial")
	void CompleteTutorial();

	/**
	 * Reset tutorial state for game restart.
	 */
	UFUNCTION(BlueprintCallable, Category="Tutorial")
	void ResetTutorial();

	/**
	 * Checks tutorial obstacle positions and fires UI prompt events when player approaches.
	 * 
	 * @param CurrentScrollSpeed Current world scroll speed
	 * @param PlayerX Player's X position (typically -5000)
	 */
	UFUNCTION(BlueprintCallable, Category="Tutorial")
	void CheckTutorialPrompts(float CurrentScrollSpeed, float PlayerX = -5000.0f);

protected:

	/**
	 * Spawn a single tutorial obstacle configuration.
	 * 
	 * @param WorldX X position to spawn at
	 * @param TutorialType Type of tutorial obstacle to spawn
	 */
	void SpawnTutorialObstacleSet(float WorldX, ETutorialObstacleType TutorialType);

	// --- Pattern Functions ---

protected:

	/**
	 * Generate obstacle layout for a segment using predefined pattern.
	 * Selects a pattern based on difficulty and weight.
	 * 
	 * @param OutObstacles Array to fill with obstacle spawn data
	 * @return True if a valid pattern was selected
	 */
	bool GenerateFromPattern(TArray<FObstacleSpawnData>& OutObstacles);

	/**
	 * Generate obstacle layout procedurally.
	 * Creates random obstacles while ensuring fairness.
	 * 
	 * @param OutObstacles Array to fill with obstacle spawn data
	 * @param ObstacleCount Number of obstacles to generate
	 */
	void GenerateProcedural(TArray<FObstacleSpawnData>& OutObstacles, int32 ObstacleCount);

	/**
	 * Calculate number of obstacles for current difficulty.
	 * 
	 * @return Number of obstacles to spawn
	 */
	int32 CalculateObstacleCount() const;

	/**
	 * Ensure obstacle layout is fair (at least one clear path exists).
	 * Removes obstacles to create a path if necessary.
	 * 
	 * @param Obstacles Array of obstacles to validate/fix
	 */
	void EnsureFairLayout(TArray<FObstacleSpawnData>& Obstacles);

	/**
	 * Select a random pattern based on difficulty and weights.
	 * Respects variety settings to avoid repeating patterns.
	 * 
	 * @param OutPatternIndex Index of selected pattern (for variety tracking)
	 * @return Pointer to selected pattern, or nullptr if none available
	 */
	const FObstaclePattern* SelectPattern(int32& OutPatternIndex);

	// --- Pooling Functions ---

protected:

	/**
	 * Initialize all obstacle pools (one per type).
	 * Creates initial pools of deactivated obstacles.
	 */
	void InitializePools();

	/**
	 * Initialize a single pool for a specific obstacle type.
	 * 
	 * @param Type The obstacle type to initialize pool for
	 */
	void InitializePoolForType(EObstacleType Type);

	/**
	 * Get an obstacle from the appropriate pool (or create new if pool empty).
	 * 
	 * @param Type The type of obstacle needed
	 * @return Obstacle ready for activation
	 */
	ABaseObstacle* GetObstacleFromPool(EObstacleType Type);

	/**
	 * Get the pool array for a specific obstacle type.
	 * 
	 * @param Type The obstacle type
	 * @return Reference to the pool array
	 */
	TArray<TObjectPtr<ABaseObstacle>>& GetPoolForType(EObstacleType Type);

	/**
	 * Get the Blueprint class for a specific obstacle type.
	 * 
	 * @param Type The obstacle type
	 * @return The TSubclassOf for spawning
	 */
	TSubclassOf<ABaseObstacle> GetClassForType(EObstacleType Type) const;

	/**
	 * Expand the pool for a specific type by PoolExpansionSize.
	 * 
	 * @param Type The obstacle type to expand pool for
	 */
	void ExpandPool(EObstacleType Type);

	/**
	 * Spawn a new obstacle actor of a specific type (used for pool initialization/expansion).
	 * 
	 * @param Type The obstacle type to spawn
	 * @return Newly spawned obstacle (deactivated)
	 */
	ABaseObstacle* SpawnObstacleActor(EObstacleType Type);

	// --- Helper Functions ---

protected:

	/**
	 * Get Y position for a lane.
	 * 
	 * @param Lane Lane enum value
	 * @return Y-axis position
	 */
	float GetLaneYPosition(ELane Lane) const;

	/**
	 * Get random lane.
	 * 
	 * @return Random lane enum value
	 */
	ELane GetRandomLane() const;

	/**
	 * Get random obstacle type.
	 * 
	 * @return Random obstacle type enum value
	 */
	EObstacleType GetRandomObstacleType() const;

	/**
	 * Update difficulty based on segments spawned.
	 */
	void UpdateDifficulty();

	/**
	 * Check if current segment should be a breather segment.
	 * Breather segments have a gap at the start before obstacles spawn.
	 * 
	 * @return True if this segment should be a breather (partial gap)
	 */
	bool ShouldBeBreatherSegment();

	/**
	 * Calculate the effective breather gap fraction for current difficulty.
	 * Gap shrinks as difficulty increases, making breathers shorter at higher speeds.
	 * 
	 * @return Gap fraction (0.0-1.0) - portion of segment that's empty at start
	 */
	float GetEffectiveBreatherGapFraction() const;

	/**
	 * Add filler obstacles to fill gaps around a pattern.
	 * Places obstacles in the empty space before/after the pattern cluster.
	 * 
	 * @param OutObstacles Array to add filler obstacles to
	 * @param PatternMinX Minimum X offset of the pattern (0.0-1.0)
	 * @param PatternMaxX Maximum X offset of the pattern (0.0-1.0)
	 */
	void AddFillerObstacles(TArray<FObstacleSpawnData>& OutObstacles, float PatternMinX, float PatternMaxX);

	/**
	 * Create default patterns if none are configured.
	 * Called in BeginPlay if PredefinedPatterns is empty.
	 */
	void CreateDefaultPatterns();

	/**
	 * Check if two obstacle types require extra spacing when in sequence (same lane).
	 * Returns the total minimum spacing required between the two obstacles.
	 * 
	 * @param FirstType The obstacle encountered first (closer to segment start)
	 * @param SecondType The obstacle encountered second (further into segment)
	 * @return Minimum relative X spacing required (base spacing + any extra for type combo)
	 */
	float GetRequiredSpacingForTypes(EObstacleType FirstType, EObstacleType SecondType) const;

	/**
	 * Validate obstacle layout for type-aware spacing violations.
	 * Returns pairs of obstacle indices that are too close and need fixing.
	 * 
	 * @param Obstacles Array of obstacles to validate
	 * @param OutViolations Output: Array of pairs (first index, second index) for violations
	 */
	void FindTypeSpacingViolations(const TArray<FObstacleSpawnData>& Obstacles, TArray<TPair<int32, int32>>& OutViolations) const;

	/**
	 * Fix type-aware spacing violations by adjusting obstacle positions or types.
	 * Called as part of EnsureFairLayout when bEnableTypeAwareSpacing is true.
	 * 
	 * @param Obstacles Array of obstacles to fix (modified in place)
	 */
	void FixTypeSpacingViolations(TArray<FObstacleSpawnData>& Obstacles);
};
