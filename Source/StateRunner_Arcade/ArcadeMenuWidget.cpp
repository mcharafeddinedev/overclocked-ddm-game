#include "ArcadeMenuWidget.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"
#include "MusicPlayerWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Framework/Application/SlateApplication.h"
#include "StateRunner_Arcade.h"

UArcadeMenuWidget::UArcadeMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Ensure this widget can receive keyboard focus
	SetIsFocusable(true);
}

void UArcadeMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Set initial focus index (but don't show visuals until keyboard navigation starts)
	if (bFocusOnConstruct && FocusableItems.Num() > 0)
	{
		// Set focus index directly without triggering visuals
		CurrentFocusIndex = FMath::Clamp(DefaultFocusIndex, 0, FocusableItems.Num() - 1);
		TakeKeyboardFocus();
	}
}

void UArcadeMenuWidget::NativeDestruct()
{
	// Clean up timer
	StopKeyRepeat();

	Super::NativeDestruct();
}

FReply UArcadeMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	// Check if this is a navigation key that should activate keyboard navigation mode
	bool bIsNavigationKey = (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up ||
							 Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down ||
							 Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left ||
							 Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right ||
							 Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom);

	// On the FIRST navigation key press, just show the focus highlight
	// without navigating (so pressing Down doesn't skip the first item)
	if (bIsNavigationKey && !bKeyboardNavigationStarted)
	{
		// Use the shared activation function which dims all items then highlights current
		ActivateKeyboardNavigation();
		
		// Don't perform navigation on this first press - just show the current focus
		// (unless it's Enter/Select, which should still work)
		if (Key != EKeys::Enter && Key != EKeys::SpaceBar && Key != EKeys::Gamepad_FaceButton_Bottom)
		{
			return FReply::Handled();
		}
	}

	// Handle navigation keys
	if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up)
	{
		NavigateUp();
		StartKeyRepeat(Key);
		return FReply::Handled();
	}
	else if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down)
	{
		NavigateDown();
		StartKeyRepeat(Key);
		return FReply::Handled();
	}
	else if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left)
	{
		NavigateLeft();
		StartKeyRepeat(Key);
		return FReply::Handled();
	}
	else if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right)
	{
		NavigateRight();
		StartKeyRepeat(Key);
		return FReply::Handled();
	}
	else if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		SelectFocusedItem();
		return FReply::Handled();
	}
	else if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		HandleBack();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UArcadeMenuWidget::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	// Stop key repeat when navigation key is released
	if (Key == CurrentHeldKey)
	{
		StopKeyRepeat();
		return FReply::Handled();
	}

	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

// --- Registration Functions ---

int32 UArcadeMenuWidget::RegisterFocusableItem(UWidget* Widget, EArcadeFocusType FocusType, UTextBlock* LabelWidget, UTextBlock* ValueWidget)
{
	if (!Widget)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("ArcadeMenuWidget: Attempted to register null widget"));
		return -1;
	}

	FArcadeFocusableItem Item;
	Item.Widget = Widget;
	Item.FocusType = FocusType;
	Item.LabelWidget = LabelWidget;
	Item.ValueWidget = ValueWidget;
	Item.bIsEnabled = true;

	int32 Index = FocusableItems.Add(Item);

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("ArcadeMenuWidget: Registered focusable item %d (Type: %d)"), 
		Index, static_cast<int32>(FocusType));

	return Index;
}

int32 UArcadeMenuWidget::RegisterButton(UButton* Button, UTextBlock* LabelWidget)
{
	return RegisterFocusableItem(Button, EArcadeFocusType::Button, LabelWidget, nullptr);
}

int32 UArcadeMenuWidget::RegisterSlider(USlider* Slider, UTextBlock* LabelWidget, UTextBlock* ValueWidget)
{
	return RegisterFocusableItem(Slider, EArcadeFocusType::Slider, LabelWidget, ValueWidget);
}

void UArcadeMenuWidget::ClearFocusableItems()
{
	FocusableItems.Empty();
	CurrentFocusIndex = -1;
}

// --- Focus Management ---

