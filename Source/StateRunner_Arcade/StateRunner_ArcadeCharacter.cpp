#include "StateRunner_ArcadeCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "DrawDebugHelpers.h"  // For debug visualization
#include "StateRunner_Arcade.h"
#include "OverclockSystemComponent.h"
#include "PickupSpawnerComponent.h"
#include "StateRunner_ArcadeGameMode.h"
#include "Camera/CameraShakeBase.h"  // For camera shake effects
#include "Kismet/GameplayStatics.h"  // For sound playback
#include "Sound/SoundBase.h"

// --- Constructor ---

AStateRunner_ArcadeCharacter::AStateRunner_ArcadeCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Disable all controller rotation - player faces forward always
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement - mostly disabled for lane-based movement
	// We keep the component active but won't use standard movement input
	GetCharacterMovement()->bOrientRotationToMovement = false;  // Don't rotate to face movement
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 0.0f, 0.0f);  // No rotation

	// Jump parameters - will be used in Phase 2
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	
	// Disable walking since we use direct position control for lane switching
	GetCharacterMovement()->MaxWalkSpeed = 0.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 0.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Enable crouch for slide system - UE's crouch handles capsule resizing
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCharacterMovement()->bCanWalkOffLedgesWhenCrouching = true;
	// CrouchedHalfHeight is set in BeginPlay after SlideCapsuleHalfHeight is finalized

	// Create camera boom - FIXED rotation, does NOT follow controller
	// Camera stays behind player, no rotation based on input
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 1000.0f;  // Matches Blueprint default
	CameraBoom->bUsePawnControlRotation = false;  // Camera does NOT rotate with controller
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bDoCollisionTest = false;  // No collision adjustment for endless runner
	
	// Set camera rotation to look at player from behind (matches Blueprint: -6° pitch)
	CameraBoom->SetRelativeRotation(FRotator(-6.0f, 0.0f, 0.0f));

	// Create follow camera (matches Blueprint: Z offset 560, -22.5° pitch)
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;  // Camera rotation fixed
	FollowCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 560.0f));
	FollowCamera->SetRelativeRotation(FRotator(-22.5f, 0.0f, 0.0f));

	// Create top-down camera boom (alternate F-Zero X inspired view)
	// Starting defaults - tweak in BP_StateRunnerCharacter as needed
	TopDownCameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("TopDownCameraBoom"));
	TopDownCameraBoom->SetupAttachment(RootComponent);
	TopDownCameraBoom->TargetArmLength = 500.0f;  // Lower default to stay inside tunnel
	TopDownCameraBoom->bUsePawnControlRotation = false;
	TopDownCameraBoom->bInheritPitch = false;
	TopDownCameraBoom->bInheritYaw = false;
	TopDownCameraBoom->bInheritRoll = false;
	TopDownCameraBoom->bDoCollisionTest = false;
	TopDownCameraBoom->SetRelativeRotation(FRotator(-45.0f, 0.0f, 0.0f));  // Less steep angle

	// Create top-down camera (starts inactive)
	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(TopDownCameraBoom, USpringArmComponent::SocketName);
	TopDownCamera->bUsePawnControlRotation = false;
	TopDownCamera->SetAutoActivate(false);  // Starts disabled

	// Disable tick by default - only enable when lane switching
	// This is a performance optimization as per design doc
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Initialize lane state
	CurrentLane = ELanePosition::Center;
	TargetLane = ELanePosition::Center;
	bIsLaneSwitching = false;

	// Initialize jump state
	bIsJumping = false;
	CurrentJumpOffset = 0.0f;
	CurrentJumpMaxHeight = 0.0f;
	JumpHoldTime = 0.0f;
	bIsJumpRising = false;
	ApexHangTimeRemaining = 0.0f;

	// Initialize slide state
	bIsSliding = false;
	SlideTimeRemaining = 0.0f;
	CurrentSlideMaxDuration = 0.0f;
	SlideHoldTime = 0.0f;
	bIsSlideKeyHeld = false;
	SlideZOffset = 0.0f;

	// Initialize fast fall state
	bIsFastFalling = false;
	bQueueSlideOnLand = false;
}

// --- Begin Play ---

void AStateRunner_ArcadeCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Set initial position at locked coordinates
	// COORDINATE SYSTEM: X = forward (locked at -5000), Y = lateral (lanes), Z = height (locked at 150)
	const float InitialY = GetLaneYPosition(CurrentLane);
	
	// If starting below ground for intro effect, apply the offset immediately
	if (bStartBelowGround)
	{
		CurrentRiseOffset = -RiseStartOffset;
		SetActorLocation(FVector(LockedXPosition, InitialY, BaseZPosition + CurrentRiseOffset));
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("Character starting below ground (offset: %.2f) - waiting for StartRiseFromGround()"), CurrentRiseOffset);
	}
	else
	{
		SetActorLocation(FVector(LockedXPosition, InitialY, BaseZPosition));
	}
	
	// Ensure character faces forward (positive X direction, into the tunnel)
	SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));

	// Configure crouch height for slide system
	// Using UE's crouch system ensures the movement component properly handles
	// capsule resizing without fighting against our position control
	GetCharacterMovement()->CrouchedHalfHeight = SlideCapsuleHalfHeight;

	// Initialize camera zoom system - uses current CameraBoom arm length as baseline
	if (CameraBoom)
	{
		// Uses current Blueprint arm length as baseline
		NormalCameraArmLength = CameraBoom->TargetArmLength;
		TargetCameraArmLength = NormalCameraArmLength;
	}
	
	// Cache top-down boom's Blueprint Z offset for independent height maintenance
	if (TopDownCameraBoom)
	{
		TopDownCameraBoomBaseZ = TopDownCameraBoom->GetRelativeLocation().Z;
	}

	// Lock cameras before Tick enables to prevent snap during intro rise
	EnforceCameraLaneLock();

}

// --- End Play ---

