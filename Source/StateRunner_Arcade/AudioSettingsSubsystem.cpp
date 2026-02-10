#include "AudioSettingsSubsystem.h"
#include "MusicPersistenceSubsystem.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "StateRunner_Arcade.h"

// Graphics first-time defaults (must match SettingsMenuWidget)
static const FString GraphicsConfigSection = TEXT("/Script/StateRunner_Arcade.GraphicsSettings");
static const FString SettingsInitializedKey = TEXT("bSettingsInitialized");

// --- Static Config Keys (must match SettingsMenuWidget) ---

const FString UAudioSettingsSubsystem::AudioConfigSection = TEXT("/Script/StateRunner_Arcade.AudioSettings");
const FString UAudioSettingsSubsystem::MasterVolumeKey = TEXT("MasterVolume");
const FString UAudioSettingsSubsystem::MusicVolumeKey = TEXT("MusicVolume");
const FString UAudioSettingsSubsystem::SFXVolumeKey = TEXT("SFXVolume");

// --- Asset Paths (prefixed to avoid Unity build collisions) ---

static const TCHAR* Audio_SoundMixPath = TEXT("/Game/_DEVELOPER/Audio/SoundClasses/SM_GameVolume.SM_GameVolume");
static const TCHAR* Audio_MasterSoundClassPath = TEXT("/Game/_DEVELOPER/Audio/SoundClasses/SClass_Master.SClass_Master");
static const TCHAR* Audio_MusicSoundClassPath = TEXT("/Game/_DEVELOPER/Audio/SoundClasses/SClass_Music.SClass_Music");
static const TCHAR* Audio_SFXSoundClassPath = TEXT("/Game/_DEVELOPER/Audio/SoundClasses/SClass_SFX.SClass_SFX");

// --- Subsystem Lifecycle ---

void UAudioSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("AudioSettingsSubsystem: Initializing..."));

	// Load audio assets
	LoadAudioAssets();

	// Try to apply audio settings immediately
	// This may fail if world isn't ready yet, so we also schedule a delayed retry
	ApplyAudioSettings();

	// Register for level load events to reapply audio settings on every level change
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UAudioSettingsSubsystem::OnPostLoadMapWithWorld);

	// Schedule a delayed apply to catch the main menu startup case
	// The world may not be fully ready during Initialize(), so we retry shortly after
	ScheduleDelayedApply();

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("AudioSettingsSubsystem: Initialized and audio settings applied"));
}

void UAudioSettingsSubsystem::Deinitialize()
{
	// Clear any pending timers
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWorld* World = GI->GetWorld())
		{
			World->GetTimerManager().ClearTimer(DelayedApplyTimerHandle);
			World->GetTimerManager().ClearTimer(IgnoreFlagClearTimerHandle);
		}
	}

	// Unregister level load delegate
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	Super::Deinitialize();

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("AudioSettingsSubsystem: Deinitialized"));
}

// --- Audio Asset Loading ---

void UAudioSettingsSubsystem::LoadAudioAssets()
{
	// Load Sound Mix
	VolumeSoundMix = LoadObject<USoundMix>(nullptr, Audio_SoundMixPath);
	if (!VolumeSoundMix)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("AudioSettingsSubsystem: Failed to load SoundMix from %s"), Audio_SoundMixPath);
	}

	// Load Sound Classes
	MasterSoundClass = LoadObject<USoundClass>(nullptr, Audio_MasterSoundClassPath);
	if (!MasterSoundClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("AudioSettingsSubsystem: Failed to load MasterSoundClass from %s"), Audio_MasterSoundClassPath);
	}

	MusicSoundClass = LoadObject<USoundClass>(nullptr, Audio_MusicSoundClassPath);
	if (!MusicSoundClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("AudioSettingsSubsystem: Failed to load MusicSoundClass from %s"), Audio_MusicSoundClassPath);
	}

	SFXSoundClass = LoadObject<USoundClass>(nullptr, Audio_SFXSoundClassPath);
	if (!SFXSoundClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("AudioSettingsSubsystem: Failed to load SFXSoundClass from %s"), Audio_SFXSoundClassPath);
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("AudioSettingsSubsystem: Audio assets loaded - SoundMix: %s, Master: %s, Music: %s, SFX: %s"),
		VolumeSoundMix ? TEXT("OK") : TEXT("MISSING"),
		MasterSoundClass ? TEXT("OK") : TEXT("MISSING"),
		MusicSoundClass ? TEXT("OK") : TEXT("MISSING"),
		SFXSoundClass ? TEXT("OK") : TEXT("MISSING"));
}