void UArcadeMenuWidget::SetFocusIndex(int32 NewIndex)
{
	// Validate index
	if (NewIndex >= 0 && NewIndex < FocusableItems.Num())
	{
		// Skip disabled items
		if (!FocusableItems[NewIndex].bIsEnabled)
		{
			return;
		}
	}
	else if (NewIndex != -1)
	{
		return;
	}

	int32 OldIndex = CurrentFocusIndex;
	CurrentFocusIndex = NewIndex;

	// Broadcast focus change
	if (OldIndex != NewIndex)
	{
		OnFocusChanged.Broadcast(NewIndex, OldIndex);
		
		// Only update visuals and play sounds after keyboard navigation has started
		// so we don't show highlighted buttons before the player uses keyboard
		if (bKeyboardNavigationStarted)
		{
			UpdateFocusVisuals(NewIndex, OldIndex);
			
			// Play navigation sound (like hover sound)
			if (NewIndex >= 0)
			{
				PlayNavigationSound();
			}
		}

	}
}

void UArcadeMenuWidget::SetItemEnabled(int32 ItemIndex, bool bEnabled)
{
	if (ItemIndex >= 0 && ItemIndex < FocusableItems.Num())
	{
		FocusableItems[ItemIndex].bIsEnabled = bEnabled;

		// If disabling the currently focused item, move focus
		if (!bEnabled && CurrentFocusIndex == ItemIndex)
		{
			int32 NextItem = FindNextEnabledItem(ItemIndex, 1);
			if (NextItem == -1)
			{
				NextItem = FindNextEnabledItem(ItemIndex, -1);
			}
			SetFocusIndex(NextItem);
		}
	}
}

void UArcadeMenuWidget::TakeKeyboardFocus()
{
	SetKeyboardFocus();
}

void UArcadeMenuWidget::ResetKeyboardNavigationState()
{
	if (bKeyboardNavigationStarted)
	{
		bKeyboardNavigationStarted = false;
		// Clear focus visuals
		UpdateFocusVisuals(-1, CurrentFocusIndex);
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("ArcadeMenuWidget: Keyboard navigation state reset"));
	}
}

void UArcadeMenuWidget::ActivateKeyboardNavigation()
{
	if (!bKeyboardNavigationStarted)
	{
		bKeyboardNavigationStarted = true;
		
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("ArcadeMenuWidget: Activating keyboard navigation, CurrentFocus=%d, Items=%d"),
			CurrentFocusIndex, FocusableItems.Num());
		
		// Dim ALL items first (set to unfocused state)
		// For buttons: use SetBackgroundColor() to tint the button style image
		// For other widgets: use SetRenderOpacity()
		const FLinearColor UnfocusedColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);  // Consistent dimmed color
		
		for (const FArcadeFocusableItem& Item : FocusableItems)
		{
			if (Item.Widget)
			{
				// For buttons, tint the background instead of whole widget opacity
				if (Item.FocusType == EArcadeFocusType::Button)
				{
					if (UButton* Button = Cast<UButton>(Item.Widget))
					{
						Button->SetBackgroundColor(UnfocusedColor);
					}
				}
				else
				{
					Item.Widget->SetRenderOpacity(0.5f);
				}
				Item.Widget->SetRenderScale(FVector2D(1.0f, 1.0f));
			}
			if (Item.LabelWidget)
			{
				Item.LabelWidget->SetRenderOpacity(0.5f);
			}
			if (Item.ValueWidget)
			{
				Item.ValueWidget->SetRenderOpacity(0.5f);
			}
		}
		
		// Then highlight the focused item (passing -1 as old since nothing was focused before)
		UpdateFocusVisuals(CurrentFocusIndex, -1);
		
		// Play navigation sound to confirm keyboard navigation activated
		PlayNavigationSound();
	}
}

// --- Navigation Functions ---

void UArcadeMenuWidget::NavigateUp()
{
	// If music player has focus, return to menu
	if (bMusicPlayerHasFocus && EmbeddedMusicPlayer)
	{
		FocusMenu();
		return;
	}

	if (FocusableItems.Num() == 0) return;

	int32 StartIndex = CurrentFocusIndex >= 0 ? CurrentFocusIndex - 1 : FocusableItems.Num() - 1;
	int32 NextIndex = FindNextEnabledItem(StartIndex, -1);

	if (NextIndex >= 0)
	{
		SetFocusIndex(NextIndex);
	}
}

void UArcadeMenuWidget::NavigateDown()
{
	// If music player has focus, return to menu
	if (bMusicPlayerHasFocus && EmbeddedMusicPlayer)
	{
		FocusMenu();
		return;
	}

	if (FocusableItems.Num() == 0) return;

	int32 StartIndex = CurrentFocusIndex >= 0 ? CurrentFocusIndex + 1 : 0;
	int32 NextIndex = FindNextEnabledItem(StartIndex, 1);

	if (NextIndex >= 0)
	{
		SetFocusIndex(NextIndex);
	}
}

