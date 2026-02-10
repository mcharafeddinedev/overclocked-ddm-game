#include "SettingsMenuWidget.h"
#include "MusicPersistenceSubsystem.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "StateRunner_Arcade.h"

// Config section for audio settings
static const FString AudioConfigSection = TEXT("/Script/StateRunner_Arcade.AudioSettings");
static const FString MasterVolumeKey = TEXT("MasterVolume");
static const FString MusicVolumeKey = TEXT("MusicVolume");
static const FString SFXVolumeKey = TEXT("SFXVolume");

// Config section for graphics settings initialization tracking
static const FString GraphicsConfigSection = TEXT("/Script/StateRunner_Arcade.GraphicsSettings");
static const FString SettingsInitializedKey = TEXT("bSettingsInitialized");

// Audio asset paths - MUST match AudioSettingsSubsystem paths exactly!
// Prefixed with "Settings_" to avoid collision with MusicPersistenceSubsystem
static const TCHAR* Settings_SoundMixPath = TEXT("/Game/_DEVELOPER/Audio/SoundClasses/SM_GameVolume.SM_GameVolume");
static const TCHAR* Settings_MasterSoundClassPath = TEXT("/Game/_DEVELOPER/Audio/SoundClasses/SClass_Master.SClass_Master");
static const TCHAR* Settings_MusicSoundClassPath = TEXT("/Game/_DEVELOPER/Audio/SoundClasses/SClass_Music.SClass_Music");
static const TCHAR* Settings_SFXSoundClassPath = TEXT("/Game/_DEVELOPER/Audio/SoundClasses/SClass_SFX.SClass_SFX");

USettingsMenuWidget::USettingsMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Default focus on first setting
	DefaultFocusIndex = INDEX_QUALITY_PRESET;

	// Initialize preset names
	InitializePresetNames();
}

void USettingsMenuWidget::NativeConstruct()
{
	// Load audio assets programmatically to ensure they match AudioSettingsSubsystem
	LoadAudioAssets();

	// Initialize resolutions before registering widgets
	InitializeResolutions();

	// =========================================================================
	// REGISTER ITEMS IN VISUAL ORDER (top to bottom)
	// Keyboard navigation order!
	// =========================================================================

	// 1. Audio sliders (top section)
	if (MasterVolumeSlider)
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("SettingsMenuWidget: MasterVolumeSlider FOUND"));
		RegisterSlider(MasterVolumeSlider, MasterVolumeLabel, MasterVolumeValue);
		MasterVolumeSlider->OnValueChanged.AddDynamic(this, &USettingsMenuWidget::OnMasterVolumeChanged);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("SettingsMenuWidget: MasterVolumeSlider NOT FOUND - check widget name in Blueprint"));
	}

	if (MusicVolumeSlider)
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("SettingsMenuWidget: MusicVolumeSlider FOUND"));
		RegisterSlider(MusicVolumeSlider, MusicVolumeLabel, MusicVolumeValue);
		MusicVolumeSlider->OnValueChanged.AddDynamic(this, &USettingsMenuWidget::OnMusicVolumeChanged);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("SettingsMenuWidget: MusicVolumeSlider NOT FOUND - check widget name in Blueprint"));
	}

	if (SFXVolumeSlider)
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("SettingsMenuWidget: SFXVolumeSlider FOUND"));
		RegisterSlider(SFXVolumeSlider, SFXVolumeLabel, SFXVolumeValue);
		SFXVolumeSlider->OnValueChanged.AddDynamic(this, &USettingsMenuWidget::OnSFXVolumeChanged);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("SettingsMenuWidget: SFXVolumeSlider NOT FOUND - check widget name in Blueprint"));
	}

	// 2. Graphics selectors (middle section)
	if (QualityPresetButton)
	{
		RegisterFocusableItem(QualityPresetButton, EArcadeFocusType::Selector, QualityPresetLabel, QualityPresetValue);
		// Bind click to cycle forward (for mouse users)
		QualityPresetButton->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnQualityPresetClicked);
	}

	if (ResolutionButton)
	{
		RegisterFocusableItem(ResolutionButton, EArcadeFocusType::Selector, ResolutionLabel, ResolutionValue);
		// Bind click to cycle forward (for mouse users)
		ResolutionButton->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnResolutionClicked);
	}

	if (FullscreenButton)
	{
		RegisterFocusableItem(FullscreenButton, EArcadeFocusType::Selector, FullscreenLabel, FullscreenValue);
		// Bind click to cycle forward (for mouse users)
		FullscreenButton->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnFullscreenClicked);
	}

	// 3. Back button (bottom)
	if (BackButton)
	{
		RegisterButton(BackButton, BackLabel);
		BackButton->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnBackButtonClicked);
	}

	// Load saved settings
	LoadSettings();

	// Update all displays
	UpdateQualityPresetDisplay();
	UpdateResolutionDisplay();
	UpdateFullscreenDisplay();

	// Call parent (sets initial focus)
	Super::NativeConstruct();

	// Push the sound mix if configured
	if (VolumeSoundMix)
	{
		UGameplayStatics::PushSoundMixModifier(this, VolumeSoundMix);
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("SettingsMenuWidget: Constructed with %d focusable items"), GetFocusableItemCount());
}

