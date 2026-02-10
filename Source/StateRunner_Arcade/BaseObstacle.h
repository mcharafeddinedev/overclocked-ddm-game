#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseObstacle.generated.h"

class UWorldScrollComponent;
class UBoxComponent;
class UStaticMeshComponent;

/**
 * Mesh variant data for obstacles. Stores mesh reference and transform adjustments.
 */
USTRUCT(BlueprintType)
struct FMeshVariantData
{
	GENERATED_BODY()

	/** Static mesh for this variant */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Mesh")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	/** Scale multiplier */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Transform")
	FVector Scale = FVector(1.0f, 1.0f, 1.0f);

	/** Position offset to fix off-center pivots */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Transform")
	FVector LocationOffset = FVector(0.0f, 0.0f, 0.0f);

	/** Rotation offset. Default 90° Yaw for FBX imports. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Transform")
	FRotator RotationOffset = FRotator(0.0f, 0.0f, 90.0f);

	/** Randomly flip yaw by 180° at spawn for visual variety */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Transform")
	bool bRandomizeYawFlip = true;

	FMeshVariantData()
		: Mesh(nullptr)
		, Scale(FVector(1.0f, 1.0f, 1.0f))
		, LocationOffset(FVector(0.0f, 0.0f, 0.0f))
		, RotationOffset(FRotator(0.0f, 0.0f, 90.0f))
		, bRandomizeYawFlip(true)
	{
	}

	explicit FMeshVariantData(UStaticMesh* InMesh)
		: Mesh(InMesh)
		, Scale(FVector(1.0f, 1.0f, 1.0f))
		, LocationOffset(FVector(0.0f, 0.0f, 0.0f))
		, RotationOffset(FRotator(0.0f, 0.0f, 90.0f))
		, bRandomizeYawFlip(true)
	{
	}
};

/**
 * Obstacle Type Enum
 * Defines the different types of obstacles and their avoidance method.
 */
UENUM(BlueprintType)
enum class EObstacleType : uint8
{
	/** Low wall that player must jump over */
	LowWall			UMETA(DisplayName = "Low Wall (Jump)"),
	
	/** High barrier that player must slide under */
	HighBarrier		UMETA(DisplayName = "High Barrier (Slide)"),
	
	/** Full wall that blocks entire lane - must change lanes */
	FullWall		UMETA(DisplayName = "Full Wall (Lane Change)")
};

/**
 * Lane Enum
 * Defines which lane the obstacle spawns in.
 */
UENUM(BlueprintType)
enum class ELane : uint8
{
	Left		UMETA(DisplayName = "Left Lane"),
	Center		UMETA(DisplayName = "Center Lane"),
	Right		UMETA(DisplayName = "Right Lane")
};

/**
 * Base Obstacle Actor
 * 
 * Parent class for all obstacles in the game.
 * Handles scrolling movement, collision detection, and object pooling.
 * 
 * COORDINATE SYSTEM:
 * - X-axis: Length of track (scroll direction - NEGATIVE X)
 * - Y-axis: Width of track (lanes)
 * - Z-axis: Height (vertical)
 * 
 * COLLISION:
 * - Uses overlap detection, NOT blocking collision
 * - Player passes through obstacles (takes damage on overlap)
 * 
 * POOLING:
 * - Supports object pooling for performance
 * - Use Activate() and Deactivate() instead of Spawn/Destroy
 */
UCLASS(Abstract)
class STATERUNNER_ARCADE_API ABaseObstacle : public AActor
{
	GENERATED_BODY()

public:

	/** Constructor */
	ABaseObstacle();

protected:

	/** Called when the game starts */
	virtual void BeginPlay() override;

public:

	/** Called every frame */
	virtual void Tick(float DeltaTime) override;

	//=============================================================================
	// COMPONENTS
	//=============================================================================

protected:

	/** Root scene component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> RootSceneComponent;

	/**
	 * Collision box for overlap detection.
	 * Configured for overlap (not blocking) so player passes through.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UBoxComponent> CollisionBox;

	/**
	 * Visual mesh for the obstacle.
	 * Child classes or Blueprints can customize this.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> ObstacleMesh;

	//=============================================================================
	// OBSTACLE CONFIGURATION
	//=============================================================================

protected:

	/**
	 * Type of obstacle (determines avoidance method).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Obstacle Config")
	EObstacleType ObstacleType = EObstacleType::LowWall;

	/**
	 * Which lane this obstacle is currently in.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Obstacle Config")
	ELane CurrentLane = ELane::Center;

	//=============================================================================
	// MESH VARIANTS
	// Configure in Blueprint to allow random mesh selection on spawn
	//=============================================================================

	/**
	 * Array of mesh variants with per-mesh transform adjustments.
	 * When populated, a random variant will be selected each time the obstacle is activated.
	 * If empty, the default mesh assigned to ObstacleMesh component is used.
	 * 
	 * Each variant includes:
	 * - Mesh: The static mesh to display
	 * - Scale: Scale multiplier (use to fix meshes that are too big/small)
	 * - LocationOffset: Position adjustment (use to fix off-center pivots)
	 * - RotationOffset: Rotation adjustment (use to fix wrong orientations)
	 * 
	 * Usage: In each Blueprint variant (BP_JumpHurdle, etc.), add your mesh options
	 * to this array with their transform corrections. On spawn, one will be randomly selected.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Mesh Variants", meta=(TitleProperty="Mesh"))
	TArray<FMeshVariantData> MeshVariants;

	/**
	 * Select and apply a random mesh variant from MeshVariants array.
	 * Called automatically during Activate().
	 * Applies the mesh and its associated transform adjustments.
	 * If MeshVariants is empty, keeps the current mesh unchanged.
	 */
	UFUNCTION(BlueprintCallable, Category="Mesh Variants")
	virtual void SelectRandomMeshVariant();

