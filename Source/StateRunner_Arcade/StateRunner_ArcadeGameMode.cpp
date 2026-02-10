#include "StateRunner_ArcadeGameMode.h"
#include "WorldScrollComponent.h"
#include "ObstacleSpawnerComponent.h"
#include "PickupSpawnerComponent.h"
#include "ScoreSystemComponent.h"
#include "LivesSystemComponent.h"
#include "OverclockSystemComponent.h"
#include "StateRunner_Arcade.h"

// --- Constructor ---

AStateRunner_ArcadeGameMode::AStateRunner_ArcadeGameMode()
{
	// Create the World Scroll System Component
	// This component manages scroll speed for all world objects
	WorldScrollComponent = CreateDefaultSubobject<UWorldScrollComponent>(TEXT("WorldScrollComponent"));

	// Create the Obstacle Spawner Component
	// This component manages obstacle spawning, pooling, and patterns
	ObstacleSpawnerComponent = CreateDefaultSubobject<UObstacleSpawnerComponent>(TEXT("ObstacleSpawnerComponent"));

	// Create the Pickup Spawner Component
	// This component manages Data Packet spawning and pooling
	PickupSpawnerComponent = CreateDefaultSubobject<UPickupSpawnerComponent>(TEXT("PickupSpawnerComponent"));

	// Create the Score System Component
	// This component manages score, time-based accumulation, and high scores
	ScoreSystemComponent = CreateDefaultSubobject<UScoreSystemComponent>(TEXT("ScoreSystemComponent"));

	// Create the Lives System Component
	// This component manages player lives, damage, and invulnerability
	LivesSystemComponent = CreateDefaultSubobject<ULivesSystemComponent>(TEXT("LivesSystemComponent"));

	// Create the OVERCLOCK System Component
	// This component manages OVERCLOCK meter, activation, and bonuses
	OverclockSystemComponent = CreateDefaultSubobject<UOverclockSystemComponent>(TEXT("OverclockSystemComponent"));
}

// --- Begin Play ---

void AStateRunner_ArcadeGameMode::BeginPlay()
{
	// Apply debug overrides BEFORE Super::BeginPlay so components see them during init
	if (bDebugEndgameBlockageTest)
	{
		// Configure obstacle spawner: force blockage + skip tutorial + late-game difficulty
		if (ObstacleSpawnerComponent)
		{
			ObstacleSpawnerComponent->bDebugForce3LaneBlockage = true;
		}

		// Configure score system: start at 1,000,000 with matching time elapsed
		// Time approximation: at BaseRate=125, +8 every 4s, ~936 seconds produces ~1M from time alone
		// (pickups add the rest in a real game). Score rate at 936s ≈ 2000 pts/s.
		if (ScoreSystemComponent)
		{
			ScoreSystemComponent->DebugInitialScore = 1000000;
			ScoreSystemComponent->DebugInitialTimeElapsed = 936.0f;
		}
	}

	Super::BeginPlay();

	// Log debug mode status AFTER component initialization
	if (bDebugEndgameBlockageTest)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT(""));
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("================================================"));
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("  DEBUG: ENDGAME BLOCKAGE TEST MODE ACTIVE"));
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("  Score: 1,000,000 | Rate: ~2000 pts/s"));
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("  Tutorial: SKIPPED | Difficulty: Late-game"));
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("  3-Lane FullWall Blockage: FORCED every segment"));
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("================================================"));
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT(""));
	}

	// Verify system components are valid (only log errors for missing ones)
	if (!WorldScrollComponent)
	{
		UE_LOG(LogStateRunner_Arcade, Error, TEXT("  - WorldScrollComponent: MISSING!"));
	}
	if (!ObstacleSpawnerComponent)
	{
		UE_LOG(LogStateRunner_Arcade, Error, TEXT("  - ObstacleSpawnerComponent: MISSING!"));
	}
	if (!PickupSpawnerComponent)
	{
		UE_LOG(LogStateRunner_Arcade, Error, TEXT("  - PickupSpawnerComponent: MISSING!"));
	}
	if (!ScoreSystemComponent)
	{
		UE_LOG(LogStateRunner_Arcade, Error, TEXT("  - ScoreSystemComponent: MISSING!"));
	}
	if (!LivesSystemComponent)
	{
		UE_LOG(LogStateRunner_Arcade, Error, TEXT("  - LivesSystemComponent: MISSING!"));
	}
	if (!OverclockSystemComponent)
	{
		UE_LOG(LogStateRunner_Arcade, Error, TEXT("  - OverclockSystemComponent: MISSING!"));
	}

	// Scrolling and scoring are NOT started here — the game flow system
	// (countdown, etc.) calls SetScrollingEnabled / StartScoring when ready.
}