void USettingsMenuWidget::NativeDestruct()
{
	// Save settings on close
	SaveSettings();

	// Unbind slider delegates to prevent dangling references
	if (MasterVolumeSlider)
	{
		MasterVolumeSlider->OnValueChanged.RemoveDynamic(this, &USettingsMenuWidget::OnMasterVolumeChanged);
	}
	if (MusicVolumeSlider)
	{
		MusicVolumeSlider->OnValueChanged.RemoveDynamic(this, &USettingsMenuWidget::OnMusicVolumeChanged);
	}
	if (SFXVolumeSlider)
	{
		SFXVolumeSlider->OnValueChanged.RemoveDynamic(this, &USettingsMenuWidget::OnSFXVolumeChanged);
	}
	// Unbind selector button delegates
	if (QualityPresetButton)
	{
		QualityPresetButton->OnClicked.RemoveDynamic(this, &USettingsMenuWidget::OnQualityPresetClicked);
	}
	if (ResolutionButton)
	{
		ResolutionButton->OnClicked.RemoveDynamic(this, &USettingsMenuWidget::OnResolutionClicked);
	}
	if (FullscreenButton)
	{
		FullscreenButton->OnClicked.RemoveDynamic(this, &USettingsMenuWidget::OnFullscreenClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked.RemoveDynamic(this, &USettingsMenuWidget::OnBackButtonClicked);
	}

	Super::NativeDestruct();
}

//=============================================================================
// INITIALIZATION
//=============================================================================

void USettingsMenuWidget::InitializeResolutions()
{
	// Standard and ultrawide resolutions for arcade/gaming
	AvailableResolutions.Empty();
	
	// Standard 16:9 resolutions
	AvailableResolutions.Add(FIntPoint(1280, 720));   // 720p
	AvailableResolutions.Add(FIntPoint(1600, 900));   // 900p
	AvailableResolutions.Add(FIntPoint(1920, 1080));  // 1080p
	AvailableResolutions.Add(FIntPoint(2560, 1440));  // 1440p
	AvailableResolutions.Add(FIntPoint(3840, 2160));  // 4K
	
	// Ultrawide 21:9 resolutions
	AvailableResolutions.Add(FIntPoint(2560, 1080));  // Ultrawide 1080p
	AvailableResolutions.Add(FIntPoint(3440, 1440));  // Ultrawide 1440p

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("InitializeResolutions: Added %d resolutions"), AvailableResolutions.Num());
	for (int32 i = 0; i < AvailableResolutions.Num(); i++)
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("  [%d] %dx%d"), i, AvailableResolutions[i].X, AvailableResolutions[i].Y);
	}

	// Get current resolution to set initial index
	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	if (Settings)
	{
		FIntPoint CurrentRes = Settings->GetScreenResolution();
		for (int32 i = 0; i < AvailableResolutions.Num(); i++)
		{
			if (AvailableResolutions[i] == CurrentRes)
			{
				CurrentResolutionIndex = i;
				break;
			}
		}

		// Get current fullscreen mode
		EWindowMode::Type WindowMode = Settings->GetFullscreenMode();
		switch (WindowMode)
		{
		case EWindowMode::Fullscreen:
			CurrentFullscreenIndex = 0;
			break;
		case EWindowMode::WindowedFullscreen:
			CurrentFullscreenIndex = 1;
			break;
		case EWindowMode::Windowed:
			CurrentFullscreenIndex = 2;
			break;
		default:
			CurrentFullscreenIndex = 0;
			break;
		}
	}
}

