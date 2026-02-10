#include "ThemeSelectorWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "ThemeSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "StateRunner_Arcade.h"

UThemeSelectorWidget::UThemeSelectorWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Theme selector uses vertical navigation
	bWrapNavigation = true;
	bLeftRightNavigatesMenu = false;
}

void UThemeSelectorWidget::NativeConstruct()
{
	// Get theme subsystem
	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(this))
	{
		ThemeSubsystem = GI->GetSubsystem<UThemeSubsystem>();
	}

	// Register theme buttons BEFORE calling Super
	if (ThemeButton_Cyan)
	{
		RegisterButton(ThemeButton_Cyan, CyanLabel);
		ThemeButton_Cyan->OnClicked.AddDynamic(this, &UThemeSelectorWidget::OnCyanClicked);
	}

	if (ThemeButton_Orange)
	{
		RegisterButton(ThemeButton_Orange, OrangeLabel);
		ThemeButton_Orange->OnClicked.AddDynamic(this, &UThemeSelectorWidget::OnOrangeClicked);
	}

	if (ThemeButton_PurplePink)
	{
		RegisterButton(ThemeButton_PurplePink, PurplePinkLabel);
		ThemeButton_PurplePink->OnClicked.AddDynamic(this, &UThemeSelectorWidget::OnPurplePinkClicked);
	}

	if (ThemeButton_Green)
	{
		RegisterButton(ThemeButton_Green, GreenLabel);
		ThemeButton_Green->OnClicked.AddDynamic(this, &UThemeSelectorWidget::OnGreenClicked);
	}

	if (ThemeButton_Ruby)
	{
		RegisterButton(ThemeButton_Ruby, RubyLabel);
		ThemeButton_Ruby->OnClicked.AddDynamic(this, &UThemeSelectorWidget::OnRubyClicked);
	}

	if (ThemeButton_Gold)
	{
		RegisterButton(ThemeButton_Gold, GoldLabel);
		ThemeButton_Gold->OnClicked.AddDynamic(this, &UThemeSelectorWidget::OnGoldClicked);
	}

	// Set default focus to current theme
	if (ThemeSubsystem)
	{
		DefaultFocusIndex = GetIndexForTheme(ThemeSubsystem->GetCurrentThemeType());
	}

	// Call parent (this sets initial focus)
	Super::NativeConstruct();

	// Update preview for initial selection
	if (ThemeSubsystem)
	{
		UpdatePreview(ThemeSubsystem->GetCurrentThemeType());
	}

	// Show current theme in UI
	if (CurrentThemeText && ThemeSubsystem)
	{
		if (UThemeDataAsset* CurrentTheme = ThemeSubsystem->GetCurrentThemeData())
		{
			CurrentThemeText->SetText(CurrentTheme->DisplayName);
		}
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("ThemeSelectorWidget: Constructed with %d theme options"), GetFocusableItemCount());
}

void UThemeSelectorWidget::NativeDestruct()
{
	// Unbind button delegates
	if (ThemeButton_Cyan)
	{
		ThemeButton_Cyan->OnClicked.RemoveDynamic(this, &UThemeSelectorWidget::OnCyanClicked);
	}
	if (ThemeButton_Orange)
	{
		ThemeButton_Orange->OnClicked.RemoveDynamic(this, &UThemeSelectorWidget::OnOrangeClicked);
	}
	if (ThemeButton_PurplePink)
	{
		ThemeButton_PurplePink->OnClicked.RemoveDynamic(this, &UThemeSelectorWidget::OnPurplePinkClicked);
	}
	if (ThemeButton_Green)
	{
		ThemeButton_Green->OnClicked.RemoveDynamic(this, &UThemeSelectorWidget::OnGreenClicked);
	}
	if (ThemeButton_Ruby)
	{
		ThemeButton_Ruby->OnClicked.RemoveDynamic(this, &UThemeSelectorWidget::OnRubyClicked);
	}
	if (ThemeButton_Gold)
	{
		ThemeButton_Gold->OnClicked.RemoveDynamic(this, &UThemeSelectorWidget::OnGoldClicked);
	}

	Super::NativeDestruct();
}

//=============================================================================
// BUTTON CLICK HANDLERS
//=============================================================================

void UThemeSelectorWidget::OnCyanClicked()
{
	SelectTheme(EThemeType::Cyan);
}

void UThemeSelectorWidget::OnOrangeClicked()
{
	SelectTheme(EThemeType::Orange);
}

void UThemeSelectorWidget::OnPurplePinkClicked()
{
	SelectTheme(EThemeType::PurplePink);
}

void UThemeSelectorWidget::OnGreenClicked()
{
	SelectTheme(EThemeType::Green);
}

