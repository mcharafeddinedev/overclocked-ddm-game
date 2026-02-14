#include "MusicPersistenceSubsystem.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "TimerManager.h"
#include "StateRunner_Arcade.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

// Helper to write debug info to file in shipping builds
static void WriteMusicDebugLog(const FString& Message)
{
#if !WITH_EDITOR
	static FString MusicDebugLogPath = FPaths::ProjectSavedDir() / TEXT("MusicDebug.log");
	static bool bFirstWrite = true;
	
	FString Timestamp = FDateTime::Now().ToString();
	FString Line = FString::Printf(TEXT("[%s] %s\n"), *Timestamp, *Message);
	
	if (bFirstWrite)
	{
		FFileHelper::SaveStringToFile(Line, *MusicDebugLogPath);
		bFirstWrite = false;
	}
	else
	{
		FFileHelper::SaveStringToFile(Line, *MusicDebugLogPath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_Append);
	}
#endif
}

// --- Default Track Paths ---
// Using FSoftObjectPath for proper shipping build support

static const FSoftObjectPath DefaultTrackPaths[] = {
	FSoftObjectPath(TEXT("/Game/_DEVELOPER/Audio/Music/Rush.Rush")),                 // Index 0
	FSoftObjectPath(TEXT("/Game/_DEVELOPER/Audio/Music/ByteChaser.ByteChaser")),     // Index 1
	FSoftObjectPath(TEXT("/Game/_DEVELOPER/Audio/Music/Circuit.Circuit")),           // Index 2
	FSoftObjectPath(TEXT("/Game/_DEVELOPER/Audio/Music/StateRunner.StateRunner")),   // Index 3
	FSoftObjectPath(TEXT("/Game/_DEVELOPER/Audio/Music/Velocity.Velocity")),         // Index 4
};

// Sound Class path for music routing
static const FSoftObjectPath Music_SoundClassPath(TEXT("/Game/_DEVELOPER/Audio/SoundClasses/SClass_Music.SClass_Music"));

// --- Lifecycle ---

void UMusicPersistenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Initializing..."));

	// Load Music Sound Class for volume routing using synchronous load
	if (Music_SoundClassPath.IsValid())
	{
		MusicSoundClass = Cast<USoundClass>(Music_SoundClassPath.TryLoad());
	}
	
	if (!MusicSoundClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MusicPersistenceSubsystem: Failed to load MusicSoundClass from %s"), *Music_SoundClassPath.ToString());
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Loaded MusicSoundClass: %s"), *MusicSoundClass->GetName());
	}

	// NOTE: We no longer auto-load tracks here. Tracks are provided by Blueprint via SetMusicTracks().
	// This ensures proper hard references for cooking and works reliably in shipping builds.
	// The MainMenu Level Blueprint calls SetMusicTracks on BeginPlay.
	
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Initialized - waiting for SetMusicTracks from Blueprint"));
}

void UMusicPersistenceSubsystem::Deinitialize()
{
	// Stop the playback monitor
	StopPlaybackMonitor();

	// Stop and clean up audio
	if (MusicAudioComponent)
	{
		MusicAudioComponent->OnAudioFinished.RemoveAll(this);
		MusicAudioComponent->Stop();
		MusicAudioComponent->DestroyComponent();
		MusicAudioComponent = nullptr;
	}

	Super::Deinitialize();

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Deinitialized"));
}

// --- Track Loading ---