void USettingsMenuWidget::InitializePresetNames()
{
	QualityPresetNames.Empty();
	QualityPresetNames.Add(TEXT("Low"));
	QualityPresetNames.Add(TEXT("Medium"));
	QualityPresetNames.Add(TEXT("High"));
	QualityPresetNames.Add(TEXT("Epic"));
	// Cinematic (index 4) removed — only 4 quality levels (0-3)

	FullscreenModeNames.Empty();
	FullscreenModeNames.Add(TEXT("Fullscreen"));
	FullscreenModeNames.Add(TEXT("Windowed Fullscreen"));
	FullscreenModeNames.Add(TEXT("Windowed"));
}

void USettingsMenuWidget::LoadAudioAssets()
{
	// Load audio assets programmatically to ensure consistency with AudioSettingsSubsystem
	// This overwrites any Blueprint-configured values to guarantee matching assets
	
	VolumeSoundMix = LoadObject<USoundMix>(nullptr, Settings_SoundMixPath);
	if (!VolumeSoundMix)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("SettingsMenuWidget: Failed to load SoundMix from %s"), Settings_SoundMixPath);
	}

	MasterSoundClass = LoadObject<USoundClass>(nullptr, Settings_MasterSoundClassPath);
	if (!MasterSoundClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("SettingsMenuWidget: Failed to load MasterSoundClass from %s"), Settings_MasterSoundClassPath);
	}

	MusicSoundClass = LoadObject<USoundClass>(nullptr, Settings_MusicSoundClassPath);
	if (!MusicSoundClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("SettingsMenuWidget: Failed to load MusicSoundClass from %s"), Settings_MusicSoundClassPath);
	}

	SFXSoundClass = LoadObject<USoundClass>(nullptr, Settings_SFXSoundClassPath);
	if (!SFXSoundClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("SettingsMenuWidget: Failed to load SFXSoundClass from %s"), Settings_SFXSoundClassPath);
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("SettingsMenuWidget: Audio assets loaded - SoundMix: %s, Master: %s, Music: %s, SFX: %s"),
		VolumeSoundMix ? TEXT("OK") : TEXT("MISSING"),
		MasterSoundClass ? TEXT("OK") : TEXT("MISSING"),
		MusicSoundClass ? TEXT("OK") : TEXT("MISSING"),
		SFXSoundClass ? TEXT("OK") : TEXT("MISSING"));
}

//=============================================================================
// DISPLAY UPDATES
//=============================================================================

void USettingsMenuWidget::UpdateQualityPresetDisplay()
{
	if (QualityPresetValue && QualityPresetNames.IsValidIndex(CurrentQualityPresetIndex))
	{
		QualityPresetValue->SetText(FText::FromString(FString::Printf(TEXT("< %s >"), *QualityPresetNames[CurrentQualityPresetIndex])));
	}
}

void USettingsMenuWidget::UpdateResolutionDisplay()
{
	if (ResolutionValue && AvailableResolutions.IsValidIndex(CurrentResolutionIndex))
	{
		const FIntPoint& Res = AvailableResolutions[CurrentResolutionIndex];
		ResolutionValue->SetText(FText::FromString(FString::Printf(TEXT("< %dx%d >"), Res.X, Res.Y)));
	}
}

