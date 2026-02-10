#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThemeDataAsset.h"
#include "ThemedMeshComponent.generated.h"

class UStaticMeshComponent;
class UThemeSubsystem;

/**
 * Themed Mesh Component
 * 
 * Actor Component that automatically applies the current theme to a Static Mesh.
 * Attach this to BP_TrackSegment alongside SM_NewTunnel1 to enable theming.
 * 
 * FEATURES:
 * - Automatically registers with ThemeSubsystem on BeginPlay
 * - Applies current theme to target mesh on spawn
 * - Responds to runtime theme changes (broadcasts)
 * - Unregisters cleanly on destroy
 * 
 * SETUP IN BP_TrackSegment:
 * 1. Add this component to BP_TrackSegment
 * 2. Set TargetMeshComponent to the SM_NewTunnel1 static mesh component
 * 3. Done! Themes will be applied automatically.
 * 
 * ALTERNATIVE: If you have multiple themed meshes per actor, you can call
 * RegisterAdditionalMesh() in Blueprint's BeginPlay for each extra mesh.
 */
UCLASS(ClassGroup=(Theme), meta=(BlueprintSpawnableComponent))
class STATERUNNER_ARCADE_API UThemedMeshComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UThemedMeshComponent();

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//=============================================================================
	// CONFIGURATION
	//=============================================================================

public:

	/**
	 * The Static Mesh Component to apply themes to.
	 * Set this to the SM_NewTunnel1 mesh in Blueprint.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Theme")
	TObjectPtr<UStaticMeshComponent> TargetMeshComponent;

	/**
	 * If true, automatically finds a StaticMeshComponent on the owner.
	 * Searches for a component with "Tunnel" in the name first, then falls back to first mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Theme")
	bool bAutoFindMesh = false;

	/**
	 * Name filter for auto-find. Component name must contain this string (case-insensitive).
	 * Default: "Tunnel" to find SM_NewTunnel1.
	 * Leave empty to use the first StaticMeshComponent found.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Theme")
	FString AutoFindNameFilter = TEXT("Tunnel");

	/**
	 * If true, applies theme immediately in BeginPlay.
	 * Set to false if you want to delay theme application.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Theme")
	bool bApplyOnBeginPlay = true;

	//=============================================================================
	// PUBLIC FUNCTIONS
	//=============================================================================

public:

	/**
	 * Manually apply the current theme to the target mesh.
	 * Call this if bApplyOnBeginPlay is false or after changing TargetMeshComponent.
	 */
	UFUNCTION(BlueprintCallable, Category="Theme")
	void ApplyCurrentTheme();

	/**
	 * Register an additional mesh to receive theme updates.
	 * Use this if your actor has multiple meshes that need theming.
	 * 
	 * @param MeshComponent Additional mesh to register
	 */
	UFUNCTION(BlueprintCallable, Category="Theme")
	void RegisterAdditionalMesh(UStaticMeshComponent* MeshComponent);

	/**
	 * Get the current theme type (convenience accessor).
	 */
	UFUNCTION(BlueprintPure, Category="Theme")
	EThemeType GetCurrentThemeType() const;

	//=============================================================================
	// THEME CHANGE CALLBACK
	//=============================================================================

protected:

	/**
	 * Called when the theme changes at runtime.
	 * Automatically applies the new theme to all registered meshes.
	 */
	UFUNCTION()
	void OnThemeChanged(EThemeType NewTheme);

	//=============================================================================
	// INTERNAL STATE
	//=============================================================================

protected:

	/** Cached reference to theme subsystem */
	UPROPERTY()
	TObjectPtr<UThemeSubsystem> ThemeSubsystem;

	/** Additional meshes registered for theming */
	UPROPERTY()
	TArray<TWeakObjectPtr<UStaticMeshComponent>> AdditionalMeshes;

	/** Whether we're registered with the theme subsystem */
	bool bIsRegistered = false;
};
