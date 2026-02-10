#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WorldScrollComponent.generated.h"

class UObstacleSpawnerComponent;

/**
 * Delegate broadcast when scroll speed changes significantly.
 * Used by UI and other systems to react to speed changes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScrollSpeedChanged, float, NewScrollSpeed);

/**
 * Manages the world scroll speed for the entire game.
 * Attached to GameMode -- other actors call GetCurrentScrollSpeed()
 * to figure out how fast they should move.
 */
UCLASS(ClassGroup=(StateRunner), meta=(BlueprintSpawnableComponent))
class STATERUNNER_ARCADE_API UWorldScrollComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Constructor */
	UWorldScrollComponent();

protected:

	/** Called when the game starts */
	virtual void BeginPlay() override;

public:

	/** Called every frame - handles scroll speed calculation */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Scroll Speed Configuration ---

protected:

	/** Base scroll speed in units/sec. Starting speed when the game begins. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Scroll Speed", meta=(ClampMin="100.0", ClampMax="5000.0"))
	float BaseScrollSpeed = 1250.0f;

	/**
	 * Speed increase per second (linear).
	 * 8.0 = about +480 units/min. Slower ramp for a more manageable curve.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Scroll Speed", meta=(ClampMin="0.0", ClampMax="100.0"))
	float ScrollSpeedIncrease = 8.0f;

	/**
	 * Max scroll speed cap so the game doesn't get impossibly fast.
	 * Lowered from 3750 for a more survivable endgame.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Scroll Speed", meta=(ClampMin="500.0", ClampMax="10000.0"))
	float MaxScrollSpeed = 3000.0f;

	/**
	 * Threshold for broadcasting OnScrollSpeedChanged.
	 * Only broadcasts when speed changes by at least this amount.
	 * Prevents excessive event firing.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Scroll Speed", meta=(ClampMin="1.0", ClampMax="100.0"))
	float SpeedChangeThreshold = 50.0f;

	// --- Overclock System ---

protected:

	/**
	 * OVERCLOCK speed multiplier (1.0 = normal, 1.5 = 50% faster).
	 * Set by UOverclockSystemComponent when OVERCLOCK is activated.
	 * Applied on top of base scroll speed calculation.
	 */
	UPROPERTY(BlueprintReadOnly, Category="OVERCLOCK")
	float OverclockMultiplier = 1.0f;

	/**
	 * Whether OVERCLOCK mode is currently active.
	 * When true, OverclockMultiplier is applied to scroll speed.
	 */
	UPROPERTY(BlueprintReadOnly, Category="OVERCLOCK")
	bool bIsOverclockActive = false;

	// --- Runtime State ---

protected:

	/**
	 * Current calculated scroll speed in units per second.
	 * Formula: BaseScrollSpeed + (TimeElapsed * ScrollSpeedIncrease), capped at MaxScrollSpeed
	 */
	UPROPERTY(BlueprintReadOnly, Category="Scroll Speed|Runtime")
	float CurrentScrollSpeed = 0.0f;

	/**
	 * Time elapsed since scrolling started (in seconds).
	 * Used to calculate the current scroll speed.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Scroll Speed|Runtime")
	float TimeElapsed = 0.0f;

	/**
	 * Whether scrolling is currently active.
	 * When false, scroll speed is effectively zero and TimeElapsed doesn't increase.
	 * Use SetScrollingEnabled() to control this.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Scroll Speed|Runtime")
	bool bIsScrolling = false;

	/**
	 * Last broadcast scroll speed - used to determine when to fire OnScrollSpeedChanged.
	 */
	float LastBroadcastSpeed = 0.0f;

	/**
	 * Time of last periodic speed log - used for debug output every 5 seconds.
	 * Helps verify scroll speed is increasing without log spam.
	 */
	float LastPeriodicLogTime = 0.0f;

	// --- Damage Slowdown ---

protected:

	/**
	 * Multiplier applied when player takes damage (0.5 = 50% slower).
	 * Creates impact feel when hitting obstacles.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Damage Slowdown", meta=(ClampMin="0.1", ClampMax="1.0"))
	float DamageSlowdownMultiplier = 0.5f;

	/**
	 * Duration of damage slowdown in seconds.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Damage Slowdown", meta=(ClampMin="0.1", ClampMax="2.0"))
	float DamageSlowdownDuration = 0.5f;

	/**
	 * Whether damage slowdown is currently active.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Damage Slowdown")
	bool bIsDamageSlowdownActive = false;

	/**
	 * Time remaining in damage slowdown.
	 */
	float DamageSlowdownTimeRemaining = 0.0f;

	// --- Cached References ---

