#include "WorldScrollComponent.h"
#include "GameDebugSubsystem.h"
#include "StateRunner_Arcade.h"
#include "StateRunner_ArcadeGameMode.h"
#include "ObstacleSpawnerComponent.h"
#include "Engine/Engine.h"

//=============================================================================
// CONSTRUCTOR
//=============================================================================

UWorldScrollComponent::UWorldScrollComponent()
{
	// Enable tick - we need to update scroll speed every frame when scrolling
	PrimaryComponentTick.bCanEverTick = true;
	
	// Start with tick disabled - enable when scrolling starts
	// This is a performance optimization
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Initialize runtime state
	CurrentScrollSpeed = 0.0f;
	TimeElapsed = 0.0f;
	bIsScrolling = false;
	LastBroadcastSpeed = 0.0f;
	LastPeriodicLogTime = 0.0f;
	bIsDamageSlowdownActive = false;
	DamageSlowdownTimeRemaining = 0.0f;
	
	// Initialize OVERCLOCK state (post-prototype feature)
	OverclockMultiplier = 1.0f;
	bIsOverclockActive = false;
}

//=============================================================================
// BEGIN PLAY
//=============================================================================

void UWorldScrollComponent::BeginPlay()
{
	Super::BeginPlay();

	// Initialize scroll speed to base value
	CurrentScrollSpeed = BaseScrollSpeed;
	LastBroadcastSpeed = BaseScrollSpeed;

	// Cache ObstacleSpawnerComponent reference for tutorial prompt checking
	if (UWorld* World = GetWorld())
	{
		if (AStateRunner_ArcadeGameMode* GameMode = Cast<AStateRunner_ArcadeGameMode>(World->GetAuthGameMode()))
		{
			CachedObstacleSpawner = GameMode->GetObstacleSpawnerComponent();
		}
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("WorldScrollComponent initialized:"));
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("  - BaseScrollSpeed: %.2f units/sec"), BaseScrollSpeed);
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("  - ScrollSpeedIncrease: %.2f units/sec²"), ScrollSpeedIncrease);
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("  - MaxScrollSpeed: %.2f units/sec"), MaxScrollSpeed);
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("  - DamageSlowdownMultiplier: %.2f (%.0f%% speed)"), 
		DamageSlowdownMultiplier, DamageSlowdownMultiplier * 100.0f);
}

//=============================================================================
// TICK COMPONENT
//=============================================================================

void UWorldScrollComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Only process if scrolling is enabled
	if (!bIsScrolling)
	{
		return;
	}

	// Update time elapsed
	TimeElapsed += DeltaTime;

	// Update scroll speed based on time elapsed
	UpdateScrollSpeed();

	// Process damage slowdown if active
	if (bIsDamageSlowdownActive)
	{
		ProcessDamageSlowdown(DeltaTime);
	}

	// Broadcast speed change if threshold exceeded
	BroadcastSpeedChangeIfNeeded();

	// Check tutorial prompts (broadcasts OnTutorialPrompt event for HUD)
	// Player is locked at X: -5000
	if (CachedObstacleSpawner)
	{
		CachedObstacleSpawner->CheckTutorialPrompts(GetCurrentScrollSpeed(), -5000.0f);
	}

	// Periodic log every 5 seconds to show speed is increasing (for debugging)
	// Log at 5, 10, 15, 20... seconds
	float NextLogTime = FMath::Floor(TimeElapsed / 5.0f) * 5.0f;
	if (NextLogTime > LastPeriodicLogTime && TimeElapsed >= 5.0f)
	{
		LastPeriodicLogTime = NextLogTime;
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("WorldScrollComponent: Speed update - %.2f units/sec (Time: %.1f sec)"), 
			CurrentScrollSpeed, TimeElapsed);
	}
	
	// Update debug subsystem with scroll speed
	if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
	{
		Debug->Stat_ScrollSpeed = GetCurrentScrollSpeed();
	}
}

//=============================================================================
// SCROLL SPEED FUNCTIONS
//=============================================================================

float UWorldScrollComponent::GetCurrentScrollSpeed() const
{
	// Return 0 if scrolling is disabled
	if (!bIsScrolling)
	{
		return 0.0f;
	}

	// Start with base calculated speed
	float FinalSpeed = CurrentScrollSpeed;

	// Apply OVERCLOCK multiplier if active (post-prototype feature)
	if (bIsOverclockActive)
	{
		FinalSpeed *= OverclockMultiplier;
	}

	// Apply damage slowdown multiplier if active (stacks with OVERCLOCK)
	if (bIsDamageSlowdownActive)
	{
		FinalSpeed *= DamageSlowdownMultiplier;
	}

	return FinalSpeed;
}

