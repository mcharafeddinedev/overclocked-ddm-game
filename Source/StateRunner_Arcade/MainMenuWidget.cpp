#include "MainMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MusicPlayerWidget.h"
#include "LeaderboardWidget.h"
#include "ThemeSelectorWidget.h"
#include "ThemeSubsystem.h"
#include "StateRunner_Arcade.h"

UMainMenuWidget::UMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Default focus on Start Game
	DefaultFocusIndex = INDEX_START_GAME;
	
	// Main menu uses custom zone-based navigation (NOT bLeftRightNavigatesMenu)
	// - Left zone: Theme button
	// - Main zone (center): Start, Settings, Controls, Credits, Quit
	// - Right zone: Leaderboard, Music Player
	// - Left/Right switches between zones
	// - Up/Down navigates within current zone
	bLeftRightNavigatesMenu = false;
}

void UMainMenuWidget::NativeConstruct()
{
	// Register buttons BEFORE calling Super (which sets focus)
	
	// Register Start Game button
	if (StartGameButton)
	{
		RegisterButton(StartGameButton, StartGameLabel);
		StartGameButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnStartGameClicked);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MainMenuWidget: StartGameButton not bound!"));
	}

	// Register Settings button
	if (SettingsButton)
	{
		RegisterButton(SettingsButton, SettingsLabel);
		SettingsButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnSettingsClicked);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MainMenuWidget: SettingsButton not bound!"));
	}

	// Register Controls button (optional)
	if (ControlsButton)
	{
		RegisterButton(ControlsButton, ControlsLabel);
		ControlsButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnControlsClicked);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: ControlsButton not bound (optional)"));
	}

	// Register Credits button (optional)
	if (CreditsButton)
	{
		RegisterButton(CreditsButton, CreditsLabel);
		CreditsButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnCreditsClicked);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: CreditsButton not bound (optional)"));
	}

	// Register Quit button
	if (QuitButton)
	{
		RegisterButton(QuitButton, QuitLabel);
		QuitButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnQuitClicked);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MainMenuWidget: QuitButton not bound!"));
	}

	// Theme button is NOT in the main focusable items
	// It's in the left zone (handled separately via zone navigation)
	if (ThemeButton)
	{
		ThemeButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnThemeClicked);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: ThemeButton not bound (optional)"));
	}

	// Leaderboard button is NOT in the main focusable items
	// It's in the right zone (handled separately via zone navigation)
	if (LeaderboardButton)
	{
		LeaderboardButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnLeaderboardClicked);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: LeaderboardButton not bound (optional)"));
	}

	// Set embedded music player for zone navigation (uses Left/Right instead of Up/Down)
	// Music player buttons aren't in the main focusable items array;
	// Left/Right keys navigate between menu and music player zones.
	if (MusicPlayerWidget)
	{
		SetEmbeddedMusicPlayer(MusicPlayerWidget);
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: Music player set for zone navigation (Left/Right)"));
	}

	// Hide theme notification on construct (it shows when a theme is applied)
	if (ThemeNotificationText)
	{
		ThemeNotificationText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ThemeNotificationContainer)
	{
		ThemeNotificationContainer->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Call parent (this sets initial focus)
	Super::NativeConstruct();

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: Constructed with %d focusable items"), GetFocusableItemCount());
}

void UMainMenuWidget::NativeDestruct()
{
	// Unbind button delegates to prevent dangling references
	if (StartGameButton)
	{
		StartGameButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::OnStartGameClicked);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::OnSettingsClicked);
	}
	if (ControlsButton)
	{
		ControlsButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::OnControlsClicked);
	}
	if (CreditsButton)
	{
		CreditsButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::OnCreditsClicked);
	}
	if (ThemeButton)
	{
		ThemeButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::OnThemeClicked);
	}
	if (LeaderboardButton)
	{
		LeaderboardButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::OnLeaderboardClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::OnQuitClicked);
	}

	// Clean up settings widget if it's still open
	if (SettingsWidget)
	{
		SettingsWidget->OnBackPressed.RemoveDynamic(this, &UMainMenuWidget::CloseSettings);
		SettingsWidget->RemoveFromParent();
		SettingsWidget = nullptr;
	}

	// Clean up controls widget if it's still open
	if (ControlsWidget)
	{
		ControlsWidget->OnBackPressed.RemoveDynamic(this, &UMainMenuWidget::CloseControls);
		ControlsWidget->RemoveFromParent();
		ControlsWidget = nullptr;
	}

	// Clean up credits widget if it's still open
	if (CreditsWidget)
	{
		CreditsWidget->OnBackPressed.RemoveDynamic(this, &UMainMenuWidget::CloseCredits);
		CreditsWidget->RemoveFromParent();
		CreditsWidget = nullptr;
	}

	// Clean up leaderboard widget if it's still open
	if (LeaderboardWidget)
	{
		LeaderboardWidget->OnBackPressed.RemoveDynamic(this, &UMainMenuWidget::CloseLeaderboard);
		LeaderboardWidget->RemoveFromParent();
		LeaderboardWidget = nullptr;
	}

	// Clean up theme selector widget if it's still open
	if (ThemeSelectorWidget)
	{
		ThemeSelectorWidget->OnBackPressed.RemoveDynamic(this, &UMainMenuWidget::CloseThemeSelector);
		ThemeSelectorWidget->RemoveFromParent();
		ThemeSelectorWidget = nullptr;
	}

	// Clear theme notification timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThemeNotificationTimerHandle);
	}

	Super::NativeDestruct();
}