void UArcadeMenuWidget::NavigateLeft()
{
	// If music player has focus, navigate within it or return to menu
	if (bMusicPlayerHasFocus && EmbeddedMusicPlayer)
	{
		if (MusicPlayerFocusIndex > 0)
		{
			// Navigate to previous music player button
			NavigateMusicPlayer(-1);
		}
		else
		{
			// At first music player button, return to menu
			FocusMenu();
		}
		return;
	}

	// Handle slider/selector adjustment when menu is focused
	if (CurrentFocusIndex >= 0 && CurrentFocusIndex < FocusableItems.Num())
	{
		const FArcadeFocusableItem& Item = FocusableItems[CurrentFocusIndex];

		switch (Item.FocusType)
		{
		case EArcadeFocusType::Slider:
			AdjustFocusedSlider(-SliderStepSize);
			return;

		case EArcadeFocusType::Selector:
			CycleFocusedSelector(-1);
			return;

		default:
			// Buttons: If bLeftRightNavigatesMenu, Left acts like Up
			if (bLeftRightNavigatesMenu)
			{
				NavigateUp();
				return;
			}
			// Otherwise, Left goes to music player if available
			if (EmbeddedMusicPlayer)
			{
				FocusMusicPlayer();
			}
			break;
		}
	}
}

void UArcadeMenuWidget::NavigateRight()
{
	// If music player has focus, navigate within it or wrap
	if (bMusicPlayerHasFocus && EmbeddedMusicPlayer)
	{
		if (MusicPlayerFocusIndex < MUSIC_PLAYER_BUTTON_COUNT - 1)
		{
			// Navigate to next music player button
			NavigateMusicPlayer(1);
		}
		// At last button, stay there (don't wrap)
		return;
	}

	// Handle slider/selector adjustment when menu is focused
	if (CurrentFocusIndex >= 0 && CurrentFocusIndex < FocusableItems.Num())
	{
		const FArcadeFocusableItem& Item = FocusableItems[CurrentFocusIndex];

		switch (Item.FocusType)
		{
		case EArcadeFocusType::Slider:
			AdjustFocusedSlider(SliderStepSize);
			return;

		case EArcadeFocusType::Selector:
			CycleFocusedSelector(1);
			return;

		default:
			// Buttons: If bLeftRightNavigatesMenu, Right acts like Down
			if (bLeftRightNavigatesMenu)
			{
				NavigateDown();
				return;
			}
			// Otherwise, Right goes to music player if available
			if (EmbeddedMusicPlayer)
			{
				FocusMusicPlayer();
			}
			break;
		}
	}
}

void UArcadeMenuWidget::SelectFocusedItem()
{
	// If music player has focus, select its button instead
	if (bMusicPlayerHasFocus && EmbeddedMusicPlayer)
	{
		SelectMusicPlayerButton();
		return;
	}

	if (CurrentFocusIndex < 0 || CurrentFocusIndex >= FocusableItems.Num()) return;

	const FArcadeFocusableItem& Item = FocusableItems[CurrentFocusIndex];

	if (!Item.bIsEnabled) return;

	// Play selection sound (like click sound)
	PlaySelectionSound();

	// Broadcast selection
	OnMenuItemSelected.Broadcast(CurrentFocusIndex);

	// Call Blueprint event
	OnItemSelected(CurrentFocusIndex);

	// If it's a button, simulate click
	if (Item.FocusType == EArcadeFocusType::Button)
	{
		if (UButton* Button = Cast<UButton>(Item.Widget))
		{
			// Broadcast the button's OnClicked event
			Button->OnClicked.Broadcast();
		}
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("ArcadeMenuWidget: Item %d selected"), CurrentFocusIndex);
}

void UArcadeMenuWidget::HandleBack()
{
	// Play back/cancel sound
	PlayBackSound();

	OnBackPressed.Broadcast();
	OnBackAction();

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("ArcadeMenuWidget: Back pressed"));
}

// --- Blueprint Native Event Implementations ---

void UArcadeMenuWidget::OnItemSelected_Implementation(int32 ItemIndex)
{
	// Default implementation
}