	/** Apply a specific mesh variant by index. */
	UFUNCTION(BlueprintCallable, Category="Mesh Variants")
	void ApplyMeshVariant(int32 VariantIndex);

	//=============================================================================
	// COLLISION BOX CONFIGURATION (Per Obstacle Type)
	// Edit EXTENT only - collision auto-centers on mesh position
	//=============================================================================

	/** LowWall collision box extent (half-size). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Collision Config|LowWall")
	FVector LowWallCollisionExtent = FVector(50.0f, 150.0f, 40.0f);

	/** LowWall collision Z offset. Raise to match mesh visual center. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Collision Config|LowWall")
	float LowWallCollisionZ = 40.0f;

	/** HighBarrier collision extent (half-size). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Collision Config|HighBarrier")
	FVector HighBarrierCollisionExtent = FVector(50.0f, 150.0f, 100.0f);

	/** HighBarrier collision Z offset. Position at head height for slide-under. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Collision Config|HighBarrier")
	float HighBarrierCollisionZ = 150.0f;

	/** FullWall collision box extent (half-size). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Collision Config|FullWall")
	FVector FullWallCollisionExtent = FVector(50.0f, 150.0f, 150.0f);

	/** FullWall collision Z offset. Center vertically on tall wall. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Collision Config|FullWall")
	float FullWallCollisionZ = 150.0f;

	/** Enable debug drawing of collision box (visible in Play mode). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Collision Config|Debug")
	bool bDrawDebugCollision = false;

	//=============================================================================
	// SCROLLING CONFIGURATION
	//=============================================================================

protected:

	/**
	 * X position threshold for despawning (return to pool).
	 * When obstacle X position goes below this, it's despawned.
	 * Player is at X: -5000, so -8000 gives buffer behind player.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Scrolling", meta=(ClampMax="-6000.0"))
	float DespawnXThreshold = -8000.0f;

	//=============================================================================
	// RUNTIME STATE
	//=============================================================================

protected:

	/**
	 * Cached reference to WorldScrollComponent for scroll speed queries.
	 * Obtained in BeginPlay and cached for performance.
	 */
	UPROPERTY()
	TObjectPtr<UWorldScrollComponent> WorldScrollComponent;

	/**
	 * Whether this obstacle is currently active (in play).
	 * When false, obstacle is in pool and should not tick or collide.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Runtime")
	bool bIsActive = false;

	/**
	 * Whether this obstacle has already triggered damage this activation.
	 * Prevents multiple damage triggers from single obstacle.
	 * Reset when obstacle is activated.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Runtime")
	bool bHasTriggeredDamage = false;

	//=============================================================================
	// POOLING FUNCTIONS
	//=============================================================================

public:

	/**
	 * Activate this obstacle from the pool.
	 * Called by ObstacleSpawnerComponent when spawning an obstacle.
	 * 
	 * @param SpawnLocation World location to place the obstacle
	 * @param Lane Which lane the obstacle is in
	 * @param Type Type of obstacle (for reconfiguration if needed)
	 */
	UFUNCTION(BlueprintCallable, Category="Pooling")
	virtual void Activate(const FVector& SpawnLocation, ELane Lane, EObstacleType Type);

	/**
	 * Deactivate this obstacle and return to pool.
	 * Called when obstacle passes despawn threshold.
	 */
	UFUNCTION(BlueprintCallable, Category="Pooling")
	virtual void Deactivate();

	/**
	 * Check if this obstacle is currently active.
	 * 
	 * @return True if active and in play
	 */
	UFUNCTION(BlueprintPure, Category="Pooling")
	bool IsActive() const { return bIsActive; }

	//=============================================================================
	// GETTERS
	//=============================================================================

public:

	/** Get the obstacle type */
	UFUNCTION(BlueprintPure, Category="Obstacle")
	EObstacleType GetObstacleType() const { return ObstacleType; }

	/** Get the current lane */
	UFUNCTION(BlueprintPure, Category="Obstacle")
	ELane GetCurrentLane() const { return CurrentLane; }

	//=============================================================================
	// COLLISION
	//=============================================================================

protected:

	/**
	 * Called when another actor overlaps this obstacle's collision box.
	 */
	UFUNCTION()
	void OnCollisionOverlapBegin(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	/**
	 * Handle collision with player.
	 * Override in child classes for special behavior.
	 * 
	 * @param PlayerActor The player character that collided
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Collision")
	void HandlePlayerCollision(AActor* PlayerActor);

	//=============================================================================
	// SCROLLING
	//=============================================================================

protected:

	/**
	 * Update obstacle position based on scroll speed.
	 * Moves in NEGATIVE X direction.
	 */
	virtual void UpdateScrolling(float DeltaTime);

	/**
	 * Check if obstacle should despawn (past despawn threshold).
	 * 
	 * @return True if should despawn
	 */
	bool ShouldDespawn() const;

	//=============================================================================
	// SETUP FUNCTIONS
	//=============================================================================

protected:

	/**
	 * Setup collision box size based on obstacle type.
	 * Called during activation or BeginPlay.
	 */
	virtual void SetupCollisionBox();

	/**
	 * Cache reference to WorldScrollComponent.
	 * Called in BeginPlay.
	 */
	void CacheWorldScrollComponent();
};
