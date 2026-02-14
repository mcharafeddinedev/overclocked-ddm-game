#include "PauseMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "MusicPlayerWidget.h"
#include "ThemeSelectorWidget.h"
#include "StateRunner_Arcade.h"

UPauseMenuWidget::UPauseMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Default focus on Resume
	DefaultFocusIndex = INDEX_RESUME;
}

void UPauseMenuWidget::NativeConstruct()
{
	// Register buttons BEFORE calling Super
	
	// Register Resume button
	if (ResumeButton)
	{
		RegisterButton(ResumeButton, ResumeLabel);
		ResumeButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnResumeClicked);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("PauseMenuWidget: ResumeButton not bound!"));
	}

	// Register Settings button (optional)
	if (SettingsButton)
	{
		RegisterButton(SettingsButton, SettingsLabel);
		SettingsButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnSettingsClicked);
	}

	// Register Quit to Menu button
	if (QuitToMenuButton)
	{
		RegisterButton(QuitToMenuButton, QuitToMenuLabel);
		QuitToMenuButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnQuitToMenuClicked);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("PauseMenuWidget: QuitToMenuButton not bound!"));
	}

	// Register theme selector button (left zone - NOT in main menu items)
	if (ThemeSelectorButton)
	{
		ThemeSelectorButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnThemeSelectorClicked);
	}

	// Set embedded music player for zone navigation (uses Left/Right instead of Up/Down)
	// Music player buttons aren't in the main focusable items array;
	// Left/Right keys navigate between menu and music player zones.
	if (MusicPlayerWidget)
	{
		SetEmbeddedMusicPlayer(MusicPlayerWidget);
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("PauseMenuWidget: Music player set for zone navigation (Right zone)"));
	}

	// Call parent (sets initial focus)
	Super::NativeConstruct();
	
	// Initialize left zone visuals (dimmed by default)
	UpdateLeftZoneFocusVisuals();

	// Pause the game
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			bWasPausedBefore = PC->IsPaused();
			if (!bWasPausedBefore)
			{
				PC->SetPause(true);
			}
		}
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("PauseMenuWidget: Constructed and game paused"));
}

void UPauseMenuWidget::NativeDestruct()
{
	// We don't unpause here — the specific action (Resume/Quit) handles it

	// Unbind button delegates to prevent dangling references
	if (ResumeButton)
	{
		ResumeButton->OnClicked.RemoveDynamic(this, &UPauseMenuWidget::OnResumeClicked);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.RemoveDynamic(this, &UPauseMenuWidget::OnSettingsClicked);
	}
	if (QuitToMenuButton)
	{
		QuitToMenuButton->OnClicked.RemoveDynamic(this, &UPauseMenuWidget::OnQuitToMenuClicked);
	}

	// Unbind theme selector button
	if (ThemeSelectorButton)
	{
		ThemeSelectorButton->OnClicked.RemoveDynamic(this, &UPauseMenuWidget::OnThemeSelectorClicked);
	}

	// Clean up settings widget if it's still open
	if (SettingsWidget)
	{
		SettingsWidget->OnBackPressed.RemoveDynamic(this, &UPauseMenuWidget::CloseSettings);
		SettingsWidget->RemoveFromParent();
		SettingsWidget = nullptr;
	}

	// Clean up theme selector widget if it's still open
	if (ThemeSelectorWidget)
	{
		ThemeSelectorWidget->OnBackPressed.RemoveDynamic(this, &UPauseMenuWidget::CloseThemeSelector);
		ThemeSelectorWidget->RemoveFromParent();
		ThemeSelectorWidget = nullptr;
	}

	Super::NativeDestruct();
}

//=============================================================================
// ACTION FUNCTIONS
//=============================================================================

void UPauseMenuWidget::ResumeGame()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("PauseMenuWidget: Resuming game"));

	// Notify Blueprint (for animations/sounds)
	OnBeforeResume();

	// Broadcast that we're closing - Blueprint should clear its reference
	OnPauseMenuClosed.Broadcast();

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			// Unpause if we paused it
			if (!bWasPausedBefore)
			{
				PC->SetPause(false);
			}

			// Restore input mode to Game Only
			FInputModeGameOnly InputMode;
			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = false;
		}
	}

	// Remove this widget
	RemoveFromParent();
}

