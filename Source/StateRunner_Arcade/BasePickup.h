#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseObstacle.h" // For ELane enum
#include "BasePickup.generated.h"

class UWorldScrollComponent;
class UBoxComponent;
class UStaticMeshComponent;
class UParticleSystem;
class UParticleSystemComponent;
class USoundBase;

/**
 * Mesh variant data for pickups. Stores mesh reference and transform adjustments.
 */
USTRUCT(BlueprintType)
struct FPickupMeshVariantData
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

	/** Base rotation before spin animation. Default 90° Yaw for FBX imports. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Transform")
	FRotator RotationOffset = FRotator(0.0f, 0.0f, 90.0f);

	/** Randomly flip yaw by 180° at spawn for visual variety */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Transform")
	bool bRandomizeYawFlip = true;

	FPickupMeshVariantData()
		: Mesh(nullptr)
		, Scale(FVector(1.0f, 1.0f, 1.0f))
		, LocationOffset(FVector(0.0f, 0.0f, 0.0f))
		, RotationOffset(FRotator(0.0f, 0.0f, 90.0f))
		, bRandomizeYawFlip(true)
	{
	}

	explicit FPickupMeshVariantData(UStaticMesh* InMesh)
		: Mesh(InMesh)
		, Scale(FVector(1.0f, 1.0f, 1.0f))
		, LocationOffset(FVector(0.0f, 0.0f, 0.0f))
		, RotationOffset(FRotator(0.0f, 0.0f, 90.0f))
		, bRandomizeYawFlip(true)
	{
	}
};

/**
 * Pickup type enumeration.
 * Determines pickup behavior on collection.
 */
UENUM(BlueprintType)
enum class EPickupType : uint8
{
	/** Standard score pickup (Data Packet) */
	DataPacket		UMETA(DisplayName = "Data Packet"),
	
	/** 1-Up pickup - restores life or gives bonus score if at max lives */
	OneUp			UMETA(DisplayName = "1-Up"),
	
	/** EMP pickup - destroys all obstacles currently in the game world (screen nuke) */
	EMP				UMETA(DisplayName = "EMP"),
	
	/** Magnet pickup - pulls nearby data packets toward the player for easy collection */
	Magnet			UMETA(DisplayName = "Magnet")
};

/** Delegate broadcast when pickup is collected. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPickupCollected, ABasePickup*, Pickup);

/**
 * Base class for all collectible pickups (data packets, 1-ups, EMPs, etc.).
 * Handles scrolling movement, overlap collection, and object pooling.
 */
UCLASS(Abstract)
class STATERUNNER_ARCADE_API ABasePickup : public AActor
{
	GENERATED_BODY()

public:

	/** Constructor */
	ABasePickup();

protected:

	/** Called when the game starts */
	virtual void BeginPlay() override;

public:

	/** Called every frame */
	virtual void Tick(float DeltaTime) override;

	// --- Components ---

protected:

	/** Root scene component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> RootSceneComponent;

	/**
	 * Collision box for collection detection.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UBoxComponent> CollisionBox;

	/**
	 * Visual mesh for the pickup.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	/**
	 * Particle system component for collection effect.
	 * The particle system scrolls with the pickup so it stays in view.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UParticleSystemComponent> CollectionParticleComponent;

	// --- Pickup Configuration ---

protected:

	/** Type of pickup (determines collection behavior). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup Config")
	EPickupType PickupType = EPickupType::DataPacket;

	/** Which lane this pickup is currently in. */
	UPROPERTY(BlueprintReadOnly, Category="Pickup Config")
	ELane CurrentLane = ELane::Center;

	/** Collision box extent (half-size). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup Config")
	FVector CollisionExtent = FVector(20.0f, 25.0f, 20.0f);

	/** Manual Z offset for collision. Use to raise collision to match floating mesh visuals. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup Config")
	float CollisionZOffset = 0.0f;

	/** Enable debug drawing of collision box (visible in Play mode). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Pickup Config|Debug")
	bool bDrawDebugCollision = false;

	/** Base rotation speed for spin effect (degrees per second). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup Config")
	float RotationSpeed = 90.0f;

	/** Random variation applied to rotation speed (0.0 to 1.0). 0.3 = +/-30% speed variation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup Config", meta=(ClampMin="0.0", ClampMax="0.5"))
	float RotationSpeedVariation = 0.25f;

	/** Vertical bob amplitude (units). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup Config")
	float BobAmplitude = 10.0f;

	/**
	 * Vertical bob frequency (cycles per second).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pickup Config")
	float BobFrequency = 2.0f;

	// --- Collection Effects ---

protected:

	/**
	 * Particle system asset for collection effect.
	 * Set this per-Blueprint (BP_DataPacket, BP_OneUp, BP_Pickup_EMP).
	 * The effect plays on the CollectionParticleComponent and scrolls with the pickup.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Collection Effects")
	TObjectPtr<UParticleSystem> CollectionParticleEffect;

	/**
	 * Scale multiplier for the collection particle effect.
	 * Adjust this if your particle system needs to be bigger/smaller.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Collection Effects")
	float CollectionParticleScale = 1.0f;

	/**
	 * Playback speed multiplier for the particle effect.
	 * Higher values = faster/snappier explosion effect.
	 * 1.0 = normal speed, 2.0 = twice as fast, etc.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Collection Effects", meta=(ClampMin="0.5", ClampMax="10.0"))
	float CollectionParticleSpeed = 4.0f;

	/**
	 * How long to keep the pickup active (scrolling) after collection.
	 * This allows the particle effect to scroll with the world before disappearing.
	 * Should be long enough for the effect to play out.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Collection Effects", meta=(ClampMin="0.1", ClampMax="5.0"))
	float CollectionEffectDuration = 0.5f;

	/**
	 * Sound to play when this pickup is collected.
	 * Set this per-Blueprint for unique collection sounds.
	 * 
	 * For 1-Up pickups, this sound is skipped -- use BonusCollectionSound instead.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Collection Effects")
	TObjectPtr<USoundBase> CollectionSound;

	/**
	 * [1-UP ONLY] Sound to play when collecting a 1-Up at MAX lives (bonus score).
	 * This is distinct from the LivesSystem's OneUpSound which plays when gaining a life.
	 * 
	 * Only used by 1-Up pickups. Other pickup types ignore this property.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Collection Effects")
	TObjectPtr<USoundBase> BonusCollectionSound;

	/**
	 * Volume multiplier for the collection sound.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Collection Effects", meta=(ClampMin="0.0", ClampMax="2.0"))
	float CollectionSoundVolume = 1.0f;

	/**
	 * Plays the collection particle effect AND sound (attached, scrolls with pickup).
	 * 
	 * @param PitchMultiplier Pitch scaling for the collection sound (1.0 = normal).
	 *                        Data Packets use streak-based pitch from ScoreSystemComponent.
	 */
	void PlayCollectionEffect(float PitchMultiplier = 1.0f);

