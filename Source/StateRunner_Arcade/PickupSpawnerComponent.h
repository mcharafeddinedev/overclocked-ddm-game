#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BaseObstacle.h" // For ELane enum
#include "BasePickup.h"   // For EPickupType enum
#include "PickupSpawnerComponent.generated.h"

/**
 * Height level for pickup spawning.
 * Maps to Z-offsets based on how high the player can jump.
 */
UENUM(BlueprintType)
enum class EPickupHeight : uint8
{
	/** Ground level - no jump required (ZOffset = 0) */
	Ground		UMETA(DisplayName = "Ground"),
	
	/** Low air - can grab during jump arc (ZOffset = 100) */
	LowAir		UMETA(DisplayName = "Low Air"),
	
	/** Mid air - requires decent jump timing (ZOffset = 200) */
	MidAir		UMETA(DisplayName = "Mid Air"),
	
	/** High air - requires near-max jump (ZOffset = 300) */
	HighAir		UMETA(DisplayName = "High Air"),
	
	/** Apex - requires max jump hold, grab at peak (ZOffset = 350) */
	Apex		UMETA(DisplayName = "Apex")
};

/**
 * Spawn data for a single pickup within a segment.
 */
USTRUCT(BlueprintType)
struct FPickupSpawnData
{
	GENERATED_BODY()

	/** Which lane to spawn in */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pickup")
	ELane Lane = ELane::Center;

	/** 
	 * X offset within segment (0.0 = segment start, 1.0 = segment end).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pickup", meta=(ClampMin="0.0", ClampMax="1.0"))
	float RelativeXOffset = 0.5f;

	/** 
	 * Height level for the pickup (semantic - easier for pattern design).
	 * This is converted to ZOffset during spawning.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pickup")
	EPickupHeight HeightLevel = EPickupHeight::Ground;

	/** 
	 * Z position offset (height above floor).
	 * If HeightLevel is not Ground, this is auto-calculated from HeightLevel.
	 * Manual ZOffset is used when HeightLevel is Ground.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pickup")
	float ZOffset = 0.0f;
	
	/** Get the effective Z offset (considers HeightLevel) */
	float GetEffectiveZOffset() const
	{
		// If using a height level, return the corresponding offset
		switch (HeightLevel)
		{
			case EPickupHeight::Ground:  return ZOffset;  // Use manual offset for ground
			case EPickupHeight::LowAir:  return 100.0f;
			case EPickupHeight::MidAir:  return 200.0f;
			case EPickupHeight::HighAir: return 300.0f;
			case EPickupHeight::Apex:    return 350.0f;
			default:                     return ZOffset;
		}
	}
};

/**
 * Pickup pattern type for mathematical formations.
 * Patterns marked with "3D" incorporate vertical (height) variation.
 */
UENUM(BlueprintType)
enum class EPickupPatternType : uint8
{
	// --- Ground Patterns (2D - Lane + Position) ---
	
	/** Random distribution (default) */
	Random			UMETA(DisplayName = "Random"),
	
	/** Line of pickups in a single lane */
	Trail			UMETA(DisplayName = "Trail"),
	
	/** Arc/curve across lanes */
	Arc				UMETA(DisplayName = "Arc"),
	
	/** Zigzag across lanes */
	Zigzag			UMETA(DisplayName = "Zigzag"),
	
	/** Tight cluster of pickups */
	Cluster			UMETA(DisplayName = "Cluster"),
	
	/** Diamond formation */
	Diamond			UMETA(DisplayName = "Diamond"),
	
	/** Wave pattern (sine-like) */
	Wave			UMETA(DisplayName = "Wave"),
	
	/** Diagonal line across lanes */
	Diagonal		UMETA(DisplayName = "Diagonal"),
	
	/** All three lanes at same X */
	TripleLine		UMETA(DisplayName = "Triple Line"),
	
	/** Scattered bonus formation */
	Scatter			UMETA(DisplayName = "Scatter"),
	
	// --- Aerial Patterns (3D - Lane + Position + Height) ---
	
	/** Vertical arc - pickups go up then down (jump timing) */
	VerticalArc		UMETA(DisplayName = "Vertical Arc"),
	
	/** Ascending line - ground to apex in one lane */
	Ascending		UMETA(DisplayName = "Ascending"),
	
