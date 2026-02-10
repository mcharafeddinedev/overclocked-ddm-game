#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LivesSystemComponent.generated.h"

class UWorldScrollComponent;
class USoundBase;

/**
 * Delegate broadcast when lives change.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLivesChanged, int32, CurrentLives, int32, MaxLives);

/**
 * Delegate broadcast when player takes damage.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDamageTaken, int32, RemainingLives);

/**
 * Delegate broadcast when player dies (all lives lost).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDied);

/**
 * Delegate broadcast when invulnerability state changes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInvulnerabilityChanged, bool, bIsInvulnerable);

/**
 * Lives System Component
 * 
 * Manages player lives, damage, invulnerability, and death.
 * 
 * DAMAGE MECHANICS:
 * - 5 lives total
 * - Lose 1 life on obstacle collision
 * - Invulnerability period after damage (prevents chain deaths)
 * - Screen shake on damage
 * - Scroll slowdown on damage (via WorldScrollComponent)
 */
UCLASS(ClassGroup=(StateRunner), meta=(BlueprintSpawnableComponent))
class STATERUNNER_ARCADE_API ULivesSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	ULivesSystemComponent();

protected:

	virtual void BeginPlay() override;

	//=============================================================================
	// LIVES CONFIGURATION
	//=============================================================================

protected:

	/**
	 * Maximum/starting lives.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lives Config", meta=(ClampMin="1", ClampMax="10"))
	int32 MaxLives = 5;

	/**
	 * Invulnerability duration after taking damage (seconds).
	 * 
	 * TUNING: Increased to 1.0 (was 0.8) - slightly more forgiving recovery time.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lives Config", meta=(ClampMin="0.1", ClampMax="3.0"))
	float InvulnerabilityDuration = 1.0f;

	/**
	 * Screen shake intensity on damage (0 = none, 1 = normal).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lives Config|Effects", meta=(ClampMin="0.0", ClampMax="2.0"))
	float DamageShakeIntensity = 1.0f;

	/**
	 * Screen shake duration on damage (seconds).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lives Config|Effects", meta=(ClampMin="0.1", ClampMax="1.0"))
	float DamageShakeDuration = 0.5f;

	/**
	 * Whether to apply scroll slowdown on damage.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lives Config|Effects")
	bool bApplyScrollSlowdown = true;

	//=============================================================================
	// SOUND EFFECTS
	//=============================================================================

protected:

	/** Sound to play when taking damage (hit by obstacle) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lives Config|Sound")
	TObjectPtr<USoundBase> DamageSound;

	/** Sound to play when collecting a 1-Up (gaining a life) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lives Config|Sound")
	TObjectPtr<USoundBase> OneUpSound;

	/** Sound to play when the player dies (all lives lost) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lives Config|Sound")
	TObjectPtr<USoundBase> DeathSound;

	/** Volume multiplier for lives-related sounds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lives Config|Sound", meta=(ClampMin="0.0", ClampMax="2.0"))
	float LivesSoundVolume = 1.0f;

	/** Play a sound (uses player location if available, otherwise world origin) */
	void PlayLifeSound(USoundBase* Sound);

	//=============================================================================
	// RUNTIME STATE
	//=============================================================================

protected:

	/**
	 * Current lives remaining.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Lives")
	int32 CurrentLives = 0;

	/**
	 * Whether player is currently invulnerable.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Lives")
	bool bIsInvulnerable = false;

	/**
	 * Whether player is dead (all lives lost).
	 */
	UPROPERTY(BlueprintReadOnly, Category="Lives")
	bool bIsDead = false;

	/**
	 * Timer handle for invulnerability.
	 */
	FTimerHandle InvulnerabilityTimer;

	/**
	 * Cached reference to WorldScrollComponent.
	 */
	UPROPERTY()
	TObjectPtr<UWorldScrollComponent> WorldScrollComponent;

	//=============================================================================
	// EVENTS
	//=============================================================================

public:

	/** Broadcast when lives count changes */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnLivesChanged OnLivesChanged;

	/** Broadcast when damage is taken */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnDamageTaken OnDamageTaken;

	/** Broadcast when player dies */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnPlayerDied OnPlayerDied;

	/** Broadcast when invulnerability state changes */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnInvulnerabilityChanged OnInvulnerabilityChanged;

	//=============================================================================
	// PUBLIC FUNCTIONS
	//=============================================================================

public:

	/**
	 * Take damage (lose 1 life).
	 * Respects invulnerability - won't damage if invulnerable.
	 * 
	 * @return True if damage was taken, false if invulnerable or dead
	 */
	UFUNCTION(BlueprintCallable, Category="Lives")
	bool TakeDamage();

	/**
	 * Check if player can take damage.
	 */
	UFUNCTION(BlueprintPure, Category="Lives")
	bool CanTakeDamage() const { return !bIsInvulnerable && !bIsDead; }

	/**
	 * Get current lives.
	 */
	UFUNCTION(BlueprintPure, Category="Lives")
	int32 GetCurrentLives() const { return CurrentLives; }

	/**
	 * Get max lives.
	 */
	UFUNCTION(BlueprintPure, Category="Lives")
	int32 GetMaxLives() const { return MaxLives; }

	/**
	 * Check if player is dead.
	 */
	UFUNCTION(BlueprintPure, Category="Lives")
	bool IsDead() const { return bIsDead; }

	/**
	 * Check if player is invulnerable.
	 */
	UFUNCTION(BlueprintPure, Category="Lives")
	bool IsInvulnerable() const { return bIsInvulnerable; }

	/**
	 * Reset lives to max (for game restart).
	 */
	UFUNCTION(BlueprintCallable, Category="Lives")
	void ResetLives();

	/**
	 * Add lives (power-up, etc.).
	 * 
	 * @param Amount Lives to add
	 */
	UFUNCTION(BlueprintCallable, Category="Lives")
	void AddLives(int32 Amount);

	/**
	 * Check if player is at maximum lives.
	 * 
	 * @return True if CurrentLives == MaxLives
	 */
	UFUNCTION(BlueprintPure, Category="Lives")
	bool IsAtMaxLives() const { return CurrentLives >= MaxLives; }

	/**
	 * Collect a 1-Up pickup.
	 * If at max lives: returns true (caller should award bonus score).
	 * If below max: adds 1 life and returns false.
	 * 
	 * @return True if at max lives (should award score bonus instead)
	 */
	UFUNCTION(BlueprintCallable, Category="Lives")
	bool Collect1Up();

	/**
	 * Set invulnerability state directly.
	 * 
	 * @param bInvulnerable Whether to be invulnerable
	 * @param Duration Duration in seconds (0 = use default, -1 = permanent until cleared)
	 */
	UFUNCTION(BlueprintCallable, Category="Lives")
	void SetInvulnerable(bool bInvulnerable, float Duration = 0.0f);

	//=============================================================================
	// PROTECTED FUNCTIONS
	//=============================================================================

protected:

	/** Apply damage effects (shake, slowdown) */
	void ApplyDamageEffects();

	/** Start invulnerability period */
	void StartInvulnerability();

	/** End invulnerability (called by timer) */
	UFUNCTION()
	void EndInvulnerability();

	/** Trigger player death */
	void TriggerDeath();

	/** Cache WorldScrollComponent reference */
	void CacheWorldScrollComponent();

	/** Apply screen shake effect */
	void ApplyScreenShake();
};
