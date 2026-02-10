#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "StateRunner_ArcadeCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UCameraShakeBase;
class USoundBase;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 * The three lanes the player can occupy.
 * Left (-316.67), Center (0), Right (316.67) on the Y-axis.
 */
UENUM(BlueprintType)
enum class ELanePosition : uint8
{
	Left = 0    UMETA(DisplayName = "Left Lane"),
	Center = 1  UMETA(DisplayName = "Center Lane"),
	Right = 2   UMETA(DisplayName = "Right Lane")
};

/**
 * Main player character for StateRunner Arcade.
 * Endless runner with lane-based movement -- the player stays in place
 * and the world scrolls past. Only Y-axis movement for lane switching.
 */
UCLASS(config=Game)
class AStateRunner_ArcadeCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera (main gameplay camera) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** Top-down camera boom (alternate view inspired by F-Zero X) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* TopDownCameraBoom;

	/** Top-down camera (alternate strategic view) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* TopDownCamera;

public:

	/** Constructor */
	AStateRunner_ArcadeCharacter();

	/** Called every frame */
	virtual void Tick(float DeltaTime) override;

protected:

	/** Called when the game starts or when spawned */
	virtual void BeginPlay() override;

	/** Called when actor is being removed from the level */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// --- Input Actions ---

protected:

	/** Jump Input Action - NOT USED in Phase 1, reserved for Phase 2 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action - DISABLED for this game (standard movement not used) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action - DISABLED for this game (camera is fixed) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action - DISABLED for this game (camera is fixed) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	UInputAction* MouseLookAction;

	/** Lane Left Input Action - Arrow Left key, switches to left lane */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Lane System")
	UInputAction* LaneLeftAction;

	/** Lane Right Input Action - Arrow Right key, switches to right lane */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Lane System")
	UInputAction* LaneRightAction;

	/** Slide Input Action - Arrow Down key, triggers slide */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Slide System")
	UInputAction* SlideAction;

	/** OVERCLOCK Input Action - L-Shift key, activates OVERCLOCK mode */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|OVERCLOCK")
	UInputAction* OverclockAction;

	// --- Position Locking ---

protected:

	/** 
	 * Locked X position (player never moves forward/backward).
	 * World scrolls in negative X direction to create movement illusion.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Position Lock")
	float LockedXPosition = -5000.0f;

	/** 
	 * Base Z position (height). Jump will offset from this value in Phase 2.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Position Lock")
	float BaseZPosition = 81.0f;

	// --- Lane System ---

protected:

	/** Y-axis position for left lane */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lane System")
	float LanePositionLeft = -316.67f;

	/** Y-axis position for center lane */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lane System")
	float LanePositionCenter = 0.0f;

	/** Y-axis position for right lane */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lane System")
	float LanePositionRight = 316.67f;

	/** Speed of lane switching in units per second (smooth interpolation) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lane System", meta=(ClampMin="100.0", ClampMax="5000.0"))
	float LaneSwitchSpeed = 1600.0f;

	/** Snap threshold - when within this distance of target lane, snap to exact position */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lane System", meta=(ClampMin="0.1", ClampMax="10.0"))
	float LaneSnapThreshold = 1.0f;

	/** Current lane the player is in (or moving toward) */
	UPROPERTY(BlueprintReadOnly, Category="Lane System")
	ELanePosition CurrentLane = ELanePosition::Center;

	/** Target lane when switching (may be same as CurrentLane when not switching) */
	UPROPERTY(BlueprintReadOnly, Category="Lane System")
	ELanePosition TargetLane = ELanePosition::Center;

	/** True while the player is actively switching between lanes */
	UPROPERTY(BlueprintReadOnly, Category="Lane System")
	bool bIsLaneSwitching = false;

	/** True while the lane left key is held - enables continuous lane switching */
	bool bIsLaneLeftHeld = false;

	/** True while the lane right key is held - enables continuous lane switching */
	bool bIsLaneRightHeld = false;

	// --- Jump System ---

