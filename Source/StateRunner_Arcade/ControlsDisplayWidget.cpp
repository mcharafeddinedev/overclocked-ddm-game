#include "ControlsDisplayWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "StateRunner_Arcade.h"

UControlsDisplayWidget::UControlsDisplayWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Default focus on Back button (user can view controls, then press Enter to close)
	DefaultFocusIndex = INDEX_BACK;

	// Initialize control type names
	InitializeControlTypeNames();

	// Pre-populate default control mappings for StateRunner Arcade
	// These can be overridden in Blueprint defaults
	
	FControlMapping MoveControl;
	MoveControl.ActionName = FText::FromString(TEXT("SWITCH LANES"));
	MoveControl.InputDescription = FText::FromString(TEXT("LEFT / RIGHT"));
	ControlMappings.Add(MoveControl);

	FControlMapping JumpControl;
	JumpControl.ActionName = FText::FromString(TEXT("JUMP"));
	JumpControl.InputDescription = FText::FromString(TEXT("UP (HOLD TO JUMP HIGHER)"));
	ControlMappings.Add(JumpControl);

	FControlMapping SlideControl;
	SlideControl.ActionName = FText::FromString(TEXT("SLIDE"));
	SlideControl.InputDescription = FText::FromString(TEXT("DOWN (HOLD TO SLIDE LONGER)"));
	ControlMappings.Add(SlideControl);

	FControlMapping OverclockControl;
	OverclockControl.ActionName = FText::FromString(TEXT("OVERCLOCK"));
	OverclockControl.InputDescription = FText::FromString(TEXT("LEFT SHIFT (WHEN METER FULL)"));
	ControlMappings.Add(OverclockControl);

	FControlMapping PauseControl;
	PauseControl.ActionName = FText::FromString(TEXT("PAUSE"));
	PauseControl.InputDescription = FText::FromString(TEXT("P / TAB"));
	ControlMappings.Add(PauseControl);

	FControlMapping SelectControl;
	SelectControl.ActionName = FText::FromString(TEXT("SELECT / CONFIRM"));
	SelectControl.InputDescription = FText::FromString(TEXT("ENTER"));
	ControlMappings.Add(SelectControl);

	FControlMapping BackControl;
	BackControl.ActionName = FText::FromString(TEXT("BACK / CANCEL"));
	BackControl.InputDescription = FText::FromString(TEXT("ESCAPE"));
	ControlMappings.Add(BackControl);
}

void UControlsDisplayWidget::NativeConstruct()
{
	// Register control type selector FIRST (index 0)
	if (ControlTypeButton)
	{
		// Register as a selector type for left/right cycling
		RegisterFocusableItem(ControlTypeButton, EArcadeFocusType::Selector, ControlTypeLabel, ControlTypeValue);
		// Also bind click to cycle forward (for mouse users)
		ControlTypeButton->OnClicked.AddDynamic(this, &UControlsDisplayWidget::OnControlTypeClicked);
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("ControlsDisplayWidget: Bound OnClicked to ControlTypeButton"));
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("ControlsDisplayWidget: ControlTypeButton not bound - selector won't work"));
	}

	// Register Back button SECOND (index 1)
	if (BackButton)
	{
		RegisterButton(BackButton, BackLabel);
		BackButton->OnClicked.AddDynamic(this, &UControlsDisplayWidget::OnBackClicked);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("ControlsDisplayWidget: BackButton not bound!"));
	}

	// Call parent to set up input handling and initial focus
	Super::NativeConstruct();

	// Initialize display
	UpdateControlTypeDisplay();
	UpdateControlImageVisibility();

	// Allow Blueprint to populate/customize controls display
	OnPopulateControls();

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("ControlsDisplayWidget: Constructed with %d focusable items, current control type: %d"), 
		GetFocusableItemCount(), static_cast<int32>(CurrentControlType));
}