//=============================================================================
// ZONE-BASED NAVIGATION
//=============================================================================

FReply UMainMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	// Check if this is a navigation key that should activate keyboard navigation mode
	bool bIsNavigationKey = (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up ||
							 Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down ||
							 Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left ||
							 Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right ||
							 Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom);

	// Activate keyboard navigation mode on first navigation key press
	if (bIsNavigationKey && !bKeyboardNavigationStarted)
	{
		ActivateKeyboardNavigation();
		
		// Don't perform navigation on this first press (just show focus)
		if (Key != EKeys::Enter && Key != EKeys::SpaceBar && Key != EKeys::Gamepad_FaceButton_Bottom)
		{
			return FReply::Handled();
		}
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
		// Right: Return to main zone (go to Credits)
		else if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right)
		{
			FocusMainZoneFromLeft();
			return FReply::Handled();
		}
		// Enter/Select: Open theme selector
		else if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
		{
			PlaySelectionSound();
			OpenThemeSelector();
			return FReply::Handled();
		}
	}
	//=========================================================================
	// RIGHT ZONE NAVIGATION (Leaderboard + Music Player)
	//=========================================================================
	else if (bRightZoneHasFocus)
	{
		// Up/Down: Navigate within right zone
		if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up)
		{
			NavigateRightZone(-1);
			return FReply::Handled();
		}
		else if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down)
		{
			NavigateRightZone(1);
			return FReply::Handled();
		}
		// Left: Return to main zone (or navigate within music player)
		else if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left)
		{
			// If on music player and it has internal navigation, handle that
			if (RightZoneFocusIndex == RIGHT_ZONE_INDEX_MUSIC_PLAYER && EmbeddedMusicPlayer && bMusicPlayerHasFocus)
			{
				if (MusicPlayerFocusIndex > 0)
				{
					// Navigate within music player
					NavigateMusicPlayer(-1);
					return FReply::Handled();
				}
			}
			// Return to main zone
			FocusMainZoneFromRight();
			return FReply::Handled();
		}
		// Right: Navigate within music player (if focused)
		else if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right)
		{
			if (RightZoneFocusIndex == RIGHT_ZONE_INDEX_MUSIC_PLAYER && EmbeddedMusicPlayer && bMusicPlayerHasFocus)
			{
				if (MusicPlayerFocusIndex < MUSIC_PLAYER_BUTTON_COUNT - 1)
				{
					NavigateMusicPlayer(1);
				}
				return FReply::Handled();
			}
			// If on Leaderboard, stay there (no further right)
			return FReply::Handled();
		}
		// Enter/Select: Activate the focused right zone item
		else if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
		{
			if (RightZoneFocusIndex == RIGHT_ZONE_INDEX_LEADERBOARD && LeaderboardButton)
			{
				PlaySelectionSound();
				OpenLeaderboard();
				return FReply::Handled();
			}
			else if (RightZoneFocusIndex == RIGHT_ZONE_INDEX_MUSIC_PLAYER && EmbeddedMusicPlayer)
			{
				SelectMusicPlayerButton();  // This already plays selection sound
				return FReply::Handled();
			}
		}
	}
	//=========================================================================
	// MAIN ZONE NAVIGATION (Start, Settings, Controls, Credits, Quit)
	//=========================================================================
	else
	{
		// Up/Down: Navigate within main menu (wrapping)
		if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up)
		{
			NavigateUp();
			return FReply::Handled();
		}
		else if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down)
		{
			NavigateDown();
			return FReply::Handled();
		}
		// Right: Go to right zone
		else if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right)
		{
			FocusRightZone();
			return FReply::Handled();
		}
		// Left: Go to left zone (only from bottom buttons: Credits or Quit)
		else if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left)
		{
			// Only allow left navigation from bottom buttons (Credits or Quit)
			if (ThemeButton && (CurrentFocusIndex == INDEX_CREDITS || CurrentFocusIndex == INDEX_QUIT))
			{
				FocusLeftZone();
			}
			return FReply::Handled();
		}
		// Enter/Select: Activate focused button
		else if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
		{
			SelectFocusedItem();
			return FReply::Handled();
		}
	}

	// Escape: No action on main menu
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

