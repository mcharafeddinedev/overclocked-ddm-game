#include "ThemeSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/GameUserSettings.h"
#include "StateRunner_Arcade.h"

//=============================================================================
// STATIC CONSTANTS
//=============================================================================

const FString UThemeSubsystem::ThemeConfigSection = TEXT("StateRunnerArcade.Theme");
const FString UThemeSubsystem::ThemeConfigKey = TEXT("CurrentTheme");

// Element index constants are defined in header

//=============================================================================
// SUBSYSTEM LIFECYCLE
//=============================================================================

void UThemeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Set up default asset paths if not already configured
	if (ThemeAssetPaths.Num() == 0)
	{
		// Default paths - adjust these to match your Content folder structure
		ThemeAssetPaths.Add(EThemeType::Cyan, FSoftObjectPath(TEXT("/Game/_DEVELOPER/Data/Themes/DA_Theme_Cyan.DA_Theme_Cyan")));
		ThemeAssetPaths.Add(EThemeType::Orange, FSoftObjectPath(TEXT("/Game/_DEVELOPER/Data/Themes/DA_Theme_Orange.DA_Theme_Orange")));
		ThemeAssetPaths.Add(EThemeType::PurplePink, FSoftObjectPath(TEXT("/Game/_DEVELOPER/Data/Themes/DA_Theme_PurplePink.DA_Theme_PurplePink")));
		ThemeAssetPaths.Add(EThemeType::Green, FSoftObjectPath(TEXT("/Game/_DEVELOPER/Data/Themes/DA_Theme_Green.DA_Theme_Green")));
		ThemeAssetPaths.Add(EThemeType::Ruby, FSoftObjectPath(TEXT("/Game/_DEVELOPER/Data/Themes/DA_Theme_Ruby.DA_Theme_Ruby")));
		ThemeAssetPaths.Add(EThemeType::Gold, FSoftObjectPath(TEXT("/Game/_DEVELOPER/Data/Themes/DA_Theme_Gold.DA_Theme_Gold")));
	}

	// Load theme assets from paths
	LoadThemeAssets();

	// Load saved theme preference
	LoadThemePreference();

	if (ThemeAssets.Num() == 0)
	{
		UE_LOG(LogStateRunner_Arcade, Error, TEXT("ThemeSubsystem: Initialized with theme %d but 0 assets loaded! Theme switching DISABLED."), 
			static_cast<int32>(CurrentThemeType));
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("ThemeSubsystem: Initialized with theme %d, %d assets loaded"), 
			static_cast<int32>(CurrentThemeType), ThemeAssets.Num());
	}
}

void UThemeSubsystem::Deinitialize()
{
	// Clear registered meshes
	RegisteredMeshes.Empty();

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("ThemeSubsystem: Deinitialized"));

	Super::Deinitialize();
}

//=============================================================================
// THEME MANAGEMENT
//=============================================================================

UThemeDataAsset* UThemeSubsystem::GetCurrentThemeData() const
{
	return GetThemeData(CurrentThemeType);
}

void UThemeSubsystem::SetCurrentTheme(EThemeType NewTheme, bool bApplyImmediately)
{
	if (CurrentThemeType == NewTheme)
	{
		return; // No change
	}

	EThemeType OldTheme = CurrentThemeType;
	CurrentThemeType = NewTheme;

	// Save preference
	SaveThemePreference();

	// Broadcast change
	OnThemeChanged.Broadcast(NewTheme);

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("ThemeSubsystem: Theme changed from %d to %d"), 
		static_cast<int32>(OldTheme), static_cast<int32>(NewTheme));

	// Apply to all registered meshes if requested
	if (bApplyImmediately)
	{
		RefreshAllThemedMeshes();
	}
}

UThemeDataAsset* UThemeSubsystem::GetThemeData(EThemeType ThemeType) const
{
	const TObjectPtr<UThemeDataAsset>* FoundAsset = ThemeAssets.Find(ThemeType);
	if (FoundAsset && *FoundAsset)
	{
		return *FoundAsset;
	}

	UE_LOG(LogStateRunner_Arcade, Warning, TEXT("ThemeSubsystem: No theme data asset found for theme type %d"), 
		static_cast<int32>(ThemeType));
	return nullptr;
}

TArray<EThemeType> UThemeSubsystem::GetAvailableThemes() const
{
	TArray<EThemeType> AvailableThemes;
	ThemeAssets.GetKeys(AvailableThemes);
	return AvailableThemes;
}

//=============================================================================
// THEME ASSET LOADING
//=============================================================================

void UThemeSubsystem::RegisterThemeAsset(EThemeType ThemeType, UThemeDataAsset* ThemeData)
{
	if (ThemeData)
	{
		ThemeAssets.Add(ThemeType, ThemeData);
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("ThemeSubsystem: Registered theme asset for type %d: %s"),
			static_cast<int32>(ThemeType), *ThemeData->GetName());
	}
}