// --- Public Functions ---

void UAudioSettingsSubsystem::ApplyAudioSettings()
{
	float MasterVol, MusicVol, SFXVol;
	GetVolumeSettings(MasterVol, MusicVol, SFXVol);

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("AudioSettingsSubsystem: Applying audio settings - Master: %.0f%%, Music: %.0f%%, SFX: %.0f%%"),
		MasterVol * 100.0f, MusicVol * 100.0f, SFXVol * 100.0f);

	// Get a valid world context (subsystems don't have direct world access)
	UWorld* World = nullptr;
	UGameInstance* GI = GetGameInstance();
	if (GI)
	{
		World = GI->GetWorld();
	}
	if (!World)
	{
		World = GEngine->GetCurrentPlayWorld();
	}
	
	if (!World)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("AudioSettingsSubsystem: No world available for audio settings"));
		return;
	}

	// Tell the music subsystem to ignore OnAudioFinished during volume adjustment
	// (SetSoundMixClassOverride can trigger false "track finished" callbacks)
	UMusicPersistenceSubsystem* MusicSubsystem = GI ? GI->GetSubsystem<UMusicPersistenceSubsystem>() : nullptr;
	if (MusicSubsystem)
	{
		MusicSubsystem->SetIgnoreAudioFinished(true);
		
		// Clear any existing re-enable timer (we'll set a new one after this apply)
		World->GetTimerManager().ClearTimer(IgnoreFlagClearTimerHandle);
	}

	// Push the sound mix first (required for SetSoundMixClassOverride to work)
	if (VolumeSoundMix)
	{
		UGameplayStatics::PushSoundMixModifier(World, VolumeSoundMix);
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("AudioSettingsSubsystem: Pushed SoundMix %s"), *VolumeSoundMix->GetName());
	}

	// Apply volume to each sound class
	ApplyVolumeToSoundClass(World, MasterSoundClass, MasterVol);
	ApplyVolumeToSoundClass(World, MusicSoundClass, MusicVol);
	ApplyVolumeToSoundClass(World, SFXSoundClass, SFXVol);

	// Re-enable OnAudioFinished after a longer delay to allow the audio system to settle
	// Using a member timer handle ensures subsequent ApplyAudioSettings calls reset the timer
	// (prevents early re-enable if multiple applies happen in quick succession)
	if (MusicSubsystem)
	{
		TWeakObjectPtr<UMusicPersistenceSubsystem> WeakMusicSubsystem = MusicSubsystem;
		World->GetTimerManager().SetTimer(IgnoreFlagClearTimerHandle, [WeakMusicSubsystem]()
		{
			if (WeakMusicSubsystem.IsValid())
			{
				WeakMusicSubsystem->SetIgnoreAudioFinished(false);
			}
		}, 0.5f, false);  // Extended from 0.1s to 0.5s for more safety margin
	}
}

void UAudioSettingsSubsystem::GetVolumeSettings(float& OutMasterVolume, float& OutMusicVolume, float& OutSFXVolume) const
{
	// Start with defaults
	OutMasterVolume = DefaultMasterVolume;
	OutMusicVolume = DefaultMusicVolume;
	OutSFXVolume = DefaultSFXVolume;

	// Try to load saved settings (overrides defaults if they exist)
	GConfig->GetFloat(*AudioConfigSection, *MasterVolumeKey, OutMasterVolume, GGameUserSettingsIni);
	GConfig->GetFloat(*AudioConfigSection, *MusicVolumeKey, OutMusicVolume, GGameUserSettingsIni);
	GConfig->GetFloat(*AudioConfigSection, *SFXVolumeKey, OutSFXVolume, GGameUserSettingsIni);
}

