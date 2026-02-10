#include "GameDebugSubsystem.h"
#include "StateRunner_Arcade.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"

// Console command to toggle debug display
static FAutoConsoleCommand ToggleDebugCmd(
	TEXT("Debug.Toggle"),
	TEXT("Toggles the on-screen debug display"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		if (GEngine)
		{
			UWorld* World = GEngine->GetCurrentPlayWorld();
			if (World)
			{
				UGameInstance* GI = World->GetGameInstance();
				if (GI)
				{
					UGameDebugSubsystem* DebugSub = GI->GetSubsystem<UGameDebugSubsystem>();
					if (DebugSub)
					{
						DebugSub->ToggleDebugDisplay();
					}
				}
			}
		}
	})
);

void UGameDebugSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("GameDebugSubsystem initialized"));
}

void UGameDebugSubsystem::Deinitialize()
{
	RecentEvents.Empty();
	Super::Deinitialize();
}

UGameDebugSubsystem* UGameDebugSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}
	
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!World)
	{
		return nullptr;
	}
	
	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}
	
	return GameInstance->GetSubsystem<UGameDebugSubsystem>();
}

void UGameDebugSubsystem::LogEvent(EDebugCategory Category, const FString& Message, bool bAlsoLogToConsole)
{
	if (!bDebugEnabled)
	{
		return;
	}
	
	// Check if category is enabled
	if (!IsCategoryEnabled(Category))
	{
		return;
	}
	
	// Add to recent events
	FDebugEvent Event;
	Event.Category = Category;
	Event.Message = Message;
	Event.Timestamp = FPlatformTime::Seconds();
	RecentEvents.Insert(Event, 0);
	
	// Trim to max size
	while (RecentEvents.Num() > MaxEventLogSize)
	{
		RecentEvents.RemoveAt(RecentEvents.Num() - 1);
	}
	
	// Show on screen immediately
	if (GEngine)
	{
		FColor Color = GetCategoryColor(Category);
		FString DisplayText = FString::Printf(TEXT("[%s] %s"), *GetCategoryName(Category), *Message);
		GEngine->AddOnScreenDebugMessage(-1, EventDisplayDuration, Color, DisplayText);
	}
	
	// Also log to console if requested
	if (bAlsoLogToConsole)
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("[%s] %s"), *GetCategoryName(Category), *Message);
	}
}

void UGameDebugSubsystem::LogError(const FString& Message)
{
	// Errors always shown
	UE_LOG(LogStateRunner_Arcade, Error, TEXT("%s"), *Message);
	
	if (GEngine && bDebugEnabled)
	{
		GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Red, FString::Printf(TEXT("ERROR: %s"), *Message));
	}
}

void UGameDebugSubsystem::LogInit(const FString& SystemName, const FString& Info)
{
	// Init info goes to log only, not on-screen
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("=== %s Initialized ==="), *SystemName);
	
	// Parse multi-line info
	TArray<FString> Lines;
	Info.ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("  %s"), *Line);
	}
}

void UGameDebugSubsystem::ToggleDebugDisplay()
{
	bDebugEnabled = !bDebugEnabled;
	
	if (GEngine)
	{
		if (bDebugEnabled)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Debug Display: ON"));
		}
		else
		{
			// Clear all debug messages when disabling
			GEngine->ClearOnScreenDebugMessages();
		}
	}
}

void UGameDebugSubsystem::ToggleCategory(EDebugCategory Category)
{
	EnabledCategories ^= static_cast<int32>(Category);
}

bool UGameDebugSubsystem::IsCategoryEnabled(EDebugCategory Category) const
{
	return (EnabledCategories & static_cast<int32>(Category)) != 0;
}

void UGameDebugSubsystem::UpdateDisplay()
{
	if (!bDebugEnabled || !GEngine)
	{
		return;
	}
	
	PruneOldEvents();
	
	// Display stat summary (Key 100 = persistent slot for stats)
	if (bShowStatSummary)
	{
		FString StatSummary = BuildStatSummary();
		GEngine->AddOnScreenDebugMessage(100, 0.0f, FColor::Cyan, StatSummary);
	}
	
	// Display event log (Keys 101-105 for events)
	if (bShowEventLog)
	{
		for (int32 i = 0; i < RecentEvents.Num() && i < MaxEventLogSize; i++)
		{
			const FDebugEvent& Event = RecentEvents[i];
			float Age = FPlatformTime::Seconds() - Event.Timestamp;
			
			// Fade out as event ages
			float Alpha = FMath::Clamp(1.0f - (Age / EventDisplayDuration), 0.2f, 1.0f);
			FColor Color = GetCategoryColor(Event.Category);
			Color.A = static_cast<uint8>(Alpha * 255);
			
			FString EventText = FString::Printf(TEXT("[%s] %s"), 
				*GetCategoryName(Event.Category), *Event.Message);
			
			GEngine->AddOnScreenDebugMessage(101 + i, 0.0f, Color, EventText);
		}
	}
}

FString UGameDebugSubsystem::GetCategoryName(EDebugCategory Category) const
{
	switch (Category)
	{
		case EDebugCategory::Spawning:  return TEXT("SPAWN");
		case EDebugCategory::Score:     return TEXT("SCORE");
		case EDebugCategory::Lives:     return TEXT("LIVES");
		case EDebugCategory::Overclock: return TEXT("OC");
		case EDebugCategory::Movement:  return TEXT("MOVE");
		case EDebugCategory::Tutorial:  return TEXT("TUTOR");
		default:                        return TEXT("???");
	}
}

FColor UGameDebugSubsystem::GetCategoryColor(EDebugCategory Category) const
{
	switch (Category)
	{
		case EDebugCategory::Spawning:  return FColor::Yellow;
		case EDebugCategory::Score:     return FColor::Green;
		case EDebugCategory::Lives:     return FColor::Red;
		case EDebugCategory::Overclock: return FColor::Orange;
		case EDebugCategory::Movement:  return FColor::Cyan;
		case EDebugCategory::Tutorial:  return FColor::Magenta;
		default:                        return FColor::White;
	}
}

FString UGameDebugSubsystem::BuildStatSummary() const
{
	// Compact single-line summary of game state
	FString OCState = Stat_OverclockActive ? TEXT("ACTIVE") : FString::Printf(TEXT("%.0f%%"), Stat_OverclockPercent);
	
	return FString::Printf(
		TEXT("D:%d | Spd:%.0f | Obs:%d | Pkp:%d | Seg:%d | Scr:%d | Liv:%d | OC:%s"),
		Stat_DifficultyLevel,
		Stat_ScrollSpeed,
		Stat_ActiveObstacles,
		Stat_ActivePickups,
		Stat_SegmentsSpawned,
		Stat_Score,
		Stat_Lives,
		*OCState
	);
}

FString UGameDebugSubsystem::BuildEventLog() const
{
	FString Log;
	for (const FDebugEvent& Event : RecentEvents)
	{
		Log += FString::Printf(TEXT("[%s] %s\n"), *GetCategoryName(Event.Category), *Event.Message);
	}
	return Log;
}

void UGameDebugSubsystem::PruneOldEvents()
{
	float CurrentTime = FPlatformTime::Seconds();
	
	RecentEvents.RemoveAll([this, CurrentTime](const FDebugEvent& Event)
	{
		return (CurrentTime - Event.Timestamp) > EventDisplayDuration;
	});
}