void AStateRunner_ArcadeCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear pending timers — timer lambdas capture object pointers that go invalid on restart
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageShakeResetTimerHandle);
		World->GetTimerManager().ClearTimer(DamageFadeOutTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

// --- Tick ---

void AStateRunner_ArcadeCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Process intro rise effect if active
	if (bIsRising)
	{
		ProcessRiseEffect(DeltaTime);
	}

	// Process lane switching if active
	if (bIsLaneSwitching)
	{
		ProcessLaneSwitching(DeltaTime);
	}

	// Process jump if active
	if (bIsJumping)
	{
		ProcessJump(DeltaTime);
	}

	// Process slide if active
	if (bIsSliding)
	{
		ProcessSlide(DeltaTime);
	}

	// Process camera zoom (OVERCLOCK effect)
	ProcessCameraZoom(DeltaTime);

	// Keep cameras centered on track (don't follow lane switches)
	EnforceCameraLaneLock();

	// Always enforce position lock (X locked, Z = BaseZPosition + JumpOffset + RiseOffset)
	EnforcePositionLock();

	// Update tick state - disable tick if no active movement AND no camera zoom in progress
	const bool bCameraZoomInProgress = CameraBoom && 
		FMath::Abs(CameraBoom->TargetArmLength - TargetCameraArmLength) > 1.0f;
	
	if (!IsAnyMovementActive() && !bCameraZoomInProgress && !bIsRising)
	{
		SetActorTickEnabled(false);
	}
}

// --- Input Setup ---

void AStateRunner_ArcadeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Lane switching - the primary movement for this game
		// Holdable: press to start switching, hold to continue switching across multiple lanes
		if (LaneLeftAction)
		{
			EnhancedInputComponent->BindAction(LaneLeftAction, ETriggerEvent::Started, this, &AStateRunner_ArcadeCharacter::OnLaneLeftPressed);
			EnhancedInputComponent->BindAction(LaneLeftAction, ETriggerEvent::Completed, this, &AStateRunner_ArcadeCharacter::OnLaneLeftReleased);
		}
		else
		{
			UE_LOG(LogStateRunner_Arcade, Warning, TEXT("LaneLeftAction is not assigned! Please assign IA_LaneLeft in the Blueprint."));
		}

		if (LaneRightAction)
		{
			EnhancedInputComponent->BindAction(LaneRightAction, ETriggerEvent::Started, this, &AStateRunner_ArcadeCharacter::OnLaneRightPressed);
			EnhancedInputComponent->BindAction(LaneRightAction, ETriggerEvent::Completed, this, &AStateRunner_ArcadeCharacter::OnLaneRightReleased);
		}
		else
		{
			UE_LOG(LogStateRunner_Arcade, Warning, TEXT("LaneRightAction is not assigned! Please assign IA_LaneRight in the Blueprint."));
		}

		// Jump - Custom holdable jump system
		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AStateRunner_ArcadeCharacter::OnJumpPressed);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AStateRunner_ArcadeCharacter::OnJumpReleased);
		}
		else
		{
			UE_LOG(LogStateRunner_Arcade, Warning, TEXT("JumpAction is not assigned! Please assign IA_Jump in the Blueprint."));
		}

		// Slide - Holdable slide system
		if (SlideAction)
		{
			EnhancedInputComponent->BindAction(SlideAction, ETriggerEvent::Started, this, &AStateRunner_ArcadeCharacter::OnSlidePressed);
			EnhancedInputComponent->BindAction(SlideAction, ETriggerEvent::Completed, this, &AStateRunner_ArcadeCharacter::OnSlideReleased);
		}
		else
		{
			UE_LOG(LogStateRunner_Arcade, Warning, TEXT("SlideAction is not assigned! Please assign IA_Slide in the Blueprint."));
		}

		// OVERCLOCK - Hold to activate speed boost
		if (OverclockAction)
		{
			EnhancedInputComponent->BindAction(OverclockAction, ETriggerEvent::Started, this, &AStateRunner_ArcadeCharacter::OnOverclockPressed);
			EnhancedInputComponent->BindAction(OverclockAction, ETriggerEvent::Completed, this, &AStateRunner_ArcadeCharacter::OnOverclockReleased);
		}
		else
		{
			UE_LOG(LogStateRunner_Arcade, Warning, TEXT("OverclockAction is not assigned! Please assign IA_Overclock in the Blueprint."));
		}

		// Camera Switch - HOLD to use top-down camera view
		if (CameraSwitchAction)
		{
			EnhancedInputComponent->BindAction(CameraSwitchAction, ETriggerEvent::Started, this, &AStateRunner_ArcadeCharacter::OnCameraSwitchPressed);
			EnhancedInputComponent->BindAction(CameraSwitchAction, ETriggerEvent::Completed, this, &AStateRunner_ArcadeCharacter::OnCameraSwitchReleased);
		}
		// CameraSwitchAction is optional - no warning if not assigned

		// MoveAction, LookAction, MouseLookAction intentionally not bound (lane-based game)
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system."), *GetNameSafe(this));
	}

	// DEBUG: Bind 9 key for difficulty increase (non-shipping only)
	// Uses raw keyboard binding since this isn't part of the normal input system
#if !UE_BUILD_SHIPPING
	if (PlayerInputComponent)
	{
		PlayerInputComponent->BindKey(EKeys::Nine, IE_Pressed, this, &AStateRunner_ArcadeCharacter::OnDebugIncreaseDifficulty);
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("9 key bound for difficulty increase"));
	}
#endif
}

// --- Lane Switching ---

void AStateRunner_ArcadeCharacter::SwitchLaneLeft()
{
	// Block input during intro/countdown
	if (!bGameplayInputEnabled)
	{
		return;
	}

	// Cannot switch if already at leftmost lane
	if (CurrentLane == ELanePosition::Left)
	{
		return;
	}

	// Can't switch while already mid-switch
	if (bIsLaneSwitching)
	{
		return;
	}

	// Lane switching does NOT cancel slide — allows fluid slide+dodge gameplay
	// Only jump cancels slide

	// Determine target lane (one to the left)
	switch (CurrentLane)
	{
		case ELanePosition::Center:
			TargetLane = ELanePosition::Left;
			break;
		case ELanePosition::Right:
			TargetLane = ELanePosition::Center;
			break;
		default:
			return;  // Already at left, shouldn't reach here
	}

	// Begin lane switch
	bIsLaneSwitching = true;
	SetActorTickEnabled(true);  // Enable tick for smooth interpolation

	// Play lane switch sound
	PlayPlayerSound(LaneSwitchSound);

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("Switching lane LEFT: %d -> %d"), 
		static_cast<int32>(CurrentLane), static_cast<int32>(TargetLane));
}