void UAudioSettingsSubsystem::ApplyAudioSettingsStatic(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("AudioSettingsSubsystem::ApplyAudioSettingsStatic - WorldContextObject is null"));
		return;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("AudioSettingsSubsystem::ApplyAudioSettingsStatic - Could not get world"));
		return;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("AudioSettingsSubsystem::ApplyAudioSettingsStatic - Could not get game instance"));
		return;
	}

	UAudioSettingsSubsystem* Subsystem = GameInstance->GetSubsystem<UAudioSettingsSubsystem>();
	if (Subsystem)
	{
		Subsystem->ApplyAudioSettings();
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("AudioSettingsSubsystem::ApplyAudioSettingsStatic - Subsystem not found"));
	}
}

// --- Internal ---

void UAudioSettingsSubsystem::ApplyVolumeToSoundClass(UWorld* World, USoundClass* SoundClass, float Volume)
{
	if (!World)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("AudioSettingsSubsystem: Cannot apply volume - World is null"));
		return;
	}

	if (!SoundClass)
	{
		return;
	}

	if (!VolumeSoundMix)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("AudioSettingsSubsystem: Cannot apply volume - SoundMix is null"));
		return;
	}

	UGameplayStatics::SetSoundMixClassOverride(World, VolumeSoundMix, SoundClass, Volume, 1.0f, 0.0f, true);
}

void UAudioSettingsSubsystem::OnPostLoadMapWithWorld(UWorld* LoadedWorld)
{
	if (!LoadedWorld)
	{
		return;
	}

	// First-time user: apply default graphics so the window starts at shipped defaults
	// LOW-END BRANCH: 1280x720, Low quality, Windowed (for GTX 700 series and similar)
	bool bSettingsInitialized = false;
	GConfig->GetBool(*GraphicsConfigSection, *SettingsInitializedKey, bSettingsInitialized, GGameUserSettingsIni);
	if (!bSettingsInitialized && GEngine)
	{
		UGameUserSettings* Settings = GEngine->GetGameUserSettings();
		if (Settings)
		{
			Settings->SetOverallScalabilityLevel(0);  // Low (for old hardware)
			Settings->SetScreenResolution(FIntPoint(1280, 720));
			Settings->SetFullscreenMode(EWindowMode::Windowed);
			if (GEngine->GameViewport)
			{
				GEngine->Exec(LoadedWorld, TEXT("r.SetRes 1280x720w"));
			}
			Settings->ApplySettings(false);
			Settings->SaveSettings();
			GConfig->SetBool(*GraphicsConfigSection, *SettingsInitializedKey, true, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("AudioSettingsSubsystem: First-time defaults applied - 1280x720, Low, Windowed (LOW-END)"));
		}
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("AudioSettingsSubsystem: Level loaded (%s) - reapplying audio settings"),
		*LoadedWorld->GetName());

	// Reapply audio settings after level load
	ApplyAudioSettings();
	
	// Also schedule a slightly delayed apply to catch any audio that starts playing
	// shortly after the level loads (like background music)
	ScheduleDelayedApply();
}

void UAudioSettingsSubsystem::ScheduleDelayedApply()
{
	UWorld* World = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		World = GI->GetWorld();
	}
	if (!World)
	{
		World = GEngine->GetCurrentPlayWorld();
	}

	if (World)
	{
		// Clear any existing timer
		World->GetTimerManager().ClearTimer(DelayedApplyTimerHandle);
		
		// Schedule multiple delayed applies to catch the audio system when it's ready
		// First at 0.1s, then again at 0.5s
		World->GetTimerManager().SetTimer(DelayedApplyTimerHandle, [this]()
		{
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("AudioSettingsSubsystem: Delayed apply (0.1s)"));
			ApplyAudioSettings();
			
			// Schedule another apply at 0.5s
			if (UGameInstance* GI = GetGameInstance())
			{
				if (UWorld* W = GI->GetWorld())
				{
					FTimerHandle SecondaryHandle;
					W->GetTimerManager().SetTimer(SecondaryHandle, [this]()
					{
						UE_LOG(LogStateRunner_Arcade, Log, TEXT("AudioSettingsSubsystem: Delayed apply (0.5s)"));
						ApplyAudioSettings();
					}, 0.4f, false);  // 0.4s after the first 0.1s = 0.5s total
				}
			}
		}, 0.1f, false);
		
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("AudioSettingsSubsystem: Could not schedule delayed apply - no world"));
	}
}
