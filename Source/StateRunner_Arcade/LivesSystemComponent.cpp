#include "LivesSystemComponent.h"
#include "WorldScrollComponent.h"
#include "ScoreSystemComponent.h"
#include "GameDebugSubsystem.h"
#include "StateRunner_ArcadeGameMode.h"
#include "StateRunner_ArcadeCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"

ULivesSystemComponent::ULivesSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void ULivesSystemComponent::BeginPlay()
{
	Super::BeginPlay();

	// Initialize lives
	CurrentLives = MaxLives;

	// Cache WorldScrollComponent
	CacheWorldScrollComponent();
}

//=============================================================================
// PUBLIC FUNCTIONS
//=============================================================================

bool ULivesSystemComponent::TakeDamage()
{
	// Check if can take damage
	if (!CanTakeDamage())
	{
		return false;
	}

	// Lose a life
	CurrentLives = FMath::Max(0, CurrentLives - 1);

	// Log damage event
	if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
	{
		Debug->Stat_Lives = CurrentLives;
		Debug->LogEvent(EDebugCategory::Lives, FString::Printf(TEXT("Damage! Lives: %d"), CurrentLives));
	}

	// Broadcast events
	OnLivesChanged.Broadcast(CurrentLives, MaxLives);
	OnDamageTaken.Broadcast(CurrentLives);

	// Play damage sound
	PlayLifeSound(DamageSound);

	// Apply damage effects
	ApplyDamageEffects();

	// Check for death
	if (CurrentLives <= 0)
	{
		TriggerDeath();
	}
	else
	{
		// Start invulnerability
		StartInvulnerability();
	}

	return true;
}

void ULivesSystemComponent::ResetLives()
{
	// Clear any existing invulnerability
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InvulnerabilityTimer);
	}

	CurrentLives = MaxLives;
	bIsInvulnerable = false;
	bIsDead = false;

	OnLivesChanged.Broadcast(CurrentLives, MaxLives);
	OnInvulnerabilityChanged.Broadcast(false);

	// Update debug subsystem
	if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
	{
		Debug->Stat_Lives = CurrentLives;
	}
}

void ULivesSystemComponent::AddLives(int32 Amount)
{
	if (Amount <= 0 || bIsDead)
	{
		return;
	}

	int32 OldLives = CurrentLives;
	CurrentLives = FMath::Min(CurrentLives + Amount, MaxLives);

	if (CurrentLives != OldLives)
	{
		OnLivesChanged.Broadcast(CurrentLives, MaxLives);
		
		if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
		{
			Debug->Stat_Lives = CurrentLives;
			Debug->LogEvent(EDebugCategory::Lives, FString::Printf(TEXT("+%d Life (%d/%d)"), Amount, CurrentLives, MaxLives));
		}
	}
}

bool ULivesSystemComponent::Collect1Up()
{
	if (bIsDead)
	{
		return false;
	}

	// Check if at max lives
	if (IsAtMaxLives())
	{
		// At max lives - return true to indicate score bonus should be awarded
		if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
		{
			Debug->LogEvent(EDebugCategory::Lives, TEXT("1-UP -> Score Bonus (Full Health)"));
		}
		return true;
	}
	else
	{
		// Below max lives - add 1 life
		AddLives(1);
		
		// Play 1-Up sound only when actually gaining a life
		PlayLifeSound(OneUpSound);
		
		return false;
	}
}

void ULivesSystemComponent::SetInvulnerable(bool bInvulnerable, float Duration)
{
	// Clear existing timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InvulnerabilityTimer);
	}

	bIsInvulnerable = bInvulnerable;
	OnInvulnerabilityChanged.Broadcast(bIsInvulnerable);

	if (bInvulnerable && Duration >= 0.0f)
	{
		// Use default duration if 0
		float ActualDuration = (Duration > 0.0f) ? Duration : InvulnerabilityDuration;
		
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				InvulnerabilityTimer,
				this,
				&ULivesSystemComponent::EndInvulnerability,
				ActualDuration,
				false
			);
		}
	}

}

//=============================================================================
// PROTECTED FUNCTIONS
//=============================================================================

void ULivesSystemComponent::ApplyDamageEffects()
{
	// Screen shake
	ApplyScreenShake();

	// Scroll slowdown
	if (bApplyScrollSlowdown && WorldScrollComponent)
	{
		WorldScrollComponent->ApplyDamageSlowdown();
	}
}

void ULivesSystemComponent::StartInvulnerability()
{
	SetInvulnerable(true, InvulnerabilityDuration);
}

void ULivesSystemComponent::EndInvulnerability()
{
	bIsInvulnerable = false;
	OnInvulnerabilityChanged.Broadcast(false);
}

void ULivesSystemComponent::TriggerDeath()
{
	bIsDead = true;
	bIsInvulnerable = false;

	// Clear invulnerability timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InvulnerabilityTimer);
	}

	// Log death event
	if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
	{
		Debug->LogEvent(EDebugCategory::Lives, TEXT("GAME OVER"));
	}

	// Play death sound using SpawnSound2D with "tick while paused" so it plays through the pause
	if (DeathSound)
	{
		UAudioComponent* DeathAudioComp = UGameplayStatics::SpawnSound2D(
			this,
			DeathSound,
			LivesSoundVolume,
			1.0f,  // Pitch
			0.0f,  // Start time
			nullptr,  // Concurrency settings
			false,  // bPersistAcrossLevelTransition
			true   // bAutoDestroy
		);
		
		if (DeathAudioComp)
		{
			DeathAudioComp->bAllowSpatialization = false;
			DeathAudioComp->SetTickableWhenPaused(true);
		}
	}

	// Stop scrolling and scoring, then save high score
	if (UWorld* World = GetWorld())
	{
		if (AStateRunner_ArcadeGameMode* GameMode = Cast<AStateRunner_ArcadeGameMode>(World->GetAuthGameMode()))
		{
			if (UWorldScrollComponent* ScrollComp = GameMode->GetWorldScrollComponent())
			{
				ScrollComp->SetScrollingEnabled(false);
			}

			if (UScoreSystemComponent* ScoreComp = GameMode->GetScoreSystemComponent())
			{
				ScoreComp->StopScoring();
				ScoreComp->CheckAndSaveHighScore();
			}
		}
	}

	// Broadcast death event (for UI, etc.)
	OnPlayerDied.Broadcast();
}

void ULivesSystemComponent::CacheWorldScrollComponent()
{
	if (UWorld* World = GetWorld())
	{
		if (AStateRunner_ArcadeGameMode* GameMode = Cast<AStateRunner_ArcadeGameMode>(World->GetAuthGameMode()))
		{
			WorldScrollComponent = GameMode->GetWorldScrollComponent();
		}
	}

}

void ULivesSystemComponent::ApplyScreenShake()
{
	if (DamageShakeIntensity <= 0.0f)
	{
		return;
	}

	// Get player character and call its camera shake function
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (AStateRunner_ArcadeCharacter* Character = Cast<AStateRunner_ArcadeCharacter>(PC->GetPawn()))
			{
				Character->PlayDamageCameraShake(DamageShakeIntensity);
			}
		}
	}
}

void ULivesSystemComponent::PlayLifeSound(USoundBase* Sound)
{
	if (!Sound)
	{
		return;
	}

	// Try to get player location for 3D sound, fallback to 2D if not available
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

	UGameplayStatics::PlaySoundAtLocation(this, Sound, SoundLocation, LivesSoundVolume);
}