void AStateRunner_ArcadeCharacter::SwitchLaneRight()
{
	// Block input during intro/countdown
	if (!bGameplayInputEnabled)
	{
		return;
	}

	// Cannot switch if already at rightmost lane
	if (CurrentLane == ELanePosition::Right)
	{
		return;
	}

	// Can't switch while already mid-switch
	if (bIsLaneSwitching)
	{
		return;
	}

	// Lane switching does NOT cancel slide — allows fluid slide+dodge gameplay
	// Only jump cancels slide

	// Determine target lane (one to the right)
	switch (CurrentLane)
	{
		case ELanePosition::Left:
			TargetLane = ELanePosition::Center;
			break;
		case ELanePosition::Center:
			TargetLane = ELanePosition::Right;
			break;
		default:
			return;  // Already at right, shouldn't reach here
	}

	// Begin lane switch
	bIsLaneSwitching = true;
	SetActorTickEnabled(true);  // Enable tick for smooth interpolation

	// Play lane switch sound
	PlayPlayerSound(LaneSwitchSound);

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("Switching lane RIGHT: %d -> %d"), 
		static_cast<int32>(CurrentLane), static_cast<int32>(TargetLane));
}

float AStateRunner_ArcadeCharacter::GetLaneYPosition(ELanePosition Lane) const
{
	switch (Lane)
	{
		case ELanePosition::Left:
			return LanePositionLeft;
		case ELanePosition::Center:
			return LanePositionCenter;
		case ELanePosition::Right:
			return LanePositionRight;
		default:
			return LanePositionCenter;
	}
}

void AStateRunner_ArcadeCharacter::ProcessLaneSwitching(float DeltaTime)
{
	// Get current and target Y positions
	const FVector CurrentLocation = GetActorLocation();
	const float CurrentY = CurrentLocation.Y;
	const float TargetY = GetLaneYPosition(TargetLane);
	const float Distance = FMath::Abs(TargetY - CurrentY);

	// Z position depends on state (jump = offset, slide = preserve, else = base)
	// Lane switching is allowed during slide for fluid gameplay
	float TargetZ;
	if (bIsJumping)
	{
		TargetZ = BaseZPosition + CurrentJumpOffset;
	}
	else if (bIsSliding)
	{
		// During slide, preserve current Z - let crouch system control it
		TargetZ = CurrentLocation.Z;
	}
	else
	{
		TargetZ = BaseZPosition;
	}

	// Check if we've reached the target (within snap threshold)
	if (Distance <= LaneSnapThreshold)
	{
		// Snap to exact target Y position, preserve appropriate Z
		SetActorLocation(FVector(LockedXPosition, TargetY, TargetZ));
		
		// Update lane state
		CurrentLane = TargetLane;
		bIsLaneSwitching = false;

		UE_LOG(LogStateRunner_Arcade, Log, TEXT("Lane switch complete. Now in lane: %d at Y: %.2f"), 
			static_cast<int32>(CurrentLane), TargetY);

		// Check if lane key is still held - queue another switch if possible
		// This allows holding left/right to smoothly cross multiple lanes
		if (bIsLaneLeftHeld && CurrentLane != ELanePosition::Left)
		{
			SwitchLaneLeft();
			return;  // New switch started, keep tick enabled
		}
		else if (bIsLaneRightHeld && CurrentLane != ELanePosition::Right)
		{
			SwitchLaneRight();
			return;  // New switch started, keep tick enabled
		}
		
		// Only disable tick if no other movement is active (jump may still be processing)
		if (!IsAnyMovementActive())
		{
			SetActorTickEnabled(false);
		}
		return;
	}

	// Calculate movement this frame
	const float Direction = FMath::Sign(TargetY - CurrentY);
	const float Movement = LaneSwitchSpeed * DeltaTime * Direction;
	
	// Calculate new Y position, clamped to not overshoot
	float NewY;
	if (Direction > 0)
	{
		// Moving right (positive Y)
		NewY = FMath::Min(CurrentY + Movement, TargetY);
	}
	else
	{
		// Moving left (negative Y)
		NewY = FMath::Max(CurrentY + Movement, TargetY);
	}

	// Apply new position - X locked, Y interpolating, Z based on state
	SetActorLocation(FVector(LockedXPosition, NewY, TargetZ));
}

void AStateRunner_ArcadeCharacter::EnforcePositionLock()
{
	// Get current position
	const FVector CurrentLocation = GetActorLocation();
	
	// Always enforce X position (player locked at -5000)
	const bool bXDrifted = !FMath::IsNearlyEqual(CurrentLocation.X, LockedXPosition, 0.1f);
	
	// Z position handling depends on current state:
	// - During jump: We manually control Z via CurrentJumpOffset
	// - During slide/crouch: UE's crouch system handles Z - do NOT fight it
	// - Standing normally: Z should be at BaseZPosition
	bool bShouldEnforceZ = false;
	float ExpectedZ = CurrentLocation.Z;  // Default: preserve current Z
	
	if (bIsJumping)
	{
		// During jump, we control Z manually (include rise offset)
		ExpectedZ = BaseZPosition + CurrentJumpOffset + CurrentRiseOffset;
		bShouldEnforceZ = true;
	}
	else if (bIsSliding)
	{
		// During slide, UE's crouch system handles Z position
		// Do NOT enforce Z - this prevents the per-frame fight between
		// our position lock and the CharacterMovementComponent
		bShouldEnforceZ = false;
	}
	else
	{
		// Standing normally - enforce base Z position (include rise offset for intro)
		ExpectedZ = BaseZPosition + CurrentRiseOffset;
		bShouldEnforceZ = true;
	}
	
	// Check if Z has drifted (only when we should be enforcing it)
	const bool bZDrifted = bShouldEnforceZ && !FMath::IsNearlyEqual(CurrentLocation.Z, ExpectedZ, 0.1f);
	
	if (bXDrifted || bZDrifted)
	{
		// Correct position drift while preserving Y (lane position)
		const float FinalZ = bZDrifted ? ExpectedZ : CurrentLocation.Z;
		SetActorLocation(FVector(LockedXPosition, CurrentLocation.Y, FinalZ));
		
		if (bXDrifted)
		{
			UE_LOG(LogStateRunner_Arcade, Warning, TEXT("X position drifted to %.2f, corrected to %.2f"), 
				CurrentLocation.X, LockedXPosition);
		}
		if (bZDrifted)
		{
			UE_LOG(LogStateRunner_Arcade, Warning, TEXT("Z position drifted to %.2f, corrected to %.2f"), 
				CurrentLocation.Z, ExpectedZ);
		}
	}
}