void UControlsDisplayWidget::NativeDestruct()
{
	// Unbind button delegates
	if (ControlTypeButton)
	{
		ControlTypeButton->OnClicked.RemoveDynamic(this, &UControlsDisplayWidget::OnControlTypeClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked.RemoveDynamic(this, &UControlsDisplayWidget::OnBackClicked);
	}

	Super::NativeDestruct();
}

//=============================================================================
// INITIALIZATION
//=============================================================================

void UControlsDisplayWidget::InitializeControlTypeNames()
{
	// Order must match EControlInputType enum: Gamepad, Keyboard, Arcade
	ControlTypeNames.Empty();
	ControlTypeNames.Add(TEXT("GAMEPAD"));
	ControlTypeNames.Add(TEXT("KEYBOARD"));
	ControlTypeNames.Add(TEXT("ARCADE"));
}

//=============================================================================
// PUBLIC FUNCTIONS
//=============================================================================

void UControlsDisplayWidget::SetControlType(EControlInputType NewType)
{
	CurrentControlType = NewType;
	CurrentControlTypeIndex = static_cast<int32>(NewType);

	UpdateControlTypeDisplay();
	UpdateControlImageVisibility();

	// Notify Blueprint
	OnControlTypeChanged(NewType);
}

void UControlsDisplayWidget::CycleControlType(int32 Direction)
{
	const int32 NumTypes = ControlTypeNames.Num();
	if (NumTypes == 0) return;

	// Cycle through types
	CurrentControlTypeIndex = (CurrentControlTypeIndex + Direction + NumTypes) % NumTypes;
	CurrentControlType = static_cast<EControlInputType>(CurrentControlTypeIndex);

	UpdateControlTypeDisplay();
	UpdateControlImageVisibility();

	// Notify Blueprint
	OnControlTypeChanged(CurrentControlType);

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("ControlsDisplayWidget: Cycled to control type %d (%s)"), 
		CurrentControlTypeIndex, *ControlTypeNames[CurrentControlTypeIndex]);
}

//=============================================================================
// DISPLAY UPDATES
//=============================================================================

void UControlsDisplayWidget::UpdateControlTypeDisplay()
{
	if (ControlTypeValue && ControlTypeNames.IsValidIndex(CurrentControlTypeIndex))
	{
		// Format as "< NAME >" to indicate it's cycleable
		FString DisplayText = FString::Printf(TEXT("< %s >"), *ControlTypeNames[CurrentControlTypeIndex]);
		ControlTypeValue->SetText(FText::FromString(DisplayText));
	}
}

void UControlsDisplayWidget::UpdateControlImageVisibility()
{
	// Hide all control images first
	if (ArcadeControlsImage)
	{
		ArcadeControlsImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (KeyboardControlsImage)
	{
		KeyboardControlsImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (GamepadControlsImage)
	{
		GamepadControlsImage->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Show the appropriate control image based on current type
	switch (CurrentControlType)
	{
		case EControlInputType::Arcade:
			if (ArcadeControlsImage)
			{
				ArcadeControlsImage->SetVisibility(ESlateVisibility::Visible);
			}
			break;

		case EControlInputType::Keyboard:
			if (KeyboardControlsImage)
			{
				KeyboardControlsImage->SetVisibility(ESlateVisibility::Visible);
			}
			break;

		case EControlInputType::Gamepad:
			if (GamepadControlsImage)
			{
				GamepadControlsImage->SetVisibility(ESlateVisibility::Visible);
			}
			break;
	}

	// Background is always visible (if bound)
	if (BackgroundImage)
	{
		BackgroundImage->SetVisibility(ESlateVisibility::Visible);
	}
}

//=============================================================================
// BUTTON HANDLERS
//=============================================================================

void UControlsDisplayWidget::OnBackClicked()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("ControlsDisplayWidget: Back button clicked"));
	
	// Broadcast back pressed event (parent will handle closing)
	OnBackPressed.Broadcast();
}

void UControlsDisplayWidget::OnControlTypeClicked()
{
	UE_LOG(LogStateRunner_Arcade, Warning, TEXT("ControlsDisplayWidget: OnControlTypeClicked - MOUSE CLICK RECEIVED"));
	// Mouse click cycles forward through control types
	CycleControlType(1);
}

//=============================================================================
// OVERRIDES
//=============================================================================

void UControlsDisplayWidget::OnSelectorOptionChanged_Implementation(int32 ItemIndex, int32 Delta)
{
	if (ItemIndex == INDEX_CONTROL_TYPE)
	{
		CycleControlType(Delta);
	}
}

void UControlsDisplayWidget::OnItemSelected_Implementation(int32 ItemIndex)
{
	// Back button click is handled by OnClicked delegate
	// Selector doesn't need Enter to do anything (just Left/Right cycles)
}

void UControlsDisplayWidget::OnBackAction_Implementation()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("ControlsDisplayWidget: Escape pressed - closing"));
	
	// Broadcast back pressed event (parent will handle closing)
	OnBackPressed.Broadcast();
}
