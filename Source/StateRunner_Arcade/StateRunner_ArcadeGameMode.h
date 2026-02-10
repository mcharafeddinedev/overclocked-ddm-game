#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StateRunner_ArcadeGameMode.generated.h"

class UWorldScrollComponent;
class UObstacleSpawnerComponent;
class UPickupSpawnerComponent;
class UScoreSystemComponent;
class ULivesSystemComponent;
class UOverclockSystemComponent;

/**
 * Central game mode for StateRunner Arcade.
 * Owns all the core system components (scrolling, spawning, scoring, lives, etc.)
 * and other actors grab references to them through here.
 */
UCLASS(abstract)
class AStateRunner_ArcadeGameMode : public AGameModeBase
{
	GENERATED_BODY()

	// --- System Components ---

protected:

	/**
	 * World Scroll System Component
	 * Manages scroll speed for track segments, obstacles, and pickups.
	 * Other actors query this to determine their movement speed.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UWorldScrollComponent> WorldScrollComponent;

	/**
	 * Obstacle Spawner Component
	 * Manages obstacle spawning, object pooling, and pattern generation.
	 * Call SpawnObstaclesForSegment() when a new track segment is created.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UObstacleSpawnerComponent> ObstacleSpawnerComponent;

	/**
	 * Pickup Spawner Component
	 * Manages pickup (Data Packet) spawning and object pooling.
	 * Call SpawnPickupsForSegment() when a new track segment is created.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPickupSpawnerComponent> PickupSpawnerComponent;

	/**
	 * Score System Component
	 * Manages score tracking, time-based accumulation, and high score persistence.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UScoreSystemComponent> ScoreSystemComponent;

	/**
	 * Lives System Component
	 * Manages player lives, damage, invulnerability, and death.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems", meta=(AllowPrivateAccess="true"))
	TObjectPtr<ULivesSystemComponent> LivesSystemComponent;

	/**
	 * OVERCLOCK System Component
	 * Manages OVERCLOCK meter, activation, and speed/score bonuses.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Systems", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UOverclockSystemComponent> OverclockSystemComponent;

public:
	
	/** Constructor */
	AStateRunner_ArcadeGameMode();

	// --- System Accessors ---

public:

	/**
	 * Get the World Scroll System Component.
	 * Queries current scroll speed or controls scrolling state.
	 * 
	 * @return World Scroll Component (never null after construction)
	 */
	UFUNCTION(BlueprintPure, Category="Systems")
	UWorldScrollComponent* GetWorldScrollComponent() const { return WorldScrollComponent; }

	/**
	 * Get the Obstacle Spawner Component.
	 * Handles obstacle spawning for track segments.
	 * 
	 * @return Obstacle Spawner Component (never null after construction)
	 */
	UFUNCTION(BlueprintPure, Category="Systems")
	UObstacleSpawnerComponent* GetObstacleSpawnerComponent() const { return ObstacleSpawnerComponent; }

	/**
	 * Get the Pickup Spawner Component.
	 * Handles pickup spawning for track segments.
	 * 
	 * @return Pickup Spawner Component (never null after construction)
	 */
	UFUNCTION(BlueprintPure, Category="Systems")
	UPickupSpawnerComponent* GetPickupSpawnerComponent() const { return PickupSpawnerComponent; }

	/**
	 * Get the Score System Component.
	 * Manages score tracking and high score persistence.
	 * 
	 * @return Score System Component (never null after construction)
	 */
	UFUNCTION(BlueprintPure, Category="Systems")
	UScoreSystemComponent* GetScoreSystemComponent() const { return ScoreSystemComponent; }

	/**
	 * Get the Lives System Component.
	 * Manages player lives, damage, and invulnerability.
	 * 
	 * @return Lives System Component (never null after construction)
	 */
	UFUNCTION(BlueprintPure, Category="Systems")
	ULivesSystemComponent* GetLivesSystemComponent() const { return LivesSystemComponent; }

	/**
	 * Get the OVERCLOCK System Component.
	 * Manages OVERCLOCK meter and activation state.
	 * 
	 * @return OVERCLOCK System Component (never null after construction)
	 */
	UFUNCTION(BlueprintPure, Category="Systems")
	UOverclockSystemComponent* GetOverclockSystemComponent() const { return OverclockSystemComponent; }

	// --- Debug Configuration ---

public:

	/**
	 * DEBUG: Endgame blockage test mode.
	 * When enabled:
	 * - Score starts at 1,000,000 with matching late-game score rate (~2000 pts/s)
	 * - Difficulty set to late-game levels (past breather cutoff, max obstacle density)
	 * - Tutorial is skipped entirely
	 * - Every segment has a forced 3-lane FullWall blockage injected
	 *   (verifies EnsureFairLayout correctly removes one to create a passable path)
	 * 
	 * WHAT TO LOOK FOR in the Output Log:
	 * - "DEBUG: Injected 3-lane FullWall blockage" — the forced blockage
	 * - "EnsureFairLayout: FullWall cluster blocks ALL 3 lanes!" — the fix catching it
	 * 
	 * Make sure to disable this for normal gameplay!
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Debug")
	bool bDebugEndgameBlockageTest = false;

protected:

	/** Called when the game starts or when spawned */
	virtual void BeginPlay() override;
};



