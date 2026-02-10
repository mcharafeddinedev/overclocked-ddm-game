#pragma once

#include "CoreMinimal.h"
#include "ArcadeMenuWidget.h"
#include "PopupPanelWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * Popup Panel Widget Base Class
 * 
 * Simple popup overlay with a single Back button.
 * Used for Controls, Credits, and similar simple display panels.
 * 
 * Navigation: Escape or Enter on Back button to close
 * Requires: BackButton in Blueprint
 */
UCLASS(Abstract, Blueprintable)
class STATERUNNER_ARCADE_API UPopupPanelWidget : public UArcadeMenuWidget
{
	GENERATED_BODY()

public:

	UPopupPanelWidget(const FObjectInitializer& ObjectInitializer);

protected:

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//=============================================================================
	// BUTTON REFERENCES
	//=============================================================================

protected:

	/**
	 * Back button to close the popup.
	 */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidget), Category="Buttons")
	TObjectPtr<UButton> BackButton;

	/**
	 * Optional text label for Back button.
	 */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Labels")
	TObjectPtr<UTextBlock> BackLabel;

	//=============================================================================
	// MENU ITEM INDICES
	//=============================================================================

public:

	/** Index for Back button in focusable items array */
	static const int32 INDEX_BACK = 0;

	//=============================================================================
	// BUTTON CLICK HANDLERS
	//=============================================================================

protected:

	/** Called when Back button is clicked */
	UFUNCTION()
	void OnBackClicked();

	//=============================================================================
	// OVERRIDES
	//=============================================================================

protected:

	virtual void OnItemSelected_Implementation(int32 ItemIndex) override;
	virtual void OnBackAction_Implementation() override;
};