// --- Input Callbacks: Lane Switching ---

void AStateRunner_ArcadeCharacter::OnLaneLeftPressed()
{
	bIsLaneLeftHeld = true;
	bIsLaneRightHeld = false;  // Cancel opposite direction
	SwitchLaneLeft();
}

void AStateRunner_ArcadeCharacter::OnLaneLeftReleased()
{
	bIsLaneLeftHeld = false;
}

void AStateRunner_ArcadeCharacter::OnLaneRightPressed()
{
	bIsLaneRightHeld = true;
	bIsLaneLeftHeld = false;  // Cancel opposite direction
	SwitchLaneRight();
}

void AStateRunner_ArcadeCharacter::OnLaneRightReleased()
{
	bIsLaneRightHeld = false;
}

// --- Jump System ---

void AStateRunner_ArcadeCharacter::StartJump()
{
	// Block input during intro/countdown
	if (!bGameplayInputEnabled)
	{
		return;
	}

	// Cannot jump if already jumping
	if (bIsJumping)
	{
		return;
	}

	// Jump cancels slide - jump takes priority
	if (bIsSliding)
	{
		ForceEndSlide();
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("Slide cancelled by jump"));
	}

	// Initialize jump state
	bIsJumping = true;
	bIsJumpRising = true;
	bJumpReleasedEarly = false;
	CurrentJumpOffset = 0.0f;
	CurrentJumpMaxHeight = JumpBaseHeight;  // Will increase as player holds
	JumpHoldTime = 0.0f;
	ApexHangTimeRemaining = 0.0f;

	// Enable tick for jump processing
	SetActorTickEnabled(true);

	// Play jump sound
	PlayPlayerSound(JumpSound);

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("Jump started"));
}

void AStateRunner_ArcadeCharacter::EndJump()
{
	// Only end jump if we're in rising phase
	// If already falling, this does nothing (prevents double-release issues)
	if (!bIsJumping || !bIsJumpRising)
	{
		return;
	}

	// QOL: If released before MinTapJumpHeight, keep rising so a quick tap still clears low obstacles
	if (CurrentJumpOffset < MinTapJumpHeight)
	{
		bJumpReleasedEarly = true;
		// Set max height to at least MinTapJumpHeight so we continue rising
		CurrentJumpMaxHeight = FMath::Max(CurrentJumpMaxHeight, MinTapJumpHeight);
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("Jump released early at %.2f - continuing to MinTapJumpHeight: %.2f"), CurrentJumpOffset, MinTapJumpHeight);
		return;  // Don't transition to falling yet
	}

	// Transition to falling phase
	bIsJumpRising = false;
	bJumpReleasedEarly = false;

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("Jump released - transitioning to fall. Max height: %.2f"), CurrentJumpMaxHeight);
}

void AStateRunner_ArcadeCharacter::ProcessJump(float DeltaTime)
{
	if (!bIsJumping)
	{
		return;
	}

	// Update hold time if still rising
	if (bIsJumpRising)
	{
		JumpHoldTime += DeltaTime;

		// Only grow max height if player is still holding (not released early)
		if (!bJumpReleasedEarly)
		{
			// Calculate max height based on hold time (capped at JumpHoldTimeToMax)
			// Using an eased curve for more responsive feel at start
			const float HoldProgress = FMath::Clamp(JumpHoldTime / JumpHoldTimeToMax, 0.0f, 1.0f);
			CurrentJumpMaxHeight = JumpBaseHeight * (1.0f + (HoldProgress * (JumpHoldMultiplier - 1.0f)));
		}

		// Rise toward max height - use faster speed for snappy arcade feel
		CurrentJumpOffset += JumpRiseSpeed * DeltaTime;

		// Check if we've reached min tap height after early release - transition to falling
		if (bJumpReleasedEarly && CurrentJumpOffset >= MinTapJumpHeight)
		{
			CurrentJumpOffset = MinTapJumpHeight;
			bIsJumpRising = false;
			bJumpReleasedEarly = false;
			ApexHangTimeRemaining = JumpApexHangTime;
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("Jump reached MinTapJumpHeight: %.2f after early release, entering apex hang"), MinTapJumpHeight);
		}
		// Check if we've reached max height (normal case)
		else if (CurrentJumpOffset >= CurrentJumpMaxHeight)
		{
			CurrentJumpOffset = CurrentJumpMaxHeight;
			bIsJumpRising = false;  // Transition to apex/falling
			bJumpReleasedEarly = false;
			ApexHangTimeRemaining = JumpApexHangTime;  // Start apex hang time
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("Jump reached max height: %.2f, entering apex hang"), CurrentJumpMaxHeight);
		}
	}
	else
	{
		// --- Fast fall check ---
		if (bIsFastFalling)
		{
			// Fast fall - use maximum fall speed, no hang time
			CurrentJumpOffset -= FastFallSpeed * DeltaTime;

			// Check if we've landed
			if (CurrentJumpOffset <= 0.0f)
			{
				CurrentJumpOffset = 0.0f;
				bIsJumping = false;
				bIsJumpRising = false;
				bJumpReleasedEarly = false;
				JumpHoldTime = 0.0f;
				CurrentJumpMaxHeight = 0.0f;
				ApexHangTimeRemaining = 0.0f;
				bIsFastFalling = false;

				UE_LOG(LogStateRunner_Arcade, Log, TEXT("Fast fall completed - landed"));

				// Immediately transition to slide if queued
				if (bQueueSlideOnLand)
				{
					bQueueSlideOnLand = false;
					// Directly start the slide (we're now on the ground)
					StartSlide();
				}
			}
		}
		// Apex hang time — brief float at peak for arcade feel
		else if (ApexHangTimeRemaining > 0.0f)
		{
			// Still in apex hang - don't fall yet, just count down
			ApexHangTimeRemaining -= DeltaTime;
			
			// Optional: slight "bob" at apex for visual interest
			// CurrentJumpOffset stays at max during hang
		}
		else
		{
			// Normal falling phase - decrease offset back to 0
			// Check if we're near apex (within threshold) - slow fall speed for more hang feel
			const float ApexThresholdHeight = CurrentJumpMaxHeight * (1.0f - JumpApexThreshold);
			float EffectiveFallSpeed = JumpFallSpeed;
			
			if (CurrentJumpOffset > ApexThresholdHeight)
			{
				// Near apex - use slower fall speed for an "arcade" style hang feel
				EffectiveFallSpeed = JumpFallSpeed * 0.5f;
			}
			
			CurrentJumpOffset -= EffectiveFallSpeed * DeltaTime;

			// Check if we've landed
			if (CurrentJumpOffset <= 0.0f)
			{
				CurrentJumpOffset = 0.0f;
				bIsJumping = false;
				bIsJumpRising = false;
				bJumpReleasedEarly = false;
				JumpHoldTime = 0.0f;
				CurrentJumpMaxHeight = 0.0f;
				ApexHangTimeRemaining = 0.0f;

				UE_LOG(LogStateRunner_Arcade, Log, TEXT("Jump completed - landed"));
			}
		}
	}

	// Apply jump offset to position (X and Y stay locked, Z = BaseZPosition + offset)
	const FVector CurrentLocation = GetActorLocation();
	SetActorLocation(FVector(LockedXPosition, CurrentLocation.Y, BaseZPosition + CurrentJumpOffset));
}