void UWorldScrollComponent::SetScrollingEnabled(bool bEnabled)
{
	if (bIsScrolling == bEnabled)
	{
		return; // No change needed
	}

	bIsScrolling = bEnabled;

	// Enable/disable tick based on scrolling state
	SetComponentTickEnabled(bEnabled);

	if (bEnabled)
	{
		// Ensure scroll speed is initialized (in case called before BeginPlay)
		if (CurrentScrollSpeed <= 0.0f)
		{
			CurrentScrollSpeed = BaseScrollSpeed;
		}
		
		// Reset periodic log timer when scrolling starts
		LastPeriodicLogTime = 0.0f;
		
		// Verify tick is actually enabled
		bool bTickEnabled = IsComponentTickEnabled();
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("WorldScrollComponent: Scrolling ENABLED at speed %.2f units/sec (Tick enabled: %s)"), 
			CurrentScrollSpeed, bTickEnabled ? TEXT("YES") : TEXT("NO"));
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("WorldScrollComponent: Scrolling DISABLED (TimeElapsed: %.2f sec)"), TimeElapsed);
	}
}

void UWorldScrollComponent::ResetScrollSpeed()
{
	TimeElapsed = 0.0f;
	CurrentScrollSpeed = BaseScrollSpeed;
	LastBroadcastSpeed = BaseScrollSpeed;
	LastPeriodicLogTime = 0.0f;
	bIsDamageSlowdownActive = false;
	DamageSlowdownTimeRemaining = 0.0f;
	
	// Reset OVERCLOCK state
	OverclockMultiplier = 1.0f;
	bIsOverclockActive = false;

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("WorldScrollComponent: Speed RESET to base (%.2f units/sec)"), BaseScrollSpeed);

	// Broadcast the reset
	OnScrollSpeedChanged.Broadcast(CurrentScrollSpeed);
}

void UWorldScrollComponent::UpdateScrollSpeed()
{
	// Calculate new scroll speed: Base + (Time * Increase), capped at Max
	// Formula: CurrentScrollSpeed = BaseScrollSpeed + (TimeElapsed * ScrollSpeedIncrease)
	float NewSpeed = BaseScrollSpeed + (TimeElapsed * ScrollSpeedIncrease);

	// Cap at maximum speed
	CurrentScrollSpeed = FMath::Min(NewSpeed, MaxScrollSpeed);
}

void UWorldScrollComponent::BroadcastSpeedChangeIfNeeded()
{
	// Only broadcast if speed changed by threshold amount
	float SpeedDifference = FMath::Abs(CurrentScrollSpeed - LastBroadcastSpeed);
	
	if (SpeedDifference >= SpeedChangeThreshold)
	{
		LastBroadcastSpeed = CurrentScrollSpeed;
		OnScrollSpeedChanged.Broadcast(CurrentScrollSpeed);

		UE_LOG(LogStateRunner_Arcade, Verbose, TEXT("WorldScrollComponent: Speed changed to %.2f units/sec (Time: %.2f sec)"), 
			CurrentScrollSpeed, TimeElapsed);
	}
}

//=============================================================================
// DAMAGE SLOWDOWN FUNCTIONS
//=============================================================================

void UWorldScrollComponent::ApplyDamageSlowdown()
{
	bIsDamageSlowdownActive = true;
	DamageSlowdownTimeRemaining = DamageSlowdownDuration;

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("WorldScrollComponent: Damage slowdown APPLIED (%.2f%% speed for %.2f sec)"), 
		DamageSlowdownMultiplier * 100.0f, DamageSlowdownDuration);
}

void UWorldScrollComponent::ProcessDamageSlowdown(float DeltaTime)
{
	DamageSlowdownTimeRemaining -= DeltaTime;

	if (DamageSlowdownTimeRemaining <= 0.0f)
	{
		bIsDamageSlowdownActive = false;
		DamageSlowdownTimeRemaining = 0.0f;

		UE_LOG(LogStateRunner_Arcade, Log, TEXT("WorldScrollComponent: Damage slowdown ENDED, speed restored to %.2f units/sec"), 
			CurrentScrollSpeed);
	}
}

//=============================================================================
// OVERCLOCK FUNCTIONS (Post-Prototype Feature Hook)
//=============================================================================

void UWorldScrollComponent::SetOverclockMultiplier(float Multiplier, bool bActivate)
{
	OverclockMultiplier = FMath::Max(Multiplier, 1.0f); // Minimum 1.0 (normal speed)
	bIsOverclockActive = bActivate;

	if (bActivate)
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("WorldScrollComponent: OVERCLOCK ACTIVATED (%.2fx speed multiplier)"), 
			OverclockMultiplier);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("WorldScrollComponent: OVERCLOCK DEACTIVATED (speed multiplier reset to 1.0x)"));
		OverclockMultiplier = 1.0f;
	}

	// Broadcast speed change since effective speed has changed
	OnScrollSpeedChanged.Broadcast(GetCurrentScrollSpeed());
}
