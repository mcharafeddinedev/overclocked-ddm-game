#include "OverclockSystemComponent.h"
#include "WorldScrollComponent.h"
#include "ScoreSystemComponent.h"
#include "GameDebugSubsystem.h"
#include "StateRunner_Arcade.h"
#include "StateRunner_ArcadeGameMode.h"
#include "StateRunner_ArcadeCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"

UOverclockSystemComponent::UOverclockSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UOverclockSystemComponent::BeginPlay()
{
	Super::BeginPlay();

	CacheComponents();

	CurrentMeter = 0.0f;
	bIsOverclockActive = false;
	bIsKeyHeld = false;
}

void UOverclockSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Update meter (passive fill or active drain)
	UpdateMeter(DeltaTime);

	// Check if should deactivate (meter depleted while active)
	if (bIsOverclockActive && CurrentMeter <= 0.0f)
	{
		DeactivateOverclock();
	}
}

//=============================================================================
// PUBLIC FUNCTIONS
//=============================================================================

void UOverclockSystemComponent::OnOverclockKeyPressed()
{
	bIsKeyHeld = true;

	// Try to activate if we can
	if (CanActivateOverclock())
	{
		ActivateOverclock();
	}
	// Not enough meter - feedback handled by UI
}

void UOverclockSystemComponent::OnOverclockKeyReleased()
{
	bIsKeyHeld = false;

	// Deactivate if active
	if (bIsOverclockActive)
	{
		DeactivateOverclock();
	}
}

void UOverclockSystemComponent::AddPickupBonus()
{
	float OldMeter = CurrentMeter;
	CurrentMeter = FMath::Min(CurrentMeter + PickupMeterBonus, MaxMeter);

	if (CurrentMeter != OldMeter)
	{
		OnOverclockMeterChanged.Broadcast(CurrentMeter, MaxMeter);
		UE_LOG(LogStateRunner_Arcade, Verbose, TEXT("OVERCLOCK meter +%.1f (now %.1f/%.1f)"), PickupMeterBonus, CurrentMeter, MaxMeter);
	}
}

void UOverclockSystemComponent::ResetOverclock()
{
	// Deactivate if active
	if (bIsOverclockActive)
	{
		DeactivateOverclock();
	}

	CurrentMeter = 0.0f;
	bIsKeyHeld = false;

	OnOverclockMeterChanged.Broadcast(CurrentMeter, MaxMeter);
}

//=============================================================================
// PROTECTED FUNCTIONS
//=============================================================================

void UOverclockSystemComponent::ActivateOverclock()
{
	if (bIsOverclockActive)
	{
		return;
	}

	bIsOverclockActive = true;

	// Apply speed multiplier
	if (WorldScrollComponent)
	{
		WorldScrollComponent->SetOverclockMultiplier(SpeedMultiplier, true);
	}

	// Activate bonus scoring
	if (ScoreSystemComponent)
	{
		ScoreSystemComponent->SetOverclockActive(true);
	}

	// Trigger camera zoom in for intensity effect
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (AStateRunner_ArcadeCharacter* Character = Cast<AStateRunner_ArcadeCharacter>(PC->GetPawn()))
			{
				Character->SetOverclockZoom(true);
			}
		}
	}

	// Play activation sound and start loop
	PlayOverclockSound(OverclockActivateSound);
	StartOverclockLoop();

	OnOverclockStateChanged.Broadcast(true);

	// Log OVERCLOCK event
	if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
	{
		Debug->Stat_OverclockActive = true;
		Debug->Stat_OverclockPercent = GetMeterPercent() * 100.0f;
		Debug->LogEvent(EDebugCategory::Overclock, TEXT("OVERCLOCK ACTIVATED!"));
	}
}

