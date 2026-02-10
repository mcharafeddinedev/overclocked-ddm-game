#include "PopupPanelWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "StateRunner_Arcade.h"

UPopupPanelWidget::UPopupPanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Default focus on Back button
	DefaultFocusIndex = INDEX_BACK;
}

void UPopupPanelWidget::NativeConstruct()
{
	// Register Back button BEFORE calling Super (which sets focus)
	if (BackButton)
	{
		RegisterButton(BackButton, BackLabel);
		BackButton->OnClicked.AddDynamic(this, &UPopupPanelWidget::OnBackClicked);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("PopupPanelWidget: BackButton not bound!"));
	}

	// Call parent (this sets initial focus)
	Super::NativeConstruct();

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("PopupPanelWidget: Constructed with %d focusable items"), GetFocusableItemCount());
}

void UPopupPanelWidget::NativeDestruct()
{
	// Unbind button delegates to prevent dangling references
	if (BackButton)
	{
		BackButton->OnClicked.RemoveDynamic(this, &UPopupPanelWidget::OnBackClicked);
	}

	Super::NativeDestruct();
}

//=============================================================================
// BUTTON CLICK HANDLERS
//=============================================================================

void UPopupPanelWidget::OnBackClicked()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("PopupPanelWidget: Back button clicked"));
	
	// Broadcast back pressed event (parent will handle closing)
	OnBackPressed.Broadcast();
}

//=============================================================================
// OVERRIDES
//=============================================================================

void UPopupPanelWidget::OnItemSelected_Implementation(int32 ItemIndex)
{
	// Back button click is handled by OnClicked delegate
}

void UPopupPanelWidget::OnBackAction_Implementation()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("PopupPanelWidget: Escape pressed - closing"));
	
	// Broadcast back pressed event (parent will handle closing)
	OnBackPressed.Broadcast();
}