protected:

	/** Base jump height in units (Z offset from BaseZPosition) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Jump System", meta=(ClampMin="10.0", ClampMax="500.0"))
	float JumpBaseHeight = 250.0f;

	/** 
	 * Minimum jump height guaranteed on a tap (quick press/release).
	 * Ensures even a quick tap clears low obstacles (LowWall Z 0-80).
	 * If player releases before reaching this height, jump continues rising to this minimum.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Jump System", meta=(ClampMin="10.0", ClampMax="300.0"))
	float MinTapJumpHeight = 120.0f;

	/** Maximum multiplier for jump height (2.5x = 375 units total at max hold) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Jump System", meta=(ClampMin="1.0", ClampMax="5.0"))
	float JumpHoldMultiplier = 1.5f;

	/** Time in seconds to hold key to reach maximum jump height (longer = more grace time) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Jump System", meta=(ClampMin="0.1", ClampMax="2.0"))
	float JumpHoldTimeToMax = 0.4f;

	/** Speed of upward movement in units per second (higher = snappier rise) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Jump System", meta=(ClampMin="100.0", ClampMax="3000.0"))
	float JumpRiseSpeed = 1100.0f;

	/** Speed of downward movement in units per second (higher = faster fall) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Jump System", meta=(ClampMin="100.0", ClampMax="3000.0"))
	float JumpFallSpeed = 1400.0f;

	/** True when player is currently jumping */
	UPROPERTY(BlueprintReadOnly, Category="Jump System")
	bool bIsJumping = false;

	/** Current Z offset from BaseZPosition (0 = on ground, positive = in air) */
	UPROPERTY(BlueprintReadOnly, Category="Jump System")
	float CurrentJumpOffset = 0.0f;

	/** Maximum jump height for current jump (based on hold time) */
	UPROPERTY(BlueprintReadOnly, Category="Jump System")
	float CurrentJumpMaxHeight = 0.0f;

	/** Time player has been holding jump key */
	UPROPERTY(BlueprintReadOnly, Category="Jump System")
	float JumpHoldTime = 0.0f;

	/** True when jump is in rising phase, false when falling */
	UPROPERTY(BlueprintReadOnly, Category="Jump System")
	bool bIsJumpRising = false;

	/** True when player released jump early but hasn't reached MinTapJumpHeight yet */
	bool bJumpReleasedEarly = false;

	/** 
	 * Apex hang time - slows descent near peak for "floaty" arcade feel.
	 * Higher value = more hang time at apex (0 = no hang time)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Jump System", meta=(ClampMin="0.0", ClampMax="1.0"))
	float JumpApexHangTime = 0.125f;

	/** Threshold below max height where apex hang time kicks in (as percentage of max height) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Jump System", meta=(ClampMin="0.0", ClampMax="0.5"))
	float JumpApexThreshold = 0.1f;

	/** Time remaining in apex hang (internal state) */
	float ApexHangTimeRemaining = 0.0f;

	// --- Slide System ---

protected:

	/** Base slide duration in seconds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Slide System", meta=(ClampMin="0.1", ClampMax="3.0"))
	float SlideBaseDuration = 0.5f;

	/** Maximum multiplier for slide duration (2x = 1.0 second total) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Slide System", meta=(ClampMin="1.0", ClampMax="5.0"))
	float SlideHoldMultiplier = 2.0f;

	/** Time in seconds to hold key to reach maximum slide duration */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Slide System", meta=(ClampMin="0.1", ClampMax="2.0"))
	float SlideHoldTimeToMax = 0.5f;

	/** 
	 * Reduced capsule half-height during slide (normal is 96.0).
	 * This value is applied via UE's Crouch system (CrouchedHalfHeight).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Slide System", meta=(ClampMin="20.0", ClampMax="96.0"))
	float SlideCapsuleHalfHeight = 48.0f;

	/** 
	 * Normal capsule half-height (reference value).
	 * Used to calculate SlideZOffset for debugging/logging.
	 * Actual capsule restoration is handled by UE's UnCrouch system.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Slide System")
	float NormalCapsuleHalfHeight = 96.0f;

	/** True when player is currently sliding */
	UPROPERTY(BlueprintReadOnly, Category="Slide System")
	bool bIsSliding = false;

	/** Time remaining in current slide (in seconds) */
	UPROPERTY(BlueprintReadOnly, Category="Slide System")
	float SlideTimeRemaining = 0.0f;

	/** Maximum slide duration for current slide (based on hold time) */
	UPROPERTY(BlueprintReadOnly, Category="Slide System")
	float CurrentSlideMaxDuration = 0.0f;

	/** Time player has been holding slide key */
	UPROPERTY(BlueprintReadOnly, Category="Slide System")
	float SlideHoldTime = 0.0f;

	/** True when slide key is currently held (allows duration extension) */
	UPROPERTY(BlueprintReadOnly, Category="Slide System")
	bool bIsSlideKeyHeld = false;

	/** 
	 * Z offset during slide (reference value for debugging/logging).
	 * Calculated as: NormalCapsuleHalfHeight - SlideCapsuleHalfHeight
	 * Actual Z positioning is handled by UE's Crouch system.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Slide System")
	float SlideZOffset = 0.0f;

	/** 
	 * PLACEHOLDER DEBUG - Enable/disable debug capsule visualization during slide.
	 * Shows green wireframe capsule to visualize reduced hitbox during slide.
	 * Disable this once slide animation is implemented.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Slide System|Debug")
	bool bShowSlideDebugCapsule = true;

	// --- Fast Fall to Slide ---

	/** 
	 * Speed of fast fall when pressing slide in air (units per second).
	 * Higher = faster magnet-to-ground effect.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Slide System|Fast Fall", meta=(ClampMin="1000.0", ClampMax="10000.0"))
	float FastFallSpeed = 5000.0f;

	/** True when player is in fast fall state (pressed slide while jumping) */
	UPROPERTY(BlueprintReadOnly, Category="Slide System|Fast Fall")
	bool bIsFastFalling = false;

	/** True when we should transition to slide immediately upon landing from fast fall */
	bool bQueueSlideOnLand = false;

	// --- Sound Effects ---