void USettingsMenuWidget::UpdateFullscreenDisplay()
{
	if (FullscreenValue && FullscreenModeNames.IsValidIndex(CurrentFullscreenIndex))
	{
		FullscreenValue->SetText(FText::FromString(FString::Printf(TEXT("< %s >"), *FullscreenModeNames[CurrentFullscreenIndex])));
	}
}

void USettingsMenuWidget::UpdateVolumeDisplay(UTextBlock* ValueText, float Volume)
{
	if (ValueText)
	{
		int32 Percent = FMath::RoundToInt(Volume * 100.0f);
		ValueText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), Percent)));
	}
}

//=============================================================================
// CYCLING FUNCTIONS
//=============================================================================

void USettingsMenuWidget::CycleQualityPreset(int32 Direction)
{
	int32 PresetCount = QualityPresetNames.Num();
	CurrentQualityPresetIndex = (CurrentQualityPresetIndex + Direction + PresetCount) % PresetCount;
	UpdateQualityPresetDisplay();
	ApplyGraphicsSettings();
	OnSettingChanged(INDEX_QUALITY_PRESET);

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("SettingsMenuWidget: Quality preset changed to %s"), *QualityPresetNames[CurrentQualityPresetIndex]);
}

void USettingsMenuWidget::CycleResolution(int32 Direction)
{
	int32 ResCount = AvailableResolutions.Num();
	CurrentResolutionIndex = (CurrentResolutionIndex + Direction + ResCount) % ResCount;
	UpdateResolutionDisplay();
	ApplyGraphicsSettings();
	OnSettingChanged(INDEX_RESOLUTION);

	const FIntPoint& Res = AvailableResolutions[CurrentResolutionIndex];
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("SettingsMenuWidget: Resolution changed to %dx%d"), Res.X, Res.Y);
}

void USettingsMenuWidget::CycleFullscreenMode(int32 Direction)
{
	int32 ModeCount = FullscreenModeNames.Num();
	CurrentFullscreenIndex = (CurrentFullscreenIndex + Direction + ModeCount) % ModeCount;
	UpdateFullscreenDisplay();
	ApplyGraphicsSettings();
	OnSettingChanged(INDEX_FULLSCREEN);

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("SettingsMenuWidget: Fullscreen mode changed to %s"), *FullscreenModeNames[CurrentFullscreenIndex]);
}

//=============================================================================
// SETTINGS APPLICATION
//=============================================================================

void USettingsMenuWidget::ApplyGraphicsSettings()
{
	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	if (!Settings)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("ApplyGraphicsSettings: No GameUserSettings!"));
		return;
	}

	// Apply quality preset (0-3 maps to Low-Epic)
	Settings->SetOverallScalabilityLevel(CurrentQualityPresetIndex);

	// Determine fullscreen mode
	EWindowMode::Type WindowMode;
	switch (CurrentFullscreenIndex)
	{
	case 0:
		WindowMode = EWindowMode::Fullscreen;
		break;
	case 1:
		WindowMode = EWindowMode::WindowedFullscreen;
		break;
	case 2:
		WindowMode = EWindowMode::Windowed;
		break;
	default:
		WindowMode = EWindowMode::Fullscreen;
		break;
	}

	// Apply resolution and fullscreen mode together
	if (AvailableResolutions.IsValidIndex(CurrentResolutionIndex))
	{
		const FIntPoint& Res = AvailableResolutions[CurrentResolutionIndex];
		
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("ApplyGraphicsSettings: Requesting %dx%d, WindowMode: %d"), 
			Res.X, Res.Y, (int32)WindowMode);
		
		// Set the resolution and window mode
		Settings->SetScreenResolution(Res);
		Settings->SetFullscreenMode(WindowMode);
		
		// For resolution to actually change, we need to use the console command approach
		// This is the most reliable method across all platforms
		if (GEngine && GEngine->GameViewport)
		{
			FString Command = FString::Printf(TEXT("r.SetRes %dx%d%s"), 
				Res.X, Res.Y,
				WindowMode == EWindowMode::Fullscreen ? TEXT("f") : 
				WindowMode == EWindowMode::WindowedFullscreen ? TEXT("wf") : TEXT("w"));
			
			GEngine->Exec(GetWorld(), *Command);
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("ApplyGraphicsSettings: Executed command: %s"), *Command);
		}
	}

	// Apply and save settings
	Settings->ApplySettings(false);
	Settings->SaveSettings();
	
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("ApplyGraphicsSettings: Settings applied and saved"));
}