void AStateRunner_ArcadeCharacter::OnJumpPressed()
{
	StartJump();
}

void AStateRunner_ArcadeCharacter::OnJumpReleased()
{
	EndJump();
}

// --- Slide System ---

void AStateRunner_ArcadeCharacter::StartSlide()
{
	// Block input during intro/countdown
	if (!bGameplayInputEnabled)
	{
		return;
	}

	// Cannot slide if already sliding
	if (bIsSliding)
	{
		return;
	}

	// FAST FALL: If jumping, trigger fast fall to slide instead of blocking
	if (bIsJumping)
	{
		StartFastFall();
		return;
	}

	// Cannot slide if already fast falling (wait for landing)
	if (bIsFastFalling)
	{
		return;
	}

	// Initialize slide state
	bIsSliding = true;
	bIsSlideKeyHeld = true;  // Key is held when slide starts
	SlideHoldTime = 0.0f;
	SlideTimeRemaining = SlideBaseDuration;  // Will increase as player holds
	CurrentSlideMaxDuration = SlideBaseDuration;

	// Calculate Z offset to keep feet on ground when capsule shrinks
	SlideZOffset = NormalCapsuleHalfHeight - SlideCapsuleHalfHeight;

	// HYBRID APPROACH:
	// 1. Notify movement component we're crouching (prevents Z position fighting)
	// 2. Manually apply capsule resize (since our MaxWalkSpeed=0 means crouch won't auto-apply)
	// 3. Manually set position (to get immediate visual feedback)
	Crouch();  // Tell movement component we're crouching
	GetCapsuleComponent()->SetCapsuleHalfHeight(SlideCapsuleHalfHeight);  // Manual resize
	
	// Apply Z offset to lower character (keeps capsule bottom at floor level)
	const FVector CurrentLocation = GetActorLocation();
	SetActorLocation(FVector(LockedXPosition, CurrentLocation.Y, BaseZPosition - SlideZOffset));

	// Compensate camera boom so the camera doesn't dip when the character lowers
	if (CameraBoom)
	{
		FVector BoomLocation = CameraBoom->GetRelativeLocation();
		BoomLocation.Z += SlideZOffset;
		CameraBoom->SetRelativeLocation(BoomLocation);
	}

	// Compensate mesh position to prevent it from sinking underground
	// The actor was lowered, so we raise the mesh back up by the same amount
	if (GetMesh())
	{
		FVector MeshLocation = GetMesh()->GetRelativeLocation();
		MeshLocation.Z += SlideZOffset;
		GetMesh()->SetRelativeLocation(MeshLocation);
	}

	// Enable tick for slide processing
	SetActorTickEnabled(true);

	// Play slide sound
	PlayPlayerSound(SlideSound);

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("Slide started - Z offset: %.2f, mesh compensated"), SlideZOffset);
}

void AStateRunner_ArcadeCharacter::EndSlide()
{
	// Only end slide if we're actually sliding
	if (!bIsSliding)
	{
		return;
	}

	// Mark key as released (slide continues but stops extending)
	bIsSlideKeyHeld = false;

	// If slide time is exhausted, end it
	if (SlideTimeRemaining <= 0.0f)
	{
		// HYBRID APPROACH:
		// 1. Notify movement component we're uncrouching
		// 2. Manually restore capsule height
		// 3. Manually restore position
		UnCrouch();  // Tell movement component we're standing
		GetCapsuleComponent()->SetCapsuleHalfHeight(NormalCapsuleHalfHeight);  // Manual restore
		
		// Restore Z position to normal standing height
		const FVector CurrentLocation = GetActorLocation();
		SetActorLocation(FVector(LockedXPosition, CurrentLocation.Y, BaseZPosition));

		// Restore camera boom to original position
		if (CameraBoom)
		{
			FVector BoomLocation = CameraBoom->GetRelativeLocation();
			BoomLocation.Z -= SlideZOffset;  // Remove the compensation we added
			CameraBoom->SetRelativeLocation(BoomLocation);
		}

		// Restore mesh position (remove the compensation we added)
		if (GetMesh())
		{
			FVector MeshLocation = GetMesh()->GetRelativeLocation();
			MeshLocation.Z -= SlideZOffset;
			GetMesh()->SetRelativeLocation(MeshLocation);
		}

		// End slide state
		bIsSliding = false;
		SlideTimeRemaining = 0.0f;
		CurrentSlideMaxDuration = 0.0f;
		SlideHoldTime = 0.0f;
		SlideZOffset = 0.0f;

		UE_LOG(LogStateRunner_Arcade, Log, TEXT("Slide ended - capsule and mesh restored"));
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("Slide key released - slide continues for %.2f seconds"), SlideTimeRemaining);
	}
}