void UArcadeMenuWidget::UpdateFocusVisuals_Implementation(int32 NewFocusIndex, int32 OldFocusIndex)
{
	// Default implementation: Use button background color tint + scale to show focus
	// Focused item = full brightness + slight scale up (1.05)
	// Unfocused = dimmed background + normal scale (1.0)
	// 
	// For buttons: Uses SetBackgroundColor() to tint the button style image
	// For other widgets: Uses SetRenderOpacity() on the whole widget
	// Text labels are dimmed separately via SetRenderOpacity()
	
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("UpdateFocusVisuals: Old=%d, New=%d, TotalItems=%d"), 
		OldFocusIndex, NewFocusIndex, FocusableItems.Num());
	
	// Colors for focused/unfocused states
	const FLinearColor FocusedColor = FLinearColor(0.4f, 1.0f, 1.0f);  // Strong cyan tint for better visibility
	const FLinearColor UnfocusedColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);  // Dimmed but visible
	
	// Unfocus the old item
	if (OldFocusIndex >= 0 && OldFocusIndex < FocusableItems.Num())
	{
		const FArcadeFocusableItem& OldItem = FocusableItems[OldFocusIndex];
		
		if (OldItem.Widget)
		{
			// For buttons, tint the background image instead of whole widget opacity
			if (OldItem.FocusType == EArcadeFocusType::Button)
			{
				if (UButton* Button = Cast<UButton>(OldItem.Widget))
				{
					Button->SetBackgroundColor(UnfocusedColor);
				}
			}
			else
			{
				// For non-button widgets, use render opacity
				OldItem.Widget->SetRenderOpacity(0.35f);
			}
			OldItem.Widget->SetRenderScale(FVector2D(1.0f, 1.0f));
		}
		if (OldItem.LabelWidget)
		{
			OldItem.LabelWidget->SetRenderOpacity(0.35f);
		}
		if (OldItem.ValueWidget)
		{
			OldItem.ValueWidget->SetRenderOpacity(0.35f);
		}
	}
	
	// Focus the new item
	if (NewFocusIndex >= 0 && NewFocusIndex < FocusableItems.Num())
	{
		const FArcadeFocusableItem& NewItem = FocusableItems[NewFocusIndex];
		
		if (NewItem.Widget)
		{
			// For buttons, tint the background image instead of whole widget opacity
			if (NewItem.FocusType == EArcadeFocusType::Button)
			{
				if (UButton* Button = Cast<UButton>(NewItem.Widget))
				{
					Button->SetBackgroundColor(FocusedColor);
				}
			}
			else
			{
				// For non-button widgets, use render opacity
				NewItem.Widget->SetRenderOpacity(1.0f);
			}
			NewItem.Widget->SetRenderScale(FVector2D(1.12f, 1.12f));
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("  Focused widget: %s"), *NewItem.Widget->GetName());
		}
		if (NewItem.LabelWidget)
		{
			NewItem.LabelWidget->SetRenderOpacity(1.0f);
		}
		if (NewItem.ValueWidget)
		{
			NewItem.ValueWidget->SetRenderOpacity(1.0f);
		}
	}
	
	// If clearing focus (NewFocusIndex == -1), restore all to normal
	if (NewFocusIndex == -1)
	{
		for (const FArcadeFocusableItem& Item : FocusableItems)
		{
			if (Item.Widget)
			{
				// For buttons, restore full brightness
				if (Item.FocusType == EArcadeFocusType::Button)
				{
					if (UButton* Button = Cast<UButton>(Item.Widget))
					{
						Button->SetBackgroundColor(FocusedColor);
					}
				}
				else
				{
					Item.Widget->SetRenderOpacity(1.0f);
				}
				Item.Widget->SetRenderScale(FVector2D(1.0f, 1.0f));
			}
			if (Item.LabelWidget)
			{
				Item.LabelWidget->SetRenderOpacity(1.0f);
			}
			if (Item.ValueWidget)
			{
				Item.ValueWidget->SetRenderOpacity(1.0f);
			}
		}
	}
}

void UArcadeMenuWidget::OnBackAction_Implementation()
{
	// Default implementation
	// Typically removes this widget and shows parent menu
}

void UArcadeMenuWidget::OnSliderValueChanged_Implementation(int32 ItemIndex, float NewValue)
{
	// Default implementation
}

void UArcadeMenuWidget::OnSelectorOptionChanged_Implementation(int32 ItemIndex, int32 Delta)
{
	// Default implementation
}