void USettingsMenuWidget::ApplyAudioSettings()
{
	// Apply volume through sound mix
	if (MasterVolumeSlider)
	{
		ApplyVolumeToSoundClass(MasterSoundClass, MasterVolumeSlider->GetValue());
	}
	if (MusicVolumeSlider)
	{
		ApplyVolumeToSoundClass(MusicSoundClass, MusicVolumeSlider->GetValue());
	}
	if (SFXVolumeSlider)
	{
		ApplyVolumeToSoundClass(SFXSoundClass, SFXVolumeSlider->GetValue());
	}
}

void USettingsMenuWidget::ApplyVolumeToSoundClass(USoundClass* SoundClass, float Volume)
{
	if (!SoundClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("SettingsMenuWidget: ApplyVolumeToSoundClass - SoundClass is NULL"));
		return;
	}
	if (!VolumeSoundMix)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("SettingsMenuWidget: ApplyVolumeToSoundClass - VolumeSoundMix is NULL"));
		return;
	}

	// Tell music subsystem to ignore OnAudioFinished during volume adjustment
	UMusicPersistenceSubsystem* MusicSubsystem = UMusicPersistenceSubsystem::GetMusicPersistenceSubsystem(this);
	if (MusicSubsystem)
	{
		MusicSubsystem->SetIgnoreAudioFinished(true);
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("SettingsMenuWidget: Applying volume %f to SoundClass %s"), Volume, *SoundClass->GetName());
	UGameplayStatics::SetSoundMixClassOverride(this, VolumeSoundMix, SoundClass, Volume, 1.0f, 0.0f, true);

	// Re-enable OnAudioFinished after a brief delay
	if (MusicSubsystem)
	{
		if (UWorld* World = GetWorld())
		{
			FTimerHandle ReenableHandle;
			World->GetTimerManager().SetTimer(ReenableHandle, [MusicSubsystem]()
			{
				if (MusicSubsystem && MusicSubsystem->IsValidLowLevel())
				{
					MusicSubsystem->SetIgnoreAudioFinished(false);
				}
			}, 0.1f, false);
		}
	}
}

//=============================================================================
// SETTINGS PERSISTENCE
//=============================================================================

