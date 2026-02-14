#pragma once

#include "CoreMinimal.h"
#include "ArcadeMenuWidget.h"
#include "ControlsDisplayWidget.generated.h"

class UTextBlock;
class UImage;
class UVerticalBox;
class UButton;

//=============================================================================
// CONTROL TYPE ENUM (must be at global scope)
//=============================================================================

/**
 * Enum for the different control input types that can be displayed.
 * Order: Gamepad (default), Keyboard, Arcade
 */
UENUM(BlueprintType)
enum class EControlInputType : uint8
{
	Gamepad		UMETA(DisplayName = "Gamepad"),
	Keyboard	UMETA(DisplayName = "Keyboard"),
	Arcade		UMETA(DisplayName = "Arcade")
};

//=============================================================================
// CONTROL MAPPING STRUCT (must be at global scope)
//=============================================================================

/**
 * Struct representing a single control mapping for display.
 */
USTRUCT(BlueprintType)
struct FControlMapping
{
	GENERATED_BODY()

	/** Action name (e.g., "Move Left/Right") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control")
	FText ActionName;

	/** Input description (e.g., "LEFT/RIGHT ARROWS") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control")
	FText InputDescription;

	/** Optional icon/image for the input */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control")
	TObjectPtr<UTexture2D> InputIcon = nullptr;
};

//=============================================================================
// CONTROLS DISPLAY WIDGET CLASS
//=============================================================================

/**
 * Controls Display Widget
 * 
 * Displays game controls with a selector to switch between Arcade, Keyboard, and Gamepad.
 * Each control type has its own image showing the control layout.
 * 
 * Layout:
 * - Title: "CONTROLS"
 * - Control Type Selector: "< ARCADE >" (cycles with Left/Right)
 * - Background Image (shared)
 * - Control Image (swaps based on selected type)
 * - Back button
 * 
 * Navigation:
 * - Arrow Up/Down: Navigate between selector and back button
 * - Arrow Left/Right: Cycle control type (Arcade/Keyboard/Gamepad)
 * - Enter on Back or Escape: Close popup
 */
UCLASS(Abstract, Blueprintable)
class STATERUNNER_ARCADE_API UControlsDisplayWidget : public UArcadeMenuWidget
{
	GENERATED_BODY()

public:

	UControlsDisplayWidget(const FObjectInitializer& ObjectInitializer);

protected:

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//=============================================================================
	// WIDGET REFERENCES
	//=============================================================================

protected:

	/** Title text (e.g., "CONTROLS") */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Display")
	TObjectPtr<UTextBlock> TitleText;

	/** Control type selector button (focusable anchor) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Selector")
	TObjectPtr<UButton> ControlTypeButton;

	/** Control type selector value text (e.g., "< ARCADE >") */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Selector")
	TObjectPtr<UTextBlock> ControlTypeValue;

	/** Optional label for the selector */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Selector")
	TObjectPtr<UTextBlock> ControlTypeLabel;

	/** Background image (shown behind all control images) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Images")
	TObjectPtr<UImage> BackgroundImage;

	/** Arcade controls image */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Images")
	TObjectPtr<UImage> ArcadeControlsImage;

	/** Keyboard controls image */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Images")
	TObjectPtr<UImage> KeyboardControlsImage;

	/** Gamepad controls image */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Images")
	TObjectPtr<UImage> GamepadControlsImage;

	/** Back button to close the popup */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidget), Category="Buttons")
	TObjectPtr<UButton> BackButton;

	/** Optional text label for Back button */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> BackLabel;

	//=============================================================================
	// MENU ITEM INDICES
	//=============================================================================

public:

	/** Index for Control Type selector in focusable items array */
	static const int32 INDEX_CONTROL_TYPE = 0;

	/** Index for Back button in focusable items array */
	static const int32 INDEX_BACK = 1;

	//=============================================================================
	// RUNTIME STATE
	//=============================================================================

protected:

	/** Current control type being displayed */
	UPROPERTY(BlueprintReadOnly, Category="State")
	EControlInputType CurrentControlType = EControlInputType::Gamepad;

	/** Current control type index (for cycling) */
	UPROPERTY(BlueprintReadOnly, Category="State")
	int32 CurrentControlTypeIndex = 0;

	/** Control type names for display */
	TArray<FString> ControlTypeNames;

	//=============================================================================
	// CONTROL DISPLAY DATA (legacy, kept for compatibility)
	//=============================================================================

protected:

	/**
	 * Array of control mappings to display.
	 * Can be set in Blueprint defaults or populated at runtime.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Controls")
	TArray<FControlMapping> ControlMappings;

	//=============================================================================
	// PUBLIC FUNCTIONS
	//=============================================================================

public:

	/**
	 * Get the currently selected control type.
	 */
	UFUNCTION(BlueprintPure, Category="Controls")
	EControlInputType GetCurrentControlType() const { return CurrentControlType; }

	/**
	 * Set the control type and update the display.
	 */
	UFUNCTION(BlueprintCallable, Category="Controls")
	void SetControlType(EControlInputType NewType);

	/**
	 * Cycle to the next/previous control type.
	 * @param Direction +1 for next, -1 for previous
	 */
	UFUNCTION(BlueprintCallable, Category="Controls")
	void CycleControlType(int32 Direction);

	//=============================================================================
	// BLUEPRINT EVENTS
	//=============================================================================

public:

	/**
	 * Called when the control type changes.
	 * Use this in Blueprint to trigger sounds or animations.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnControlTypeChanged(EControlInputType NewType);

	/**
	 * Called when the widget is constructed.
	 * Use this in Blueprint to populate control display content.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnPopulateControls();

	//=============================================================================
	// INTERNAL FUNCTIONS
	//=============================================================================

protected:

	/** Initialize control type names array */
	void InitializeControlTypeNames();

	/** Update the control type selector display text */
	void UpdateControlTypeDisplay();

	/** Update which control image is visible */
	void UpdateControlImageVisibility();

	/** Called when Back button is clicked */
	UFUNCTION()
	void OnBackClicked();

	/** Called when Control Type selector button is clicked (cycles forward) */
	UFUNCTION()
	void OnControlTypeClicked();

	//=============================================================================
	// OVERRIDES
	//=============================================================================

protected:

	virtual void OnSelectorOptionChanged_Implementation(int32 ItemIndex, int32 Delta) override;
	virtual void OnItemSelected_Implementation(int32 ItemIndex) override;
	virtual void OnBackAction_Implementation() override;
};