protected:

	/** Sound to play when jumping */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound Effects|Movement")
	TObjectPtr<USoundBase> JumpSound;

	/** Sound to play when starting a slide */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound Effects|Movement")
	TObjectPtr<USoundBase> SlideSound;

	/** Sound to play when switching lanes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound Effects|Movement")
	TObjectPtr<USoundBase> LaneSwitchSound;

	/** Volume multiplier for movement sounds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound Effects|Movement", meta=(ClampMin="0.0", ClampMax="2.0"))
	float MovementSoundVolume = 1.0f;

	/** Play a sound at the player's location */
	void PlayPlayerSound(USoundBase* Sound, float VolumeMultiplier = 1.0f);

	// --- Lane Switching Functions ---

public:

	/** 
	 * Attempt to switch to the lane on the left.
	 * Will be ignored if already in the leftmost lane or currently switching.
	 */
	UFUNCTION(BlueprintCallable, Category="Lane System")
	void SwitchLaneLeft();

	/** 
	 * Attempt to switch to the lane on the right.
	 * Will be ignored if already in the rightmost lane or currently switching.
	 */
	UFUNCTION(BlueprintCallable, Category="Lane System")
	void SwitchLaneRight();

	/** Get the Y-axis position for a given lane */
	UFUNCTION(BlueprintPure, Category="Lane System")
	float GetLaneYPosition(ELanePosition Lane) const;

	/** Get the current lane */
	UFUNCTION(BlueprintPure, Category="Lane System")
	ELanePosition GetCurrentLane() const { return CurrentLane; }

	/** Check if currently switching lanes */
	UFUNCTION(BlueprintPure, Category="Lane System")
	bool IsLaneSwitching() const { return bIsLaneSwitching; }

protected:

	/** Process lane switching interpolation - called from Tick when switching */
	void ProcessLaneSwitching(float DeltaTime);

	/** Enforce position locking (X and Z locked, Y can change for lanes) */
	void EnforcePositionLock();

	/** Input callback for lane left action - pressed */
	void OnLaneLeftPressed();

	/** Input callback for lane left action - released */
	void OnLaneLeftReleased();

	/** Input callback for lane right action - pressed */
	void OnLaneRightPressed();

	/** Input callback for lane right action - released */
	void OnLaneRightReleased();

	// --- Jump System Functions ---

public:

	/** Start jump - called when Arrow Up is pressed */
	UFUNCTION(BlueprintCallable, Category="Jump System")
	void StartJump();

	/** End jump - called when Arrow Up is released (begins falling phase) */
	UFUNCTION(BlueprintCallable, Category="Jump System")
	void EndJump();

	/** Get current jump state */
	UFUNCTION(BlueprintPure, Category="Jump System")
	bool IsJumping() const { return bIsJumping; }

	/** Get current jump offset */
	UFUNCTION(BlueprintPure, Category="Jump System")
	float GetCurrentJumpOffset() const { return CurrentJumpOffset; }

protected:

	/** Process jump state machine - called from Tick when jumping */
	void ProcessJump(float DeltaTime);

	/** Input callback for jump pressed */
	void OnJumpPressed();

	/** Input callback for jump released */
	void OnJumpReleased();

	// --- Slide System Functions ---