	/** Descending line - apex to ground in one lane */
	Descending		UMETA(DisplayName = "Descending"),
	
	/** 3D helix/spiral - zigzag with height oscillation */
	Helix			UMETA(DisplayName = "Helix"),
	
	/** Jump arc - follows natural jump trajectory */
	JumpArc			UMETA(DisplayName = "Jump Arc"),
	
	/** Staircase - ascending diagonally across lanes */
	Staircase		UMETA(DisplayName = "Staircase"),
	
	/** Sky trail - high altitude line (requires sustained jump) */
	SkyTrail		UMETA(DisplayName = "Sky Trail"),
	
	/** Tower - vertical stack in one lane (requires multiple jumps/timing) */
	Tower			UMETA(DisplayName = "Tower"),
	
	/** Rainbow - 3D arc across lanes AND height */
	Rainbow			UMETA(DisplayName = "Rainbow"),
	
	/** Bouncing arcs - alternating L->R and R->L aerial arcs, collecting 3 pickups per bound */
	BouncingArcs	UMETA(DisplayName = "Bouncing Arcs")
};

/**
 * Predefined pickup pattern configuration.
 */
USTRUCT(BlueprintType)
struct FPickupPattern
{
	GENERATED_BODY()

	/** Display name for debugging */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern")
	FString PatternName = TEXT("Unnamed Pattern");

	/** Pattern type for procedural generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern")
	EPickupPatternType PatternType = EPickupPatternType::Random;

	/** Array of pickups in this pattern (for manual patterns) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern")
	TArray<FPickupSpawnData> Pickups;

	/** Number of pickups to generate (for procedural patterns) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="1", ClampMax="12"))
	int32 PickupCount = 4;

	/** Minimum difficulty level to use this pattern (0 = always available) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="0"))
	int32 MinDifficultyLevel = 0;

	/** Weight for random selection (higher = more likely) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pattern", meta=(ClampMin="0.1"))
	float SelectionWeight = 1.0f;
};

/**
 * Delegate broadcast when a pickup is spawned.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPickupSpawned, ABasePickup*, Pickup, const FPickupSpawnData&, SpawnData);

/**
 * Delegate broadcast when the Magnet pull effect activates or deactivates.
 * Allows UI/VFX systems to respond to magnet state changes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMagnetStateChanged, bool, bIsActive);

/**
 * Handles spawning and pooling pickups (Data Packets, 1-Ups, EMPs, Magnets).
 * Lives on the GameMode. Pickups spawn alongside track segments,
 * with density scaling up as difficulty increases.
 */
UCLASS(ClassGroup=(StateRunner), meta=(BlueprintSpawnableComponent))
class STATERUNNER_ARCADE_API UPickupSpawnerComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UPickupSpawnerComponent();

protected:

	virtual void BeginPlay() override;

	/** Tick is only active during magnet effect — pulls DataPackets toward player */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Pickup Configuration ---

protected:

	/** BP class for Data Packet pickups */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup Config|Classes")
	TSubclassOf<ABasePickup> DataPacketClass;

	/** Old name for DataPacketClass, kept for BP compat */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup Config|Classes", meta=(DisplayName="PickupClass (DEPRECATED)"))
	TSubclassOf<ABasePickup> PickupClass;

	/** BP class for 1-Up pickups */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup Config|Classes")
	TSubclassOf<ABasePickup> OneUpClass;

	/** BP class for EMP pickups - destroys all obstacles on screen */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup Config|Classes")
	TSubclassOf<ABasePickup> EMPClass;

	/**
	 * Initial pool size.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup Config|Pooling", meta=(ClampMin="10", ClampMax="200"))
	int32 InitialPoolSize = 50;

	/**
	 * Pool expansion size.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup Config|Pooling", meta=(ClampMin="5", ClampMax="50"))
	int32 PoolExpansionSize = 10;

	// --- Lane Configuration ---

protected:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lane Config")
	float LeftLaneY = -316.67f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lane Config")
	float CenterLaneY = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lane Config")
	float RightLaneY = 316.67f;

	/**
	 * Z position for pickups (above floor).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lane Config")
	float PickupSpawnZ = 70.0f;

	// --- Spawning Configuration ---

protected:

	/** Minimum pickups per segment */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawning", meta=(ClampMin="1", ClampMax="12"))
	int32 MinPickupsPerSegment = 2;