void AStateRunner_ArcadeCharacter::ForceEndSlide()
{
	// Only end slide if we're actually sliding
	if (!bIsSliding)
	{
		return;
	}

	// HYBRID APPROACH:
	// 1. Notify movement component we're uncrouching
	// 2. Manually restore capsule height
	// 3. Manually restore position
	UnCrouch();  // Tell movement component we're standing
	GetCapsuleComponent()->SetCapsuleHalfHeight(NormalCapsuleHalfHeight);  // Manual restore
	
	// Restore Z position to normal standing height
	const FVector CurrentLocation = GetActorLocation();
	SetActorLocation(FVector(LockedXPosition, CurrentLocation.Y, BaseZPosition));

	// Restore camera boom to original position
	if (CameraBoom)
	{
		FVector BoomLocation = CameraBoom->GetRelativeLocation();
		BoomLocation.Z -= SlideZOffset;  // Remove the compensation we added
		CameraBoom->SetRelativeLocation(BoomLocation);
	}

	// Restore mesh position (remove the compensation we added)
	if (GetMesh())
	{
		FVector MeshLocation = GetMesh()->GetRelativeLocation();
		MeshLocation.Z -= SlideZOffset;
		GetMesh()->SetRelativeLocation(MeshLocation);
	}

	// End slide completely
	bIsSliding = false;
	bIsSlideKeyHeld = false;
	SlideTimeRemaining = 0.0f;
	CurrentSlideMaxDuration = 0.0f;
	SlideHoldTime = 0.0f;
	SlideZOffset = 0.0f;

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("Slide FORCE ended - cancelled by jump, mesh restored"));
}

void AStateRunner_ArcadeCharacter::ProcessSlide(float DeltaTime)
{
	if (!bIsSliding)
	{
		return;
	}

	// While key is held: slide continues, duration grows toward max
	// Time does NOT count down while holding - countdown starts on release or when max is reached
	if (bIsSlideKeyHeld)
	{
		SlideHoldTime += DeltaTime;

		// Calculate max duration based on hold time (capped at SlideHoldTimeToMax)
		// Max possible duration = SlideBaseDuration * SlideHoldMultiplier
		const float HoldProgress = FMath::Clamp(SlideHoldTime / SlideHoldTimeToMax, 0.0f, 1.0f);
		CurrentSlideMaxDuration = SlideBaseDuration * (1.0f + (HoldProgress * (SlideHoldMultiplier - 1.0f)));

		// Set remaining time to current max (slide maintains while holding)
		SlideTimeRemaining = CurrentSlideMaxDuration;

		// Force release if max duration is reached (can't hold forever)
		const float MaxPossibleDuration = SlideBaseDuration * SlideHoldMultiplier;
		if (SlideHoldTime >= SlideHoldTimeToMax)
		{
			bIsSlideKeyHeld = false;
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("Slide max duration reached (%.2f) - countdown starting"), MaxPossibleDuration);
		}
	}

	// --- Debug capsule visualization (toggle via bShowSlideDebugCapsule) ---
	if (bShowSlideDebugCapsule && GetWorld())
	{
		// For ACharacter, GetActorLocation() returns the capsule CENTER
		// DrawDebugCapsule expects the center point, so use GetActorLocation() directly
		const FVector CapsuleCenter = GetActorLocation();
		const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
		
		// Draw green wireframe capsule to show reduced height during slide
		// This should show the capsule with its bottom at floor level
		DrawDebugCapsule(
			GetWorld(),
			CapsuleCenter,
			CapsuleHalfHeight,
			CapsuleRadius,
			GetActorRotation().Quaternion(),
			FColor::Green,
			false,  // bPersistentLines = false (clears each frame)
			-1.0f,  // LifeTime = -1 (draws every frame)
			0,      // DepthPriority
			2.0f    // Thickness
		);
	}
	// --- End debug visualization ---

	// Countdown only happens when key is NOT held (after release or max reached)
	if (!bIsSlideKeyHeld)
	{
		SlideTimeRemaining -= DeltaTime;

		// Check if slide should end
		if (SlideTimeRemaining <= 0.0f)
		{
			EndSlide();
		}
	}
}

void AStateRunner_ArcadeCharacter::OnSlidePressed()
{
	StartSlide();
}

void AStateRunner_ArcadeCharacter::OnSlideReleased()
{
	// Key released - stop extending slide duration, but slide continues
	if (bIsSliding)
	{
		bIsSlideKeyHeld = false;
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("Slide key released - duration locked at %.2f seconds"), SlideTimeRemaining);
	}
}

// --- Fast Fall to Slide ---

void AStateRunner_ArcadeCharacter::StartFastFall()
{
	// Can only fast fall if currently jumping
	if (!bIsJumping)
	{
		return;
	}

	if (bIsFastFalling)
	{
		return;
	}

	// Activate fast fall state
	bIsFastFalling = true;
	bQueueSlideOnLand = true;  // Queue slide to start when we land
	
	// Cancel the rising phase immediately - we're going DOWN
	bIsJumpRising = false;
	bJumpReleasedEarly = false;
	ApexHangTimeRemaining = 0.0f;

	// Play slide sound to indicate the action was registered
	PlayPlayerSound(SlideSound);

	UE_LOG(LogStateRunner_Arcade, Log, TEXT("Fast fall initiated at height %.2f - will slide on landing"), CurrentJumpOffset);
}

bool AStateRunner_ArcadeCharacter::IsAnyMovementActive() const
{
	return bIsLaneSwitching || bIsJumping || bIsSliding || bIsFastFalling;
}

// --- Overclock System ---