void UOverclockSystemComponent::DeactivateOverclock()
{
	if (!bIsOverclockActive)
	{
		return;
	}

	bIsOverclockActive = false;

	// Remove speed multiplier
	if (WorldScrollComponent)
	{
		WorldScrollComponent->SetOverclockMultiplier(1.0f, false);
	}

	// Deactivate bonus scoring
	if (ScoreSystemComponent)
	{
		ScoreSystemComponent->SetOverclockActive(false);
	}

	// Trigger camera zoom out to normal
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (AStateRunner_ArcadeCharacter* Character = Cast<AStateRunner_ArcadeCharacter>(PC->GetPawn()))
			{
				Character->SetOverclockZoom(false);
			}
		}
	}

	// Stop loop and play deactivation sound
	StopOverclockLoop();
	PlayOverclockSound(OverclockDeactivateSound);

	OnOverclockStateChanged.Broadcast(false);

	// Log OVERCLOCK deactivation
	if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
	{
		Debug->Stat_OverclockActive = false;
		Debug->LogEvent(EDebugCategory::Overclock, TEXT("OVERCLOCK ended"));
	}
}

void UOverclockSystemComponent::UpdateMeter(float DeltaTime)
{
	float OldMeter = CurrentMeter;

	if (bIsOverclockActive)
	{
		// Drain meter while active
		CurrentMeter = FMath::Max(0.0f, CurrentMeter - (ActiveDrainRate * DeltaTime));
	}
	else
	{
		// Passive fill when not active
		CurrentMeter = FMath::Min(MaxMeter, CurrentMeter + (PassiveFillRate * DeltaTime));
	}

	// Broadcast change if significant
	if (FMath::Abs(CurrentMeter - OldMeter) > 0.1f)
	{
		OnOverclockMeterChanged.Broadcast(CurrentMeter, MaxMeter);
		
		// Update debug subsystem stats
		if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
		{
			Debug->Stat_OverclockPercent = GetMeterPercent() * 100.0f;
		}
	}
}

void UOverclockSystemComponent::CacheComponents()
{
	if (UWorld* World = GetWorld())
	{
		if (AStateRunner_ArcadeGameMode* GameMode = Cast<AStateRunner_ArcadeGameMode>(World->GetAuthGameMode()))
		{
			WorldScrollComponent = GameMode->GetWorldScrollComponent();
			ScoreSystemComponent = GameMode->GetScoreSystemComponent();
		}
	}
}

//=============================================================================
// SOUND EFFECTS
//=============================================================================

void UOverclockSystemComponent::PlayOverclockSound(USoundBase* Sound)
{
	if (!Sound)
	{
		return;
	}

	// Try to get player location for 3D sound
	FVector SoundLocation = FVector::ZeroVector;
	
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				SoundLocation = Pawn->GetActorLocation();
			}
		}
	}

	UGameplayStatics::PlaySoundAtLocation(
		this,
		Sound,
		SoundLocation,
		OverclockSoundVolume
	);
}

void UOverclockSystemComponent::StartOverclockLoop()
{
	if (!OverclockLoopSound)
	{
		return;
	}

	// Stop any existing loop
	StopOverclockLoop();

	// Spawn a new looping audio component
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				ActiveLoopAudio = UGameplayStatics::SpawnSoundAttached(
					OverclockLoopSound,
					Pawn->GetRootComponent(),
					NAME_None,
					FVector::ZeroVector,
					EAttachLocation::KeepRelativeOffset,
					false,  // Don't stop when attached-to is destroyed (we manage it)
					OverclockSoundVolume,
					1.0f,   // Pitch
					0.0f,   // Start time
					nullptr,
					nullptr,
					true    // Auto-destroy when finished (shouldn't happen for loops)
				);

				if (ActiveLoopAudio)
				{
					// Fade in
					ActiveLoopAudio->FadeIn(LoopSoundFadeDuration);
				}
			}
		}
	}
}

void UOverclockSystemComponent::StopOverclockLoop()
{
	if (ActiveLoopAudio)
	{
		// Fade out and stop
		ActiveLoopAudio->FadeOut(LoopSoundFadeDuration, 0.0f);
		ActiveLoopAudio = nullptr;
	}
}