	/** Maximum pickups per segment */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawning", meta=(ClampMin="3", ClampMax="20"))
	int32 MaxPickupsPerSegment = 10;

	/**
	 * Current pickup density (increases with difficulty).
	 * 0 = minimum, higher = more pickups
	 */
	UPROPERTY(BlueprintReadOnly, Category="Spawning")
	int32 CurrentDensityLevel = 0;

	/**
	 * Minimum X spacing between pickups (0.0 to 1.0 relative).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawning", meta=(ClampMin="0.03", ClampMax="0.2"))
	float MinPickupSpacing = 0.06f;

	/**
	 * Minimum relative X offset from segment start (normal difficulty).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawning", meta=(ClampMin="0.0", ClampMax="0.2"))
	float MinSpawnOffset = 0.1f;

	/**
	 * Maximum relative X offset from segment start (normal difficulty).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawning", meta=(ClampMin="0.8", ClampMax="1.0"))
	float MaxSpawnOffset = 0.9f;

	// --- High Difficulty Density ---

	/**
	 * Difficulty level at which "endgame density" kicks in.
	 * At this level and above, spawn ranges expand and pickup counts increase.
	 * Should match obstacle spawner's DifficultyToDisableBreathers for consistency.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawning|High Difficulty", meta=(ClampMin="1", ClampMax="8"))
	int32 HighDensityDifficultyThreshold = 4;

	/**
	 * Minimum spawn offset at high difficulty (expands range).
	 * Closer to 0 = pickups can spawn closer to segment start.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawning|High Difficulty", meta=(ClampMin="0.0", ClampMax="0.1"))
	float HighDensityMinSpawnOffset = 0.05f;

	/**
	 * Maximum spawn offset at high difficulty (expands range).
	 * Closer to 1 = pickups can spawn closer to segment end.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawning|High Difficulty", meta=(ClampMin="0.9", ClampMax="1.0"))
	float HighDensityMaxSpawnOffset = 0.95f;

	/**
	 * Bonus pickups per segment at high difficulty.
	 * Added on top of normal density calculation.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawning|High Difficulty", meta=(ClampMin="0", ClampMax="5"))
	int32 HighDensityBonusPickups = 2;

	/** Min distance between pickups and obstacles so players can safely collect them */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawning|Obstacle Avoidance", meta=(ClampMin="50.0", ClampMax="500.0"))
	float MinDistanceFromObstacle = 175.0f;