void AStateRunner_ArcadeCharacter::OnOverclockPressed()
{
	// Block input during intro/countdown
	if (!bGameplayInputEnabled)
	{
		return;
	}

	// Get OVERCLOCK system from GameMode
	if (UWorld* World = GetWorld())
	{
		if (AStateRunner_ArcadeGameMode* GameMode = Cast<AStateRunner_ArcadeGameMode>(World->GetAuthGameMode()))
		{
			if (UOverclockSystemComponent* OverclockSystem = GameMode->GetOverclockSystemComponent())
			{
				OverclockSystem->OnOverclockKeyPressed();
			}
		}
	}
}

void AStateRunner_ArcadeCharacter::OnOverclockReleased()
{
	// Get OVERCLOCK system from GameMode
	if (UWorld* World = GetWorld())
	{
		if (AStateRunner_ArcadeGameMode* GameMode = Cast<AStateRunner_ArcadeGameMode>(World->GetAuthGameMode()))
		{
			if (UOverclockSystemComponent* OverclockSystem = GameMode->GetOverclockSystemComponent())
			{
				OverclockSystem->OnOverclockKeyReleased();
			}
		}
	}
}

// --- Disabled Standard Movement Functions (kept for interface compatibility) ---

void AStateRunner_ArcadeCharacter::Move(const FInputActionValue& Value)
{
	// DISABLED: This game uses lane-based movement, not free movement
	// Standard WASD/analog stick movement is not used
}

void AStateRunner_ArcadeCharacter::Look(const FInputActionValue& Value)
{
	// DISABLED: Camera is fixed in this game
	// No camera rotation allowed
}

void AStateRunner_ArcadeCharacter::DoMove(float Right, float Forward)
{
	// DISABLED: This game uses lane-based movement, not free movement
	// Function kept for Blueprint compatibility but does nothing
}

void AStateRunner_ArcadeCharacter::DoLook(float Yaw, float Pitch)
{
	// DISABLED: Camera is fixed in this game
	// Function kept for Blueprint compatibility but does nothing
}

void AStateRunner_ArcadeCharacter::DoJumpStart()
{
	// Legacy function - redirects to new jump system
	StartJump();
}

void AStateRunner_ArcadeCharacter::DoJumpEnd()
{
	// Legacy function - redirects to new jump system
	EndJump();
}

// --- Intro Rise Effect ---

void AStateRunner_ArcadeCharacter::StartRiseFromGround()
{
	// Start below ground
	CurrentRiseOffset = -RiseStartOffset;
	TargetRiseOffset = 0.0f;
	
	// Calculate speed based on duration
	RiseSpeed = RiseStartOffset / RiseDuration;
	
	bIsRising = true;
	SetActorTickEnabled(true);
	
	// Immediately apply the starting position
	EnforcePositionLock();
	
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("Intro rise started - Rising %.2f units over %.2f seconds"), 
		RiseStartOffset, RiseDuration);
}

void AStateRunner_ArcadeCharacter::ProcessRiseEffect(float DeltaTime)
{
	if (!bIsRising)
	{
		return;
	}
	
	// Interpolate toward target (0)
	const float Movement = RiseSpeed * DeltaTime;
	CurrentRiseOffset = FMath::Min(CurrentRiseOffset + Movement, TargetRiseOffset);
	
	// Check if rise is complete
	if (CurrentRiseOffset >= TargetRiseOffset - 0.1f)
	{
		CurrentRiseOffset = 0.0f;
		bIsRising = false;
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("Intro rise complete - Character at normal position"));
	}
}

// --- Gameplay Input Control ---

void AStateRunner_ArcadeCharacter::EnableGameplayInput()
{
	if (!bGameplayInputEnabled)
	{
		bGameplayInputEnabled = true;
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("Gameplay input ENABLED - Player can now move, jump, slide, etc."));
	}
}

void AStateRunner_ArcadeCharacter::DisableGameplayInput()
{
	if (bGameplayInputEnabled)
	{
		bGameplayInputEnabled = false;
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("Gameplay input DISABLED - Player movement blocked"));
	}
}

// --- Camera Effects ---

void AStateRunner_ArcadeCharacter::PlayDamageCameraShake(float Intensity)
{
	// Use provided intensity or default
	const float ShakeIntensity = (Intensity > 0.0f) ? Intensity : DamageShakeIntensity;
	
	if (ShakeIntensity <= 0.0f)
	{
		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (DamageCameraShake)
		{
			// Use custom camera shake class if provided
			PC->ClientStartCameraShake(DamageCameraShake, ShakeIntensity);
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("Playing custom damage camera shake (intensity: %.2f)"), ShakeIntensity);
		}
		else if (PC->PlayerCameraManager && CameraBoom)
		{
			// Procedural shake: Temporarily offset the camera boom with a quick jolt
			// This creates an impact feel by rapidly moving the camera
			
			// Apply a quick random offset to the boom
			const FVector OriginalOffset = CameraBoom->TargetOffset;
			const FVector ShakeOffset = FVector(
				FMath::RandRange(-25.0f, 25.0f) * ShakeIntensity,
				FMath::RandRange(-20.0f, 20.0f) * ShakeIntensity,
				FMath::RandRange(-15.0f, 15.0f) * ShakeIntensity
			);
			
			CameraBoom->TargetOffset = OriginalOffset + ShakeOffset;
			
			// Schedule reset after a brief duration (simulate shake recovery)
			// Use member timer handle so it can be cleared in EndPlay to prevent crashes on restart
			TWeakObjectPtr<USpringArmComponent> WeakCameraBoom = CameraBoom;
			GetWorld()->GetTimerManager().SetTimer(
				DamageShakeResetTimerHandle,
				[WeakCameraBoom, OriginalOffset]()
				{
					if (WeakCameraBoom.IsValid())
					{
						WeakCameraBoom->TargetOffset = OriginalOffset;
					}
				},
				0.1f,  // Quick reset after 0.1 seconds
				false
			);
			
			// Also add a brief red flash for damage feedback
			// Fade TO red quickly, then immediately start fading back to clear
			PC->PlayerCameraManager->StartCameraFade(0.0f, 0.4f, 0.08f, FLinearColor::Red, false, false);
			
			// Schedule the fade-out back to normal
			// Use TWeakObjectPtr to safely handle PlayerController destruction during restart
			TWeakObjectPtr<APlayerController> WeakPC = PC;
			GetWorld()->GetTimerManager().SetTimer(
				DamageFadeOutTimerHandle,
				[WeakPC]()
				{
					if (WeakPC.IsValid() && WeakPC->PlayerCameraManager)
					{
						WeakPC->PlayerCameraManager->StartCameraFade(0.4f, 0.0f, 0.2f, FLinearColor::Red, false, false);
					}
				},
				0.08f,  // Start fade-out after the fade-in completes
				false
			);
			
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("Playing procedural damage shake (intensity: %.2f)"), ShakeIntensity);
		}
	}
}