void UMusicPersistenceSubsystem::LoadDefaultTracks()
{
	MusicTracks.Empty();

	const int32 NumTracks = UE_ARRAY_COUNT(DefaultTrackPaths);
	
	for (int32 i = 0; i < NumTracks; i++)
	{
		const FSoftObjectPath& Path = DefaultTrackPaths[i];
		
		if (!Path.IsValid())
		{
			UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MusicPersistenceSubsystem: Invalid path at index %d"), i);
			continue;
		}
		
		// Use TryLoad for synchronous loading - works in shipping builds
		USoundBase* Sound = Cast<USoundBase>(Path.TryLoad());
		
		if (Sound)
		{
			MusicTracks.Add(Sound);
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Loaded track %d: %s"), i, *Sound->GetName());
		}
		else
		{
			UE_LOG(LogStateRunner_Arcade, Error, TEXT("MusicPersistenceSubsystem: FAILED to load track %d from path: %s"), i, *Path.ToString());
		}
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Loaded %d/%d tracks"), MusicTracks.Num(), NumTracks);
	
	// Extra diagnostic for shipping builds
	if (MusicTracks.Num() == 0)
	{
		UE_LOG(LogStateRunner_Arcade, Error, TEXT("MusicPersistenceSubsystem: NO TRACKS LOADED! Music will not play. Check that audio assets are cooked."));
	}
}

void UMusicPersistenceSubsystem::SetMusicTracks(const TArray<USoundBase*>& InTracks)
{
	UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MusicPersistenceSubsystem: SetMusicTracks called with %d tracks"), InTracks.Num());
	WriteMusicDebugLog(FString::Printf(TEXT("SetMusicTracks called with %d tracks"), InTracks.Num()));
	
	// Stop current playback and monitor while we swap tracks
	StopPlaybackMonitor();
	if (MusicAudioComponent && MusicAudioComponent->IsValidLowLevel())
	{
		MusicAudioComponent->OnAudioFinished.RemoveAll(this);
		MusicAudioComponent->Stop();
		MusicAudioComponent->DestroyComponent();
		MusicAudioComponent = nullptr;
	}
	
	MusicTracks.Empty();
	int32 NullCount = 0;
	for (USoundBase* Track : InTracks)
	{
		if (Track)
		{
			MusicTracks.Add(Track);
			FString TrackInfo = FString::Printf(TEXT("Added track: %s (Class: %s)"), *Track->GetName(), *Track->GetClass()->GetName());
			UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MusicPersistenceSubsystem: %s"), *TrackInfo);
			WriteMusicDebugLog(TrackInfo);
		}
		else
		{
			NullCount++;
			UE_LOG(LogStateRunner_Arcade, Error, TEXT("MusicPersistenceSubsystem: NULL track in input array!"));
			WriteMusicDebugLog(TEXT("ERROR: NULL track in input array!"));
		}
	}

	FString Summary = FString::Printf(TEXT("Set %d music tracks from Blueprint (%d were null)"), MusicTracks.Num(), NullCount);
	UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MusicPersistenceSubsystem: %s"), *Summary);
	WriteMusicDebugLog(Summary);

	// Always start playback when tracks are provided from Blueprint
	// This is the primary way tracks should be set in shipping builds
	if (MusicTracks.Num() > 0)
	{
		bMusicHasStarted = false; // Reset so PlayCurrentTrack works correctly
		bTransitioningTracks = false;
		CurrentTrackIndex = 0; // Start from first track
		
		// Small delay to ensure audio system is ready
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
			FTimerHandle TimerHandle;
			World->GetTimerManager().SetTimer(TimerHandle, [this]()
			{
				PlayCurrentTrack();
			}, 0.1f, false);
		}
		else
		{
			// Try immediately if no world for timer
			PlayCurrentTrack();
		}
	}
}

// --- Audio Component Management ---

void UMusicPersistenceSubsystem::EnsureAudioComponent()
{
	// If we already have a valid, playing audio component, keep it
	if (MusicAudioComponent && MusicAudioComponent->IsValidLowLevel())
	{
		return;
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Creating new audio component"));

	// Get a world context - try multiple sources
	UWorld* World = nullptr;
	
	if (UGameInstance* GI = GetGameInstance())
	{
		World = GI->GetWorld();
	}
	
	if (!World)
	{
		// Try to get any valid world
		World = GEngine->GetCurrentPlayWorld();
	}

	if (!World)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MusicPersistenceSubsystem: No valid world for audio component"));
		return;
	}

	// Create audio component using CreateSound2D for reliability
	if (MusicTracks.IsValidIndex(CurrentTrackIndex) && MusicTracks[CurrentTrackIndex])
	{
		MusicAudioComponent = UGameplayStatics::CreateSound2D(World, MusicTracks[CurrentTrackIndex], 1.0f, 1.0f, 0.0f, nullptr, false, false);
		
		if (MusicAudioComponent)
		{
			MusicAudioComponent->bIsUISound = true;
			MusicAudioComponent->bAutoDestroy = false;
			
			// Bind completion event
			MusicAudioComponent->OnAudioFinished.AddDynamic(this, &UMusicPersistenceSubsystem::OnAudioFinished);
			
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Audio component created successfully"));
		}
	}
}