void UPauseMenuWidget::OpenSettings()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("PauseMenuWidget: Opening Settings"));

	if (!SettingsWidgetClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("PauseMenuWidget: SettingsWidgetClass not set!"));
		return;
	}

	// Create and show settings widget
	SettingsWidget = CreateWidget<UArcadeMenuWidget>(GetOwningPlayer(), SettingsWidgetClass);
	if (SettingsWidget)
	{
		SettingsWidget->AddToViewport(20); // Above pause menu
		SettingsWidget->TakeKeyboardFocus();

		// Bind to settings back action to close it
		SettingsWidget->OnBackPressed.AddDynamic(this, &UPauseMenuWidget::CloseSettings);

		// Hide pause menu while settings is open
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UPauseMenuWidget::QuitToMainMenu()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("PauseMenuWidget: Quitting to main menu"));

	// Notify Blueprint
	OnBeforeQuitToMenu();

	// Unpause
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetPause(false);
		}
	}

	// Load main menu level
	if (!MainMenuLevelName.IsNone())
	{
		// Remove widget from viewport BEFORE level transition to prevent world leak
		RemoveFromParent();
		
		UGameplayStatics::OpenLevel(this, MainMenuLevelName);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("PauseMenuWidget: MainMenuLevelName not set!"));
	}
}

void UPauseMenuWidget::CloseSettings()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("PauseMenuWidget: Closing Settings"));

	if (SettingsWidget)
	{
		SettingsWidget->RemoveFromParent();
		SettingsWidget = nullptr;
	}

	// Show pause menu again
	SetVisibility(ESlateVisibility::Visible);

	// Refocus pause menu
	TakeKeyboardFocus();

	// Notify Blueprint
	OnSettingsMenuClosed();
}

void UPauseMenuWidget::OpenThemeSelector()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("PauseMenuWidget: Opening Theme Selector"));

	if (!ThemeSelectorWidgetClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("PauseMenuWidget: ThemeSelectorWidgetClass not set!"));
		return;
	}

	// Create and show theme selector widget
	ThemeSelectorWidget = CreateWidget<UThemeSelectorWidget>(GetOwningPlayer(), ThemeSelectorWidgetClass);
	if (ThemeSelectorWidget)
	{
		ThemeSelectorWidget->AddToViewport(20); // Above pause menu
		ThemeSelectorWidget->TakeKeyboardFocus();

		// Bind to back action to close it
		ThemeSelectorWidget->OnBackPressed.AddDynamic(this, &UPauseMenuWidget::CloseThemeSelector);

		// Hide pause menu while theme selector is open
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UPauseMenuWidget::CloseThemeSelector()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("PauseMenuWidget: Closing Theme Selector"));

	if (ThemeSelectorWidget)
	{
		ThemeSelectorWidget->OnBackPressed.RemoveDynamic(this, &UPauseMenuWidget::CloseThemeSelector);
		ThemeSelectorWidget->RemoveFromParent();
		ThemeSelectorWidget = nullptr;
	}

	// Show pause menu again
	SetVisibility(ESlateVisibility::Visible);

	// Refocus pause menu (return to left zone since that's where we came from)
	TakeKeyboardFocus();
	FocusLeftZone();
}

//=============================================================================
// BUTTON HANDLERS
//=============================================================================

void UPauseMenuWidget::OnResumeClicked()
{
	ResumeGame();
}

void UPauseMenuWidget::OnSettingsClicked()
{
	OpenSettings();
}

void UPauseMenuWidget::OnQuitToMenuClicked()
{
	QuitToMainMenu();
}

void UPauseMenuWidget::OnThemeSelectorClicked()
{
	OpenThemeSelector();
}

//=============================================================================
// ZONE NAVIGATION
//=============================================================================

void UPauseMenuWidget::FocusLeftZone()
{
	if (!ThemeSelectorButton)
	{
		return; // No left zone items available
	}

	bLeftZoneHasFocus = true;
	bMusicPlayerHasFocus = false;
	
	// Dim ALL main menu items
	DimMenuItems();
	
	// Dim music player
	DimMusicPlayerButtons();
	
	// Update left zone visuals (highlight theme button)
	UpdateLeftZoneFocusVisuals();
	
	PlayNavigationSound();

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("PauseMenuWidget: Focused left zone (Theme button) from main index %d"), CurrentFocusIndex);
}

void UPauseMenuWidget::FocusMainZoneFromLeft()
{
	bLeftZoneHasFocus = false;
	
	// Dim theme button
	UpdateLeftZoneFocusVisuals();
	
	// Return to Settings or Quit (safe destination near the Theme button's vertical position)
	int32 OldFocusIndex = CurrentFocusIndex;
	
	// Go to Settings if it exists, otherwise Quit
	if (SettingsButton)
	{
		CurrentFocusIndex = INDEX_SETTINGS;
	}
	else
	{
		CurrentFocusIndex = INDEX_QUIT_TO_MENU;
	}
	
	// Restore main menu focus
	UpdateFocusVisuals(CurrentFocusIndex, OldFocusIndex);
	
	PlayNavigationSound();

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("PauseMenuWidget: Focused main zone from left (going to index %d)"), CurrentFocusIndex);
}

