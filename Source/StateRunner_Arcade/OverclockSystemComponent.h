#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OverclockSystemComponent.generated.h"

class UWorldScrollComponent;
class UScoreSystemComponent;
class USoundBase;
class UAudioComponent;

/**
 * Delegate broadcast when OVERCLOCK state changes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOverclockStateChanged, bool, bIsActive);

/**
 * Delegate broadcast when OVERCLOCK meter changes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOverclockMeterChanged, float, CurrentMeter, float, MaxMeter);

/**
 * OVERCLOCK System Component
 * 
 * Manages the OVERCLOCK mechanic - a risk/reward speed boost system.
 * 
 * MECHANICS:
 * - Meter fills passively over time and from collecting pickups
 * - Player activates with L-SHIFT when meter is above threshold
 * - While active: Speed increases, bonus score earned, meter drains
 * - Deactivates when meter empties or key released
 * 
 * INTEGRATION:
 * - WorldScrollComponent: Speed multiplier
 * - ScoreSystemComponent: Bonus score rate
 */
UCLASS(ClassGroup=(StateRunner), meta=(BlueprintSpawnableComponent))
class STATERUNNER_ARCADE_API UOverclockSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UOverclockSystemComponent();

protected:

	virtual void BeginPlay() override;

public:

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	//=============================================================================
	// OVERCLOCK CONFIGURATION
	//=============================================================================

protected:

	/**
	 * Maximum OVERCLOCK meter value.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OVERCLOCK Config", meta=(ClampMin="10.0", ClampMax="200.0"))
	float MaxMeter = 100.0f;

	/**
	 * Minimum meter required to activate OVERCLOCK.
	 * Prevents rapid on/off toggling.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OVERCLOCK Config", meta=(ClampMin="5.0", ClampMax="50.0"))
	float ActivationThreshold = 25.0f;

	/**
	 * Passive meter fill rate (units per second).
	 * Meter fills while playing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OVERCLOCK Config", meta=(ClampMin="0.0", ClampMax="50.0"))
	float PassiveFillRate = 5.0f;

	/**
	 * Meter bonus from collecting a Data Packet.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OVERCLOCK Config", meta=(ClampMin="1.0", ClampMax="100.0"))
	float PickupMeterBonus = 12.0f;

	/**
	 * Meter drain rate while OVERCLOCK is active (units per second).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OVERCLOCK Config", meta=(ClampMin="5.0", ClampMax="100.0"))
	float ActiveDrainRate = 20.0f;

	/** Speed multiplier while OVERCLOCK is active. 2.5 = 150% faster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OVERCLOCK Config", meta=(ClampMin="1.1", ClampMax="5.0"))
	float SpeedMultiplier = 2.5f;

	//=============================================================================
	// SOUND EFFECTS
	//=============================================================================

protected:

	/** Sound to play when OVERCLOCK activates */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="OVERCLOCK Config|Sound")
	TObjectPtr<USoundBase> OverclockActivateSound;

	/** Sound to play when OVERCLOCK deactivates */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="OVERCLOCK Config|Sound")
	TObjectPtr<USoundBase> OverclockDeactivateSound;

	/** 
	 * Looping sound to play while OVERCLOCK is active (engine hum, energy loop, etc.)
	 * This will fade in when activated and fade out when deactivated.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="OVERCLOCK Config|Sound")
	TObjectPtr<USoundBase> OverclockLoopSound;

	/** Volume multiplier for OVERCLOCK sounds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="OVERCLOCK Config|Sound", meta=(ClampMin="0.0", ClampMax="2.0"))
	float OverclockSoundVolume = 1.0f;

	/** Fade duration for the loop sound (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="OVERCLOCK Config|Sound", meta=(ClampMin="0.0", ClampMax="2.0"))
	float LoopSoundFadeDuration = 0.3f;

	/** Active audio component for the loop sound */
	UPROPERTY()
	TObjectPtr<UAudioComponent> ActiveLoopAudio;

	/** Play a one-shot OVERCLOCK sound */
	void PlayOverclockSound(USoundBase* Sound);

	/** Start the looping OVERCLOCK sound */
	void StartOverclockLoop();

	/** Stop the looping OVERCLOCK sound with fade */
	void StopOverclockLoop();

	//=============================================================================
	// RUNTIME STATE
	//=============================================================================

protected:

	/**
	 * Current OVERCLOCK meter value.
	 */
	UPROPERTY(BlueprintReadOnly, Category="OVERCLOCK")
	float CurrentMeter = 0.0f;

	/**
	 * Whether OVERCLOCK is currently active.
	 */
	UPROPERTY(BlueprintReadOnly, Category="OVERCLOCK")
	bool bIsOverclockActive = false;

	/**
	 * Whether the OVERCLOCK key is currently held.
	 */
	UPROPERTY(BlueprintReadOnly, Category="OVERCLOCK")
	bool bIsKeyHeld = false;

	/**
	 * Cached reference to WorldScrollComponent.
	 */
	UPROPERTY()
	TObjectPtr<UWorldScrollComponent> WorldScrollComponent;

	/**
	 * Cached reference to ScoreSystemComponent.
	 */
	UPROPERTY()
	TObjectPtr<UScoreSystemComponent> ScoreSystemComponent;

	//=============================================================================
	// EVENTS
	//=============================================================================

public:

	/** Broadcast when OVERCLOCK activates or deactivates */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnOverclockStateChanged OnOverclockStateChanged;

	/** Broadcast when meter value changes */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnOverclockMeterChanged OnOverclockMeterChanged;

	//=============================================================================
	// PUBLIC FUNCTIONS
	//=============================================================================

public:

	/**
	 * Called when OVERCLOCK key is pressed.
	 */
	UFUNCTION(BlueprintCallable, Category="OVERCLOCK")
	void OnOverclockKeyPressed();

	/**
	 * Called when OVERCLOCK key is released.
	 */
	UFUNCTION(BlueprintCallable, Category="OVERCLOCK")
	void OnOverclockKeyReleased();

	/**
	 * Add meter from pickup collection.
	 */
	UFUNCTION(BlueprintCallable, Category="OVERCLOCK")
	void AddPickupBonus();

	/**
	 * Get current meter value.
	 */
	UFUNCTION(BlueprintPure, Category="OVERCLOCK")
	float GetCurrentMeter() const { return CurrentMeter; }

	/**
	 * Get meter as percentage (0.0 to 1.0).
	 */
	UFUNCTION(BlueprintPure, Category="OVERCLOCK")
	float GetMeterPercent() const { return MaxMeter > 0.0f ? CurrentMeter / MaxMeter : 0.0f; }

	/**
	 * Check if OVERCLOCK is currently active.
	 */
	UFUNCTION(BlueprintPure, Category="OVERCLOCK")
	bool IsOverclockActive() const { return bIsOverclockActive; }

	/** Check if OVERCLOCK can be activated (meter above threshold). */
	UFUNCTION(BlueprintPure, Category="OVERCLOCK")
	bool CanActivateOverclock() const { return CurrentMeter >= ActivationThreshold && !bIsOverclockActive; }

	/** Reset OVERCLOCK state (for game restart). */
	UFUNCTION(BlueprintCallable, Category="OVERCLOCK")
	void ResetOverclock();

	//=============================================================================
	// PROTECTED FUNCTIONS
	//=============================================================================

protected:

	/** Activate OVERCLOCK mode */
	void ActivateOverclock();

	/** Deactivate OVERCLOCK mode */
	void DeactivateOverclock();

	/** Update meter (fill or drain) */
	void UpdateMeter(float DeltaTime);

	/** Cache component references */
	void CacheComponents();
};
