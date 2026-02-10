#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AudioSettingsSubsystem.generated.h"

class USoundMix;
class USoundClass;

/**
 * Audio Settings Subsystem
 * 
 * Manages audio volume settings persistence and application.
 * - Loads saved volume settings on game startup
 * - Applies volume settings on every level load
 * - Provides static utility functions for applying audio settings from anywhere
 * 
 * Volume gets applied BEFORE any audio plays, not just when
 * the Settings menu is opened.
 */
UCLASS()
class STATERUNNER_ARCADE_API UAudioSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	// --- Subsystem Lifecycle ---

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Config Keys (must match SettingsMenuWidget) ---

	static const FString AudioConfigSection;
	static const FString MasterVolumeKey;
	static const FString MusicVolumeKey;
	static const FString SFXVolumeKey;

	// --- Default Volumes (for first-time users / shipped package) ---

	static constexpr float DefaultMasterVolume = 0.85f;   // 85%
	static constexpr float DefaultMusicVolume = 0.5f;    // 50%
	static constexpr float DefaultSFXVolume = 1.0f;      // 100%

	// --- Public Functions ---

	/**
	 * Load and apply audio settings from config.
	 * Called automatically on startup and level load.
	 */
	UFUNCTION(BlueprintCallable, Category="Audio")
	void ApplyAudioSettings();

	/**
	 * Get the current volume settings from config.
	 */
	UFUNCTION(BlueprintCallable, Category="Audio")
	void GetVolumeSettings(float& OutMasterVolume, float& OutMusicVolume, float& OutSFXVolume) const;

	/**
	 * Static helper to apply audio settings from anywhere.
	 * Gets the subsystem from the game instance and applies settings.
	 */
	UFUNCTION(BlueprintCallable, Category="Audio", meta=(WorldContext="WorldContextObject"))
	static void ApplyAudioSettingsStatic(const UObject* WorldContextObject);

protected:

	// --- Audio Assets (loaded from paths) ---

	/** Sound Mix for volume control */
	UPROPERTY()
	TObjectPtr<USoundMix> VolumeSoundMix;

	/** Master sound class */
	UPROPERTY()
	TObjectPtr<USoundClass> MasterSoundClass;

	/** Music sound class */
	UPROPERTY()
	TObjectPtr<USoundClass> MusicSoundClass;

	/** SFX sound class */
	UPROPERTY()
	TObjectPtr<USoundClass> SFXSoundClass;

	// --- Internal ---

	/** Load audio assets from configured paths */
	void LoadAudioAssets();

	/** Apply volume to a specific sound class */
	void ApplyVolumeToSoundClass(UWorld* World, USoundClass* SoundClass, float Volume);

	/** Handle level load event */
	void OnPostLoadMapWithWorld(UWorld* LoadedWorld);

	/** Schedule a delayed apply to catch audio that starts after initialization */
	void ScheduleDelayedApply();

	/** Delegate handle for level load */
	FDelegateHandle PostLoadMapHandle;

	/** Timer handle for delayed apply */
	FTimerHandle DelayedApplyTimerHandle;

	/** Timer handle for re-enabling OnAudioFinished in MusicPersistenceSubsystem */
	FTimerHandle IgnoreFlagClearTimerHandle;
};