// --- Internal Functions ---

int32 UArcadeMenuWidget::FindNextEnabledItem(int32 StartIndex, int32 Direction) const
{
	if (FocusableItems.Num() == 0) return -1;

	int32 ItemCount = FocusableItems.Num();
	int32 CheckedCount = 0;
	int32 CurrentIndex = StartIndex;

	// Wrap index if needed
	if (bWrapNavigation)
	{
		if (CurrentIndex < 0) CurrentIndex = ItemCount - 1;
		if (CurrentIndex >= ItemCount) CurrentIndex = 0;
	}
	else
	{
		if (CurrentIndex < 0 || CurrentIndex >= ItemCount) return -1;
	}

	while (CheckedCount < ItemCount)
	{
		if (CurrentIndex >= 0 && CurrentIndex < ItemCount && FocusableItems[CurrentIndex].bIsEnabled)
		{
			return CurrentIndex;
		}

		CurrentIndex += Direction;
		CheckedCount++;

		// Handle wrapping
		if (bWrapNavigation)
		{
			if (CurrentIndex < 0) CurrentIndex = ItemCount - 1;
			if (CurrentIndex >= ItemCount) CurrentIndex = 0;
		}
		else
		{
			if (CurrentIndex < 0 || CurrentIndex >= ItemCount) return -1;
		}
	}

	return -1;
}

void UArcadeMenuWidget::OnKeyRepeatTick()
{
	// Activate repeat after initial delay
	if (!bKeyRepeatActive)
	{
		bKeyRepeatActive = true;
		
		// Switch to faster repeat rate
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(KeyRepeatTimerHandle);
			World->GetTimerManager().SetTimer(KeyRepeatTimerHandle, this, &UArcadeMenuWidget::OnKeyRepeatTick, KeyRepeatRate, true);
		}
	}

	// Execute the navigation action
	if (CurrentHeldKey == EKeys::Up || CurrentHeldKey == EKeys::Gamepad_DPad_Up)
	{
		NavigateUp();
	}
	else if (CurrentHeldKey == EKeys::Down || CurrentHeldKey == EKeys::Gamepad_DPad_Down)
	{
		NavigateDown();
	}
	else if (CurrentHeldKey == EKeys::Left || CurrentHeldKey == EKeys::Gamepad_DPad_Left)
	{
		NavigateLeft();
	}
	else if (CurrentHeldKey == EKeys::Right || CurrentHeldKey == EKeys::Gamepad_DPad_Right)
	{
		NavigateRight();
	}
}

void UArcadeMenuWidget::StartKeyRepeat(const FKey& Key)
{
	CurrentHeldKey = Key;
	bKeyRepeatActive = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(KeyRepeatTimerHandle, this, &UArcadeMenuWidget::OnKeyRepeatTick, KeyRepeatDelay, false);
	}
}

void UArcadeMenuWidget::StopKeyRepeat()
{
	CurrentHeldKey = FKey();
	bKeyRepeatActive = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(KeyRepeatTimerHandle);
	}
}

void UArcadeMenuWidget::AdjustFocusedSlider(float Delta)
{
	if (CurrentFocusIndex < 0 || CurrentFocusIndex >= FocusableItems.Num()) return;

	FArcadeFocusableItem& Item = FocusableItems[CurrentFocusIndex];
	if (Item.FocusType != EArcadeFocusType::Slider) return;

	USlider* Slider = Cast<USlider>(Item.Widget);
	if (!Slider) return;

	float CurrentValue = Slider->GetValue();
	float NewValue = FMath::Clamp(CurrentValue + Delta, 0.0f, 1.0f);

	if (NewValue != CurrentValue)
	{
		Slider->SetValue(NewValue);

		// Broadcast events
		OnSliderAdjusted.Broadcast(CurrentFocusIndex, NewValue);
		OnSliderValueChanged(CurrentFocusIndex, NewValue);

	}
}

void UArcadeMenuWidget::CycleFocusedSelector(int32 Delta)
{
	if (CurrentFocusIndex < 0 || CurrentFocusIndex >= FocusableItems.Num()) return;

	// Broadcast events - Blueprint handles the actual cycling logic
	OnSelectorChanged.Broadcast(CurrentFocusIndex, Delta);
	OnSelectorOptionChanged(CurrentFocusIndex, Delta);

}

// --- Audio Feedback ---