	/**
	 * Maximum attempts to find a valid spawn position before skipping a pickup.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawning|Obstacle Avoidance", meta=(ClampMin="3", ClampMax="20"))
	int32 MaxObstacleAvoidanceAttempts = 10;

	/** Segments between density increases. Should match ObstacleSpawner for sync. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Density", meta=(ClampMin="1", ClampMax="15"))
	int32 SegmentsPerDensityIncrease = 7;

	// --- 1-Up Configuration ---

protected:

	/** Chance to spawn a 1-Up per segment (0.0 to 1.0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="1-Up Config", meta=(ClampMin="0.0", ClampMax="0.5"))
	float OneUpSpawnChance = 0.12f;

	/** Min difficulty before 1-Ups can spawn */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="1-Up Config", meta=(ClampMin="0", ClampMax="5"))
	int32 OneUpMinDifficulty = 1;

	/** If no 1-Up has appeared by this difficulty, one will be forced */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="1-Up Config", meta=(ClampMin="1", ClampMax="10"))
	int32 OneUpGuaranteedDifficulty = 1;

	/**
	 * 1-Up pool size (smaller since they're rare).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="1-Up Config", meta=(ClampMin="3", ClampMax="20"))
	int32 OneUpPoolSize = 8;

	// --- EMP Configuration ---

protected:

	/** Chance to spawn an EMP per segment (0.0 to 1.0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EMP Config", meta=(ClampMin="0.0", ClampMax="0.2"))
	float EMPSpawnChance = 0.12f;

	/** Min difficulty before EMPs can spawn */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EMP Config", meta=(ClampMin="0", ClampMax="10"))
	int32 EMPMinDifficulty = 2;

	/** Extra spawn chance added when obstacle count exceeds EMPHighDensityThreshold */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EMP Config", meta=(ClampMin="0.0", ClampMax="0.1"))
	float EMPHighDensityBonusChance = 0.05f;

	/**
	 * Obstacle count threshold for high-density bonus.
	 * If current segment has this many or more obstacles, bonus chance applies.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EMP Config", meta=(ClampMin="5", ClampMax="20"))
	int32 EMPHighDensityThreshold = 10;

	/**
	 * EMP pool size (very small since they're extremely rare).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EMP Config", meta=(ClampMin="2", ClampMax="10"))
	int32 EMPPoolSize = 3;

	// --- Magnet Configuration ---

protected:

	/**
	 * Blueprint class to spawn for Magnet pickups.
	 * Should be a Blueprint child of ABasePickup with PickupType set to Magnet.
	 * Magnet temporarily pulls nearby DataPackets toward the player's lane.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup Config|Classes")
	TSubclassOf<ABasePickup> MagnetClass;

	/**
	 * Chance to spawn a Magnet per segment (0.0 to 1.0).
	 * 0.08 = 8% chance per segment. Feeds into combo system for guaranteed streaks.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Magnet Config", meta=(ClampMin="0.0", ClampMax="0.3"))
	float MagnetSpawnChance = 0.08f;

	/**
	 * Minimum difficulty level before Magnets can spawn.
	 * Appears at mid-game so players understand combos first.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Magnet Config", meta=(ClampMin="0", ClampMax="10"))
	int32 MagnetMinDifficulty = 3;

	/**
	 * Magnet pool size (small since they're rare).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Magnet Config", meta=(ClampMin="2", ClampMax="10"))
	int32 MagnetPoolSize = 3;

	/**
	 * Duration of the magnet pull effect in seconds.
	 * How long nearby pickups are attracted to the player after collecting a Magnet.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Magnet Config|Effect", meta=(ClampMin="1.0", ClampMax="15.0"))
	float MagnetDuration = 10.0f;

	/**
	 * Speed at which DataPackets slide laterally toward the player's Y position.
	 * This is used as an interpolation speed (divided by 100 for FInterpTo).
	 * Higher = snappier pull, lower = more gradual drift.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Magnet Config|Effect", meta=(ClampMin="100.0", ClampMax="3000.0"))
	float MagnetPullSpeed = 800.0f;

	/**
	 * How far ahead of the player (in +X world units) the magnet reaches.
	 * Only DataPackets within this X range are pulled toward the player.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Magnet Config|Effect", meta=(ClampMin="1000.0", ClampMax="10000.0"))
	float MagnetPullRange = 4000.0f;

	// --- Debug Configuration ---

protected:

	/**
	 * Spawn a pickup showcase row after tutorial completion.
	 * Places: 1-Up (Left) | EMP (Center) | 1-Up (Right)
	 * 
	 * EMP is in the center because it's the largest and most visually impressive.
	 * 1-Ups flank it on both sides. No DataPacket needed since players have
	 * already collected many during the intro segment.
	 * 
	 * This gives players a preview of special pickups they'll encounter later.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Pickup Showcase")
	bool bDebugSpawnAllPickupTypes = false;

	/**
	 * X position offset for the pickup showcase row.
	 * This offset is applied AFTER tutorial obstacles, so players see the showcase
	 * after completing the tutorial but before regular procedural spawning begins.
	 * 
	 * Spawns the pickups this far past the last tutorial obstacle position.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Pickup Showcase", meta=(ClampMin="500.0", ClampMax="12000.0"))
	float DebugPickupRowOffset = 1500.0f;

	/** Has the debug pickup row already spawned this session? */
	bool bDebugPickupsSpawned = false;

#if !UE_BUILD_SHIPPING
	/** Debug: When true, prevents automatic density recalculation (set by DebugIncreaseDifficulty) */
	bool bDebugDensityOverride = false;
#endif

	/** Spawn the debug pickup row (one of each type). Returns true if spawned. */
	bool TrySpawnDebugPickupRow(float SegmentStartX, float SegmentEndX);

	// --- Pattern Configuration ---