// --- Playback Control ---

void UMusicPersistenceSubsystem::PlayCurrentTrack()
{
	if (!MusicTracks.IsValidIndex(CurrentTrackIndex))
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MusicPersistenceSubsystem: Invalid track index %d"), CurrentTrackIndex);
		return;
	}

	USoundBase* TrackToPlay = MusicTracks[CurrentTrackIndex];
	if (!TrackToPlay)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MusicPersistenceSubsystem: Track at index %d is null"), CurrentTrackIndex);
		return;
	}

	// Guard: Don't restart if we're already playing this exact track
	if (MusicAudioComponent && MusicAudioComponent->IsValidLowLevel() && MusicAudioComponent->IsPlaying())
	{
		if (MusicAudioComponent->Sound == TrackToPlay)
		{
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Track %d already playing, skipping restart"), CurrentTrackIndex);
			return;
		}
	}

	// Stop and cleanup current audio component if any
	if (MusicAudioComponent)
	{
		MusicAudioComponent->OnAudioFinished.RemoveAll(this);
		MusicAudioComponent->Stop();
		MusicAudioComponent->DestroyComponent();
		MusicAudioComponent = nullptr;
	}

	// Get world for spawning
	UWorld* World = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		World = GI->GetWorld();
	}
	if (!World)
	{
		World = GEngine->GetCurrentPlayWorld();
	}

	if (!World)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MusicPersistenceSubsystem: No world available for PlayCurrentTrack"));
		WriteMusicDebugLog(TEXT("ERROR: No world available for PlayCurrentTrack"));
		return;
	}

	WriteMusicDebugLog(FString::Printf(TEXT("PlayCurrentTrack: Attempting to play track %d: %s"), CurrentTrackIndex, *TrackToPlay->GetName()));

	// Use CreateSound2D which creates without auto-playing, then configure and play manually
	// This gives us full control and works more reliably across PIE and shipping
	MusicAudioComponent = UGameplayStatics::CreateSound2D(World, TrackToPlay, 1.0f, 1.0f, 0.0f, nullptr, false, false);
	
	if (MusicAudioComponent)
	{
		WriteMusicDebugLog(TEXT("PlayCurrentTrack: Audio component created successfully"));
		
		// Set the Sound Class override to ensure proper volume routing
		if (MusicSoundClass)
		{
			MusicAudioComponent->SoundClassOverride = MusicSoundClass;
			WriteMusicDebugLog(FString::Printf(TEXT("PlayCurrentTrack: Set SoundClass to %s"), *MusicSoundClass->GetName()));
		}
		
		// CRITICAL: Mark as UI sound so music plays even when game is paused
		MusicAudioComponent->bIsUISound = true;
		
		// Don't auto-destroy - we manage lifecycle
		MusicAudioComponent->bAutoDestroy = false;
		
		// Bind completion event for auto-advance
		MusicAudioComponent->OnAudioFinished.AddDynamic(this, &UMusicPersistenceSubsystem::OnAudioFinished);
		
		// Actually start playing - CreateSound2D doesn't auto-play
		MusicAudioComponent->Play();

		bMusicHasStarted = true;
		bTransitioningTracks = false;
		
		// Record start time to detect false OnAudioFinished triggers
		CurrentTrackStartTime = FPlatformTime::Seconds();

		FString TrackName = GetCurrentTrackName();
		bool bIsActuallyPlaying = MusicAudioComponent->IsPlaying();
		
		FString PlayStatus = FString::Printf(TEXT("PlayCurrentTrack: Now playing track %d: %s (IsPlaying=%s, Sound=%s)"), 
			CurrentTrackIndex, *TrackName, bIsActuallyPlaying ? TEXT("YES") : TEXT("NO"),
			MusicAudioComponent->Sound ? *MusicAudioComponent->Sound->GetName() : TEXT("NULL"));
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: %s"), *PlayStatus);
		WriteMusicDebugLog(PlayStatus);

		// Start the fallback playback monitor
		StartPlaybackMonitor();

		// Broadcast track change
		OnTrackChanged.Broadcast(TrackName);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Error, TEXT("MusicPersistenceSubsystem: Failed to create audio component for track %d"), CurrentTrackIndex);
		WriteMusicDebugLog(FString::Printf(TEXT("ERROR: Failed to create audio component for track %d"), CurrentTrackIndex));
		bTransitioningTracks = false;
	}
}