void USettingsMenuWidget::LoadSettings()
{
	// Load audio settings from config
	// Default volumes for first-time users (before any settings are saved)
	// MUST match AudioSettingsSubsystem defaults!
	float MasterVol = 0.85f;  // 85%
	float MusicVol = 0.5f;    // 50%
	float SFXVol = 1.0f;      // 100%

	GConfig->GetFloat(*AudioConfigSection, *MasterVolumeKey, MasterVol, GGameUserSettingsIni);
	GConfig->GetFloat(*AudioConfigSection, *MusicVolumeKey, MusicVol, GGameUserSettingsIni);
	GConfig->GetFloat(*AudioConfigSection, *SFXVolumeKey, SFXVol, GGameUserSettingsIni);

	// Apply to sliders
	if (MasterVolumeSlider)
	{
		MasterVolumeSlider->SetValue(MasterVol);
		UpdateVolumeDisplay(MasterVolumeValue, MasterVol);
	}
	if (MusicVolumeSlider)
	{
		MusicVolumeSlider->SetValue(MusicVol);
		UpdateVolumeDisplay(MusicVolumeValue, MusicVol);
	}
	if (SFXVolumeSlider)
	{
		SFXVolumeSlider->SetValue(SFXVol);
		UpdateVolumeDisplay(SFXVolumeValue, SFXVol);
	}

	// Apply audio settings
	ApplyAudioSettings();

	// Check if this is first-time user (no saved graphics settings)
	bool bSettingsInitialized = false;
	GConfig->GetBool(*GraphicsConfigSection, *SettingsInitializedKey, bSettingsInitialized, GGameUserSettingsIni);

	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	if (Settings)
	{
		if (!bSettingsInitialized)
		{
			// First-time user: Apply shipped package defaults
			// LOW-END BRANCH: Quality: Low (index 0), Resolution: 1280x720, Windowed
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("SettingsMenuWidget: First-time user detected - applying default settings (LOW-END)"));
			
			CurrentQualityPresetIndex = 0;  // Low (for old hardware compatibility)
			CurrentResolutionIndex = 0;     // 1280x720 (index 0 in AvailableResolutions)
			CurrentFullscreenIndex = 2;     // Windowed
			
			// Apply these defaults
			Settings->SetOverallScalabilityLevel(CurrentQualityPresetIndex);
			if (AvailableResolutions.IsValidIndex(CurrentResolutionIndex))
			{
				Settings->SetScreenResolution(AvailableResolutions[CurrentResolutionIndex]);
			}
			Settings->SetFullscreenMode(EWindowMode::Windowed);
			Settings->ApplySettings(false);
			Settings->SaveSettings();
			
			// Mark settings as initialized so we don't override user preferences later
			GConfig->SetBool(*GraphicsConfigSection, *SettingsInitializedKey, true, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
			
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("SettingsMenuWidget: Default settings applied - Quality: Low, Resolution: 1280x720, Windowed (LOW-END)"));
		}
		else
		{
			// Returning user: Load saved graphics settings
			CurrentQualityPresetIndex = Settings->GetOverallScalabilityLevel();
			// Clamp to valid range (0-3: Low, Medium, High, Epic)
			if (CurrentQualityPresetIndex < 0 || CurrentQualityPresetIndex > 3)
			{
				CurrentQualityPresetIndex = 0; // Default to Low if invalid
			}
			
			// Resolution and fullscreen are already loaded in InitializeResolutions()
		}
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("SettingsMenuWidget: Settings loaded - Master: %.0f%%, Music: %.0f%%, SFX: %.0f%%"),
		MasterVol * 100.0f, MusicVol * 100.0f, SFXVol * 100.0f);
}

void USettingsMenuWidget::SaveSettings()
{
	// Save audio settings to config
	if (MasterVolumeSlider)
	{
		GConfig->SetFloat(*AudioConfigSection, *MasterVolumeKey, MasterVolumeSlider->GetValue(), GGameUserSettingsIni);
	}
	if (MusicVolumeSlider)
	{
		GConfig->SetFloat(*AudioConfigSection, *MusicVolumeKey, MusicVolumeSlider->GetValue(), GGameUserSettingsIni);
	}
	if (SFXVolumeSlider)
	{
		GConfig->SetFloat(*AudioConfigSection, *SFXVolumeKey, SFXVolumeSlider->GetValue(), GGameUserSettingsIni);
	}

	GConfig->Flush(false, GGameUserSettingsIni);

	// Graphics settings are saved automatically by UGameUserSettings
	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	if (Settings)
	{
		Settings->SaveSettings();
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("SettingsMenuWidget: Settings saved"));
}

//=============================================================================
// VOLUME GETTERS/SETTERS
//=============================================================================

float USettingsMenuWidget::GetMasterVolume() const
{
	return MasterVolumeSlider ? MasterVolumeSlider->GetValue() : 0.5f;
}

float USettingsMenuWidget::GetMusicVolume() const
{
	return MusicVolumeSlider ? MusicVolumeSlider->GetValue() : 0.5f;
}

float USettingsMenuWidget::GetSFXVolume() const
{
	return SFXVolumeSlider ? SFXVolumeSlider->GetValue() : 0.5f;
}

void USettingsMenuWidget::SetMasterVolume(float Volume)
{
	Volume = FMath::Clamp(Volume, 0.0f, 1.0f);
	if (MasterVolumeSlider)
	{
		MasterVolumeSlider->SetValue(Volume);
	}
	ApplyVolumeToSoundClass(MasterSoundClass, Volume);
	UpdateVolumeDisplay(MasterVolumeValue, Volume);
}