void AStateRunner_ArcadeCharacter::SetOverclockZoom(bool bZoomIn)
{
	bIsOverclockZoomActive = bZoomIn;
	TargetCameraArmLength = bZoomIn ? OverclockCameraArmLength : NormalCameraArmLength;
	
	// Enable tick to process the zoom interpolation
	SetActorTickEnabled(true);
	
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("OVERCLOCK zoom %s - Target arm length: %.2f"), 
		bZoomIn ? TEXT("IN") : TEXT("OUT"), TargetCameraArmLength);
}

void AStateRunner_ArcadeCharacter::SetMagnetEffectActive(bool bActivate)
{
	// Fire the BlueprintImplementableEvent so the character Blueprint can
	// activate/deactivate a Niagara or Cascade particle system component.
	OnMagnetEffectChanged(bActivate);
	
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("Player Magnet VFX: %s"), bActivate ? TEXT("ON") : TEXT("OFF"));
}

void AStateRunner_ArcadeCharacter::ToggleCameraView()
{
	SetCameraView(!bIsTopDownCameraActive);
}

void AStateRunner_ArcadeCharacter::SetCameraView(bool bUseTopDown)
{
	if (bIsTopDownCameraActive == bUseTopDown)
	{
		return;  // Already in requested view
	}
	
	bIsTopDownCameraActive = bUseTopDown;
	
	if (bUseTopDown)
	{
		// Switch to top-down camera
		if (FollowCamera)
		{
			FollowCamera->SetActive(false);
		}
		if (TopDownCamera)
		{
			TopDownCamera->SetActive(true);
		}
		
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("Switched to TOP-DOWN camera view"));
	}
	else
	{
		// Switch to main follow camera
		if (TopDownCamera)
		{
			TopDownCamera->SetActive(false);
		}
		if (FollowCamera)
		{
			FollowCamera->SetActive(true);
		}
		
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("Switched to MAIN follow camera view"));
	}
}

void AStateRunner_ArcadeCharacter::ProcessCameraZoom(float DeltaTime)
{
	if (!CameraBoom)
	{
		return;
	}
	
	const float CurrentArmLength = CameraBoom->TargetArmLength;
	const float Difference = TargetCameraArmLength - CurrentArmLength;
	
	// Skip if already at target (within threshold)
	if (FMath::Abs(Difference) < 1.0f)
	{
		if (CurrentArmLength != TargetCameraArmLength)
		{
			CameraBoom->TargetArmLength = TargetCameraArmLength;
		}
		return;
	}
	
	// Interpolate toward target
	const float Direction = FMath::Sign(Difference);
	const float Movement = CameraZoomSpeed * DeltaTime * Direction;
	
	// Clamp to not overshoot
	if (FMath::Abs(Movement) >= FMath::Abs(Difference))
	{
		CameraBoom->TargetArmLength = TargetCameraArmLength;
	}
	else
	{
		CameraBoom->TargetArmLength = CurrentArmLength + Movement;
	}
}

void AStateRunner_ArcadeCharacter::EnforceCameraLaneLock()
{
	if (!bLockCameraToCenter)
	{
		return;
	}

	const FVector CharacterLocation = GetActorLocation();
	
	// Y offset keeps camera at center lane (world Y=0) regardless of lane switches
	const float CameraOffsetY = -CharacterLocation.Y;
	// Z offset keeps camera at configured height, prevents ceiling clipping during jumps
	const float CameraOffsetZ = BaseZPosition - CharacterLocation.Z;

	if (CameraBoom)
	{
		FVector BoomLocation = CameraBoom->GetRelativeLocation();
		BoomLocation.Y = CameraOffsetY;
		BoomLocation.Z = CameraOffsetZ;
		CameraBoom->SetRelativeLocation(BoomLocation);
	}

	// Top-down uses its own cached Z baseline from Blueprint
	if (TopDownCameraBoom)
	{
		FVector BoomLocation = TopDownCameraBoom->GetRelativeLocation();
		BoomLocation.Y = CameraOffsetY;
		BoomLocation.Z = TopDownCameraBoomBaseZ + CameraOffsetZ;
		TopDownCameraBoom->SetRelativeLocation(BoomLocation);
	}
}

void AStateRunner_ArcadeCharacter::OnCameraSwitchPressed()
{
	// Hold to use: switch to top-down when pressed
	SetCameraView(true);
}

void AStateRunner_ArcadeCharacter::OnCameraSwitchReleased()
{
	// Return to main camera when released
	SetCameraView(false);
}

// --- Sound Effects ---

void AStateRunner_ArcadeCharacter::PlayPlayerSound(USoundBase* Sound, float VolumeMultiplier)
{
	if (!Sound)
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(
		this,
		Sound,
		GetActorLocation(),
		MovementSoundVolume * VolumeMultiplier
	);
}

// --- Debug Input (Non-shipping builds only) ---

#if !UE_BUILD_SHIPPING
void AStateRunner_ArcadeCharacter::OnDebugIncreaseDifficulty()
{
	// Get PickupSpawnerComponent from GameMode and increase difficulty
	if (UWorld* World = GetWorld())
	{
		if (AStateRunner_ArcadeGameMode* GameMode = Cast<AStateRunner_ArcadeGameMode>(World->GetAuthGameMode()))
		{
			if (UPickupSpawnerComponent* PickupSpawner = GameMode->GetPickupSpawnerComponent())
			{
				PickupSpawner->DebugIncreaseDifficulty();
			}
		}
	}
}
#endif
