#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ThemeDataAsset.generated.h"

/**
 * Theme Type Enum
 * Identifies the preset themes available in the game.
 */
UENUM(BlueprintType)
enum class EThemeType : uint8
{
	Cyan        UMETA(DisplayName = "Cyan"),
	Orange      UMETA(DisplayName = "Orange"),
	PurplePink  UMETA(DisplayName = "Purple-Pink"),
	Green       UMETA(DisplayName = "Green"),
	Ruby        UMETA(DisplayName = "Ruby"),
	Gold        UMETA(DisplayName = "Gold")
};

/**
 * Theme Data Asset
 * 
 * Defines a visual theme for the track environment.
 * Contains:
 * - Emissive color for circuit patterns (MI_CircuitPattern elements 0-255)
 * - Laser pointer material to swap (LaserPointerMaterial_ elements 258-265, 269-272)
 * - Theme metadata (name, icon)
 * 
 * Floor materials (elements 257, 266, 267, 268) are NOT affected by themes.
 * 
 * Usage:
 * 1. Create ThemeDataAsset in Content Browser (right-click → Miscellaneous → Data Asset → ThemeDataAsset)
 * 2. Configure emissive color and laser material
 * 3. Register with ThemeSubsystem or reference in theme selector
 */
UCLASS(BlueprintType)
class STATERUNNER_ARCADE_API UThemeDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	//=============================================================================
	// THEME IDENTIFICATION
	//=============================================================================

	/**
	 * Theme type identifier.
	 * Used for persistence and quick lookup.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Theme")
	EThemeType ThemeType = EThemeType::Cyan;

	/**
	 * Display name for the theme.
	 * Shown in theme selector UI.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Theme")
	FText DisplayName;

	/**
	 * Optional icon for theme selector.
	 * Can be a small preview image or color swatch.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Theme")
	TObjectPtr<UTexture2D> ThemeIcon;

	//=============================================================================
	// CIRCUIT PATTERN SETTINGS (Elements 0-255: MI_CircuitPattern)
	//=============================================================================

	/**
	 * Emissive color for circuit patterns.
	 * Applied to the "EmissiveColor" parameter on MI_CircuitPattern materials.
	 * This affects elements 0-255 on the tunnel mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Circuit Pattern")
	FLinearColor CircuitEmissiveColor = FLinearColor(0.0f, 1.0f, 1.0f, 1.0f); // Default cyan

	/**
	 * Emissive intensity multiplier for circuit patterns.
	 * Higher values create a stronger glow effect.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Circuit Pattern", meta=(ClampMin="0.1", ClampMax="100.0"))
	float CircuitEmissiveIntensity = 5.0f;

	//=============================================================================
	// LASER POINTER SETTINGS (Elements 258-265, 269-272)
	//=============================================================================

	/**
	 * Laser pointer material to use for this theme.
	 * Should be a LaserPointerMaterial_[COLOR] variant.
	 * This material will be swapped onto elements 258-265 and 269-272.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Laser Pointer")
	TObjectPtr<UMaterialInterface> LaserPointerMaterial;

	//=============================================================================
	// OPTIONAL: ADDITIONAL ACCENT COLORS
	//=============================================================================

	/**
	 * Primary accent color for UI elements matching the theme.
	 * Can be used for HUD accents, particle effects, etc.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Accents")
	FLinearColor PrimaryAccentColor = FLinearColor(0.0f, 1.0f, 1.0f, 1.0f);

	/**
	 * Secondary accent color (for gradients or highlights).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Accents")
	FLinearColor SecondaryAccentColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	//=============================================================================
	// DATA ASSET OVERRIDES
	//=============================================================================

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("ThemeDataAsset"), GetFName());
	}
};