protected:

	/**
	 * Chance to use predefined pattern vs random generation (0.0 to 1.0).
	 * 0.6 = 60% chance to use a pattern formation.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Patterns", meta=(ClampMin="0.0", ClampMax="1.0"))
	float PatternChance = 0.6f;

	/**
	 * Array of predefined pickup patterns.
	 * Can be edited in Blueprint for custom pattern creation.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Patterns")
	TArray<FPickupPattern> PredefinedPatterns;

	// --- Runtime State ---

protected:

	/** Pool for Data Packet pickups */
	UPROPERTY()
	TArray<TObjectPtr<ABasePickup>> DataPacketPool;

	/** Pool for 1-Up pickups */
	UPROPERTY()
	TArray<TObjectPtr<ABasePickup>> OneUpPool;

	/** Pool for EMP pickups */
	UPROPERTY()
	TArray<TObjectPtr<ABasePickup>> EMPPool;

	/** Pool for Magnet pickups */
	UPROPERTY()
	TArray<TObjectPtr<ABasePickup>> MagnetPool;

	/** All currently active pickups */
	UPROPERTY()
	TArray<TObjectPtr<ABasePickup>> ActivePickups;

	/** Segments spawned counter */
	int32 SegmentsSpawned = 0;

	/** Whether at least one 1-Up has spawned this session */
	bool bHasSpawnedFirst1Up = false;

	/**
	 * Lane of the last spawned 1-Up.
	 * Used to ensure consecutive 1-Ups are always in different lanes.
	 * Players should never see two 1-Ups in the same lane back-to-back.
	 */
	ELane Last1UpLane = ELane::Center;

	/**
	 * Segment index when the last 1-Up was spawned.
	 * Used to enforce cooldown - no 1-Up can spawn in the segment immediately after one spawned.
	 */
	int32 Last1UpSpawnSegment = -999;

	/**
	 * Segment index when the last EMP was spawned.
	 * Used to enforce cooldown - no EMP can spawn in the segment immediately after one spawned.
	 */
	int32 LastEMPSpawnSegment = -999;

	/**
	 * Segment index when the last Magnet was spawned.
	 * Used to enforce cooldown - no Magnet can spawn in the segment immediately after one spawned.
	 */
	int32 LastMagnetSpawnSegment = -999;

	/** Whether the magnet pull effect is currently active */
	UPROPERTY(BlueprintReadOnly, Category="Magnet Config|Runtime")
	bool bIsMagnetActive = false;

	/** Remaining time on the magnet effect */
	float MagnetTimeRemaining = 0.0f;

	/**
	 * Cached obstacle positions for current segment (used for avoidance).
	 * Populated before spawning pickups for a segment.
	 */
	TArray<FVector> CachedObstaclePositions;

	/**
	 * Cached obstacle types for current segment.
	 * Parallel array to CachedObstaclePositions - same index = same obstacle.
	 * Used to prevent aerial patterns from leading into FullWalls/HighBarriers.
	 */
	TArray<EObstacleType> CachedObstacleTypes;

	/**
	 * True if current segment has blocking obstacles (FullWall or HighBarrier).
	 * When true, aerial pickup patterns should be avoided to prevent "trick" spawns.
	 */
	bool bSegmentHasBlockingObstacles = false;

	// --- Events ---

public:

	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnPickupSpawned OnPickupSpawned;

	/** Broadcast when Magnet effect activates or deactivates */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnMagnetStateChanged OnMagnetStateChanged;

	// --- Public Functions ---