//=============================================================================
// RIGHT ZONE NAVIGATION
//=============================================================================

void UMainMenuWidget::FocusRightZone()
{
	if (!LeaderboardButton && !MusicPlayerWidget)
	{
		return; // No right zone items available
	}

	bRightZoneHasFocus = true;
	bLeftZoneHasFocus = false;
	
	// Choose right zone target based on vertical position in main menu:
	// - Credits (3) or Quit (4) are at the bottom → go to Music Player (also at the bottom)
	// - Start (0), Settings (1), Controls (2) are at the top → go to Leaderboard (also at the top)
	// This creates more intuitive horizontal navigation
	bool bGoToMusicPlayer = (CurrentFocusIndex == INDEX_CREDITS || CurrentFocusIndex == INDEX_QUIT);
	
	if (bGoToMusicPlayer && MusicPlayerWidget)
	{
		RightZoneFocusIndex = RIGHT_ZONE_INDEX_MUSIC_PLAYER;
	}
	else if (LeaderboardButton)
	{
		RightZoneFocusIndex = RIGHT_ZONE_INDEX_LEADERBOARD;
	}
	else if (MusicPlayerWidget)
	{
		RightZoneFocusIndex = RIGHT_ZONE_INDEX_MUSIC_PLAYER;
	}

	// Dim ALL main menu items and left zone
	DimMenuItems();
	UpdateLeftZoneFocusVisuals();
	
	// Update right zone visuals (this will call FocusMusicPlayer if needed)
	UpdateRightZoneFocusVisuals();
	
	// Only play navigation sound if NOT going to music player (FocusMusicPlayer already plays it)
	if (RightZoneFocusIndex != RIGHT_ZONE_INDEX_MUSIC_PLAYER)
	{
		PlayNavigationSound();
	}

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: Focused right zone (index %d) from main index %d"), RightZoneFocusIndex, CurrentFocusIndex);
}

void UMainMenuWidget::FocusMainZoneFromRight()
{
	// Remember which right zone item we're leaving
	int32 LeavingRightZoneIndex = RightZoneFocusIndex;
	
	bRightZoneHasFocus = false;
	bMusicPlayerHasFocus = false;
	
	// Dim and reset right zone items (Leaderboard button and music player)
	const FLinearColor UnfocusedColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
	
	if (LeaderboardButton)
	{
		LeaderboardButton->SetBackgroundColor(UnfocusedColor);
		LeaderboardButton->SetRenderScale(FVector2D(1.0f, 1.0f));
		if (LeaderboardLabel)
		{
			LeaderboardLabel->SetRenderOpacity(0.5f);
		}
	}
	
	// Reset music player buttons
	DimMusicPlayerButtons();
	
	// Determine which main menu item to return to:
	// - From Music Player → return to Credits (prevents accidental Quit)
	// - From Leaderboard → return to wherever we were
	int32 OldFocusIndex = CurrentFocusIndex;
	if (LeavingRightZoneIndex == RIGHT_ZONE_INDEX_MUSIC_PLAYER)
	{
		CurrentFocusIndex = INDEX_CREDITS;
	}
	
	// Restore main menu focus on the target item
	UpdateFocusVisuals(CurrentFocusIndex, OldFocusIndex);
	
	PlayNavigationSound();

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: Focused main zone from right (index %d, was %d)"), CurrentFocusIndex, OldFocusIndex);
}

