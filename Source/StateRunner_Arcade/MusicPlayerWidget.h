#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MusicPlayerWidget.generated.h"

class UButton;
class UTextBlock;
class UCheckBox;
class UImage;
class UMusicPersistenceSubsystem;

/**
 * Music Player Widget
 * 
 * Small "Now Playing" display for menus.
 * Shows current track, skip button, and shuffle toggle.
 * 
 * Designed to be placed in the corner of the screen as an overlay.
 * Communicates directly with MusicPersistenceSubsystem (C++ - no Blueprint wiring needed).
 * 
 * UI Elements (bind in Blueprint):
 * - NowPlayingText: "Now Playing" label
 * - TrackNameText: Current track name
 * - SkipButton: Skip to next track
 * - ShuffleButton: Toggle shuffle mode (alternative to checkbox)
 * - ShuffleCheckBox: Toggle shuffle mode (alternative to button)
 * - ShuffleIndicator: Visual indicator when shuffle is on
 */
UCLASS(Abstract, Blueprintable)
class STATERUNNER_ARCADE_API UMusicPlayerWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UMusicPlayerWidget(const FObjectInitializer& ObjectInitializer);

protected:

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//=============================================================================
	// UI BINDINGS
	//=============================================================================

protected:

	/** "Now Playing" label text */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="UI")
	TObjectPtr<UTextBlock> NowPlayingText;

	/** Current track name display */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="UI")
	TObjectPtr<UTextBlock> TrackNameText;

	/** Skip to next track button */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="UI")
	TObjectPtr<UButton> SkipButton;

	/** Shuffle toggle button (use this OR ShuffleCheckBox) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="UI")
	TObjectPtr<UButton> ShuffleButton;

	/** Shuffle toggle checkbox (use this OR ShuffleButton) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="UI")
	TObjectPtr<UCheckBox> ShuffleCheckBox;

	/** Visual indicator image when shuffle is on (optional) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="UI")
	TObjectPtr<UImage> ShuffleIndicator;

	/** Text label for shuffle button/state */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="UI")
	TObjectPtr<UTextBlock> ShuffleLabel;

	//=============================================================================
	// STATE
	//=============================================================================

protected:

	/** Current shuffle state */
	UPROPERTY(BlueprintReadOnly, Category="State")
	bool bIsShuffleEnabled = false;

	/** Current track name */
	UPROPERTY(BlueprintReadOnly, Category="State")
	FString CurrentTrackName;

	/** Cached reference to the music subsystem */
	UPROPERTY()
	TObjectPtr<UMusicPersistenceSubsystem> MusicSubsystem;

	//=============================================================================
	// PUBLIC FUNCTIONS
	//=============================================================================

public:

	/**
	 * Set the displayed track name.
	 * Called automatically by subsystem binding, but can be called manually.
	 * 
	 * @param TrackName The track name to display
	 */
	UFUNCTION(BlueprintCallable, Category="Music Player")
	void SetTrackName(const FString& TrackName);

	/**
	 * Set the shuffle state display.
	 * Called automatically, but can be called manually.
	 * 
	 * @param bShuffleEnabled Whether shuffle is enabled
	 */
	UFUNCTION(BlueprintCallable, Category="Music Player")
	void SetShuffleState(bool bShuffleEnabled);

	/**
	 * Get current shuffle state.
	 */
	UFUNCTION(BlueprintPure, Category="Music Player")
	bool IsShuffleEnabled() const { return bIsShuffleEnabled; }

	/**
	 * Get the Skip button for external focus registration.
	 * Used by parent widgets to include music player in keyboard navigation.
	 */
	UFUNCTION(BlueprintPure, Category="Music Player")
	UButton* GetSkipButton() const { return SkipButton; }

	/**
	 * Get the Shuffle button for external focus registration.
	 * Used by parent widgets to include music player in keyboard navigation.
	 */
	UFUNCTION(BlueprintPure, Category="Music Player")
	UButton* GetShuffleButton() const { return ShuffleButton; }

	//=============================================================================
	// BUTTON HANDLERS
	//=============================================================================

protected:

	/** Internal handler for Skip button click */
	UFUNCTION()
	void HandleSkipClicked();

	/** Internal handler for Shuffle button click */
	UFUNCTION()
	void HandleShuffleButtonClicked();

	/** Internal handler for Shuffle checkbox state change */
	UFUNCTION()
	void HandleShuffleCheckBoxChanged(bool bIsChecked);

	/** Called when subsystem reports track change */
	UFUNCTION()
	void HandleTrackChanged(const FString& NewTrackName);

	//=============================================================================
	// INTERNAL
	//=============================================================================

protected:

	/** Update the shuffle visual indicator */
	void UpdateShuffleVisuals();

	/** Get and cache the music subsystem */
	UMusicPersistenceSubsystem* GetMusicSubsystem();

	/** Initialize from current subsystem state */
	void SyncWithSubsystem();
};