public:

	/**
	 * Spawn pickups for a track segment.
	 * 
	 * @param SegmentStartX World X position where segment starts
	 * @param SegmentEndX World X position where segment ends
	 */
	UFUNCTION(BlueprintCallable, Category="Spawning")
	void SpawnPickupsForSegment(float SegmentStartX, float SegmentEndX);

	/**
	 * Clear all active pickups.
	 */
	UFUNCTION(BlueprintCallable, Category="Spawning")
	void ClearAllPickups();

	/**
	 * Get number of active pickups.
	 */
	UFUNCTION(BlueprintPure, Category="Spawning")
	int32 GetActivePickupCount() const { return ActivePickups.Num(); }

	/**
	 * Set density level (affects pickup count per segment).
	 */
	UFUNCTION(BlueprintCallable, Category="Spawning")
	void SetDensityLevel(int32 Level) { CurrentDensityLevel = FMath::Max(0, Level); }

	/**
	 * Get current density level.
	 */
	UFUNCTION(BlueprintPure, Category="Spawning")
	int32 GetCurrentDensityLevel() const { return CurrentDensityLevel; }

	/**
	 * Get minimum difficulty for 1-Up spawns.
	 */
	UFUNCTION(BlueprintPure, Category="Spawning")
	int32 GetOneUpMinDifficulty() const { return OneUpMinDifficulty; }

	/**
	 * Get minimum difficulty for EMP spawns.
	 */
	UFUNCTION(BlueprintPure, Category="Spawning")
	int32 GetEMPMinDifficulty() const { return EMPMinDifficulty; }

	/**
	 * Activate the magnet pull effect.
	 * For the next MagnetDuration seconds, all active pickups (except other Magnets)
	 * within MagnetPullRange ahead of the player are pulled toward the player's lane.
	 * Collecting a second Magnet while active resets the timer (does NOT stack).
	 * Broadcasts OnMagnetStateChanged and activates player VFX.
	 */
	UFUNCTION(BlueprintCallable, Category="Magnet")
	void ActivateMagnet();

	/**
	 * Check if the magnet pull effect is currently active.
	 */
	UFUNCTION(BlueprintPure, Category="Magnet")
	bool IsMagnetActive() const { return bIsMagnetActive; }

	/**
	 * Debug: Increase difficulty by 1 and print status to screen.
	 * Only works in non-shipping builds.
	 */
	UFUNCTION(BlueprintCallable, Category="Debug")
	void DebugIncreaseDifficulty();

	/**
	 * Reset spawner state for game restart.
	 */
	UFUNCTION(BlueprintCallable, Category="Spawning")
	void ResetSpawner();

	// --- Protected Functions ---

protected:

	/** Generate pickup layout for a segment */
	void GeneratePickupLayout(TArray<FPickupSpawnData>& OutPickups, int32 PickupCount);

	/** Calculate pickup count for current density */
	int32 CalculatePickupCount() const;

	/** Get effective minimum spawn offset (adjusts for high difficulty) */
	float GetEffectiveMinSpawnOffset() const;

	/** Get effective maximum spawn offset (adjusts for high difficulty) */
	float GetEffectiveMaxSpawnOffset() const;

	/**
	 * Get safe spawn range for aerial patterns.
	 * Aerial patterns need landing room at the END of the pattern.
	 * Returns a narrower range (15% buffer at end) so players have clear landing space.
	 */
	void GetAerialPatternBounds(float& OutStartX, float& OutEndX) const;

	/** Check if high density mode is active */
	bool IsHighDensityActive() const { return CurrentDensityLevel >= HighDensityDifficultyThreshold; }

	/** Initialize all pickup pools */
	void InitializePools();

	/** Initialize pool for a specific pickup type */
	void InitializePoolForType(EPickupType Type);

	/** Get pickup from pool by type */
	ABasePickup* GetPickupFromPool(EPickupType Type);

	/** Get pool array for a specific type */
	TArray<TObjectPtr<ABasePickup>>& GetPoolForType(EPickupType Type);

	/** Get class for a specific pickup type */
	TSubclassOf<ABasePickup> GetClassForType(EPickupType Type) const;

	/** Expand pool for a specific type */
	void ExpandPool(EPickupType Type);

	/** Spawn a new pickup actor of specific type */
	ABasePickup* SpawnPickupActor(EPickupType Type);

	/** Get Y position for a lane */
	float GetLaneYPosition(ELane Lane) const;

	/** Get random lane */
	ELane GetRandomLane() const;

	/** Check if should spawn 1-Up this segment */
	bool ShouldSpawn1Up() const;

	/** Check if should spawn EMP this segment */
	bool ShouldSpawnEMP() const;

	/** Check if should spawn Magnet this segment */
	bool ShouldSpawnMagnet() const;

	/**
	 * Spawn an EMP pickup within obstacle clusters for maximum impact.
	 * 
	 * @param SegmentStartX Start X of the segment
	 * @param SegmentEndX End X of the segment
	 * @param ObstacleCount Number of obstacles in current segment (for positioning)
	 */
	void SpawnEMP(float SegmentStartX, float SegmentEndX, int32 ObstacleCount);

	/**
	 * Caches obstacle positions from ObstacleSpawnerComponent for avoidance checking.
	 * Must be called before spawning pickups for a segment.
	 * 
	 * @param SegmentStartX Start X of the segment
	 * @param SegmentEndX End X of the segment
	 */
	void CacheObstaclePositions(float SegmentStartX, float SegmentEndX);

	/**
	 * Check if a pickup position is safe (far enough from all obstacles).
	 * 
	 * @param WorldPosition The world position to check
	 * @return True if position is safe, false if too close to an obstacle
	 */
	bool IsPositionSafeFromObstacles(const FVector& WorldPosition) const;

	/**
	 * Find a safe spawn position near the desired position.
	 * Tries to offset the position if the original is too close to obstacles.
	 * 
	 * @param DesiredPosition Original spawn position
	 * @param Lane Lane the pickup is in
	 * @param OutSafePosition Output: Safe position (may be same as desired if already safe)
	 * @return True if a safe position was found, false if no safe position exists
	 */
	bool FindSafeSpawnPosition(const FVector& DesiredPosition, ELane Lane, FVector& OutSafePosition) const;

	/**
	 * Spawn a 1-Up pickup.
	 * 
	 * @param SegmentStartX Start X of the segment
	 * @param SegmentEndX End X of the segment
	 * @param bIsGuaranteed If true, this is the guaranteed first 1-Up - spawns in center lane at front of segment
	 */
	void Spawn1Up(float SegmentStartX, float SegmentEndX, bool bIsGuaranteed = false);

	/**
	 * Spawn a Magnet pickup in a random safe lane.
	 * Uses the same safe-position logic as 1-Up spawning.
	 * 
	 * @param SegmentStartX Start X of the segment
	 * @param SegmentEndX End X of the segment
	 */
	void SpawnMagnet(float SegmentStartX, float SegmentEndX);

	/** Update density level based on segments spawned (controls 1-Up spawn eligibility) */
	void UpdateDensity();

	// --- Pattern Functions ---

