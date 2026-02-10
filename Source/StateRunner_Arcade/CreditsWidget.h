#pragma once

#include "CoreMinimal.h"
#include "PopupPanelWidget.h"
#include "CreditsWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class URichTextBlock;

//=============================================================================
// CREDITS ENTRY STRUCT (must be at global scope)
//=============================================================================

/**
 * Struct representing a credits entry.
 */
USTRUCT(BlueprintType)
struct FCreditsEntry
{
	GENERATED_BODY()

	/** Role/category (e.g., "Developer", "Audio", "Special Thanks") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Credits")
	FText Role;

	/** Name(s) for this role */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Credits")
	FText Name;

	/** Whether this is a section header (larger text, no name) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Credits")
	bool bIsSectionHeader = false;
};

//=============================================================================
// CREDITS WIDGET CLASS
//=============================================================================

/**
 * Credits Widget
 * 
 * Displays game credits in a popup panel.
 * Simple display showing developer credits, acknowledgments, etc.
 * 
 * Layout suggestion for Blueprint:
 * - Title: "CREDITS"
 * - Scrollable credits content
 * - Back button at bottom
 * 
 * Navigation: Escape or Enter on Back button to close
 */
UCLASS(Abstract, Blueprintable)
class STATERUNNER_ARCADE_API UCreditsWidget : public UPopupPanelWidget
{
	GENERATED_BODY()

public:

	UCreditsWidget(const FObjectInitializer& ObjectInitializer);

protected:

	virtual void NativeConstruct() override;

	//=============================================================================
	// OPTIONAL WIDGET REFERENCES
	// These are optional - design the layout in Blueprint
	//=============================================================================

protected:

	/** Title text (e.g., "CREDITS") */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Display")
	TObjectPtr<UTextBlock> TitleText;

	/** Container for credits content */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Display")
	TObjectPtr<UVerticalBox> CreditsContainer;

	/** Main credits text block (if using single text approach) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Display")
	TObjectPtr<UTextBlock> CreditsText;

	/** Rich text block for styled credits (if using rich text approach) */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidgetOptional), Category="Display")
	TObjectPtr<URichTextBlock> CreditsRichText;

	//=============================================================================
	// CREDITS DATA
	//=============================================================================

protected:

	/**
	 * Array of credits entries to display.
	 * Can be set in Blueprint defaults or populated at runtime.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Credits")
	TArray<FCreditsEntry> CreditsEntries;

	/**
	 * Game title to display at top of credits.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Credits")
	FText GameTitle = FText::FromString(TEXT("STATE RUNNER: ARCADE"));

	/**
	 * Copyright/legal text to display at bottom.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Credits")
	FText CopyrightText = FText::FromString(TEXT("(C) 2026"));

	//=============================================================================
	// BLUEPRINT IMPLEMENTABLE EVENTS
	//=============================================================================

public:

	/**
	 * Called when the widget is constructed.
	 * Use this in Blueprint to populate credits display content.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Events")
	void OnPopulateCredits();

	//=============================================================================
	// PUBLIC FUNCTIONS
	//=============================================================================

public:

	/**
	 * Get formatted credits text (combines all entries).
	 * Useful for simple text block display.
	 */
	UFUNCTION(BlueprintPure, Category="Credits")
	FText GetFormattedCreditsText() const;
};