public:

	/** Start slide - called when Arrow Down is pressed */
	UFUNCTION(BlueprintCallable, Category="Slide System")
	void StartSlide();

	/** End slide - called when Arrow Down is released (begins recovery) */
	UFUNCTION(BlueprintCallable, Category="Slide System")
	void EndSlide();

	/** 
	 * Force end slide immediately - used when jump cancels slide.
	 * Unlike EndSlide(), this ignores remaining slide time and ends immediately.
	 */
	UFUNCTION(BlueprintCallable, Category="Slide System")
	void ForceEndSlide();

	/** Get current slide state */
	UFUNCTION(BlueprintPure, Category="Slide System")
	bool IsSliding() const { return bIsSliding; }

	/** Get remaining slide time */
	UFUNCTION(BlueprintPure, Category="Slide System")
	float GetSlideTimeRemaining() const { return SlideTimeRemaining; }

	/** Check if currently in fast fall state (pressed slide while in air) */
	UFUNCTION(BlueprintPure, Category="Slide System")
	bool IsFastFalling() const { return bIsFastFalling; }

	/**
	 * Start fast fall - called when slide is pressed while jumping.
	 * Rapidly descends and transitions to slide upon landing.
	 */
	UFUNCTION(BlueprintCallable, Category="Slide System")
	void StartFastFall();

protected:

	/** Process slide state machine - called from Tick when sliding */
	void ProcessSlide(float DeltaTime);

	/** Input callback for slide pressed */
	void OnSlidePressed();

	/** Input callback for slide released */
	void OnSlideReleased();

	/** Check if any active movement is happening (jump, slide, or lane switch) */
	bool IsAnyMovementActive() const;

	// --- Overclock System Functions ---

protected:

	/** Input callback for OVERCLOCK pressed */
	void OnOverclockPressed();

	/** Input callback for OVERCLOCK released */
	void OnOverclockReleased();

	// --- Intro Rise Effect ---

protected:

	/** If true, character spawns below ground and waits for StartRiseFromGround() to be called */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Intro Effect")
	bool bStartBelowGround = true;

	/** How far below normal position the character starts (in units) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Intro Effect", meta=(ClampMin="50.0", ClampMax="500.0"))
	float RiseStartOffset = 200.0f;

	/** How long the rise takes (in seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Intro Effect", meta=(ClampMin="0.5", ClampMax="5.0"))
	float RiseDuration = 2.0f;

	/** Current rise offset (0 = at normal position, negative = below ground) */
	float CurrentRiseOffset = 0.0f;

	/** Target rise offset (interpolates toward this) */
	float TargetRiseOffset = 0.0f;

	/** Whether the rise animation is currently playing */
	bool bIsRising = false;

	/** Speed of rise interpolation (calculated from duration) */
	float RiseSpeed = 0.0f;

public:

	/**
	 * Triggers the rise-from-ground intro effect.
	 * Character starts below ground and rises to normal position over RiseDuration.
	 */
	UFUNCTION(BlueprintCallable, Category="Intro Effect")
	void StartRiseFromGround();

	/**
	 * Check if the rise animation is currently playing.
	 */
	UFUNCTION(BlueprintPure, Category="Intro Effect")
	bool IsRising() const { return bIsRising; }

protected:

	/** Process the rise interpolation (called from Tick) */
	void ProcessRiseEffect(float DeltaTime);

	// --- Camera Effects System ---

