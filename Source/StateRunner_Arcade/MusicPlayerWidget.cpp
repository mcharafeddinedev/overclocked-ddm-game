#include "MusicPlayerWidget.h"
#include "MusicPersistenceSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/CheckBox.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "StateRunner_Arcade.h"

UMusicPlayerWidget::UMusicPlayerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMusicPlayerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind Skip button
	if (SkipButton)
	{
		SkipButton->OnClicked.AddDynamic(this, &UMusicPlayerWidget::HandleSkipClicked);
	}

	// Bind Shuffle button (if using button instead of checkbox)
	if (ShuffleButton)
	{
		ShuffleButton->OnClicked.AddDynamic(this, &UMusicPlayerWidget::HandleShuffleButtonClicked);
	}

	// Bind Shuffle checkbox (if using checkbox instead of button)
	if (ShuffleCheckBox)
	{
		ShuffleCheckBox->OnCheckStateChanged.AddDynamic(this, &UMusicPlayerWidget::HandleShuffleCheckBoxChanged);
	}

	// Get the music subsystem and bind to its events
	if (UMusicPersistenceSubsystem* Subsystem = GetMusicSubsystem())
	{
		// Bind to track change events
		Subsystem->OnTrackChanged.AddDynamic(this, &UMusicPlayerWidget::HandleTrackChanged);

		// Initialize with current state
		SyncWithSubsystem();
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MusicPlayerWidget: Could not find MusicPersistenceSubsystem"));
	}

	// Initialize visuals
	UpdateShuffleVisuals();

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPlayerWidget: Constructed and connected to subsystem"));
}

void UMusicPlayerWidget::NativeDestruct()
{
	// Unbind from subsystem events
	if (MusicSubsystem && MusicSubsystem->IsValidLowLevel())
	{
		MusicSubsystem->OnTrackChanged.RemoveDynamic(this, &UMusicPlayerWidget::HandleTrackChanged);
	}

	// Unbind button delegates
	if (SkipButton)
	{
		SkipButton->OnClicked.RemoveDynamic(this, &UMusicPlayerWidget::HandleSkipClicked);
	}

	if (ShuffleButton)
	{
		ShuffleButton->OnClicked.RemoveDynamic(this, &UMusicPlayerWidget::HandleShuffleButtonClicked);
	}

	if (ShuffleCheckBox)
	{
		ShuffleCheckBox->OnCheckStateChanged.RemoveDynamic(this, &UMusicPlayerWidget::HandleShuffleCheckBoxChanged);
	}

	Super::NativeDestruct();

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPlayerWidget: Destructed"));
}

//=============================================================================
// SUBSYSTEM ACCESS
//=============================================================================

UMusicPersistenceSubsystem* UMusicPlayerWidget::GetMusicSubsystem()
{
	if (!MusicSubsystem)
	{
		MusicSubsystem = UMusicPersistenceSubsystem::GetMusicPersistenceSubsystem(this);
	}
	return MusicSubsystem;
}

void UMusicPlayerWidget::SyncWithSubsystem()
{
	if (UMusicPersistenceSubsystem* Subsystem = GetMusicSubsystem())
	{
		// Get current track name
		SetTrackName(Subsystem->GetCurrentTrackName());

		// Get current shuffle state
		SetShuffleState(Subsystem->IsShuffleEnabled());
	}
}

//=============================================================================
// PUBLIC FUNCTIONS
//=============================================================================

void UMusicPlayerWidget::SetTrackName(const FString& TrackName)
{
	CurrentTrackName = TrackName;

	if (TrackNameText)
	{
		TrackNameText->SetText(FText::FromString(TrackName));
	}

	UE_LOG(LogStateRunner_Arcade, Verbose, TEXT("MusicPlayerWidget: Now playing: %s"), *TrackName);
}

void UMusicPlayerWidget::SetShuffleState(bool bShuffleEnabled)
{
	bIsShuffleEnabled = bShuffleEnabled;
	UpdateShuffleVisuals();

	UE_LOG(LogStateRunner_Arcade, Verbose, TEXT("MusicPlayerWidget: Shuffle %s"), bShuffleEnabled ? TEXT("ON") : TEXT("OFF"));
}

//=============================================================================
// BUTTON HANDLERS
//=============================================================================

void UMusicPlayerWidget::HandleSkipClicked()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPlayerWidget: Skip clicked"));

	if (UMusicPersistenceSubsystem* Subsystem = GetMusicSubsystem())
	{
		Subsystem->SkipTrack();
	}
}

void UMusicPlayerWidget::HandleShuffleButtonClicked()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPlayerWidget: Shuffle button clicked"));

	if (UMusicPersistenceSubsystem* Subsystem = GetMusicSubsystem())
	{
		Subsystem->ToggleShuffle();
		// Update our local state to match
		SetShuffleState(Subsystem->IsShuffleEnabled());
	}
}

void UMusicPlayerWidget::HandleShuffleCheckBoxChanged(bool bIsChecked)
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPlayerWidget: Shuffle checkbox changed to %s"), bIsChecked ? TEXT("ON") : TEXT("OFF"));

	if (UMusicPersistenceSubsystem* Subsystem = GetMusicSubsystem())
	{
		// If checkbox state doesn't match subsystem, toggle it
		if (Subsystem->IsShuffleEnabled() != bIsChecked)
		{
			Subsystem->ToggleShuffle();
		}
		SetShuffleState(Subsystem->IsShuffleEnabled());
	}
}

void UMusicPlayerWidget::HandleTrackChanged(const FString& NewTrackName)
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MusicPlayerWidget: Track changed to: %s"), *NewTrackName);
	SetTrackName(NewTrackName);
}

//=============================================================================
// INTERNAL
//=============================================================================

void UMusicPlayerWidget::UpdateShuffleVisuals()
{
	// Update checkbox state (if using checkbox)
	if (ShuffleCheckBox)
	{
		ShuffleCheckBox->SetIsChecked(bIsShuffleEnabled);
	}

	// Update shuffle indicator visibility (if using indicator image)
	if (ShuffleIndicator)
	{
		ShuffleIndicator->SetVisibility(bIsShuffleEnabled ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Update shuffle label text (if using label)
	if (ShuffleLabel)
	{
		ShuffleLabel->SetText(FText::FromString(bIsShuffleEnabled ? TEXT("Shuffle: ON") : TEXT("Shuffle: OFF")));
	}
}