void UMusicPersistenceSubsystem::SkipTrack()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Skip requested"));
	
	// Temporarily ignore OnAudioFinished callbacks during track change
	// to avoid infinite loops when volume is 0% (audio fires "finished" immediately)
	bIgnoreAudioFinished = true;
	
	AdvanceTrackIndex();
	PlayCurrentTrack();
	
	// Clear the flag after a short delay to allow the audio system to settle
	UWorld* World = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		World = GI->GetWorld();
	}
	if (World)
	{
		FTimerHandle TempHandle;
		World->GetTimerManager().SetTimer(TempHandle, [this]()
		{
			bIgnoreAudioFinished = false;
		}, 0.2f, false);
	}
	else
	{
		// Fallback: just clear it immediately if no world
		bIgnoreAudioFinished = false;
	}
}

void UMusicPersistenceSubsystem::ToggleShuffle()
{
	bShuffleEnabled = !bShuffleEnabled;
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Shuffle %s"), bShuffleEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
}

bool UMusicPersistenceSubsystem::IsPlaying() const
{
	return MusicAudioComponent && MusicAudioComponent->IsValidLowLevel() && MusicAudioComponent->IsPlaying();
}

void UMusicPersistenceSubsystem::EnsureMusicPlaying()
{
	// If already playing, nothing to do
	if (IsPlaying())
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: EnsureMusicPlaying - already playing"));
		return;
	}
	
	// If we have tracks (regardless of bMusicHasStarted), restart playback
	// This handles level transition where audio component was destroyed but tracks remain
	if (MusicTracks.Num() > 0)
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: EnsureMusicPlaying - restarting playback with %d tracks"), MusicTracks.Num());
		bTransitioningTracks = false; // Clear any stuck transition state
		
		// Clean up stale audio component reference if it's no longer valid
		if (MusicAudioComponent && !MusicAudioComponent->IsValidLowLevel())
		{
			MusicAudioComponent = nullptr;
		}
		
		PlayCurrentTrack();
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MusicPersistenceSubsystem: EnsureMusicPlaying called but no tracks available"));
	}
}

FString UMusicPersistenceSubsystem::GetCurrentTrackName() const
{
	if (MusicTracks.IsValidIndex(CurrentTrackIndex) && MusicTracks[CurrentTrackIndex])
	{
		return MusicTracks[CurrentTrackIndex]->GetName();
	}
	return TEXT("Unknown");
}

// --- Internal ---

void UMusicPersistenceSubsystem::AdvanceTrackIndex()
{
	if (MusicTracks.Num() == 0)
	{
		return;
	}

	if (bShuffleEnabled)
	{
		// Random track (avoid same track if possible)
		if (MusicTracks.Num() > 1)
		{
			int32 NewIndex;
			do
			{
				NewIndex = FMath::RandRange(0, MusicTracks.Num() - 1);
			} while (NewIndex == CurrentTrackIndex);
			CurrentTrackIndex = NewIndex;
		}
	}
	else
	{
		// Sequential
		CurrentTrackIndex = (CurrentTrackIndex + 1) % MusicTracks.Num();
	}

}

void UMusicPersistenceSubsystem::SetIgnoreAudioFinished(bool bIgnore)
{
	bIgnoreAudioFinished = bIgnore;
}

