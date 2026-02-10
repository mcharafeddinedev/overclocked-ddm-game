#include "MusicPersistenceSubsystem.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "StateRunner_Arcade.h"

// --- Default Track Paths ---

static const TCHAR* DefaultTrackPaths[] = {
	TEXT("/Game/_DEVELOPER/Audio/Music/Rush.Rush"),                 // Index 0 (was NVL)
	TEXT("/Game/_DEVELOPER/Audio/Music/ByteChaser.ByteChaser"),     // Index 1 (was ENR)
	TEXT("/Game/_DEVELOPER/Audio/Music/Circuit.Circuit"),           // Index 2 (was NCC)
	TEXT("/Game/_DEVELOPER/Audio/Music/StateRunner.StateRunner"),   // Index 3 (was NPL)
	TEXT("/Game/_DEVELOPER/Audio/Music/Velocity.Velocity"),         // Index 4 (was NCR)
};

// Sound Class path for music routing (prefixed to avoid Unity build collisions)
static const TCHAR* Music_SoundClassPath = TEXT("/Game/_DEVELOPER/Audio/SoundClasses/SClass_Music.SClass_Music");

// --- Lifecycle ---

void UMusicPersistenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Initializing..."));

	// Load Music Sound Class for volume routing
	MusicSoundClass = LoadObject<USoundClass>(nullptr, Music_SoundClassPath);
	if (!MusicSoundClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MusicPersistenceSubsystem: Failed to load MusicSoundClass from %s"), Music_SoundClassPath);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Loaded MusicSoundClass: %s"), *MusicSoundClass->GetName());
	}

	// Load default tracks
	LoadDefaultTracks();

	// Start playing music
	if (MusicTracks.Num() > 0)
	{
		// Small delay to ensure world is ready
		if (UWorld* World = GetGameInstance()->GetWorld())
		{
			FTimerHandle TimerHandle;
			World->GetTimerManager().SetTimer(TimerHandle, [this]()
			{
				PlayCurrentTrack();
			}, 0.1f, false);
		}
		else
		{
			// World not ready yet, will be started when SetMusicTracks is called from Blueprint
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: World not ready, music will start when configured"));
		}
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Initialized with %d tracks"), MusicTracks.Num());
}

void UMusicPersistenceSubsystem::Deinitialize()
{
	// Stop and clean up audio
	if (MusicAudioComponent && MusicAudioComponent->IsValidLowLevel())
	{
		MusicAudioComponent->OnAudioFinished.RemoveAll(this);
		MusicAudioComponent->Stop();
		MusicAudioComponent->RemoveFromRoot();  // Must remove from root before destroying
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

	for (const TCHAR* Path : DefaultTrackPaths)
	{
		USoundBase* Sound = LoadObject<USoundBase>(nullptr, Path);
		if (Sound)
		{
			MusicTracks.Add(Sound);
		}
		else
		{
			UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MusicPersistenceSubsystem: Failed to load track: %s"), Path);
		}
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Loaded %d/%d tracks"), 
		MusicTracks.Num(), (int32)(sizeof(DefaultTrackPaths) / sizeof(DefaultTrackPaths[0])));
}

void UMusicPersistenceSubsystem::SetMusicTracks(const TArray<USoundBase*>& InTracks)
{
	MusicTracks.Empty();
	for (USoundBase* Track : InTracks)
	{
		if (Track)
		{
			MusicTracks.Add(Track);
		}
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Set %d music tracks from Blueprint"), MusicTracks.Num());

	// Start playing if we haven't yet
	if (!bMusicHasStarted && MusicTracks.Num() > 0)
	{
		PlayCurrentTrack();
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

	// SpawnSound2D creates an audio component that isn't attached to any actor
	// This means it won't be destroyed when levels change!
	if (MusicTracks.IsValidIndex(CurrentTrackIndex) && MusicTracks[CurrentTrackIndex])
	{
		MusicAudioComponent = UGameplayStatics::SpawnSound2D(World, MusicTracks[CurrentTrackIndex], 1.0f, 1.0f, 0.0f, nullptr, false, false);
		
		if (MusicAudioComponent)
		{
			// Prevent garbage collection
			MusicAudioComponent->AddToRoot();
			
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

	// Stop current playback if any (different track or not playing)
	if (MusicAudioComponent && MusicAudioComponent->IsValidLowLevel())
	{
		MusicAudioComponent->OnAudioFinished.RemoveAll(this);
		MusicAudioComponent->Stop();
		MusicAudioComponent->RemoveFromRoot();
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
		return;
	}

	// Spawn new audio component with the track
	MusicAudioComponent = UGameplayStatics::SpawnSound2D(World, TrackToPlay, 1.0f, 1.0f, 0.0f, nullptr, true, false);

	if (MusicAudioComponent)
	{
		// Prevent garbage collection so it survives level changes
		MusicAudioComponent->AddToRoot();
		
		// Set the Sound Class override to ensure proper volume routing
		if (MusicSoundClass)
		{
			MusicAudioComponent->SoundClassOverride = MusicSoundClass;
		}
		
		// Bind completion event for auto-advance
		MusicAudioComponent->OnAudioFinished.AddDynamic(this, &UMusicPersistenceSubsystem::OnAudioFinished);

		bMusicHasStarted = true;

		FString TrackName = GetCurrentTrackName();
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Now playing: %s"), *TrackName);

		// Broadcast track change
		OnTrackChanged.Broadcast(TrackName);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Error, TEXT("MusicPersistenceSubsystem: Failed to create audio component"));
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
	if (UWorld* World = GetWorld())
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
	return MusicAudioComponent && MusicAudioComponent->IsPlaying();
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
		return;
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPersistenceSubsystem: Track finished, advancing to next"));
	AdvanceTrackIndex();
	PlayCurrentTrack();
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