void UMainMenuWidget::NavigateRightZone(int32 Direction)
{
	int32 OldIndex = RightZoneFocusIndex;
	
	if (Direction < 0)
	{
		// Up: Go to previous item
		if (RightZoneFocusIndex == RIGHT_ZONE_INDEX_MUSIC_PLAYER)
		{
			if (LeaderboardButton)
			{
				RightZoneFocusIndex = RIGHT_ZONE_INDEX_LEADERBOARD;
			}
		}
	}
	else
	{
		// Down: Go to next item
		if (RightZoneFocusIndex == RIGHT_ZONE_INDEX_LEADERBOARD)
		{
			if (MusicPlayerWidget)
			{
				RightZoneFocusIndex = RIGHT_ZONE_INDEX_MUSIC_PLAYER;
			}
		}
	}

	if (OldIndex != RightZoneFocusIndex)
	{
		bool bTransitioningToMusicPlayer = (RightZoneFocusIndex == RIGHT_ZONE_INDEX_MUSIC_PLAYER);
		
		UpdateRightZoneFocusVisuals();
		
		if (!bTransitioningToMusicPlayer)
		{
			PlayNavigationSound();
		}
		
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: Right zone navigation %d -> %d"), OldIndex, RightZoneFocusIndex);
	}
}

void UMainMenuWidget::UpdateRightZoneFocusVisuals()
{
	const FLinearColor FocusedColor = FLinearColor(0.4f, 1.0f, 1.0f);
	const FLinearColor UnfocusedColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
	const FVector2D FocusedScale = FVector2D(1.12f, 1.12f);
	const FVector2D UnfocusedScale = FVector2D(1.0f, 1.0f);

	// Update Leaderboard button
	if (LeaderboardButton)
	{
		bool bIsFocused = bRightZoneHasFocus && (RightZoneFocusIndex == RIGHT_ZONE_INDEX_LEADERBOARD);
		LeaderboardButton->SetBackgroundColor(bIsFocused ? FocusedColor : UnfocusedColor);
		LeaderboardButton->SetRenderScale(bIsFocused ? FocusedScale : UnfocusedScale);
		
		if (LeaderboardLabel)
		{
			LeaderboardLabel->SetRenderOpacity(bIsFocused ? 1.0f : 0.5f);
		}
	}

	// Handle music player focus
	if (MusicPlayerWidget)
	{
		bool bMusicFocused = bRightZoneHasFocus && (RightZoneFocusIndex == RIGHT_ZONE_INDEX_MUSIC_PLAYER);
		if (bMusicFocused && !bMusicPlayerHasFocus)
		{
			FocusMusicPlayer();
		}
		else if (!bMusicFocused && bMusicPlayerHasFocus)
		{
			bMusicPlayerHasFocus = false;
			DimMusicPlayerButtons();
		}
	}
}

//=============================================================================
// LEFT ZONE NAVIGATION
//=============================================================================

void UMainMenuWidget::FocusLeftZone()
{
	if (!ThemeButton)
	{
		return; // No left zone items available
	}

	bLeftZoneHasFocus = true;
	bRightZoneHasFocus = false;
	bMusicPlayerHasFocus = false;
	
	// Dim ALL main menu items and right zone
	DimMenuItems();
	DimMusicPlayerButtons();
	
	// Dim leaderboard button
	const FLinearColor UnfocusedColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
	if (LeaderboardButton)
	{
		LeaderboardButton->SetBackgroundColor(UnfocusedColor);
		LeaderboardButton->SetRenderScale(FVector2D(1.0f, 1.0f));
		if (LeaderboardLabel)
		{
			LeaderboardLabel->SetRenderOpacity(0.5f);
		}
	}
	
	// Update left zone visuals (highlight theme button)
	UpdateLeftZoneFocusVisuals();
	
	PlayNavigationSound();

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: Focused left zone (Theme button) from main index %d"), CurrentFocusIndex);
}

void UMainMenuWidget::FocusMainZoneFromLeft()
{
	bLeftZoneHasFocus = false;
	
	// Dim theme button
	UpdateLeftZoneFocusVisuals();
	
	// Return to Credits (safe destination near the Theme button's vertical position)
	int32 OldFocusIndex = CurrentFocusIndex;
	CurrentFocusIndex = INDEX_CREDITS;
	
	// Restore main menu focus
	UpdateFocusVisuals(CurrentFocusIndex, OldFocusIndex);
	
	PlayNavigationSound();

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: Focused main zone from left (going to Credits)"));
}