void UThemeSubsystem::LoadThemeAssets()
{
	int32 LoadedCount = 0;

	for (const auto& Pair : ThemeAssetPaths)
	{
		EThemeType ThemeType = Pair.Key;
		const FSoftObjectPath& AssetPath = Pair.Value;

		if (AssetPath.IsValid())
		{
			// Synchronously load the asset
			// In shipping builds, TryLoad() fails if the asset wasn't cooked.
			// Make sure theme directories are in DirectoriesToAlwaysCook (DefaultGame.ini).
			UObject* LoadedObject = AssetPath.TryLoad();
			if (UThemeDataAsset* ThemeData = Cast<UThemeDataAsset>(LoadedObject))
			{
				ThemeAssets.Add(ThemeType, ThemeData);
				LoadedCount++;
				UE_LOG(LogStateRunner_Arcade, Log, TEXT("ThemeSubsystem: Loaded theme asset for type %d: %s"),
					static_cast<int32>(ThemeType), *ThemeData->GetName());
			}
			else
			{
				// Use Error severity so this is visible in shipping builds
				UE_LOG(LogStateRunner_Arcade, Error, TEXT("ThemeSubsystem: FAILED to load theme asset at path: %s (Object: %s). Ensure /Game/_DEVELOPER/Data/Themes is in DirectoriesToAlwaysCook!"),
					*AssetPath.ToString(), LoadedObject ? *LoadedObject->GetClass()->GetName() : TEXT("NULL"));
			}
		}
	}

	if (LoadedCount == 0)
	{
		UE_LOG(LogStateRunner_Arcade, Error, TEXT("ThemeSubsystem: ZERO theme assets loaded out of %d paths! Themes will NOT work. Check DirectoriesToAlwaysCook in DefaultGame.ini."), ThemeAssetPaths.Num());
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("ThemeSubsystem: Loaded %d/%d theme assets"), LoadedCount, ThemeAssetPaths.Num());
	}
}

//=============================================================================
// MESH APPLICATION
//=============================================================================

void UThemeSubsystem::ApplyThemeToMesh(UStaticMeshComponent* MeshComponent)
{
	if (!MeshComponent)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("ThemeSubsystem: ApplyThemeToMesh called with null mesh"));
		return;
	}

	const UThemeDataAsset* ThemeData = GetCurrentThemeData();
	if (!ThemeData)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("ThemeSubsystem: No theme data available for current theme"));
		return;
	}

	// Apply circuit pattern emissive colors (elements 0-255)
	ApplyCircuitPatternTheme(MeshComponent, ThemeData);

	// Apply laser pointer materials (elements 258-265, 269-272)
	ApplyLaserPointerTheme(MeshComponent, ThemeData);

	UE_LOG(LogStateRunner_Arcade, Verbose, TEXT("ThemeSubsystem: Applied theme %s to mesh %s"),
		*ThemeData->DisplayName.ToString(), *MeshComponent->GetName());
}

void UThemeSubsystem::RegisterThemedMesh(UStaticMeshComponent* MeshComponent)
{
	if (!MeshComponent)
	{
		return;
	}

	// Check if already registered
	for (const TWeakObjectPtr<UStaticMeshComponent>& Weak : RegisteredMeshes)
	{
		if (Weak.Get() == MeshComponent)
		{
			return; // Already registered
		}
	}

	RegisteredMeshes.Add(MeshComponent);

	// Apply current theme immediately
	ApplyThemeToMesh(MeshComponent);

	UE_LOG(LogStateRunner_Arcade, Verbose, TEXT("ThemeSubsystem: Registered mesh %s (total: %d)"),
		*MeshComponent->GetName(), RegisteredMeshes.Num());
}

void UThemeSubsystem::UnregisterThemedMesh(UStaticMeshComponent* MeshComponent)
{
	if (!MeshComponent)
	{
		return;
	}

	for (int32 i = RegisteredMeshes.Num() - 1; i >= 0; --i)
	{
		if (RegisteredMeshes[i].Get() == MeshComponent)
		{
			RegisteredMeshes.RemoveAt(i);
			UE_LOG(LogStateRunner_Arcade, Verbose, TEXT("ThemeSubsystem: Unregistered mesh %s"), *MeshComponent->GetName());
			return;
		}
	}
}

void UThemeSubsystem::RefreshAllThemedMeshes()
{
	// Clean up any invalid references first
	CleanupInvalidMeshReferences();

	int32 RefreshedCount = 0;
	for (const TWeakObjectPtr<UStaticMeshComponent>& Weak : RegisteredMeshes)
	{
		if (UStaticMeshComponent* Mesh = Weak.Get())
		{
			ApplyThemeToMesh(Mesh);
			RefreshedCount++;
		}
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("ThemeSubsystem: Refreshed %d themed meshes"), RefreshedCount);
}

