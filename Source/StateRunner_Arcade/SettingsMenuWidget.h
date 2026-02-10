#pragma once

#include "CoreMinimal.h"
#include "ArcadeMenuWidget.h"
#include "SettingsMenuWidget.generated.h"

class UButton;
class USlider;
class UTextBlock;
class USoundMix;
class USoundClass;

/**
 * Graphics quality preset options for StateRunner settings.
 * Named ESRGraphicsPreset to avoid collision with engine's EGraphicsPreset.
 */
UENUM(BlueprintType)
enum class ESRGraphicsPreset : uint8
{
	Low			UMETA(DisplayName = "Low"),
	Medium		UMETA(DisplayName = "Medium"),
	High		UMETA(DisplayName = "High"),
	Epic		UMETA(DisplayName = "Epic"),
	Cinematic	UMETA(DisplayName = "Cinematic")
};

/**
 * Fullscreen mode options.
 */
UENUM(BlueprintType)
enum class EFullscreenSetting : uint8
{
	Fullscreen			UMETA(DisplayName = "Fullscreen"),
	WindowedFullscreen	UMETA(DisplayName = "Windowed Fullscreen"),
	Windowed			UMETA(DisplayName = "Windowed")
};

/**
 * Settings Menu Widget for StateRunner Arcade
 * 
 * Provides graphics and audio settings with full keyboard navigation.
 * 
 * GRAPHICS SETTINGS:
 * - Quality Preset: Low/Medium/High/Epic/Cinematic (cycles with Left/Right)
 * - Resolution: 720p/1080p/1440p/4K (cycles with Left/Right)
 * - Fullscreen Mode: Fullscreen/Windowed Fullscreen/Windowed (cycles with Left/Right)
 * 
 * AUDIO SETTINGS:
 * - Master Volume: 0-100% slider
 * - Music Volume: 0-100% slider
 * - SFX Volume: 0-100% slider
 * 
 * ARCADE NAVIGATION:
 * - Arrow Up/Down: Navigate between settings
 * - Arrow Left/Right: Adjust slider values / cycle through options
 * - Enter: Confirm (apply changes if applicable)
 * - Escape: Go back to main menu
 * 
 * PERSISTENCE:
 * - Graphics settings use UGameUserSettings (saved automatically)
 * - Audio settings stored in custom config section
 */
UCLASS(Abstract, Blueprintable)
class STATERUNNER_ARCADE_API USettingsMenuWidget : public UArcadeMenuWidget
{
	GENERATED_BODY()

public:

	USettingsMenuWidget(const FObjectInitializer& ObjectInitializer);

protected:

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//=============================================================================
	// GRAPHICS WIDGETS
	//=============================================================================

protected:

	/** Quality preset selector - displays current preset name */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Graphics")
	TObjectPtr<UTextBlock> QualityPresetValue;

	/** Resolution selector - displays current resolution */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Graphics")
	TObjectPtr<UTextBlock> ResolutionValue;

	/** Fullscreen mode selector - displays current mode */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Graphics")
	TObjectPtr<UTextBlock> FullscreenValue;

	/** Invisible buttons used as focusable anchors for selectors */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Graphics")
	TObjectPtr<UButton> QualityPresetButton;

	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Graphics")
	TObjectPtr<UButton> ResolutionButton;

	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Graphics")
	TObjectPtr<UButton> FullscreenButton;

	//=============================================================================
	// AUDIO WIDGETS
	//=============================================================================

protected:

	/** Master volume slider */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Audio")
	TObjectPtr<USlider> MasterVolumeSlider;

	/** Music volume slider */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Audio")
	TObjectPtr<USlider> MusicVolumeSlider;

	/** SFX volume slider */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Audio")
	TObjectPtr<USlider> SFXVolumeSlider;

	/** Master volume percentage text */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Audio")
	TObjectPtr<UTextBlock> MasterVolumeValue;

	/** Music volume percentage text */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Audio")
	TObjectPtr<UTextBlock> MusicVolumeValue;

	/** SFX volume percentage text */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Audio")
	TObjectPtr<UTextBlock> SFXVolumeValue;

	//=============================================================================
	// NAVIGATION WIDGETS
	//=============================================================================

protected:

	/** Back button (returns to main menu) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Navigation")
	TObjectPtr<UButton> BackButton;

	//=============================================================================
	// OPTIONAL LABELS
	// For visual focus feedback
	//=============================================================================

protected:

	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> QualityPresetLabel;

	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> ResolutionLabel;

	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> FullscreenLabel;

	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> MasterVolumeLabel;

	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> MusicVolumeLabel;

	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> SFXVolumeLabel;

	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> BackLabel;

	//=============================================================================
	// MENU ITEM INDICES
	// Order matches visual layout (top to bottom) and registration order
	//=============================================================================

public:

	static const int32 INDEX_MASTER_VOLUME = 0;
	static const int32 INDEX_MUSIC_VOLUME = 1;
	static const int32 INDEX_SFX_VOLUME = 2;
	static const int32 INDEX_QUALITY_PRESET = 3;
	static const int32 INDEX_RESOLUTION = 4;
	static const int32 INDEX_FULLSCREEN = 5;
	static const int32 INDEX_BACK = 6;

	//=============================================================================
	// AUDIO CONFIGURATION
	//=============================================================================

protected:

	/**
	 * Sound Mix asset for volume control.
	 * Create this in Editor: Right-click → Sounds → Sound Mix
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio Config")
	TObjectPtr<USoundMix> VolumeSoundMix;

	/**
	 * Sound Class for master volume.
	 * This should be the root sound class that all others inherit from.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio Config")
	TObjectPtr<USoundClass> MasterSoundClass;

	/**
	 * Sound Class for music.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio Config")
	TObjectPtr<USoundClass> MusicSoundClass;

	/**
	 * Sound Class for SFX.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio Config")
	TObjectPtr<USoundClass> SFXSoundClass;

	//=============================================================================
	// RUNTIME STATE
	//=============================================================================

protected:

	/** Current graphics quality preset index */
	UPROPERTY(BlueprintReadOnly, Category="State")
	int32 CurrentQualityPresetIndex = 0; // Low (LOW-END branch default)