void UThemeSelectorWidget::OnRubyClicked()
{
	SelectTheme(EThemeType::Ruby);
}

void UThemeSelectorWidget::OnGoldClicked()
{
	SelectTheme(EThemeType::Gold);
}

//=============================================================================
// THEME SELECTION
//=============================================================================

void UThemeSelectorWidget::SelectTheme(EThemeType ThemeType)
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("ThemeSelectorWidget: Selected theme %d"), static_cast<int32>(ThemeType));

	// Apply theme via subsystem
	FString ThemeName;
	if (ThemeSubsystem)
	{
		ThemeSubsystem->SetCurrentTheme(ThemeType, true);
		
		// Get the theme display name for notification
		if (UThemeDataAsset* ThemeData = ThemeSubsystem->GetThemeData(ThemeType))
		{
			ThemeName = ThemeData->DisplayName.ToString();
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("ThemeSelectorWidget: Got theme name: %s"), *ThemeName);
		}
		else
		{
			UE_LOG(LogStateRunner_Arcade, Warning, TEXT("ThemeSelectorWidget: Failed to get theme data for type %d"), static_cast<int32>(ThemeType));
		}
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("ThemeSelectorWidget: No ThemeSubsystem available!"));
	}

	// Notify Blueprint
	OnThemeSelected(ThemeType);

	// Broadcast theme applied with name (for main menu notification)
	if (!ThemeName.IsEmpty())
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("ThemeSelectorWidget: Broadcasting OnThemeApplied with name: %s"), *ThemeName);
		OnThemeApplied.Broadcast(ThemeName);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("ThemeSelectorWidget: ThemeName is empty, not broadcasting OnThemeApplied"));
	}

	// Broadcast back pressed to close this widget
	OnBackPressed.Broadcast();
}

void UThemeSelectorWidget::UpdatePreview(EThemeType ThemeType)
{
	if (!ThemeSubsystem)
	{
		return;
	}

	UThemeDataAsset* ThemeData = ThemeSubsystem->GetThemeData(ThemeType);
	if (!ThemeData)
	{
		return;
	}

	// Update preview image color
	if (ThemePreviewImage)
	{
		ThemePreviewImage->SetColorAndOpacity(ThemeData->CircuitEmissiveColor);
	}

	// Update theme name text
	if (CurrentThemeText)
	{
		CurrentThemeText->SetText(ThemeData->DisplayName);
	}
}

EThemeType UThemeSelectorWidget::GetThemeForIndex(int32 Index) const
{
	switch (Index)
	{
		case INDEX_CYAN:        return EThemeType::Cyan;
		case INDEX_ORANGE:      return EThemeType::Orange;
		case INDEX_PURPLE_PINK: return EThemeType::PurplePink;
		case INDEX_GREEN:       return EThemeType::Green;
		case INDEX_RUBY:        return EThemeType::Ruby;
		case INDEX_GOLD:        return EThemeType::Gold;
		default:                return EThemeType::Cyan;
	}
}

int32 UThemeSelectorWidget::GetIndexForTheme(EThemeType ThemeType) const
{
	switch (ThemeType)
	{
		case EThemeType::Cyan:       return INDEX_CYAN;
		case EThemeType::Orange:     return INDEX_ORANGE;
		case EThemeType::PurplePink: return INDEX_PURPLE_PINK;
		case EThemeType::Green:      return INDEX_GREEN;
		case EThemeType::Ruby:       return INDEX_RUBY;
		case EThemeType::Gold:       return INDEX_GOLD;
		default:                     return INDEX_CYAN;
	}
}

//=============================================================================
// OVERRIDES
//=============================================================================

void UThemeSelectorWidget::OnItemSelected_Implementation(int32 ItemIndex)
{
	// Convert index to theme type and select
	EThemeType SelectedTheme = GetThemeForIndex(ItemIndex);
	SelectTheme(SelectedTheme);
}

void UThemeSelectorWidget::OnBackAction_Implementation()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("ThemeSelectorWidget: Back pressed - cancelling"));

	// Notify Blueprint
	OnSelectionCancelled();

	// Broadcast to close widget
	OnBackPressed.Broadcast();
}

void UThemeSelectorWidget::UpdateFocusVisuals_Implementation(int32 NewFocusIndex, int32 OldFocusIndex)
{
	// Call base implementation for standard focus visuals
	Super::UpdateFocusVisuals_Implementation(NewFocusIndex, OldFocusIndex);

	// Update preview when focus changes
	if (NewFocusIndex >= 0)
	{
		EThemeType PreviewTheme = GetThemeForIndex(NewFocusIndex);
		UpdatePreview(PreviewTheme);
	}
}