protected:

	/**
	 * Cached reference to ObstacleSpawnerComponent for tutorial prompt checking.
	 */
	UPROPERTY()
	TObjectPtr<UObstacleSpawnerComponent> CachedObstacleSpawner;

	// --- Events ---

public:

	/** Broadcast when scroll speed changes by at least SpeedChangeThreshold. */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnScrollSpeedChanged OnScrollSpeedChanged;

	// --- Public Functions ---

public:

	/**
	 * Returns current scroll speed in units per second.
	 * 
	 * @return Current scroll speed (0 if scrolling is disabled)
	 */
	UFUNCTION(BlueprintPure, Category="Scroll Speed")
	float GetCurrentScrollSpeed() const;

	/**
	 * Get the raw scroll speed without damage slowdown multiplier.
	 * Useful for UI display or systems that shouldn't be affected by slowdown.
	 * 
	 * @return Base calculated scroll speed (ignoring damage slowdown)
	 */
	UFUNCTION(BlueprintPure, Category="Scroll Speed")
	float GetRawScrollSpeed() const { return CurrentScrollSpeed; }

	/**
	 * Enable or disable scrolling.
	 * When disabled, GetCurrentScrollSpeed() returns 0 and time doesn't advance.
	 * Useful for menus, death sequences, pausing, etc.
	 * 
	 * @param bEnabled True to enable scrolling, false to pause
	 */
	UFUNCTION(BlueprintCallable, Category="Scroll Speed")
	void SetScrollingEnabled(bool bEnabled);

	/**
	 * Check if scrolling is currently enabled.
	 * 
	 * @return True if scrolling is active
	 */
	UFUNCTION(BlueprintPure, Category="Scroll Speed")
	bool IsScrollingEnabled() const { return bIsScrolling; }

	/**
	 * Resets scroll speed to initial state.
	 * Resets TimeElapsed to 0 and recalculates CurrentScrollSpeed.
	 */
	UFUNCTION(BlueprintCallable, Category="Scroll Speed")
	void ResetScrollSpeed();

	/**
	 * Get the time elapsed since scrolling started.
	 * 
	 * @return Time in seconds
	 */
	UFUNCTION(BlueprintPure, Category="Scroll Speed")
	float GetTimeElapsed() const { return TimeElapsed; }

	/**
	 * Apply damage slowdown effect.
	 * Called when player hits an obstacle.
	 * Temporarily reduces scroll speed by DamageSlowdownMultiplier.
	 */
	UFUNCTION(BlueprintCallable, Category="Damage Slowdown")
	void ApplyDamageSlowdown();

	/**
	 * Check if damage slowdown is currently active.
	 * 
	 * @return True if slowdown is active
	 */
	UFUNCTION(BlueprintPure, Category="Damage Slowdown")
	bool IsDamageSlowdownActive() const { return bIsDamageSlowdownActive; }

	// --- Overclock Functions ---

public:

	/**
	 * Set the OVERCLOCK speed multiplier.
	 * Called by UOverclockSystemComponent when OVERCLOCK is activated/deactivated.
	 * 
	 * @param Multiplier Speed multiplier (1.0 = normal, 1.5 = 50% faster)
	 * @param bActivate Whether OVERCLOCK mode is being activated
	 */
	UFUNCTION(BlueprintCallable, Category="OVERCLOCK")
	void SetOverclockMultiplier(float Multiplier, bool bActivate);

	/**
	 * Get the current OVERCLOCK multiplier.
	 * 
	 * @return Current multiplier (1.0 if OVERCLOCK not active)
	 */
	UFUNCTION(BlueprintPure, Category="OVERCLOCK")
	float GetOverclockMultiplier() const { return bIsOverclockActive ? OverclockMultiplier : 1.0f; }

	/**
	 * Check if OVERCLOCK mode is currently active.
	 * 
	 * @return True if OVERCLOCK is active
	 */
	UFUNCTION(BlueprintPure, Category="OVERCLOCK")
	bool IsOverclockActive() const { return bIsOverclockActive; }

protected:

	/**
	 * Calculate and update the current scroll speed based on time elapsed.
	 * Called every tick when scrolling is enabled.
	 */
	void UpdateScrollSpeed();

	/**
	 * Process damage slowdown timer.
	 * Called every tick to handle slowdown duration.
	 */
	void ProcessDamageSlowdown(float DeltaTime);

	/**
	 * Broadcast scroll speed change if threshold exceeded.
	 */
	void BroadcastSpeedChangeIfNeeded();
};