	/** Current resolution index */
	UPROPERTY(BlueprintReadOnly, Category="State")
	int32 CurrentResolutionIndex = 0; // 1280x720 (LOW-END branch default)

	/** Current fullscreen mode index */
	UPROPERTY(BlueprintReadOnly, Category="State")
	int32 CurrentFullscreenIndex = 2; // Windowed (shipped default)

	/** Available resolutions */
	UPROPERTY(BlueprintReadOnly, Category="State")
	TArray<FIntPoint> AvailableResolutions;

	/** Quality preset names for display */
	TArray<FString> QualityPresetNames;

	/** Fullscreen mode names for display */
	TArray<FString> FullscreenModeNames;

	//=============================================================================
	// PUBLIC FUNCTIONS
	//=============================================================================

public:

	/**
	 * Apply current graphics settings.
	 */
	UFUNCTION(BlueprintCallable, Category="Settings")
	void ApplyGraphicsSettings();

	/**
	 * Apply current audio settings.
	 */
	UFUNCTION(BlueprintCallable, Category="Settings")
	void ApplyAudioSettings();

	/**
	 * Load saved settings from config.
	 */
	UFUNCTION(BlueprintCallable, Category="Settings")
	void LoadSettings();

	/**
	 * Save current settings to config.
	 */
	UFUNCTION(BlueprintCallable, Category="Settings")
	void SaveSettings();

	/**
	 * Get current master volume (0.0 to 1.0).
	 */
	UFUNCTION(BlueprintPure, Category="Audio")
	float GetMasterVolume() const;

	/**
	 * Get current music volume (0.0 to 1.0).
	 */
	UFUNCTION(BlueprintPure, Category="Audio")
	float GetMusicVolume() const;

	/**
	 * Get current SFX volume (0.0 to 1.0).
	 */
	UFUNCTION(BlueprintPure, Category="Audio")
	float GetSFXVolume() const;

	/**
	 * Set master volume (0.0 to 1.0).
	 */
	UFUNCTION(BlueprintCallable, Category="Audio")
	void SetMasterVolume(float Volume);

	/**
	 * Set music volume (0.0 to 1.0).
	 */
	UFUNCTION(BlueprintCallable, Category="Audio")
	void SetMusicVolume(float Volume);

	/**
	 * Set SFX volume (0.0 to 1.0).
	 */
	UFUNCTION(BlueprintCallable, Category="Audio")
	void SetSFXVolume(float Volume);

	//=============================================================================
	// BLUEPRINT EVENTS
	//=============================================================================

public:

	/**
	 * Called when any setting changes.
	 * Sound effects, visual feedback hook.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnSettingChanged(int32 SettingIndex);

protected:

	//=============================================================================
	// INTERNAL FUNCTIONS
	//=============================================================================

	/** Initialize available resolutions list */
	void InitializeResolutions();

	/** Initialize quality preset names */
	void InitializePresetNames();

	/** Load audio assets programmatically (ensures consistency with AudioSettingsSubsystem) */
	void LoadAudioAssets();

	/** Update display text for quality preset */
	void UpdateQualityPresetDisplay();

	/** Update display text for resolution */
	void UpdateResolutionDisplay();

	/** Update display text for fullscreen mode */
	void UpdateFullscreenDisplay();

	/** Update display text for volume slider */
	void UpdateVolumeDisplay(UTextBlock* ValueText, float Volume);

	/** Cycle quality preset (direction: -1 = previous, +1 = next) */
	void CycleQualityPreset(int32 Direction);

	/** Cycle resolution (direction: -1 = previous, +1 = next) */
	void CycleResolution(int32 Direction);

	/** Cycle fullscreen mode (direction: -1 = previous, +1 = next) */
	void CycleFullscreenMode(int32 Direction);

	/** Apply volume to a sound class via the sound mix */
	void ApplyVolumeToSoundClass(USoundClass* SoundClass, float Volume);

	//=============================================================================
	// SLIDER CALLBACKS
	//=============================================================================

	UFUNCTION()
	void OnMasterVolumeChanged(float Value);

	UFUNCTION()
	void OnMusicVolumeChanged(float Value);

	UFUNCTION()
	void OnSFXVolumeChanged(float Value);

	UFUNCTION()
	void OnBackButtonClicked();

	//=============================================================================
	// SELECTOR BUTTON CLICK CALLBACKS (for mouse users)
	//=============================================================================

	/** Called when Quality Preset selector is clicked (cycles forward) */
	UFUNCTION()
	void OnQualityPresetClicked();

	/** Called when Resolution selector is clicked (cycles forward) */
	UFUNCTION()
	void OnResolutionClicked();

	/** Called when Fullscreen selector is clicked (cycles forward) */
	UFUNCTION()
	void OnFullscreenClicked();

	//=============================================================================
	// OVERRIDES
	//=============================================================================

	virtual void OnSelectorOptionChanged_Implementation(int32 ItemIndex, int32 Delta) override;
	virtual void OnSliderValueChanged_Implementation(int32 ItemIndex, float NewValue) override;
	virtual void OnBackAction_Implementation() override;
	virtual void OnItemSelected_Implementation(int32 ItemIndex) override;
};