void UMainMenuWidget::UpdateLeftZoneFocusVisuals()
{
	if (!ThemeButton)
	{
		return;
	}

	const FLinearColor FocusedColor = FLinearColor(0.4f, 1.0f, 1.0f);
	const FLinearColor UnfocusedColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
	const FVector2D FocusedScale = FVector2D(1.12f, 1.12f);
	const FVector2D UnfocusedScale = FVector2D(1.0f, 1.0f);

	bool bIsFocused = bLeftZoneHasFocus;
	ThemeButton->SetBackgroundColor(bIsFocused ? FocusedColor : UnfocusedColor);
	ThemeButton->SetRenderScale(bIsFocused ? FocusedScale : UnfocusedScale);
	
	if (ThemeLabel)
	{
		ThemeLabel->SetRenderOpacity(bIsFocused ? 1.0f : 0.5f);
	}
}

//=============================================================================
// BUTTON CLICK HANDLERS
//=============================================================================

void UMainMenuWidget::OnStartGameClicked()
{
	StartGame();
}

void UMainMenuWidget::OnSettingsClicked()
{
	OpenSettings();
}

void UMainMenuWidget::OnControlsClicked()
{
	OpenControls();
}

void UMainMenuWidget::OnCreditsClicked()
{
	OpenCredits();
}

void UMainMenuWidget::OnLeaderboardClicked()
{
	OpenLeaderboard();
}

void UMainMenuWidget::OnThemeClicked()
{
	OpenThemeSelector();
}

void UMainMenuWidget::OnQuitClicked()
{
	QuitGame();
}

//=============================================================================
// PUBLIC ACTIONS
//=============================================================================

void UMainMenuWidget::StartGame()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: Starting game - Loading level: %s"), *GameplayLevelName.ToString());

	// Notify Blueprint
	OnBeforeStartGame();

	// Load the gameplay level
	if (!GameplayLevelName.IsNone())
	{
		// Remove widget from viewport BEFORE level transition to prevent world leak
		RemoveFromParent();
		
		UGameplayStatics::OpenLevel(this, GameplayLevelName);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MainMenuWidget: GameplayLevelName is not set!"));
	}
}

void UMainMenuWidget::OpenSettings()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: Opening Settings"));

	if (!SettingsWidgetClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MainMenuWidget: SettingsWidgetClass is not set!"));
		return;
	}

	SettingsWidget = CreateWidget<UArcadeMenuWidget>(GetOwningPlayer(), SettingsWidgetClass);
	if (SettingsWidget)
	{
		SettingsWidget->AddToViewport(10);
		SettingsWidget->TakeKeyboardFocus();
		SettingsWidget->OnBackPressed.AddDynamic(this, &UMainMenuWidget::CloseSettings);
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMainMenuWidget::QuitGame()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: Quitting game"));
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UMainMenuWidget::CloseSettings()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: Closing Settings"));

	if (SettingsWidget)
	{
		SettingsWidget->RemoveFromParent();
		SettingsWidget = nullptr;
	}

	SetVisibility(ESlateVisibility::Visible);
	TakeKeyboardFocus();
	OnSettingsMenuClosed();
}

void UMainMenuWidget::OpenControls()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: Opening Controls"));

	if (!ControlsWidgetClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MainMenuWidget: ControlsWidgetClass is not set!"));
		return;
	}

	ControlsWidget = CreateWidget<UArcadeMenuWidget>(GetOwningPlayer(), ControlsWidgetClass);
	if (ControlsWidget)
	{
		ControlsWidget->AddToViewport(10);
		ControlsWidget->TakeKeyboardFocus();
		ControlsWidget->OnBackPressed.AddDynamic(this, &UMainMenuWidget::CloseControls);
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMainMenuWidget::CloseControls()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: Closing Controls"));

	if (ControlsWidget)
	{
		ControlsWidget->RemoveFromParent();
		ControlsWidget = nullptr;
	}

	SetVisibility(ESlateVisibility::Visible);
	TakeKeyboardFocus();
}

void UMainMenuWidget::OpenCredits()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: Opening Credits"));

	if (!CreditsWidgetClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MainMenuWidget: CreditsWidgetClass is not set!"));
		return;
	}

	CreditsWidget = CreateWidget<UArcadeMenuWidget>(GetOwningPlayer(), CreditsWidgetClass);
	if (CreditsWidget)
	{
		CreditsWidget->AddToViewport(10);
		CreditsWidget->TakeKeyboardFocus();
		CreditsWidget->OnBackPressed.AddDynamic(this, &UMainMenuWidget::CloseCredits);
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMainMenuWidget::CloseCredits()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: Closing Credits"));

	if (CreditsWidget)
	{
		CreditsWidget->RemoveFromParent();
		CreditsWidget = nullptr;
	}

	SetVisibility(ESlateVisibility::Visible);
	TakeKeyboardFocus();
}