protected:

	// --- Camera Shake ---

	/** 
	 * Camera shake class to play on damage.
	 * If not set, a default procedural shake will be used.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera Effects|Shake")
	TSubclassOf<UCameraShakeBase> DamageCameraShake;

	/** Intensity of damage camera shake (scales the shake) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera Effects|Shake", meta=(ClampMin="0.1", ClampMax="5.0"))
	float DamageShakeIntensity = 1.5f;

	/** Timer handle for camera shake reset (cleared on EndPlay to prevent crashes) */
	FTimerHandle DamageShakeResetTimerHandle;

	/** Timer handle for camera fade out (cleared on EndPlay to prevent crashes) */
	FTimerHandle DamageFadeOutTimerHandle;

	// --- Overclock Zoom ---

	/** Normal camera arm length (distance from player) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera Effects|OVERCLOCK Zoom", meta=(ClampMin="100.0", ClampMax="2000.0"))
	float NormalCameraArmLength = 1000.0f;

	/** Camera arm length during OVERCLOCK (closer = more intense feel) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera Effects|OVERCLOCK Zoom", meta=(ClampMin="100.0", ClampMax="2000.0"))
	float OverclockCameraArmLength = 800.0f;

	/** Speed at which camera zooms in/out during OVERCLOCK transition */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera Effects|OVERCLOCK Zoom", meta=(ClampMin="50.0", ClampMax="2000.0"))
	float CameraZoomSpeed = 400.0f;

	/** Current target arm length (interpolates toward this) */
	float TargetCameraArmLength = 1000.0f;

	/** Whether we're currently in OVERCLOCK zoom state */
	bool bIsOverclockZoomActive = false;

	// --- Alternate Camera (Top-Down View) ---

	/** Whether the top-down camera is currently active */
	UPROPERTY(BlueprintReadOnly, Category="Camera Effects|Top-Down Camera")
	bool bIsTopDownCameraActive = false;

	/** Top-down boom Z offset cached from Blueprint for independent height lock */
	float TopDownCameraBoomBaseZ = 0.0f;

	/** Camera Switch Input Action - cycles between camera views */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Camera")
	UInputAction* CameraSwitchAction;

public:

	// --- Camera Effects Functions ---

	/** 
	 * Play damage camera shake. Called by LivesSystemComponent.
	 * @param Intensity - Optional intensity multiplier (default uses DamageShakeIntensity)
	 */
	UFUNCTION(BlueprintCallable, Category="Camera Effects")
	void PlayDamageCameraShake(float Intensity = 0.0f);

	/**
	 * Set OVERCLOCK zoom state. Called when OVERCLOCK activates/deactivates.
	 * @param bZoomIn - True to zoom in (OVERCLOCK active), false to zoom out
	 */
	UFUNCTION(BlueprintCallable, Category="Camera Effects")
	void SetOverclockZoom(bool bZoomIn);

	/**
	 * Set Magnet VFX state on the player. Called when Magnet activates/deactivates.
	 * Fires OnMagnetEffectChanged for Blueprint to respond (e.g. toggle a particle system).
	 * @param bActivate - True to show magnet VFX, false to hide
	 */
	UFUNCTION(BlueprintCallable, Category="VFX")
	void SetMagnetEffectActive(bool bActivate);

	/**
	 * Blueprint event fired when Magnet effect changes state.
	 * Use this to activate/deactivate a Niagara or Cascade particle system on the player mesh.
	 * @param bIsActive - True when magnet is active, false when it expires
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="VFX")
	void OnMagnetEffectChanged(bool bIsActive);

	/**
	 * Toggle between main camera and top-down camera.
	 */
	UFUNCTION(BlueprintCallable, Category="Camera Effects")
	void ToggleCameraView();

	/**
	 * Switch to a specific camera view.
	 * @param bUseTopDown - True for top-down view, false for main follow camera
	 */
	UFUNCTION(BlueprintCallable, Category="Camera Effects")
	void SetCameraView(bool bUseTopDown);

	/** Check if top-down camera is active */
	UFUNCTION(BlueprintPure, Category="Camera Effects")
	bool IsTopDownCameraActive() const { return bIsTopDownCameraActive; }

protected:

	// --- Camera Position Lock ---

	/** Lock cameras to center lane and configured height (prevents motion blur and ceiling clipping) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera Effects|Position Lock")
	bool bLockCameraToCenter = true;

	/** Process camera zoom interpolation (called from Tick) */
	void ProcessCameraZoom(float DeltaTime);

	/** Counteract character Y/Z movement to keep cameras fixed on track */
	void EnforceCameraLaneLock();

	/** Input callback for camera switch - pressed (switch to top-down) */
	void OnCameraSwitchPressed();

	/** Input callback for camera switch - released (return to main camera) */
	void OnCameraSwitchReleased();

	// --- Debug Input ---

#if !UE_BUILD_SHIPPING
protected:

	/** Debug: Increase difficulty (bound to 9 key) */
	void OnDebugIncreaseDifficulty();
#endif

	// --- Disabled Standard Movement ---

protected:

	/** Called for movement input - DISABLED, does nothing */
	void Move(const FInputActionValue& Value);

	/** Called for looking input - DISABLED, does nothing */
	void Look(const FInputActionValue& Value);

public:

	/** Standard movement - DISABLED for lane-based movement */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Look input - DISABLED, camera is fixed */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Jump start - Reserved for Phase 2 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Jump end - Reserved for Phase 2 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