void UPauseMenuWidget::UpdateLeftZoneFocusVisuals()
{
	if (!ThemeSelectorButton)
	{
		return;
	}

	const FLinearColor FocusedColor = FLinearColor(0.4f, 1.0f, 1.0f);
	const FLinearColor UnfocusedColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
	const FVector2D FocusedScale = FVector2D(1.12f, 1.12f);
	const FVector2D UnfocusedScale = FVector2D(1.0f, 1.0f);

	bool bIsFocused = bLeftZoneHasFocus;
	ThemeSelectorButton->SetBackgroundColor(bIsFocused ? FocusedColor : UnfocusedColor);
	ThemeSelectorButton->SetRenderScale(bIsFocused ? FocusedScale : UnfocusedScale);
	
	if (ThemeSelectorLabel)
	{
		ThemeSelectorLabel->SetRenderOpacity(bIsFocused ? 1.0f : 0.5f);
	}
}

//=============================================================================
// OVERRIDES
//=============================================================================

FReply UPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	// P, Tab, and Gamepad Start are pause toggle keys - they should also unpause/close the menu
	if (Key == EKeys::P || Key == EKeys::Tab || Key == EKeys::Gamepad_Special_Right)
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("PauseMenuWidget: %s pressed - resuming game"), *Key.ToString());
		ResumeGame();
		return FReply::Handled();
	}

	//=========================================================================
	// LEFT ZONE NAVIGATION (Theme Button)
	//=========================================================================
	if (bLeftZoneHasFocus)
	{
		// Up/Down: No action (only one item in left zone)
		if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up ||
			Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down)
		{
			return FReply::Handled();
		}
		// Left: No action (already at left edge)
		else if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left)
		{
			return FReply::Handled();
		}
		// Right: Return to main zone
		else if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right)
		{
			FocusMainZoneFromLeft();
			return FReply::Handled();
		}
		// Enter/Select: Open theme selector
		else if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
		{
			OpenThemeSelector();
			return FReply::Handled();
		}
		// Escape/Back: Return to main zone (or resume game)
		else if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
		{
			FocusMainZoneFromLeft();
			return FReply::Handled();
		}
		
		return FReply::Handled();
	}

	//=========================================================================
	// RIGHT ZONE NAVIGATION (Music Player)
	// Handled by parent class via SetEmbeddedMusicPlayer
	//=========================================================================
	if (bMusicPlayerHasFocus)
	{
		// Let parent handle music player navigation
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}

	//=========================================================================
	// MAIN ZONE NAVIGATION
	//=========================================================================
	// Left: Go to left zone (Theme button) - from any main menu button
	if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left)
	{
		if (ThemeSelectorButton)
		{
			FocusLeftZone();
		}
		return FReply::Handled();
	}
	// Right: Go to right zone (music player) - from any main menu button
	else if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right)
	{
		if (MusicPlayerWidget)
		{
			FocusMusicPlayer();
		}
		return FReply::Handled();
	}

	// Let the parent class handle other keys (Up/Down navigation, Enter, Escape, etc.)
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UPauseMenuWidget::OnItemSelected_Implementation(int32 ItemIndex)
{
	// Button clicks are handled by OnClicked delegates
}

void UPauseMenuWidget::OnBackAction_Implementation()
{
	// On Pause Menu, Escape resumes the game
	ResumeGame();
}

void UPauseMenuWidget::FocusMenu()
{
	if (!bMusicPlayerHasFocus)
	{
		return;
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("PauseMenuWidget: Returning focus from music player to main zone"));

	// Dim music player buttons
	DimMusicPlayerButtons();

	// Clear music player focus
	bMusicPlayerHasFocus = false;

	// Return to Settings button (above Quit) - mirrors FocusMainZoneFromLeft behavior
	int32 OldFocusIndex = CurrentFocusIndex;
	
	if (SettingsButton)
	{
		CurrentFocusIndex = INDEX_SETTINGS;
	}
	else
	{
		CurrentFocusIndex = INDEX_RESUME;
	}

	// Restore menu focus visuals
	UpdateFocusVisuals(CurrentFocusIndex, OldFocusIndex);

	// Play navigation sound
	PlayNavigationSound();
}
