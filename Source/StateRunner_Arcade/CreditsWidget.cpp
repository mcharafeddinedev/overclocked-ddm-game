#include "CreditsWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/RichTextBlock.h"
#include "StateRunner_Arcade.h"

UCreditsWidget::UCreditsWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Pre-populate default credits for StateRunner Arcade
	// These can be overridden in Blueprint defaults
	
	// Game title section
	FCreditsEntry TitleEntry;
	TitleEntry.Role = FText::FromString(TEXT("STATE RUNNER: ARCADE"));
	TitleEntry.bIsSectionHeader = true;
	CreditsEntries.Add(TitleEntry);

	// Development section header
	FCreditsEntry DevHeader;
	DevHeader.Role = FText::FromString(TEXT("DEVELOPMENT"));
	DevHeader.bIsSectionHeader = true;
	CreditsEntries.Add(DevHeader);

	// Developer placeholder - should be set in Blueprint
	FCreditsEntry DevEntry;
	DevEntry.Role = FText::FromString(TEXT("Developer"));
	DevEntry.Name = FText::FromString(TEXT("Your Name Here"));
	CreditsEntries.Add(DevEntry);

	// Tools section header
	FCreditsEntry ToolsHeader;
	ToolsHeader.Role = FText::FromString(TEXT("TOOLS"));
	ToolsHeader.bIsSectionHeader = true;
	CreditsEntries.Add(ToolsHeader);

	FCreditsEntry EngineEntry;
	EngineEntry.Role = FText::FromString(TEXT("Engine"));
	EngineEntry.Name = FText::FromString(TEXT("Unreal Engine 5"));
	CreditsEntries.Add(EngineEntry);

	// Special thanks section header
	FCreditsEntry ThanksHeader;
	ThanksHeader.Role = FText::FromString(TEXT("SPECIAL THANKS"));
	ThanksHeader.bIsSectionHeader = true;
	CreditsEntries.Add(ThanksHeader);

	FCreditsEntry ThanksEntry;
	ThanksEntry.Role = FText::FromString(TEXT(""));
	ThanksEntry.Name = FText::FromString(TEXT("HCC Game Development Program"));
	CreditsEntries.Add(ThanksEntry);
}

void UCreditsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// If CreditsText is bound and not empty, populate it with formatted credits
	if (CreditsText && !CreditsText->GetText().IsEmpty())
	{
		// Text already set in Blueprint, leave it alone
	}
	else if (CreditsText)
	{
		// Populate with our formatted credits
		CreditsText->SetText(GetFormattedCreditsText());
	}

	// Allow Blueprint to populate/customize credits display
	OnPopulateCredits();

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("CreditsWidget: Constructed with %d credits entries"), CreditsEntries.Num());
}

FText UCreditsWidget::GetFormattedCreditsText() const
{
	FString FormattedText;

	for (const FCreditsEntry& Entry : CreditsEntries)
	{
		if (Entry.bIsSectionHeader)
		{
			// Section header - just the role/title, centered, with extra spacing
			if (!FormattedText.IsEmpty())
			{
				FormattedText += TEXT("\n\n");
			}
			FormattedText += Entry.Role.ToString();
			FormattedText += TEXT("\n");
		}
		else
		{
			// Regular entry - role: name
			if (!Entry.Role.IsEmptyOrWhitespace())
			{
				FormattedText += Entry.Role.ToString();
				FormattedText += TEXT(": ");
			}
			FormattedText += Entry.Name.ToString();
			FormattedText += TEXT("\n");
		}
	}

	// Add copyright at bottom
	if (!CopyrightText.IsEmptyOrWhitespace())
	{
		FormattedText += TEXT("\n\n");
		FormattedText += CopyrightText.ToString();
	}

	return FText::FromString(FormattedText);
}
