#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MusicPersistenceSubsystem.generated.h"

class UAudioComponent;
class USoundBase;
class USoundClass;

/**
 * Delegate for when track changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMusicTrackChanged, const FString&, TrackName);

/**
 * Music Manager Subsystem
 * 
 * Handles persistent music playback across level loads.
 * Since this is a GameInstanceSubsystem, it persists for the entire game session.
 * Uses SpawnSound2D to create an audio component that survives level transitions.
 * 
 * This completely replaces BP_MusicManager - all music logic lives here.
 */
UCLASS()
class STATERUNNER_ARCADE_API UMusicPersistenceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//=============================================================================
	// MUSIC PLAYBACK
	//=============================================================================

	/**
	 * Play the track at the current index.
	 */
	UFUNCTION(BlueprintCallable, Category="Music")
	void PlayCurrentTrack();

	/**
	 * Skip to the next track.
	 */
	UFUNCTION(BlueprintCallable, Category="Music")
	void SkipTrack();

	/**
	 * Toggle shuffle mode.
	 */
	UFUNCTION(BlueprintCallable, Category="Music")
	void ToggleShuffle();

	/**
	 * Check if music is currently playing.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Music")
	bool IsPlaying() const;

	/**
	 * Get the current track name.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Music")
	FString GetCurrentTrackName() const;

	/**
	 * Get the current track index.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Music")
	int32 GetCurrentTrackIndex() const { return CurrentTrackIndex; }

	/**
	 * Check if shuffle is enabled.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Music")
	bool IsShuffleEnabled() const { return bShuffleEnabled; }

	/**
	 * Check if music has been initialized (first track started).
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Music")
	bool HasMusicStarted() const { return bMusicHasStarted; }

	//=============================================================================
	// TRACK CONFIGURATION
	//=============================================================================

	/**
	 * Set the music tracks array. Call this once at startup.
	 * Can be called from Blueprint to configure tracks.
	 */
	UFUNCTION(BlueprintCallable, Category="Music")
	void SetMusicTracks(const TArray<USoundBase*>& InTracks);

	/**
	 * Get the number of tracks.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Music")
	int32 GetTrackCount() const { return MusicTracks.Num(); }

	//=============================================================================
	// EVENTS
	//=============================================================================

	/** Broadcast when the track changes */
	UPROPERTY(BlueprintAssignable, Category="Music")
	FOnMusicTrackChanged OnTrackChanged;

	//=============================================================================
	// STATIC HELPER
	//=============================================================================

	/**
	 * Static helper to get subsystem from world context.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Music", meta=(WorldContext="WorldContextObject"))
	static UMusicPersistenceSubsystem* GetMusicPersistenceSubsystem(const UObject* WorldContextObject);

protected:

	//=============================================================================
	// INTERNAL STATE
	//=============================================================================

	/** The currently playing audio component (persists across levels) */
	UPROPERTY()
	TObjectPtr<UAudioComponent> MusicAudioComponent;

	/** Music Sound Class for volume control routing */
	UPROPERTY()
	TObjectPtr<USoundClass> MusicSoundClass;

	/** Array of music tracks to play */
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> MusicTracks;

	/** Current track index */
	UPROPERTY()
	int32 CurrentTrackIndex = 0;

	/** Whether shuffle is enabled */
	UPROPERTY()
	bool bShuffleEnabled = false;

	/** Whether music has started playing */
	UPROPERTY()
	bool bMusicHasStarted = false;

	/** Flag to ignore OnAudioFinished during volume adjustments */
	bool bIgnoreAudioFinished = false;

public:
	/** Call this before adjusting volume to prevent false track-skip triggers */
	void SetIgnoreAudioFinished(bool bIgnore);

protected:
	//=============================================================================
	// INTERNAL FUNCTIONS
	//=============================================================================

	/** Advance to the next track index (considering shuffle) */
	void AdvanceTrackIndex();

	/** Called when a track finishes playing */
	UFUNCTION()
	void OnAudioFinished();

	/** Create/recreate the audio component */
	void EnsureAudioComponent();

	/** Load default tracks from asset paths */
	void LoadDefaultTracks();
};