void UMainMenuWidget::OpenLeaderboard()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: Opening Leaderboard (view-only mode)"));

	if (!LeaderboardWidgetClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MainMenuWidget: LeaderboardWidgetClass is not set!"));
		return;
	}

	LeaderboardWidget = CreateWidget<ULeaderboardWidget>(GetOwningPlayer(), LeaderboardWidgetClass);
	if (LeaderboardWidget)
	{
		LeaderboardWidget->bViewOnlyMode = true;
		LeaderboardWidget->AddToViewport(10);
		LeaderboardWidget->TakeKeyboardFocus();
		LeaderboardWidget->OnBackPressed.AddDynamic(this, &UMainMenuWidget::CloseLeaderboard);
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMainMenuWidget::CloseLeaderboard()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: Closing Leaderboard"));

	if (LeaderboardWidget)
	{
		LeaderboardWidget->OnBackPressed.RemoveDynamic(this, &UMainMenuWidget::CloseLeaderboard);
		LeaderboardWidget->RemoveFromParent();
		LeaderboardWidget = nullptr;
	}

	SetVisibility(ESlateVisibility::Visible);
	TakeKeyboardFocus();
}

void UMainMenuWidget::OpenThemeSelector()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: Opening Theme Selector"));

	if (!ThemeSelectorWidgetClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MainMenuWidget: ThemeSelectorWidgetClass is not set!"));
		return;
	}

	ThemeSelectorWidget = CreateWidget<UThemeSelectorWidget>(GetOwningPlayer(), ThemeSelectorWidgetClass);
	if (ThemeSelectorWidget)
	{
		ThemeSelectorWidget->AddToViewport(10);
		ThemeSelectorWidget->TakeKeyboardFocus();
		ThemeSelectorWidget->OnBackPressed.AddDynamic(this, &UMainMenuWidget::CloseThemeSelector);
		ThemeSelectorWidget->OnThemeApplied.AddDynamic(this, &UMainMenuWidget::ShowThemeNotification);
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMainMenuWidget::CloseThemeSelector()
{
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: Closing Theme Selector"));

	if (ThemeSelectorWidget)
	{
		ThemeSelectorWidget->OnBackPressed.RemoveDynamic(this, &UMainMenuWidget::CloseThemeSelector);
		ThemeSelectorWidget->OnThemeApplied.RemoveDynamic(this, &UMainMenuWidget::ShowThemeNotification);
		ThemeSelectorWidget->RemoveFromParent();
		ThemeSelectorWidget = nullptr;
	}

	SetVisibility(ESlateVisibility::Visible);
	TakeKeyboardFocus();
}

//=============================================================================
// THEME NOTIFICATION
//=============================================================================

void UMainMenuWidget::ShowThemeNotification(const FString& ThemeName)
{
	FString NotificationText = FString::Printf(TEXT("%s Applied!"), *ThemeName);

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: ShowThemeNotification called with theme: %s"), *ThemeName);

	if (ThemeNotificationText)
	{
		ThemeNotificationText->SetText(FText::FromString(NotificationText));
		ThemeNotificationText->SetVisibility(ESlateVisibility::Visible);
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("MainMenuWidget: Set notification text to: %s"), *NotificationText);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MainMenuWidget: ThemeNotificationText is not bound!"));
	}

	if (ThemeNotificationContainer)
	{
		ThemeNotificationContainer->SetVisibility(ESlateVisibility::Visible);
	}

	// Set timer to hide notification
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ThemeNotificationTimerHandle,
			this,
			&UMainMenuWidget::HideThemeNotification,
			ThemeNotificationDuration,
			false
		);
	}
}

void UMainMenuWidget::HideThemeNotification()
{
	if (ThemeNotificationText)
	{
		ThemeNotificationText->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (ThemeNotificationContainer)
	{
		ThemeNotificationContainer->SetVisibility(ESlateVisibility::Collapsed);
	}
}

//=============================================================================
// OVERRIDES
//=============================================================================

void UMainMenuWidget::OnItemSelected_Implementation(int32 ItemIndex)
{
	// Button clicks are already handled by OnClicked delegates
}

void UMainMenuWidget::OnBackAction_Implementation()
{
	// On main menu, Escape does nothing
}