void USettingsMenuWidget::SetMusicVolume(float Volume)
{
	Volume = FMath::Clamp(Volume, 0.0f, 1.0f);
	if (MusicVolumeSlider)
	{
		MusicVolumeSlider->SetValue(Volume);
	}
	ApplyVolumeToSoundClass(MusicSoundClass, Volume);
	UpdateVolumeDisplay(MusicVolumeValue, Volume);
}

void USettingsMenuWidget::SetSFXVolume(float Volume)
{
	Volume = FMath::Clamp(Volume, 0.0f, 1.0f);
	if (SFXVolumeSlider)
	{
		SFXVolumeSlider->SetValue(Volume);
	}
	ApplyVolumeToSoundClass(SFXSoundClass, Volume);
	UpdateVolumeDisplay(SFXVolumeValue, Volume);
}

//=============================================================================
// SLIDER CALLBACKS
//=============================================================================

void USettingsMenuWidget::OnMasterVolumeChanged(float Value)
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("SettingsMenuWidget: OnMasterVolumeChanged - Value: %f"), Value);
	ApplyVolumeToSoundClass(MasterSoundClass, Value);
	UpdateVolumeDisplay(MasterVolumeValue, Value);
	OnSettingChanged(INDEX_MASTER_VOLUME);
}

void USettingsMenuWidget::OnMusicVolumeChanged(float Value)
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("SettingsMenuWidget: OnMusicVolumeChanged - Value: %f"), Value);
	ApplyVolumeToSoundClass(MusicSoundClass, Value);
	UpdateVolumeDisplay(MusicVolumeValue, Value);
	OnSettingChanged(INDEX_MUSIC_VOLUME);
}

void USettingsMenuWidget::OnSFXVolumeChanged(float Value)
{
	ApplyVolumeToSoundClass(SFXSoundClass, Value);
	UpdateVolumeDisplay(SFXVolumeValue, Value);
	OnSettingChanged(INDEX_SFX_VOLUME);
}

void USettingsMenuWidget::OnBackButtonClicked()
{
	// Trigger the same back flow as Escape key
	HandleBack();
}

void USettingsMenuWidget::OnQualityPresetClicked()
{
	// Mouse click cycles forward through quality presets
	CycleQualityPreset(1);
}

void USettingsMenuWidget::OnResolutionClicked()
{
	// Mouse click cycles forward through resolutions
	CycleResolution(1);
}

void USettingsMenuWidget::OnFullscreenClicked()
{
	// Mouse click cycles forward through fullscreen modes
	CycleFullscreenMode(1);
}

//=============================================================================
// OVERRIDES
//=============================================================================

void USettingsMenuWidget::OnSelectorOptionChanged_Implementation(int32 ItemIndex, int32 Delta)
{
	switch (ItemIndex)
	{
	case INDEX_QUALITY_PRESET:
		CycleQualityPreset(Delta);
		break;
	case INDEX_RESOLUTION:
		CycleResolution(Delta);
		break;
	case INDEX_FULLSCREEN:
		CycleFullscreenMode(Delta);
		break;
	}
}

void USettingsMenuWidget::OnSliderValueChanged_Implementation(int32 ItemIndex, float NewValue)
{
	// Slider changes are handled by the OnValueChanged delegates
	// This is called for keyboard adjustments which also trigger OnValueChanged
}

void USettingsMenuWidget::OnBackAction_Implementation()
{
	// Save settings before closing
	SaveSettings();
	
	// Don't call RemoveFromParent() here — the parent widget handles
	// removal via OnBackPressed delegate.
}

void USettingsMenuWidget::OnItemSelected_Implementation(int32 ItemIndex)
{
	if (ItemIndex == INDEX_BACK)
	{
		// Save and close
		OnBackAction_Implementation();
	}
	// For selectors, Enter could toggle through options (optional behavior)
	// Currently, Left/Right handles cycling
}