//=============================================================================
// PERSISTENCE
//=============================================================================

void UThemeSubsystem::SaveThemePreference()
{
	// Save to GameUserSettings (persists across sessions)
	if (GConfig)
	{
		GConfig->SetInt(*ThemeConfigSection, *ThemeConfigKey, static_cast<int32>(CurrentThemeType), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);

		UE_LOG(LogStateRunner_Arcade, Log, TEXT("ThemeSubsystem: Saved theme preference %d"), static_cast<int32>(CurrentThemeType));
	}
}

void UThemeSubsystem::LoadThemePreference()
{
	// Load from GameUserSettings
	if (GConfig)
	{
		int32 SavedTheme = 0;
		if (GConfig->GetInt(*ThemeConfigSection, *ThemeConfigKey, SavedTheme, GGameUserSettingsIni))
		{
			// Validate the loaded value
			if (SavedTheme >= 0 && SavedTheme <= static_cast<int32>(EThemeType::Gold))
			{
				CurrentThemeType = static_cast<EThemeType>(SavedTheme);
				UE_LOG(LogStateRunner_Arcade, Log, TEXT("ThemeSubsystem: Loaded theme preference %d"), SavedTheme);
			}
			else
			{
				UE_LOG(LogStateRunner_Arcade, Warning, TEXT("ThemeSubsystem: Invalid saved theme %d, using default"), SavedTheme);
			}
		}
		else
		{
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("ThemeSubsystem: No saved theme preference, using default Cryogenic (Cyan)"));
		}
	}
}

//=============================================================================
// INTERNAL HELPERS
//=============================================================================

void UThemeSubsystem::ApplyCircuitPatternTheme(UStaticMeshComponent* MeshComponent, const UThemeDataAsset* ThemeData)
{
	if (!MeshComponent || !ThemeData)
	{
		return;
	}

	// Calculate the final emissive color with intensity
	FLinearColor FinalEmissiveColor = ThemeData->CircuitEmissiveColor * ThemeData->CircuitEmissiveIntensity;

	// Apply to elements 0-255 (circuit patterns)
	for (int32 ElementIndex = CIRCUIT_PATTERN_START; ElementIndex <= CIRCUIT_PATTERN_END; ++ElementIndex)
	{
		// Get the existing material
		UMaterialInterface* BaseMaterial = MeshComponent->GetMaterial(ElementIndex);
		if (!BaseMaterial)
		{
			continue;
		}

		// Create or get dynamic material instance
		UMaterialInstanceDynamic* DynamicMat = Cast<UMaterialInstanceDynamic>(BaseMaterial);
		if (!DynamicMat)
		{
			// Create dynamic instance from the base material
			DynamicMat = MeshComponent->CreateDynamicMaterialInstance(ElementIndex, BaseMaterial);
		}

		if (DynamicMat)
		{
			// Set the emissive color parameter
			// Common parameter names: "EmissiveColor", "Emissive Color", "EmissiveTint"
			DynamicMat->SetVectorParameterValue(FName("EmissiveColor"), FinalEmissiveColor);
			
			// Also try alternate parameter names in case material uses different naming
			DynamicMat->SetVectorParameterValue(FName("Emissive Color"), FinalEmissiveColor);
			DynamicMat->SetVectorParameterValue(FName("EmissiveTint"), FinalEmissiveColor);
		}
	}
}

void UThemeSubsystem::ApplyLaserPointerTheme(UStaticMeshComponent* MeshComponent, const UThemeDataAsset* ThemeData)
{
	if (!MeshComponent || !ThemeData || !ThemeData->LaserPointerMaterial)
	{
		return;
	}

	// Apply laser material to first range (2-5)
	for (int32 ElementIndex = LASER_RANGE1_START; ElementIndex <= LASER_RANGE1_END; ++ElementIndex)
	{
		MeshComponent->SetMaterial(ElementIndex, ThemeData->LaserPointerMaterial);
	}

	// Apply laser material to second range (9-16), skipping floor elements (8 and 17 are outside this range)
	for (int32 ElementIndex = LASER_RANGE2_START; ElementIndex <= LASER_RANGE2_END; ++ElementIndex)
	{
		MeshComponent->SetMaterial(ElementIndex, ThemeData->LaserPointerMaterial);
	}
}

void UThemeSubsystem::CleanupInvalidMeshReferences()
{
	for (int32 i = RegisteredMeshes.Num() - 1; i >= 0; --i)
	{
		if (!RegisteredMeshes[i].IsValid())
		{
			RegisteredMeshes.RemoveAt(i);
		}
	}
}