void UArcadeMenuWidget::PlayNavigationSound(UButton* FallbackButton)
{
	// Prefer the explicit NavigationSound asset if set
	if (NavigationSound)
	{
		UGameplayStatics::PlaySound2D(this, NavigationSound);
		return;
	}

	// Fallback: play HoveredSlateSound from the button's widget style
	// so keyboard/gamepad navigation matches mouse hover sounds
	UButton* ButtonForSound = FallbackButton;
	if (!ButtonForSound && CurrentFocusIndex >= 0 && CurrentFocusIndex < FocusableItems.Num())
	{
		const FArcadeFocusableItem& Item = FocusableItems[CurrentFocusIndex];
		if (Item.FocusType == EArcadeFocusType::Button)
		{
			ButtonForSound = Cast<UButton>(Item.Widget);
		}
	}

	if (ButtonForSound && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().PlaySound(ButtonForSound->WidgetStyle.HoveredSlateSound);
	}
}

void UArcadeMenuWidget::PlaySelectionSound(UButton* FallbackButton)
{
	// Prefer the explicit SelectionSound asset if set
	if (SelectionSound)
	{
		UGameplayStatics::PlaySound2D(this, SelectionSound);
		return;
	}

	// Fallback: play PressedSlateSound from the button's widget style
	// so keyboard/gamepad selection matches mouse click sounds
	UButton* ButtonForSound = FallbackButton;
	if (!ButtonForSound && CurrentFocusIndex >= 0 && CurrentFocusIndex < FocusableItems.Num())
	{
		const FArcadeFocusableItem& Item = FocusableItems[CurrentFocusIndex];
		if (Item.FocusType == EArcadeFocusType::Button)
		{
			ButtonForSound = Cast<UButton>(Item.Widget);
		}
	}

	if (ButtonForSound && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().PlaySound(ButtonForSound->WidgetStyle.PressedSlateSound);
	}
}

void UArcadeMenuWidget::PlayBackSound()
{
	if (BackSound)
	{
		UGameplayStatics::PlaySound2D(this, BackSound);
	}
}

// --- Music Player Zone Navigation ---

void UArcadeMenuWidget::SetEmbeddedMusicPlayer(UMusicPlayerWidget* MusicPlayer)
{
	EmbeddedMusicPlayer = MusicPlayer;
	
	if (EmbeddedMusicPlayer)
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("ArcadeMenuWidget: Embedded music player set"));
	}
}

void UArcadeMenuWidget::FocusMusicPlayer()
{
	if (!EmbeddedMusicPlayer)
	{
		return;
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("ArcadeMenuWidget: Focusing music player zone"));

	// Dim menu items
	DimMenuItems();

	// Set music player focus
	bMusicPlayerHasFocus = true;
	MusicPlayerFocusIndex = MUSIC_PLAYER_INDEX_SKIP; // Start with Skip button

	// Update music player focus visuals
	UpdateMusicPlayerFocusVisuals(MusicPlayerFocusIndex, -1);

	// Play navigation sound (pass the skip button for style sound fallback)
	PlayNavigationSound(EmbeddedMusicPlayer->GetSkipButton());
}

void UArcadeMenuWidget::FocusMenu()
{
	if (!bMusicPlayerHasFocus)
	{
		return;
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("ArcadeMenuWidget: Returning focus to menu zone"));

	// Dim music player buttons
	DimMusicPlayerButtons();

	// Clear music player focus
	int32 OldMusicIndex = MusicPlayerFocusIndex;
	bMusicPlayerHasFocus = false;

	// Restore menu focus visuals
	if (CurrentFocusIndex >= 0 && CurrentFocusIndex < FocusableItems.Num())
	{
		UpdateFocusVisuals(CurrentFocusIndex, -1);
	}

	// Play navigation sound
	PlayNavigationSound();
}

void UArcadeMenuWidget::NavigateMusicPlayer(int32 Direction)
{
	if (!EmbeddedMusicPlayer || !bMusicPlayerHasFocus)
	{
		return;
	}

	int32 OldIndex = MusicPlayerFocusIndex;
	int32 NewIndex = FMath::Clamp(MusicPlayerFocusIndex + Direction, 0, MUSIC_PLAYER_BUTTON_COUNT - 1);

	if (NewIndex != OldIndex)
	{
		MusicPlayerFocusIndex = NewIndex;
		UpdateMusicPlayerFocusVisuals(NewIndex, OldIndex);
		
		// Play navigation sound (pass the newly focused music player button for style sound fallback)
		UButton* NewButton = (NewIndex == MUSIC_PLAYER_INDEX_SKIP) ?
			EmbeddedMusicPlayer->GetSkipButton() : EmbeddedMusicPlayer->GetShuffleButton();
		PlayNavigationSound(NewButton);
		
	}
}

