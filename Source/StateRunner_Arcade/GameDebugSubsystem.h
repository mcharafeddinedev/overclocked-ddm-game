#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameDebugSubsystem.generated.h"

/**
 * Debug category flags for filtering what gets displayed.
 * Use bitwise OR to combine categories.
 */
UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class EDebugCategory : uint8
{
	None        = 0        UMETA(Hidden),
	Spawning    = 1 << 0   UMETA(DisplayName = "Spawning"),      // Obstacle/Pickup spawning
	Score       = 1 << 1   UMETA(DisplayName = "Score"),         // Score changes
	Lives       = 1 << 2   UMETA(DisplayName = "Lives"),         // Damage, death, invuln
	Overclock   = 1 << 3   UMETA(DisplayName = "Overclock"),     // OVERCLOCK state
	Movement    = 1 << 4   UMETA(DisplayName = "Movement"),      // Jump, slide, lane
	Tutorial    = 1 << 5   UMETA(DisplayName = "Tutorial"),      // Tutorial prompts
	All         = 0xFF     UMETA(DisplayName = "All")
};
ENUM_CLASS_FLAGS(EDebugCategory);

/**
 * Game Debug Subsystem
 * 
 * Centralized debug display system for StateRunner Arcade.
 * Replaces scattered UE_LOG and on-screen debug messages with a
 * clean, organized, toggleable debug HUD.
 * 
 * Shows a compact on-screen debug summary with category-based filtering
 * and a scrolling event log. Toggle on/off via console or Blueprint.
 * 
 * Use LogEvent() for important gameplay events and UpdateStat() for
 * persistent stat values. The display auto-updates every tick.
 */
UCLASS()
class STATERUNNER_ARCADE_API UGameDebugSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//=========================================================================
	// CONFIGURATION
	//=========================================================================

	/** Master toggle for debug display. False = no on-screen messages. */
	UPROPERTY(BlueprintReadWrite, Category="Debug")
	bool bDebugEnabled = false;  // Off by default for production

	/** Which categories to display (bitmask). */
	UPROPERTY(BlueprintReadWrite, Category="Debug")
	int32 EnabledCategories = static_cast<int32>(EDebugCategory::All);

	/** Show the compact stat summary on screen. */
	UPROPERTY(BlueprintReadWrite, Category="Debug")
	bool bShowStatSummary = true;

	/** Show the recent events log on screen. */
	UPROPERTY(BlueprintReadWrite, Category="Debug")
	bool bShowEventLog = true;

	/** Maximum events to keep in the log. */
	UPROPERTY(BlueprintReadWrite, Category="Debug")
	int32 MaxEventLogSize = 5;

	/** How long events stay visible (seconds). */
	UPROPERTY(BlueprintReadWrite, Category="Debug")
	float EventDisplayDuration = 4.0f;

	//=========================================================================
	// STAT TRACKING (Updated by game systems)
	//=========================================================================

	/** Current difficulty level */
	int32 Stat_DifficultyLevel = 0;

	/** Current scroll speed */
	float Stat_ScrollSpeed = 0.0f;

	/** Active obstacles count */
	int32 Stat_ActiveObstacles = 0;

	/** Active pickups count */
	int32 Stat_ActivePickups = 0;

	/** Segments spawned */
	int32 Stat_SegmentsSpawned = 0;

	/** Current score */
	int32 Stat_Score = 0;

	/** Current lives */
	int32 Stat_Lives = 0;

	/** OVERCLOCK meter percent (0-100) */
	float Stat_OverclockPercent = 0.0f;

	/** Is OVERCLOCK active */
	bool Stat_OverclockActive = false;

	/** Last pattern used */
	FString Stat_LastPattern = TEXT("None");

	//=========================================================================
	// PUBLIC FUNCTIONS
	//=========================================================================

	/**
	 * Log a debug event that will appear in the event log.
	 * 
	 * @param Category Which category this event belongs to
	 * @param Message Short message describing the event
	 * @param bAlsoLogToConsole If true, also writes to Output Log
	 */
	UFUNCTION(BlueprintCallable, Category="Debug")
	void LogEvent(EDebugCategory Category, const FString& Message, bool bAlsoLogToConsole = false);

	/**
	 * Log an error (always shown, always logged to console).
	 * 
	 * @param Message Error message
	 */
	UFUNCTION(BlueprintCallable, Category="Debug")
	void LogError(const FString& Message);

	/**
	 * Log initialization info (shown once at startup).
	 * Goes to Output Log only, not on-screen.
	 * 
	 * @param SystemName Name of the system being initialized
	 * @param Info Multi-line info string
	 */
	UFUNCTION(BlueprintCallable, Category="Debug")
	void LogInit(const FString& SystemName, const FString& Info);

	/**
	 * Toggle debug display on/off.
	 */
	UFUNCTION(BlueprintCallable, Category="Debug")
	void ToggleDebugDisplay();

	/**
	 * Toggle a specific category.
	 */
	UFUNCTION(BlueprintCallable, Category="Debug")
	void ToggleCategory(EDebugCategory Category);

	/**
	 * Check if a category is enabled.
	 */
	UFUNCTION(BlueprintPure, Category="Debug")
	bool IsCategoryEnabled(EDebugCategory Category) const;

	/**
	 * Update the on-screen display. Called each frame by a game system.
	 */
	UFUNCTION(BlueprintCallable, Category="Debug")
	void UpdateDisplay();

	/**
	 * Get the subsystem from a world context.
	 */
	UFUNCTION(BlueprintPure, Category="Debug", meta=(WorldContext="WorldContextObject"))
	static UGameDebugSubsystem* Get(const UObject* WorldContextObject);

protected:

	//=========================================================================
	// INTERNAL STATE
	//=========================================================================

	/** Recent events with timestamps */
	struct FDebugEvent
	{
		EDebugCategory Category;
		FString Message;
		float Timestamp;
	};
	TArray<FDebugEvent> RecentEvents;

	/** Get category name for display */
	FString GetCategoryName(EDebugCategory Category) const;

	/** Get category color for display */
	FColor GetCategoryColor(EDebugCategory Category) const;

	/** Build the stat summary string */
	FString BuildStatSummary() const;

	/** Build the event log string */
	FString BuildEventLog() const;

	/** Prune old events */
	void PruneOldEvents();
};