protected:

	/**
	 * Generate pickups from a predefined or procedural pattern.
	 * @param OutPickups Array to fill with pickup spawn data
	 * @return True if a pattern was used
	 */
	bool GenerateFromPattern(TArray<FPickupSpawnData>& OutPickups);

	/**
	 * Generate pickups using a specific pattern type.
	 * @param OutPickups Array to fill with pickup spawn data
	 * @param PatternType The mathematical pattern to use
	 * @param PickupCount Number of pickups to generate
	 */
	void GeneratePatternPickups(TArray<FPickupSpawnData>& OutPickups, EPickupPatternType PatternType, int32 PickupCount);

	/**
	 * Generate a trail pattern (line of pickups in one lane).
	 * @param OutPickups Array to fill
	 * @param Count Number of pickups
	 * @param Lane Which lane for the trail
	 */
	void GenerateTrailPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, ELane Lane);

	/**
	 * Generate an arc pattern (curve across lanes).
	 * @param OutPickups Array to fill
	 * @param Count Number of pickups
	 * @param bLeftToRight Direction of arc
	 */
	void GenerateArcPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, bool bLeftToRight);

	/**
	 * Generate a zigzag pattern across lanes.
	 * @param OutPickups Array to fill
	 * @param Count Number of pickups
	 */
	void GenerateZigzagPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count);

	/**
	 * Generate a cluster pattern (tight grouping).
	 * @param OutPickups Array to fill
	 * @param Count Number of pickups
	 * @param CenterX Center X position of cluster
	 */
	void GenerateClusterPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, float CenterX);

	/**
	 * Generate a diamond formation.
	 * @param OutPickups Array to fill
	 * @param CenterX Center X position
	 */
	void GenerateDiamondPattern(TArray<FPickupSpawnData>& OutPickups, float CenterX);

	/**
	 * Generate a wave pattern (sine-like curve).
	 * @param OutPickups Array to fill
	 * @param Count Number of pickups
	 */
	void GenerateWavePattern(TArray<FPickupSpawnData>& OutPickups, int32 Count);

	/**
	 * Generate a diagonal line pattern.
	 * @param OutPickups Array to fill
	 * @param Count Number of pickups
	 * @param bLeftToRight Direction of diagonal
	 */
	void GenerateDiagonalPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, bool bLeftToRight);

	/**
	 * Generate a triple line (all three lanes at same X).
	 * @param OutPickups Array to fill
	 * @param XPosition X position for the line
	 */
	void GenerateTripleLinePattern(TArray<FPickupSpawnData>& OutPickups, float XPosition);

	/**
	 * Generate a scatter pattern (random but visually interesting).
	 * @param OutPickups Array to fill
	 * @param Count Number of pickups
	 */
	void GenerateScatterPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count);

	// --- Aerial Pattern Generators ---

	/**
	 * Generate a vertical arc pattern (ground -> apex -> ground in one lane).
	 * Perfect for jump timing practice.
	 * @param OutPickups Array to fill
	 * @param Count Number of pickups
	 * @param Lane Which lane to place the arc
	 */
	void GenerateVerticalArcPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, ELane Lane);

	/**
	 * Generate an ascending pattern (ground to apex in one lane).
	 * Rewards sustained jump.
	 * @param OutPickups Array to fill
	 * @param Count Number of pickups
	 * @param Lane Which lane
	 */
	void GenerateAscendingPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, ELane Lane);

	/**
	 * Generate a descending pattern (apex to ground in one lane).
	 * @param OutPickups Array to fill
	 * @param Count Number of pickups
	 * @param Lane Which lane
	 */
	void GenerateDescendingPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, ELane Lane);

	/**
	 * Generate a helix/spiral pattern (zigzag across lanes with height oscillation).
	 * 3D corkscrew effect.
	 * @param OutPickups Array to fill
	 * @param Count Number of pickups
	 */
	void GenerateHelixPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count);

	/**
	 * Generate a jump arc pattern (follows natural parabolic jump trajectory).
	 * Matches the player's jump arc for satisfying collection.
	 * @param OutPickups Array to fill
	 * @param Count Number of pickups
	 * @param Lane Which lane
	 * @param bHighJump If true, uses max jump height; otherwise uses base jump
	 */
	void GenerateJumpArcPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, ELane Lane, bool bHighJump = false);

	/**
	 * Generate a staircase pattern (ascending diagonally across lanes).
	 * Each lane step goes higher.
	 * @param OutPickups Array to fill
	 * @param Count Number of pickups
	 * @param bLeftToRight Direction of stairs
	 */
	void GenerateStaircasePattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, bool bLeftToRight = true);

	/**
	 * Generate a sky trail pattern (high altitude line).
	 * Requires sustained max jump to collect.
	 * @param OutPickups Array to fill
	 * @param Count Number of pickups
	 * @param Lane Which lane
	 */
	void GenerateSkyTrailPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, ELane Lane);

	/**
	 * Generate a tower pattern (vertical stack in one lane).
	 * Multiple pickups at same X, different heights.
	 * @param OutPickups Array to fill
	 * @param CenterX X position for the tower
	 * @param Lane Which lane
	 */
	void GenerateTowerPattern(TArray<FPickupSpawnData>& OutPickups, float CenterX, ELane Lane);

	/**
	 * Generate a rainbow pattern (3D arc across lanes AND height).
	 * Spectacular formation - goes L->C->R while also going up and down.
	 * @param OutPickups Array to fill
	 * @param Count Number of pickups
	 * @param bLeftToRight Direction of rainbow
	 */
	void GenerateRainbowPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, bool bLeftToRight = true);

	/**
	 * Generate a bouncing arcs pattern (alternating L->R and R->L aerial arcs).
	 * Each "bound" is a 3-pickup arc across all lanes at different heights.
	 * Creates a bouncing ball effect with 12+ pickups for INSANE combo.
	 * @param OutPickups Array to fill
	 * @param NumBounds Number of bounds/arcs (each bound = 3 pickups across lanes)
	 */
	void GenerateBouncingArcsPattern(TArray<FPickupSpawnData>& OutPickups, int32 NumBounds);

	/**
	 * Select a random pattern based on difficulty and weights.
	 * Filters out aerial patterns if segment has blocking obstacles.
	 * @return Pointer to selected pattern, or nullptr if none available
	 */
	const FPickupPattern* SelectPattern() const;

	/**
	 * Check if a pattern type is an aerial pattern (uses height variation).
	 * Aerial patterns should be avoided when blocking obstacles are present.
	 * @param PatternType The pattern type to check
	 * @return True if this is an aerial pattern
	 */
	static bool IsAerialPattern(EPickupPatternType PatternType);

	/**
	 * Create default patterns if none are configured.
	 * Called in BeginPlay if PredefinedPatterns is empty.
	 */
	void CreateDefaultPatterns();
};
