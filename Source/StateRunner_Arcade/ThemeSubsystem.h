#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ThemeDataAsset.h"
#include "ThemeSubsystem.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Delegate broadcast when the theme changes.
 * Subscribers can use this to update their visuals.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThemeChanged, EThemeType, NewTheme);

/**
 * Game Instance Subsystem that manages visual themes (colors, materials).
 * Persists theme selection across level loads via GameUserSettings.
 * Apply themes to track meshes and broadcast changes to subscribers.
 */
UCLASS()
class STATERUNNER_ARCADE_API UThemeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	// --- Subsystem Lifecycle ---

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Theme Management ---

	/**
	 * Get the currently selected theme type.
	 */
	UFUNCTION(BlueprintCallable, Category="Theme")
	EThemeType GetCurrentThemeType() const { return CurrentThemeType; }

	/**
	 * Get the current theme data asset.
	 * Returns nullptr if theme assets are not configured.
	 */
	UFUNCTION(BlueprintCallable, Category="Theme")
	UThemeDataAsset* GetCurrentThemeData() const;

	/**
	 * Set the current theme by type.
	 * Saves the selection and broadcasts OnThemeChanged.
	 * 
	 * @param NewTheme The theme type to activate
	 * @param bApplyImmediately If true, refreshes all registered meshes
	 */
	UFUNCTION(BlueprintCallable, Category="Theme")
	void SetCurrentTheme(EThemeType NewTheme, bool bApplyImmediately = true);

	/**
	 * Get the theme data asset for a specific theme type.
	 */
	UFUNCTION(BlueprintCallable, Category="Theme")
	UThemeDataAsset* GetThemeData(EThemeType ThemeType) const;

	/**
	 * Get all available theme types.
	 */
	UFUNCTION(BlueprintCallable, Category="Theme")
	TArray<EThemeType> GetAvailableThemes() const;

	// --- Mesh Application ---

	/**
	 * Apply the current theme to a static mesh component.
	 * Creates dynamic material instances for circuit patterns and swaps laser materials.
	 * 
	 * @param MeshComponent The SM_NewTunnel1 mesh to theme
	 */
	UFUNCTION(BlueprintCallable, Category="Theme")
	void ApplyThemeToMesh(UStaticMeshComponent* MeshComponent);

	/**
	 * Register a mesh component to receive automatic theme updates.
	 * Registered meshes are updated when SetCurrentTheme is called with bApplyImmediately=true.
	 * 
	 * @param MeshComponent The mesh to register
	 */
	UFUNCTION(BlueprintCallable, Category="Theme")
	void RegisterThemedMesh(UStaticMeshComponent* MeshComponent);

	/**
	 * Unregister a mesh component from automatic theme updates.
	 * Call this when the mesh is being destroyed.
	 * 
	 * @param MeshComponent The mesh to unregister
	 */
	UFUNCTION(BlueprintCallable, Category="Theme")
	void UnregisterThemedMesh(UStaticMeshComponent* MeshComponent);

	/**
	 * Refresh all registered meshes with the current theme.
	 * Useful after loading a level or changing themes.
	 */
	UFUNCTION(BlueprintCallable, Category="Theme")
	void RefreshAllThemedMeshes();

	// --- Events ---

	/**
	 * Broadcast when the theme changes.
	 * UI and gameplay systems can subscribe to update their visuals.
	 */
	UPROPERTY(BlueprintAssignable, Category="Theme")
	FOnThemeChanged OnThemeChanged;

	// --- Theme Asset Configuration ---

	/**
	 * Map of theme types to their data assets.
	 * These are auto-loaded from the ThemeAssetPaths or can be set manually.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Theme Configuration")
	TMap<EThemeType, TObjectPtr<UThemeDataAsset>> ThemeAssets;

	/**
	 * Paths to theme data assets (for auto-loading).
	 * Format: Full asset path like "/Game/_DEVELOPER/Data/Themes/DA_Theme_Cyan.DA_Theme_Cyan"
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Theme Configuration")
	TMap<EThemeType, FSoftObjectPath> ThemeAssetPaths;

	/**
	 * Manually register a theme data asset at runtime.
	 * Call this from Blueprint if not using auto-load paths.
	 */
	UFUNCTION(BlueprintCallable, Category="Theme")
	void RegisterThemeAsset(EThemeType ThemeType, UThemeDataAsset* ThemeData);

	/**
	 * Load theme assets from the configured paths.
	 * Called automatically on Initialize, but can be called manually if paths change.
	 */
	UFUNCTION(BlueprintCallable, Category="Theme")
	void LoadThemeAssets();

protected:

	// --- Internal State ---

	/** Currently active theme type */
	UPROPERTY()
	EThemeType CurrentThemeType = EThemeType::Cyan;

	/** Registered mesh components that receive automatic theme updates */
	UPROPERTY()
	TArray<TWeakObjectPtr<UStaticMeshComponent>> RegisteredMeshes;

	/** Config section name for saving theme preference */
	static const FString ThemeConfigSection;
	static const FString ThemeConfigKey;

	// --- Internal Helpers ---

	/** Save theme preference to config */
	void SaveThemePreference();

	/** Load theme preference from config */
	void LoadThemePreference();

	/** Apply theme colors to circuit pattern elements (0-255) */
	void ApplyCircuitPatternTheme(UStaticMeshComponent* MeshComponent, const UThemeDataAsset* ThemeData);

	/** Swap laser pointer materials (258-265, 269-272) */
	void ApplyLaserPointerTheme(UStaticMeshComponent* MeshComponent, const UThemeDataAsset* ThemeData);

	/** Clean up invalid weak references in RegisteredMeshes */
	void CleanupInvalidMeshReferences();

	// --- Element Index Constants (SM_NewTunnel1) ---

public:

	/** First circuit pattern element index */
	static const int32 CIRCUIT_PATTERN_START = 18;
	
	/** Last circuit pattern element index */
	static const int32 CIRCUIT_PATTERN_END = 272;

	/** Floor element indices (NOT themed) - these are skipped */
	static const int32 FLOOR_DARKER_1 = 6;
	static const int32 FLOOR_DARKER_2 = 7;
	static const int32 FLOOR_DARK_1 = 8;
	static const int32 FLOOR_DARK_2 = 17;

	/** Laser pointer element ranges */
	static const int32 LASER_RANGE1_START = 2;
	static const int32 LASER_RANGE1_END = 5;
	static const int32 LASER_RANGE2_START = 9;
	static const int32 LASER_RANGE2_END = 16;
};
