#pragma once

#include "CoreMinimal.h"
#include "ArcadeMenuWidget.h"
#include "ThemeDataAsset.h"
#include "ThemeSelectorWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UThemeSubsystem;

/**
 * Delegate broadcast when a theme is successfully selected (not cancelled).
 * Parameter is the display name of the selected theme.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThemeApplied, const FString&, ThemeName);

/**
 * Theme Selector Widget
 * 
 * Popup widget for selecting visual themes from the main menu.
 * Displays available themes (Cyan, Orange, Purple-Pink, Green) with preview.
 * 
 * ARCADE NAVIGATION:
 * - Arrow Up/Down: Navigate between theme options
 * - Enter: Select theme (saves choice and closes widget)
 * - Escape: Cancel (close without saving)
 * 
 * USAGE:
 * 1. Create WBP_ThemeSelector Blueprint extending this class
 * 2. Add BindWidget components: ThemeButton_Cyan, ThemeButton_Orange, etc.
 * 3. Reference this class in MainMenuWidget's ThemeSelectorWidgetClass
 * 4. Opens when player clicks/selects "Theme" button on main menu
 */
UCLASS(Abstract, Blueprintable)
class STATERUNNER_ARCADE_API UThemeSelectorWidget : public UArcadeMenuWidget
{
	GENERATED_BODY()

public:

	UThemeSelectorWidget(const FObjectInitializer& ObjectInitializer);

protected:

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//=============================================================================
	// THEME BUTTON REFERENCES
	// Bind these in the Blueprint widget
	//=============================================================================

protected:

	/** Cyan theme button */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Theme Buttons")
	TObjectPtr<UButton> ThemeButton_Cyan;

	/** Orange theme button */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Theme Buttons")
	TObjectPtr<UButton> ThemeButton_Orange;

	/** Purple-Pink theme button */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Theme Buttons")
	TObjectPtr<UButton> ThemeButton_PurplePink;

	/** Green theme button */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Theme Buttons")
	TObjectPtr<UButton> ThemeButton_Green;

	/** Ruby theme button */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Theme Buttons")
	TObjectPtr<UButton> ThemeButton_Ruby;

	/** Gold theme button */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Theme Buttons")
	TObjectPtr<UButton> ThemeButton_Gold;

	//=============================================================================
	// OPTIONAL LABEL REFERENCES
	// For visual focus feedback
	//=============================================================================

protected:

	/** Label for Cyan theme */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> CyanLabel;

	/** Label for Orange theme */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> OrangeLabel;

	/** Label for Purple-Pink theme */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> PurplePinkLabel;

	/** Label for Green theme */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> GreenLabel;

	/** Label for Ruby theme */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> RubyLabel;

	/** Label for Gold theme */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> GoldLabel;

	//=============================================================================
	// PREVIEW ELEMENTS (Optional)
	//=============================================================================

protected:

	/**
	 * Preview image that shows the theme color.
	 * Updated when navigating between themes.
	 */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Preview")
	TObjectPtr<UImage> ThemePreviewImage;

	/**
	 * Text displaying the currently selected theme name.
	 */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Preview")
	TObjectPtr<UTextBlock> CurrentThemeText;

	//=============================================================================
	// MENU ITEM INDICES
	//=============================================================================

public:

	static const int32 INDEX_CYAN = 0;
	static const int32 INDEX_ORANGE = 1;
	static const int32 INDEX_PURPLE_PINK = 2;
	static const int32 INDEX_GREEN = 3;
	static const int32 INDEX_RUBY = 4;
	static const int32 INDEX_GOLD = 5;

	//=============================================================================
	// BUTTON CLICK HANDLERS
	//=============================================================================

protected:

	UFUNCTION()
	void OnCyanClicked();

	UFUNCTION()
	void OnOrangeClicked();

	UFUNCTION()
	void OnPurplePinkClicked();

	UFUNCTION()
	void OnGreenClicked();

	UFUNCTION()
	void OnRubyClicked();

	UFUNCTION()
	void OnGoldClicked();

	//=============================================================================
	// THEME SELECTION
	//=============================================================================

protected:

	/**
	 * Select a theme and close the selector.
	 * 
	 * @param ThemeType The theme to activate
	 */
	void SelectTheme(EThemeType ThemeType);

	/**
	 * Update the preview elements to show a theme.
	 * Called when focus changes between theme options.
	 * 
	 * @param ThemeType Theme to preview
	 */
	void UpdatePreview(EThemeType ThemeType);

	/**
	 * Get the theme type for a menu index.
	 */
	EThemeType GetThemeForIndex(int32 Index) const;

	/**
	 * Get the menu index for a theme type.
	 */
	int32 GetIndexForTheme(EThemeType ThemeType) const;

	//=============================================================================
	// CACHED REFERENCE
	//=============================================================================

protected:

	/** Cached reference to theme subsystem */
	UPROPERTY()
	TObjectPtr<UThemeSubsystem> ThemeSubsystem;

	//=============================================================================
	// OVERRIDES
	//=============================================================================

protected:

	virtual void OnItemSelected_Implementation(int32 ItemIndex) override;
	virtual void OnBackAction_Implementation() override;
	virtual void UpdateFocusVisuals_Implementation(int32 NewFocusIndex, int32 OldFocusIndex) override;

	//=============================================================================
	// EVENTS
	//=============================================================================

public:

	/**
	 * Broadcast when a theme is applied (not cancelled).
	 * Main menu can subscribe to show notification.
	 */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnThemeApplied OnThemeApplied;

	/**
	 * Called when a theme is selected (Blueprint event).
	 * Use for animations or sounds before closing.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnThemeSelected(EThemeType SelectedTheme);

	/**
	 * Called when the selector is cancelled (back pressed).
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnSelectionCancelled();
};