void UMusicPersistenceSubsystem::OnAudioFinished()
{
	// If volume adjustment is in progress, ignore this callback
	// (SetSoundMixClassOverride can trigger false "finished" events)
	if (bIgnoreAudioFinished)
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: OnAudioFinished ignored (bIgnoreAudioFinished=true)"));
		return;
	}

	// Prevent re-entry during track transition
	if (bTransitioningTracks)
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: OnAudioFinished ignored (already transitioning)"));
		return;
	}
	
	// Check if track played for a reasonable amount of time
	// In shipping builds, OnAudioFinished can fire immediately if audio fails to play
	double PlaybackDuration = FPlatformTime::Seconds() - CurrentTrackStartTime;
	if (PlaybackDuration < MinPlaybackTimeBeforeFinish)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MusicPersistenceSubsystem: OnAudioFinished fired after only %.2f seconds - likely audio playback failure, ignoring"), PlaybackDuration);
		return;
	}

	// Check if we have a valid world - if not, we're in a level transition
	// Just advance the index and let the fallback monitor restart playback later
	UWorld* World = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		World = GI->GetWorld();
	}
	if (!World)
	{
		World = GEngine->GetCurrentPlayWorld();
	}
	
	if (!World)
	{
		// Level transition - audio component died but track didn't naturally finish
		// Don't advance track, just clear the reference and let monitor restart the SAME track
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Audio stopped during level transition - will resume track %d"), CurrentTrackIndex);
		MusicAudioComponent = nullptr;
		return;
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Track %d finished, advancing to next"), CurrentTrackIndex);
	
	// Mark that we're transitioning to prevent re-entry
	bTransitioningTracks = true;
	
	// Stop the playback monitor during transition
	StopPlaybackMonitor();
	
	AdvanceTrackIndex();
	PlayCurrentTrack();
}

// --- Playback Monitor (Fallback) ---

void UMusicPersistenceSubsystem::StartPlaybackMonitor()
{
	StopPlaybackMonitor(); // Clear any existing timer
	
	UWorld* World = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		World = GI->GetWorld();
	}
	
	if (World)
	{
		// Check frequently (0.25s) for quick recovery after level transitions
		// Using lambda because UGameInstanceSubsystem isn't a valid direct timer target
		FTimerDelegate TimerDelegate = FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			CheckPlaybackStatus();
		});
		
		constexpr float CheckInterval = 0.25f;
		constexpr bool bLoop = true;
		constexpr float InitialDelay = 0.0f; // Start checking immediately
		
		World->GetTimerManager().SetTimer(
			PlaybackMonitorHandle,
			TimerDelegate,
			CheckInterval,
			bLoop,
			InitialDelay
		);
	}
}

void UMusicPersistenceSubsystem::StopPlaybackMonitor()
{
	UWorld* World = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		World = GI->GetWorld();
	}
	
	if (World && PlaybackMonitorHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(PlaybackMonitorHandle);
	}
	PlaybackMonitorHandle.Invalidate();
}

void UMusicPersistenceSubsystem::CheckPlaybackStatus()
{
	// Skip check if we're ignoring callbacks
	if (bIgnoreAudioFinished)
	{
		return;
	}
	
	// Don't try to restart if we have no tracks loaded
	if (MusicTracks.Num() == 0)
	{
		return;
	}
	
	// If we're stuck in transitioning state for too long, clear it
	if (bTransitioningTracks)
	{
		// The transition state should be brief - if we're still here after a check, something went wrong
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MusicPersistenceSubsystem: Clearing stuck transition state"));
		bTransitioningTracks = false;
	}
	
	// Check if audio component exists and is valid
	bool bNeedsRestart = false;
	
	if (!MusicAudioComponent || !MusicAudioComponent->IsValidLowLevel())
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Monitor detected invalid audio component - will restart"));
		bNeedsRestart = true;
	}
	else if (!MusicAudioComponent->IsPlaying())
	{
		// Audio component exists but not playing - track may have ended without callback
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Monitor detected playback stopped - will restart"));
		bNeedsRestart = true;
	}
	
	if (bNeedsRestart && bMusicHasStarted)
	{
		// Throttle restart attempts to prevent rapid-fire restarts (which cause UI flashing)
		double CurrentTime = FPlatformTime::Seconds();
		if (CurrentTime - LastRestartAttemptTime < MinRestartInterval)
		{
			return; // Too soon since last restart attempt
		}
		LastRestartAttemptTime = CurrentTime;
		
		// Don't advance track - just restart the current one
		// Track only advances when OnAudioFinished fires with a valid world (natural end)
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Monitor restarting track %d"), CurrentTrackIndex);
		PlayCurrentTrack();
	}
}

// --- Static Helper ---

UMusicPersistenceSubsystem* UMusicPersistenceSubsystem::GetMusicPersistenceSubsystem(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<UMusicPersistenceSubsystem>();
}