	/** Plays the collection particle effect WITHOUT sound (for 1-Up which handles sound separately). */
	void PlayCollectionEffectNoSound();

	/** Called when collection effect duration expires. */
	void OnCollectionEffectFinished();

	// --- Mesh Variants ---

	/** Mesh variants for random selection on spawn. If empty, uses default mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Mesh Variants", meta=(TitleProperty="Mesh"))
	TArray<FPickupMeshVariantData> MeshVariants;

	/** Selects and applies a random mesh variant. Called automatically during Activate(). */
	UFUNCTION(BlueprintCallable, Category="Mesh Variants")
	virtual void SelectRandomMeshVariant();

	/** Apply a specific mesh variant by index. */
	UFUNCTION(BlueprintCallable, Category="Mesh Variants")
	void ApplyMeshVariant(int32 VariantIndex);

	/** Base rotation from current variant. Spin animation adds to this. */
	FRotator MeshBaseRotation = FRotator::ZeroRotator;

	// --- Scrolling Configuration ---

protected:

	/**
	 * X position threshold for despawning.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Scrolling", meta=(ClampMax="-6000.0"))
	float DespawnXThreshold = -8000.0f;

	// --- Runtime State ---

protected:

	/** Cached reference to WorldScrollComponent */
	UPROPERTY()
	TObjectPtr<UWorldScrollComponent> WorldScrollComponent;

	/** Whether this pickup is currently active */
	UPROPERTY(BlueprintReadOnly, Category="Runtime")
	bool bIsActive = false;

	/** Whether this pickup has been collected and is playing its effect */
	UPROPERTY(BlueprintReadOnly, Category="Runtime")
	bool bIsPlayingCollectionEffect = false;

	/** Timer handle for collection effect duration */
	FTimerHandle CollectionEffectTimerHandle;

	/** Base Z position for bob effect */
	float BaseZ = 0.0f;

	/** Time accumulator for bob effect */
	float BobTime = 0.0f;

	/** Instance-specific rotation speed (randomized on activation) */
	float CurrentRotationSpeed = 0.0f;

	// --- Events ---

public:

	/** Broadcast when this pickup is collected */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnPickupCollected OnCollected;

	// --- Pooling Functions ---

public:

	/**
	 * Activate this pickup from the pool.
	 * 
	 * @param SpawnLocation World location to place the pickup
	 * @param Lane Which lane the pickup is in
	 */
	UFUNCTION(BlueprintCallable, Category="Pooling")
	virtual void Activate(const FVector& SpawnLocation, ELane Lane);

	/**
	 * Deactivate this pickup and return to pool.
	 */
	UFUNCTION(BlueprintCallable, Category="Pooling")
	virtual void Deactivate();

	/**
	 * Check if this pickup is currently active.
	 */
	UFUNCTION(BlueprintPure, Category="Pooling")
	bool IsActive() const { return bIsActive; }

	// --- Getters ---

public:

	/** Get the pickup type */
	UFUNCTION(BlueprintPure, Category="Pickup")
	EPickupType GetPickupType() const { return PickupType; }

	/** Get the current lane */
	UFUNCTION(BlueprintPure, Category="Pickup")
	ELane GetCurrentLane() const { return CurrentLane; }

	// --- Collection ---

protected:

	/**
	 * Called when player overlaps this pickup.
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
	 * Handle collection by player.
	 * 
	 * @param PlayerActor The player that collected this pickup
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Collection")
	void HandleCollection(AActor* PlayerActor);

	// --- Movement ---

protected:

	/** Update position based on scroll speed */
	virtual void UpdateScrolling(float DeltaTime);

	/** Update visual effects (rotation, bob) */
	virtual void UpdateVisualEffects(float DeltaTime);

	/** Check if should despawn */
	bool ShouldDespawn() const;

	/** Cache WorldScrollComponent reference */
	void CacheWorldScrollComponent();

	/** Setup collision box from Blueprint-configured values. Called in Activate(). */
	void SetupCollisionBox();
};