void UArcadeMenuWidget::SelectMusicPlayerButton()
{
	if (!EmbeddedMusicPlayer || !bMusicPlayerHasFocus)
	{
		return;
	}

	// Determine the button first so we can pass it for style sound fallback
	UButton* ButtonToClick = nullptr;

	switch (MusicPlayerFocusIndex)
	{
	case MUSIC_PLAYER_INDEX_SKIP:
		ButtonToClick = EmbeddedMusicPlayer->GetSkipButton();
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("ArcadeMenuWidget: Music player Skip selected"));
		break;

	case MUSIC_PLAYER_INDEX_SHUFFLE:
		ButtonToClick = EmbeddedMusicPlayer->GetShuffleButton();
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("ArcadeMenuWidget: Music player Shuffle selected"));
		break;

	default:
		break;
	}

	// Play selection sound (pass the music player button for style sound fallback)
	PlaySelectionSound(ButtonToClick);

	if (ButtonToClick)
	{
		ButtonToClick->OnClicked.Broadcast();
	}
}

void UArcadeMenuWidget::UpdateMusicPlayerFocusVisuals(int32 NewIndex, int32 OldIndex)
{
	if (!EmbeddedMusicPlayer)
	{
		return;
	}

	const FLinearColor FocusedColor = FLinearColor(0.4f, 1.0f, 1.0f);  // Strong cyan tint for better visibility
	const FLinearColor UnfocusedColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);  // Dimmed but visible

	// Get buttons
	UButton* SkipButton = EmbeddedMusicPlayer->GetSkipButton();
	UButton* ShuffleButton = EmbeddedMusicPlayer->GetShuffleButton();

	// Unfocus old button
	if (OldIndex >= 0)
	{
		UButton* OldButton = (OldIndex == MUSIC_PLAYER_INDEX_SKIP) ? SkipButton : ShuffleButton;
		if (OldButton)
		{
			OldButton->SetBackgroundColor(UnfocusedColor);
			OldButton->SetRenderScale(FVector2D(1.0f, 1.0f));
		}
	}

	// Focus new button
	UButton* NewButton = (NewIndex == MUSIC_PLAYER_INDEX_SKIP) ? SkipButton : ShuffleButton;
	if (NewButton)
	{
		NewButton->SetBackgroundColor(FocusedColor);
		NewButton->SetRenderScale(FVector2D(1.12f, 1.12f));
	}
}

void UArcadeMenuWidget::DimMenuItems()
{
	const FLinearColor UnfocusedColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);  // Consistent dimmed color

	for (const FArcadeFocusableItem& Item : FocusableItems)
	{
		if (Item.Widget)
		{
			if (Item.FocusType == EArcadeFocusType::Button)
			{
				if (UButton* Button = Cast<UButton>(Item.Widget))
				{
					Button->SetBackgroundColor(UnfocusedColor);
				}
			}
			else
			{
				Item.Widget->SetRenderOpacity(0.5f);
			}
			Item.Widget->SetRenderScale(FVector2D(1.0f, 1.0f));
		}
		if (Item.LabelWidget)
		{
			Item.LabelWidget->SetRenderOpacity(0.5f);
		}
		if (Item.ValueWidget)
		{
			Item.ValueWidget->SetRenderOpacity(0.5f);
		}
	}
}

void UArcadeMenuWidget::DimMusicPlayerButtons()
{
	if (!EmbeddedMusicPlayer)
	{
		return;
	}

	const FLinearColor UnfocusedColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);  // Consistent dimmed color

	if (UButton* SkipButton = EmbeddedMusicPlayer->GetSkipButton())
	{
		SkipButton->SetBackgroundColor(UnfocusedColor);
		SkipButton->SetRenderScale(FVector2D(1.0f, 1.0f));
	}

	if (UButton* ShuffleButton = EmbeddedMusicPlayer->GetShuffleButton())
	{
		ShuffleButton->SetBackgroundColor(UnfocusedColor);
		ShuffleButton->SetRenderScale(FVector2D(1.0f, 1.0f));
	}
}
