#include "PickupSpawnerComponent.h"
#include "BasePickup.h"
#include "BaseObstacle.h"
#include "ObstacleSpawnerComponent.h"
#include "StateRunner_ArcadeGameMode.h"
#include "WorldScrollComponent.h"
#include "GameDebugSubsystem.h"
#include "StateRunner_Arcade.h"
#include "StateRunner_ArcadeCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

UPickupSpawnerComponent::UPickupSpawnerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // Only ticks during Magnet effect
	
	// Set up default patterns so they can be edited in the Blueprint editor
	CreateDefaultPatterns();
}

void UPickupSpawnerComponent::BeginPlay()
{
	Super::BeginPlay();

	// Validate pickup class is set
	TSubclassOf<ABasePickup> DataPacketClassToUse = GetClassForType(EPickupType::DataPacket);
	if (!DataPacketClassToUse)
	{
		if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
		{
			Debug->LogError(TEXT("NO DATA PACKET CLASS SET! Set DataPacketClass in GameMode Blueprint."));
		}
	}

	// Validate 1-Up class
	if (!OneUpClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("PickupSpawner: OneUpClass is NOT SET! 1-Ups will not spawn. Assign BP_OneUpPickup in GameMode Blueprint."));
		if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
		{
			Debug->LogError(TEXT("OneUpClass NOT SET - 1-Ups disabled!"));
		}
	}

	// Validate EMP class
	if (!EMPClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("PickupSpawner: EMPClass is NOT SET! EMPs will not spawn. Assign BP_EMPPickup in GameMode Blueprint."));
		if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
		{
			Debug->LogError(TEXT("EMPClass NOT SET - EMPs disabled!"));
		}
	}

	// Validate Magnet class
	if (!MagnetClass)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("PickupSpawner: MagnetClass is NOT SET! Magnets will not spawn. Assign BP_Pickup_Magnet in GameMode Blueprint."));
	}

	InitializePools();

	// Log init summary to debug subsystem
	if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
	{
		FString InitInfo = FString::Printf(
			TEXT("Pools: DP:%d 1Up:%d EMP:%d\nPatterns: %d"),
			DataPacketPool.Num(), OneUpPool.Num(), EMPPool.Num(), PredefinedPatterns.Num()
		);
		Debug->LogInit(TEXT("PickupSpawner"), InitInfo);
	}
}

// --- Public Functions ---

void UPickupSpawnerComponent::SpawnPickupsForSegment(float SegmentStartX, float SegmentEndX)
{
	SegmentsSpawned++;
	
	// Update density level
	UpdateDensity();

	// Spawn all pickup types after tutorial for testing
	if (bDebugSpawnAllPickupTypes && !bDebugPickupsSpawned)
	{
		bool bTutorialDone = false;
		if (AStateRunner_ArcadeGameMode* GameMode = Cast<AStateRunner_ArcadeGameMode>(GetOwner()))
		{
			if (UObstacleSpawnerComponent* ObstacleSpawner = GameMode->GetObstacleSpawnerComponent())
			{
				bTutorialDone = ObstacleSpawner->IsTutorialComplete();
			}
		}
		
		if (bTutorialDone)
		{
			if (TrySpawnDebugPickupRow(SegmentStartX, SegmentEndX))
			{
				UE_LOG(LogStateRunner_Arcade, Warning, TEXT("*** All pickup types spawned for testing (tutorial complete)! ***"));
			}
		}
	}

	// Cache obstacle positions so pickups don't overlap with obstacles
	CacheObstaclePositions(SegmentStartX, SegmentEndX);

	// Generate layout - try pattern first, fall back to random
	TArray<FPickupSpawnData> PickupLayout;
	
	bool bUsePattern = FMath::FRand() < PatternChance;
	
	if (bUsePattern && GenerateFromPattern(PickupLayout))
	{
		// Pattern selected
	}
	else
	{
		// Fall back to procedural generation
		int32 PickupCount = CalculatePickupCount();
		GeneratePickupLayout(PickupLayout, PickupCount);
	}

	float SegmentLength = SegmentEndX - SegmentStartX;
	
	int32 PickupsSpawned = 0;
	int32 PickupsSkipped = 0;
	
	// Spawn Data Packet pickups
	for (const FPickupSpawnData& SpawnData : PickupLayout)
	{
		ABasePickup* Pickup = GetPickupFromPool(EPickupType::DataPacket);
		if (!Pickup)
		{
			UE_LOG(LogStateRunner_Arcade, Warning, TEXT("Failed to get Data Packet from pool! Pool size: %d"), DataPacketPool.Num());
			continue;
		}

		float WorldX = SegmentStartX + (SpawnData.RelativeXOffset * SegmentLength);
		float WorldY = GetLaneYPosition(SpawnData.Lane);
		float WorldZ = PickupSpawnZ + SpawnData.GetEffectiveZOffset();

		FVector DesiredLocation(WorldX, WorldY, WorldZ);
		FVector SafeLocation;

		if (FindSafeSpawnPosition(DesiredLocation, SpawnData.Lane, SafeLocation))
		{
			Pickup->Activate(SafeLocation, SpawnData.Lane);
			ActivePickups.Add(Pickup);
			OnPickupSpawned.Broadcast(Pickup, SpawnData);
			PickupsSpawned++;
		}
		else
		{
			PickupsSkipped++;
		}
	}

	// Try to spawn 1-Up (rare, risk/reward placement)
	if (ShouldSpawn1Up())
	{
		bool bIsGuaranteed = !bHasSpawnedFirst1Up && CurrentDensityLevel >= OneUpGuaranteedDifficulty;
		Spawn1Up(SegmentStartX, SegmentEndX, bIsGuaranteed);
	}

	// Try to spawn EMP (very rare, late-game screen nuke)
	bool bEMPEligible = (CurrentDensityLevel >= EMPMinDifficulty) && EMPClass;
	
	if (bEMPEligible)
	{
		if (ShouldSpawnEMP())
		{
			SpawnEMP(SegmentStartX, SegmentEndX, CachedObstaclePositions.Num());
		}
	}

	// Try to spawn Magnet (rare, mid-game combo enabler)
	// Don't stack special pickups on the same segment as an EMP
	bool bEMPSpawnedThisSegment = (LastEMPSpawnSegment == SegmentsSpawned);
	bool bMagnetEligible = (CurrentDensityLevel >= MagnetMinDifficulty) && MagnetClass && !bEMPSpawnedThisSegment;

	if (bMagnetEligible)
	{
		if (ShouldSpawnMagnet())
		{
			SpawnMagnet(SegmentStartX, SegmentEndX);
		}
	}
}

void UPickupSpawnerComponent::ClearAllPickups()
{
	for (ABasePickup* Pickup : ActivePickups)
	{
		if (Pickup && Pickup->IsActive())
		{
			Pickup->Deactivate();
		}
	}

	ActivePickups.Empty();
}

void UPickupSpawnerComponent::ResetSpawner()
{
	ClearAllPickups();
	CurrentDensityLevel = 0;
	SegmentsSpawned = 0;
	bHasSpawnedFirst1Up = false;
	bDebugPickupsSpawned = false;
#if !UE_BUILD_SHIPPING
	bDebugDensityOverride = false;
#endif
	Last1UpLane = ELane::Center;
	Last1UpSpawnSegment = -999;
	LastEMPSpawnSegment = -999;
	LastMagnetSpawnSegment = -999;
	// If magnet was active, clean up VFX before resetting
	if (bIsMagnetActive)
	{
		OnMagnetStateChanged.Broadcast(false);
		if (UWorld* World = GetWorld())
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				if (AStateRunner_ArcadeCharacter* Character = Cast<AStateRunner_ArcadeCharacter>(PC->GetPawn()))
				{
					Character->SetMagnetEffectActive(false);
				}
			}
		}
	}
	bIsMagnetActive = false;
	MagnetTimeRemaining = 0.0f;
	SetComponentTickEnabled(false);
}

// --- Protected Functions ---

void UPickupSpawnerComponent::GeneratePickupLayout(TArray<FPickupSpawnData>& OutPickups, int32 PickupCount)
{
	OutPickups.Reserve(PickupCount);

	TArray<float> UsedXOffsets;

	for (int32 i = 0; i < PickupCount; i++)
	{
		FPickupSpawnData SpawnData;

		// Random lane
		SpawnData.Lane = GetRandomLane();

		// Generate X offset that doesn't overlap
		float XOffset;
		int32 Attempts = 0;
		const int32 MaxAttempts = 20;

		do
		{
			XOffset = FMath::FRandRange(GetEffectiveMinSpawnOffset(), GetEffectiveMaxSpawnOffset());
			Attempts++;
		}
		while (Attempts < MaxAttempts && UsedXOffsets.ContainsByPredicate(
			[XOffset, this](float Used) { return FMath::Abs(Used - XOffset) < MinPickupSpacing; }
		));

		SpawnData.RelativeXOffset = XOffset;
		UsedXOffsets.Add(XOffset);

		SpawnData.ZOffset = 0.0f;

		OutPickups.Add(SpawnData);
	}
}

int32 UPickupSpawnerComponent::CalculatePickupCount() const
{
	// Base count increases with density
	int32 BaseCount = MinPickupsPerSegment + CurrentDensityLevel;
	
	// Bonus pickups at high density for denser content
	if (IsHighDensityActive())
	{
		BaseCount += HighDensityBonusPickups;
	}
	
	int32 RandomVariance = FMath::RandRange(-1, 1);
	return FMath::Clamp(BaseCount + RandomVariance, MinPickupsPerSegment, MaxPickupsPerSegment);
}

float UPickupSpawnerComponent::GetEffectiveMinSpawnOffset() const
{
	return IsHighDensityActive() ? HighDensityMinSpawnOffset : MinSpawnOffset;
}

float UPickupSpawnerComponent::GetEffectiveMaxSpawnOffset() const
{
	return IsHighDensityActive() ? HighDensityMaxSpawnOffset : MaxSpawnOffset;
}

void UPickupSpawnerComponent::GetAerialPatternBounds(float& OutStartX, float& OutEndX) const
{
	// Reserve space at end for safe landing, small buffer at start
	const float LandingBuffer = 0.15f;
	const float StartBuffer = 0.05f;
	
	float EffectiveMin = GetEffectiveMinSpawnOffset();
	float EffectiveMax = GetEffectiveMaxSpawnOffset();
	
	OutStartX = FMath::Max(EffectiveMin, StartBuffer);
	OutEndX = FMath::Min(EffectiveMax - LandingBuffer, 0.75f);
}

void UPickupSpawnerComponent::InitializePools()
{
	InitializePoolForType(EPickupType::DataPacket);
	InitializePoolForType(EPickupType::OneUp);
	InitializePoolForType(EPickupType::EMP);
	InitializePoolForType(EPickupType::Magnet);
}

void UPickupSpawnerComponent::InitializePoolForType(EPickupType Type)
{
	TSubclassOf<ABasePickup> ClassToUse = GetClassForType(Type);
	
	if (!ClassToUse)
	{
		// EMP and Magnet classes are optional
		if (Type != EPickupType::EMP && Type != EPickupType::Magnet)
		{
			UE_LOG(LogStateRunner_Arcade, Warning, TEXT("PickupSpawnerComponent: No class set for pickup type %d!"), (int32)Type);
		}
		return;
	}

	TArray<TObjectPtr<ABasePickup>>& Pool = GetPoolForType(Type);
	
	int32 PoolSize;
	switch (Type)
	{
		case EPickupType::OneUp: PoolSize = OneUpPoolSize; break;
		case EPickupType::EMP: PoolSize = EMPPoolSize; break;
		case EPickupType::Magnet: PoolSize = MagnetPoolSize; break;
		default: PoolSize = InitialPoolSize; break;
	}
	
	Pool.Reserve(PoolSize);

	for (int32 i = 0; i < PoolSize; i++)
	{
		ABasePickup* Pickup = SpawnPickupActor(Type);
		if (Pickup)
		{
			Pool.Add(Pickup);
		}
	}
}

ABasePickup* UPickupSpawnerComponent::GetPickupFromPool(EPickupType Type)
{
	TArray<TObjectPtr<ABasePickup>>& Pool = GetPoolForType(Type);
	
	// Find inactive pickup
	for (ABasePickup* Pickup : Pool)
	{
		if (Pickup && !Pickup->IsActive())
		{
			return Pickup;
		}
	}

	// Pool exhausted, expand it
	ExpandPool(Type);

	// Try again
	for (ABasePickup* Pickup : Pool)
	{
		if (Pickup && !Pickup->IsActive())
		{
			return Pickup;
		}
	}

	UE_LOG(LogStateRunner_Arcade, Warning, TEXT("Failed to get pickup from pool after expansion!"));
	return nullptr;
}

TArray<TObjectPtr<ABasePickup>>& UPickupSpawnerComponent::GetPoolForType(EPickupType Type)
{
	switch (Type)
	{
		case EPickupType::OneUp:
			return OneUpPool;
		case EPickupType::EMP:
			return EMPPool;
		case EPickupType::Magnet:
			return MagnetPool;
		case EPickupType::DataPacket:
		default:
			return DataPacketPool;
	}
}

TSubclassOf<ABasePickup> UPickupSpawnerComponent::GetClassForType(EPickupType Type) const
{
	switch (Type)
	{
		case EPickupType::OneUp:
			return OneUpClass;
		case EPickupType::EMP:
			return EMPClass;
		case EPickupType::Magnet:
			return MagnetClass;
		case EPickupType::DataPacket:
		default:
			if (DataPacketClass)
			{
				return DataPacketClass;
			}
			return PickupClass; // Backward compatibility
	}
}

void UPickupSpawnerComponent::ExpandPool(EPickupType Type)
{
	TArray<TObjectPtr<ABasePickup>>& Pool = GetPoolForType(Type);
	
	// Rare pickups get smaller expansion
	int32 ExpansionSize;
	switch (Type)
	{
		case EPickupType::OneUp: ExpansionSize = 3; break;
		case EPickupType::EMP: ExpansionSize = 2; break;
		case EPickupType::Magnet: ExpansionSize = 2; break;
		default: ExpansionSize = PoolExpansionSize; break;
	}
	
	int32 OldSize = Pool.Num();
	Pool.Reserve(OldSize + ExpansionSize);

	for (int32 i = 0; i < ExpansionSize; i++)
	{
		ABasePickup* Pickup = SpawnPickupActor(Type);
		if (Pickup)
		{
			Pool.Add(Pickup);
		}
	}
}

ABasePickup* UPickupSpawnerComponent::SpawnPickupActor(EPickupType Type)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TSubclassOf<ABasePickup> ClassToUse = GetClassForType(Type);
	if (!ClassToUse)
	{
		return nullptr;
	}

	FVector SpawnLocation(0.0f, 0.0f, -10000.0f);
	FRotator SpawnRotation = FRotator::ZeroRotator;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABasePickup* Pickup = World->SpawnActor<ABasePickup>(ClassToUse, SpawnLocation, SpawnRotation, SpawnParams);

	if (Pickup)
	{
		Pickup->Deactivate();
	}

	return Pickup;
}

bool UPickupSpawnerComponent::ShouldSpawn1Up() const
{
	if (!OneUpClass)
	{
		return false;
	}
	
	// Not high enough density yet
	if (CurrentDensityLevel < OneUpMinDifficulty)
	{
		return false;
	}
	
	// Cooldown: don't spawn back-to-back
	if (SegmentsSpawned <= Last1UpSpawnSegment + 1)
	{
		return false;
	}
	
	// Force-spawn the first 1-Up once we hit the guaranteed threshold
	if (!bHasSpawnedFirst1Up && CurrentDensityLevel >= OneUpGuaranteedDifficulty)
	{
		return true;
	}
	
	// Random chance
	float Roll = FMath::FRand();
	bool bSpawn = Roll < OneUpSpawnChance;
	return bSpawn;
}

void UPickupSpawnerComponent::Spawn1Up(float SegmentStartX, float SegmentEndX, bool bIsGuaranteed)
{
	ABasePickup* OneUp = GetPickupFromPool(EPickupType::OneUp);
	if (!OneUp)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("Failed to get 1-Up from pool!"));
		return;
	}

	float SegmentLength = SegmentEndX - SegmentStartX;
	FVector SafeLocation;
	bool bFoundSafeSpot = false;
	ELane FinalLane = ELane::Center;
	
	// --- Guaranteed 1-Up ---
	// Always center lane, front of segment, easy to see and reach
	if (bIsGuaranteed)
	{
		FinalLane = ELane::Center;
		
		// 20% into segment so the player sees it coming
		float GuaranteedX = SegmentStartX + (0.20f * SegmentLength);
		float WorldY = GetLaneYPosition(ELane::Center);
		float WorldZ = PickupSpawnZ + 60.0f; // 1-Ups are larger than data packets
		
		FVector GuaranteedLocation(GuaranteedX, WorldY, WorldZ);
		
		if (FindSafeSpawnPosition(GuaranteedLocation, ELane::Center, SafeLocation))
		{
			bFoundSafeSpot = true;
		}
		else
		{
			// Try multiple positions along segment front half
			for (float RelX = 0.15f; RelX <= 0.40f && !bFoundSafeSpot; RelX += 0.05f)
			{
				FVector TryLocation(SegmentStartX + (RelX * SegmentLength), WorldY, WorldZ);
				if (FindSafeSpawnPosition(TryLocation, ELane::Center, SafeLocation))
				{
					bFoundSafeSpot = true;
				}
			}
		}
		
		// Last resort: spawn anyway
		if (!bFoundSafeSpot)
		{
			SafeLocation = GuaranteedLocation;
			bFoundSafeSpot = true;
			UE_LOG(LogStateRunner_Arcade, Warning, TEXT("Guaranteed 1-Up: Forced spawn despite obstacle proximity"));
		}
	}
	// --- Normal 1-Up ---
	// Lane diversification: avoid spawning consecutive 1-Ups in the same lane
	else
	{
		TArray<ELane> AllowedLanes;
		if (Last1UpLane != ELane::Left)   AllowedLanes.Add(ELane::Left);
		if (Last1UpLane != ELane::Center) AllowedLanes.Add(ELane::Center);
		if (Last1UpLane != ELane::Right)  AllowedLanes.Add(ELane::Right);
		
		// Prefer center lane (risky placement)
		if (AllowedLanes.Contains(ELane::Center))
		{
			AllowedLanes.Remove(ELane::Center);
			AllowedLanes.Insert(ELane::Center, 0);
		}
		
		const int32 AttemptsPerLane = 3;
		
		for (ELane TryLane : AllowedLanes)
		{
			if (bFoundSafeSpot) break;
			
			for (int32 Attempt = 0; Attempt < AttemptsPerLane && !bFoundSafeSpot; Attempt++)
			{
				float RelativeX = FMath::FRandRange(0.25f, 0.75f);
				
				float WorldX = SegmentStartX + (RelativeX * SegmentLength);
				float WorldY = GetLaneYPosition(TryLane);
				float WorldZ = PickupSpawnZ + 60.0f;

				FVector DesiredLocation(WorldX, WorldY, WorldZ);
				
				if (FindSafeSpawnPosition(DesiredLocation, TryLane, SafeLocation))
				{
					bFoundSafeSpot = true;
					FinalLane = TryLane;
				}
			}
		}
		
		if (!bFoundSafeSpot)
		{
			// Last resort: try segment edges
			for (ELane EdgeLane : AllowedLanes)
			{
				if (bFoundSafeSpot) break;
				
				float EdgeX = SegmentStartX + (FMath::RandBool() ? 0.15f : 0.85f) * SegmentLength;
				FVector EdgeLocation(EdgeX, GetLaneYPosition(EdgeLane), PickupSpawnZ + 60.0f);
				
				if (FindSafeSpawnPosition(EdgeLocation, EdgeLane, SafeLocation))
				{
					bFoundSafeSpot = true;
					FinalLane = EdgeLane;
				}
			}
		}
	}
	
	// --- Spawn the 1-Up ---
	if (bFoundSafeSpot)
	{
		OneUp->Activate(SafeLocation, FinalLane);
		ActivePickups.Add(OneUp);

		bHasSpawnedFirst1Up = true;
		Last1UpLane = FinalLane;
		Last1UpSpawnSegment = SegmentsSpawned;

		const TCHAR* LaneName = (FinalLane == ELane::Center) ? TEXT("Center") : 
								(FinalLane == ELane::Left) ? TEXT("Left") : TEXT("Right");
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("*** 1-UP SPAWNED! *** Lane: %s, Guaranteed: %s, Density: %d, Position: (%.0f, %.0f, %.0f)"),
			LaneName, bIsGuaranteed ? TEXT("YES") : TEXT("NO"), CurrentDensityLevel,
			SafeLocation.X, SafeLocation.Y, SafeLocation.Z);

		if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
		{
			if (bIsGuaranteed)
			{
				Debug->LogEvent(EDebugCategory::Spawning, FString::Printf(TEXT("1-UP (FREE) %s lane"), LaneName));
			}
			else
			{
				Debug->LogEvent(EDebugCategory::Spawning, FString::Printf(TEXT("1-UP %s lane"), LaneName));
			}
		}
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("1-Up spawn FAILED - no safe spawn position found!"));
	}
}

bool UPickupSpawnerComponent::ShouldSpawnEMP() const
{
	if (!EMPClass)
	{
		return false;
	}
	
	// EMPs only show up in late game
	if (CurrentDensityLevel < EMPMinDifficulty)
	{
		return false;
	}
	
	// Cooldown: don't spawn back-to-back
	if (SegmentsSpawned <= LastEMPSpawnSegment + 1)
	{
		return false;
	}
	
	// Bonus chance in segments with lots of obstacles
	float EffectiveChance = EMPSpawnChance;
	if (CachedObstaclePositions.Num() >= EMPHighDensityThreshold)
	{
		EffectiveChance += EMPHighDensityBonusChance;
	}
	
	float Roll = FMath::FRand();
	bool bSpawn = Roll < EffectiveChance;
	return bSpawn;
}

void UPickupSpawnerComponent::SpawnEMP(float SegmentStartX, float SegmentEndX, int32 ObstacleCount)
{
	ABasePickup* EMP = GetPickupFromPool(EPickupType::EMP);
	if (!EMP)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("Failed to get EMP from pool!"));
		return;
	}

	float SegmentLength = SegmentEndX - SegmentStartX;
	FVector SafeLocation;
	bool bFoundSafeSpot = false;
	ELane FinalLane = ELane::Center;
	
	// Weighted lane selection: 60% center, 20% left, 20% right
	TArray<ELane> LanePriority;
	float LaneRoll = FMath::FRand();
	if (LaneRoll < 0.6f)
	{
		LanePriority.Add(ELane::Center);
		if (FMath::RandBool())
		{
			LanePriority.Add(ELane::Left);
			LanePriority.Add(ELane::Right);
		}
		else
		{
			LanePriority.Add(ELane::Right);
			LanePriority.Add(ELane::Left);
		}
	}
	else if (LaneRoll < 0.8f)
	{
		LanePriority = { ELane::Left, ELane::Center, ELane::Right };
	}
	else
	{
		LanePriority = { ELane::Right, ELane::Center, ELane::Left };
	}

	// Find the X range where obstacles actually exist in this segment
	float ObstacleMinX = SegmentEndX;
	float ObstacleMaxX = SegmentStartX;
	
	for (const FVector& ObstaclePos : CachedObstaclePositions)
	{
		if (ObstaclePos.X >= SegmentStartX && ObstaclePos.X <= SegmentEndX)
		{
			ObstacleMinX = FMath::Min(ObstacleMinX, ObstaclePos.X);
			ObstacleMaxX = FMath::Max(ObstacleMaxX, ObstaclePos.X);
		}
	}
	
	// If we found a cluster of obstacles, try to spawn within their X range
	bool bHasObstacleCluster = (ObstacleMaxX > ObstacleMinX) && (CachedObstaclePositions.Num() >= 3);
	
	if (bHasObstacleCluster)
	{
		// Spread spawn range slightly beyond the cluster edges
		float ClusterRange = ObstacleMaxX - ObstacleMinX;
		float SpawnMinX = ObstacleMinX - (ClusterRange * 0.1f);
		float SpawnMaxX = ObstacleMaxX + (ClusterRange * 0.1f);
		
		// Clamp to segment bounds
		SpawnMinX = FMath::Max(SpawnMinX, SegmentStartX + (SegmentLength * 0.15f));
		SpawnMaxX = FMath::Min(SpawnMaxX, SegmentEndX - (SegmentLength * 0.15f));
		
		if (SpawnMaxX <= SpawnMinX)
		{
			SpawnMinX = SegmentStartX + (SegmentLength * 0.3f);
			SpawnMaxX = SegmentEndX - (SegmentLength * 0.3f);
		}
		
		for (ELane TryLane : LanePriority)
		{
			if (bFoundSafeSpot) break;
			
			for (int32 Attempt = 0; Attempt < 8 && !bFoundSafeSpot; Attempt++)
			{
				float WorldX = FMath::FRandRange(SpawnMinX, SpawnMaxX);
				float WorldY = GetLaneYPosition(TryLane);
				float WorldZ = PickupSpawnZ + 80.0f; // EMPs are larger than data packets

				FVector DesiredLocation(WorldX, WorldY, WorldZ);
				
				if (FindSafeSpawnPosition(DesiredLocation, TryLane, SafeLocation))
				{
					bFoundSafeSpot = true;
					FinalLane = TryLane;
				}
			}
		}
	}
	
	// Fallback: wider segment range
	if (!bFoundSafeSpot)
	{
		for (ELane TryLane : LanePriority)
		{
			if (bFoundSafeSpot) break;
			
			for (int32 Attempt = 0; Attempt < 5 && !bFoundSafeSpot; Attempt++)
			{
				float RelativeX = FMath::FRandRange(0.25f, 0.75f);
				
				float WorldX = SegmentStartX + (RelativeX * SegmentLength);
				float WorldY = GetLaneYPosition(TryLane);
				float WorldZ = PickupSpawnZ + 80.0f;

				FVector DesiredLocation(WorldX, WorldY, WorldZ);
				
				if (FindSafeSpawnPosition(DesiredLocation, TryLane, SafeLocation))
				{
					bFoundSafeSpot = true;
					FinalLane = TryLane;
				}
			}
		}
	}
	
	// Last resort - force spawn
	if (!bFoundSafeSpot)
	{
		ELane RandomLane = LanePriority[0];
		float RandomX = SegmentStartX + (FMath::FRandRange(0.3f, 0.7f) * SegmentLength);
		SafeLocation = FVector(RandomX, GetLaneYPosition(RandomLane), PickupSpawnZ + 80.0f);
		bFoundSafeSpot = true;
		FinalLane = RandomLane;
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("EMP: Forced spawn at random position despite obstacles"));
	}
	
	if (bFoundSafeSpot)
	{
		EMP->Activate(SafeLocation, FinalLane);
		ActivePickups.Add(EMP);
		LastEMPSpawnSegment = SegmentsSpawned;

		const TCHAR* LaneName = (FinalLane == ELane::Center) ? TEXT("Center") : 
								(FinalLane == ELane::Left) ? TEXT("Left") : TEXT("Right");
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("*** EMP SPAWNED! *** Lane: %s, Density: %d, Obstacles: %d, Position: (%.0f, %.0f, %.0f)"),
			LaneName, CurrentDensityLevel, ObstacleCount,
			SafeLocation.X, SafeLocation.Y, SafeLocation.Z);

		if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
		{
			Debug->LogEvent(EDebugCategory::Spawning, FString::Printf(TEXT("EMP %s lane"), LaneName));
		}
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("EMP spawn FAILED - no safe spawn position found!"));
	}
}

// --- Magnet (Tractor Beam) ---

bool UPickupSpawnerComponent::ShouldSpawnMagnet() const
{
	if (!MagnetClass)
	{
		return false;
	}
	
	// Magnets appear at mid-game
	if (CurrentDensityLevel < MagnetMinDifficulty)
	{
		return false;
	}
	
	// Cooldown: don't spawn back-to-back
	if (SegmentsSpawned <= LastMagnetSpawnSegment + 1)
	{
		return false;
	}
	
	float Roll = FMath::FRand();
	bool bSpawn = Roll < MagnetSpawnChance;
	return bSpawn;
}

void UPickupSpawnerComponent::SpawnMagnet(float SegmentStartX, float SegmentEndX)
{
	ABasePickup* Magnet = GetPickupFromPool(EPickupType::Magnet);
	if (!Magnet)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("Failed to get Magnet from pool!"));
		return;
	}

	float SegmentLength = SegmentEndX - SegmentStartX;
	FVector SafeLocation;
	bool bFoundSafeSpot = false;
	ELane FinalLane = ELane::Center;

	// Shuffle lanes for variety
	TArray<ELane> Lanes = { ELane::Left, ELane::Center, ELane::Right };
	for (int32 i = Lanes.Num() - 1; i > 0; --i)
	{
		int32 j = FMath::RandRange(0, i);
		Lanes.Swap(i, j);
	}

	for (ELane TryLane : Lanes)
	{
		if (bFoundSafeSpot) break;
		
		for (int32 Attempt = 0; Attempt < 8 && !bFoundSafeSpot; Attempt++)
		{
			float RelativeX = FMath::FRandRange(0.2f, 0.8f);
			float WorldX = SegmentStartX + (RelativeX * SegmentLength);
			float WorldY = GetLaneYPosition(TryLane);
			float WorldZ = PickupSpawnZ;

			FVector DesiredLocation(WorldX, WorldY, WorldZ);
			
			if (FindSafeSpawnPosition(DesiredLocation, TryLane, SafeLocation))
			{
				bFoundSafeSpot = true;
				FinalLane = TryLane;
			}
		}
	}
	
	// Last resort - force spawn
	if (!bFoundSafeSpot)
	{
		ELane RandomLane = Lanes[0];
		float RandomX = SegmentStartX + (FMath::FRandRange(0.3f, 0.7f) * SegmentLength);
		SafeLocation = FVector(RandomX, GetLaneYPosition(RandomLane), PickupSpawnZ);
		bFoundSafeSpot = true;
		FinalLane = RandomLane;
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("Magnet: Forced spawn at random position despite obstacles"));
	}

	if (bFoundSafeSpot)
	{
		Magnet->Activate(SafeLocation, FinalLane);
		ActivePickups.Add(Magnet);
		LastMagnetSpawnSegment = SegmentsSpawned;

		const TCHAR* LaneName = (FinalLane == ELane::Center) ? TEXT("Center") : 
								(FinalLane == ELane::Left) ? TEXT("Left") : TEXT("Right");
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("*** MAGNET SPAWNED! *** Lane: %s, Density: %d, Position: (%.0f, %.0f, %.0f)"),
			LaneName, CurrentDensityLevel,
			SafeLocation.X, SafeLocation.Y, SafeLocation.Z);

		if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
		{
			Debug->LogEvent(EDebugCategory::Spawning, FString::Printf(TEXT("MAGNET %s lane"), LaneName));
		}
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("Magnet spawn FAILED - no safe spawn position found!"));
	}
}

void UPickupSpawnerComponent::ActivateMagnet()
{
	bIsMagnetActive = true;
	MagnetTimeRemaining = MagnetDuration;
	
	// Enable tick for the duration of the magnet effect
	SetComponentTickEnabled(true);
	
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("MAGNET activated! Duration: %.1fs, PullSpeed: %.0f, Range: %.0f"),
		MagnetDuration, MagnetPullSpeed, MagnetPullRange);

	// Broadcast state change for UI/VFX
	OnMagnetStateChanged.Broadcast(true);

	// Turn on magnet VFX on the player
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (AStateRunner_ArcadeCharacter* Character = Cast<AStateRunner_ArcadeCharacter>(PC->GetPawn()))
			{
				Character->SetMagnetEffectActive(true);
			}
		}
	}
}

void UPickupSpawnerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (!bIsMagnetActive)
	{
		SetComponentTickEnabled(false);
		return;
	}
	
	// Count down magnet timer
	MagnetTimeRemaining -= DeltaTime;
	if (MagnetTimeRemaining <= 0.0f)
	{
		bIsMagnetActive = false;
		MagnetTimeRemaining = 0.0f;
		SetComponentTickEnabled(false);
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("MAGNET expired"));

		// Broadcast state change and deactivate player VFX
		OnMagnetStateChanged.Broadcast(false);
		if (UWorld* World = GetWorld())
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				if (AStateRunner_ArcadeCharacter* Character = Cast<AStateRunner_ArcadeCharacter>(PC->GetPawn()))
				{
					Character->SetMagnetEffectActive(false);
				}
			}
		}
		return;
	}
	
	// Get player position for targeting
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn) return;
	
	const FVector PlayerLoc = PlayerPawn->GetActorLocation();
	
	// Pull all active pickups toward the player (except other Magnets).
	// No dead zones -- pickups in any direction get pulled.
	// Overlap collision handles collection when they arrive.
	for (ABasePickup* Pickup : ActivePickups)
	{
		if (!Pickup || !Pickup->IsActive()) continue;
		if (Pickup->GetPickupType() == EPickupType::Magnet) continue;
		
		FVector PickupLoc = Pickup->GetActorLocation();
		float XDist = PickupLoc.X - PlayerLoc.X;
		float YDiff = PlayerLoc.Y - PickupLoc.Y;
		
		// Range check (symmetric -- behind the player counts too)
		if (FMath::Abs(XDist) > MagnetPullRange) continue;
		
		float Distance2D = FMath::Sqrt(XDist * XDist + YDiff * YDiff);
		float ZDiff = PlayerLoc.Z - PickupLoc.Z;
		float Distance3D = FMath::Sqrt(XDist * XDist + YDiff * YDiff + ZDiff * ZDiff);
		
		// Two-phase pull:
		// FAR (>500u): FInterpTo for smooth vacuum feel (percentage-based, decelerates)
		// CLOSE (<=500u): Constant-velocity pull to guarantee arrival
		//
		// FInterpTo is asymptotic and slows as the gap shrinks. At high scroll speeds
		// (OVERCLOCK at ~8000+ u/s) the world scroll can outrun the weakening interp.
		// Constant-velocity at close range always wins.
		
		const float ConstantPullThreshold = 500.0f;
		
		if (Distance3D <= ConstantPullThreshold)
		{
			// Close range: constant-velocity pull that outruns scroll speed
			float ActualScrollSpeed = 0.0f;
			if (AStateRunner_ArcadeGameMode* GM = Cast<AStateRunner_ArcadeGameMode>(GetOwner()))
			{
				if (UWorldScrollComponent* WSC = GM->GetWorldScrollComponent())
				{
					ActualScrollSpeed = WSC->GetCurrentScrollSpeed();
				}
			}
			// Pull at 2x scroll speed (min 4000 u/s fallback)
			float ConstantSpeed = FMath::Max(ActualScrollSpeed * 2.0f, 4000.0f);
			FVector NewLoc = FMath::VInterpConstantTo(PickupLoc, PlayerLoc, DeltaTime, ConstantSpeed);
			Pickup->SetActorLocation(NewLoc);
		}
		else
		{
			// Far range: FInterpTo for smooth vacuum pull
			float BaseInterpSpeed = MagnetPullSpeed / 100.0f;
			float ProximityBoost = FMath::GetMappedRangeValueClamped(
				FVector2D(0.0f, MagnetPullRange),
				FVector2D(3.0f, 1.0f),
				Distance2D
			);
			float InterpSpeed = BaseInterpSpeed * ProximityBoost;
			
			// Pull X toward player from any direction
			PickupLoc.X = FMath::FInterpTo(PickupLoc.X, PlayerLoc.X, DeltaTime, InterpSpeed);
			
			// Pull Y (lateral)
			if (FMath::Abs(YDiff) > 1.0f)
			{
				PickupLoc.Y = FMath::FInterpTo(PickupLoc.Y, PlayerLoc.Y, DeltaTime, InterpSpeed);
			}
			
			// Pull Z so bobbing/elevated pickups reach the capsule
			if (FMath::Abs(ZDiff) > 1.0f)
			{
				PickupLoc.Z = FMath::FInterpTo(PickupLoc.Z, PlayerLoc.Z, DeltaTime, InterpSpeed);
			}
			
			Pickup->SetActorLocation(PickupLoc);
		}
	}
}

void UPickupSpawnerComponent::UpdateDensity()
{
#if !UE_BUILD_SHIPPING
	if (bDebugDensityOverride)
	{
		return;
	}
#endif

	int32 NewDensity = SegmentsSpawned / SegmentsPerDensityIncrease;
	
	if (NewDensity != CurrentDensityLevel)
	{
		CurrentDensityLevel = NewDensity;
	}
}

void UPickupSpawnerComponent::DebugIncreaseDifficulty()
{
#if !UE_BUILD_SHIPPING
	bDebugDensityOverride = true;
	CurrentDensityLevel++;
	
	bool bOneUpCanSpawn = (CurrentDensityLevel >= OneUpMinDifficulty) && OneUpClass != nullptr;
	bool bEMPCanSpawn = (CurrentDensityLevel >= EMPMinDifficulty) && EMPClass != nullptr;
	
	FString DebugMessage = FString::Printf(
		TEXT("Difficulty LOCKED -> %d | 1-Up: %s (need %d) | EMP: %s (need %d)"),
		CurrentDensityLevel,
		bOneUpCanSpawn ? TEXT("YES") : TEXT("NO"), OneUpMinDifficulty,
		bEMPCanSpawn ? TEXT("YES") : TEXT("NO"), EMPMinDifficulty
	);
	
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, DebugMessage);
	}
	
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("%s"), *DebugMessage);
#endif
}

float UPickupSpawnerComponent::GetLaneYPosition(ELane Lane) const
{
	switch (Lane)
	{
		case ELane::Left:	return LeftLaneY;
		case ELane::Center:	return CenterLaneY;
		case ELane::Right:	return RightLaneY;
		default:			return CenterLaneY;
	}
}

ELane UPickupSpawnerComponent::GetRandomLane() const
{
	int32 RandomValue = FMath::RandRange(0, 2);
	switch (RandomValue)
	{
		case 0:		return ELane::Left;
		case 1:		return ELane::Center;
		default:	return ELane::Right;
	}
}

// --- Obstacle Avoidance ---

void UPickupSpawnerComponent::CacheObstaclePositions(float SegmentStartX, float SegmentEndX)
{
	CachedObstaclePositions.Empty();
	CachedObstacleTypes.Empty();
	bSegmentHasBlockingObstacles = false;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AStateRunner_ArcadeGameMode* GameMode = Cast<AStateRunner_ArcadeGameMode>(World->GetAuthGameMode());
	if (!GameMode)
	{
		return;
	}

	UObstacleSpawnerComponent* ObstacleSpawner = GameMode->GetObstacleSpawnerComponent();
	if (!ObstacleSpawner)
	{
		return;
	}

	// Include obstacles slightly outside segment bounds
	const float SearchMargin = 500.0f;
	
	TArray<AActor*> FoundObstacles;
	UGameplayStatics::GetAllActorsOfClass(World, ABaseObstacle::StaticClass(), FoundObstacles);
	
	for (AActor* Actor : FoundObstacles)
	{
		ABaseObstacle* Obstacle = Cast<ABaseObstacle>(Actor);
		if (Obstacle && Obstacle->IsActive())
		{
			FVector ObstaclePos = Obstacle->GetActorLocation();
			
			if (ObstaclePos.X >= (SegmentStartX - SearchMargin) && 
				ObstaclePos.X <= (SegmentEndX + SearchMargin))
			{
				CachedObstaclePositions.Add(ObstaclePos);
				
				EObstacleType ObsType = Obstacle->GetObstacleType();
				CachedObstacleTypes.Add(ObsType);
				
				// FullWalls and HighBarriers require dodging
				if (ObsType == EObstacleType::FullWall || ObsType == EObstacleType::HighBarrier)
				{
					bSegmentHasBlockingObstacles = true;
				}
			}
		}
	}
}

bool UPickupSpawnerComponent::IsPositionSafeFromObstacles(const FVector& WorldPosition) const
{
	for (const FVector& ObstaclePos : CachedObstaclePositions)
	{
		float DeltaX = FMath::Abs(WorldPosition.X - ObstaclePos.X);
		float DeltaY = FMath::Abs(WorldPosition.Y - ObstaclePos.Y);
		
		// Same-lane check uses tighter X threshold, cross-lane uses diagonal distance
		bool bSameLane = DeltaY < 200.0f;
		
		if (bSameLane)
		{
			if (DeltaX < MinDistanceFromObstacle)
			{
				return false;
			}
		}
		else
		{
			float Distance2D = FMath::Sqrt(DeltaX * DeltaX + DeltaY * DeltaY);
			if (Distance2D < MinDistanceFromObstacle * 0.6f)
			{
				return false;
			}
		}
	}

	return true;
}

bool UPickupSpawnerComponent::FindSafeSpawnPosition(const FVector& DesiredPosition, ELane Lane, FVector& OutSafePosition) const
{
	if (IsPositionSafeFromObstacles(DesiredPosition))
	{
		OutSafePosition = DesiredPosition;
		return true;
	}

	// Try offsetting in X to find a clear spot
	const float OffsetStep = 75.0f;
	const float MaxOffset = MinDistanceFromObstacle * 2.0f;

	for (int32 Attempt = 1; Attempt <= MaxObstacleAvoidanceAttempts; Attempt++)
	{
		float Offset = OffsetStep * Attempt;

		// Try forward
		FVector ForwardPos = DesiredPosition + FVector(Offset, 0.0f, 0.0f);
		if (IsPositionSafeFromObstacles(ForwardPos))
		{
			OutSafePosition = ForwardPos;
			return true;
		}

		// Try backward
		FVector BackwardPos = DesiredPosition + FVector(-Offset, 0.0f, 0.0f);
		if (IsPositionSafeFromObstacles(BackwardPos))
		{
			OutSafePosition = BackwardPos;
			return true;
		}

		if (Offset >= MaxOffset)
		{
			break;
		}
	}

	return false;
}

// --- Pattern Functions ---

bool UPickupSpawnerComponent::GenerateFromPattern(TArray<FPickupSpawnData>& OutPickups)
{
	const FPickupPattern* SelectedPattern = SelectPattern();
	
	if (!SelectedPattern)
	{
		return false;
	}

	// If the pattern has manually placed pickups, use those directly
	if (SelectedPattern->Pickups.Num() > 0)
	{
		OutPickups = SelectedPattern->Pickups;
		return true;
	}

	// Otherwise generate procedurally based on pattern type
	GeneratePatternPickups(OutPickups, SelectedPattern->PatternType, SelectedPattern->PickupCount);
	
	return OutPickups.Num() > 0;
}

void UPickupSpawnerComponent::GeneratePatternPickups(TArray<FPickupSpawnData>& OutPickups, EPickupPatternType PatternType, int32 PickupCount)
{
	switch (PatternType)
	{
		// --- Ground Patterns ---
		case EPickupPatternType::Trail:
			GenerateTrailPattern(OutPickups, PickupCount, GetRandomLane());
			break;
			
		case EPickupPatternType::Arc:
			GenerateArcPattern(OutPickups, PickupCount, FMath::RandBool());
			break;
			
		case EPickupPatternType::Zigzag:
			GenerateZigzagPattern(OutPickups, PickupCount);
			break;
			
		case EPickupPatternType::Cluster:
			GenerateClusterPattern(OutPickups, PickupCount, FMath::FRandRange(0.3f, 0.7f));
			break;
			
		case EPickupPatternType::Diamond:
			GenerateDiamondPattern(OutPickups, FMath::FRandRange(0.3f, 0.7f));
			break;
			
		case EPickupPatternType::Wave:
			GenerateWavePattern(OutPickups, PickupCount);
			break;
			
		case EPickupPatternType::Diagonal:
			GenerateDiagonalPattern(OutPickups, PickupCount, FMath::RandBool());
			break;
			
		case EPickupPatternType::TripleLine:
			GenerateTripleLinePattern(OutPickups, FMath::FRandRange(0.3f, 0.7f));
			break;
			
		case EPickupPatternType::Scatter:
			GenerateScatterPattern(OutPickups, PickupCount);
			break;
		
		// --- Aerial Patterns ---
		case EPickupPatternType::VerticalArc:
			GenerateVerticalArcPattern(OutPickups, PickupCount, GetRandomLane());
			break;
			
		case EPickupPatternType::Ascending:
			GenerateAscendingPattern(OutPickups, PickupCount, GetRandomLane());
			break;
			
		case EPickupPatternType::Descending:
			GenerateDescendingPattern(OutPickups, PickupCount, GetRandomLane());
			break;
			
		case EPickupPatternType::Helix:
			GenerateHelixPattern(OutPickups, PickupCount);
			break;
			
		case EPickupPatternType::JumpArc:
			GenerateJumpArcPattern(OutPickups, PickupCount, GetRandomLane(), FMath::RandBool());
			break;
			
		case EPickupPatternType::Staircase:
			GenerateStaircasePattern(OutPickups, PickupCount, FMath::RandBool());
			break;
			
		case EPickupPatternType::SkyTrail:
			GenerateSkyTrailPattern(OutPickups, PickupCount, GetRandomLane());
			break;
			
		case EPickupPatternType::Tower:
			GenerateTowerPattern(OutPickups, FMath::FRandRange(0.3f, 0.7f), GetRandomLane());
			break;
			
		case EPickupPatternType::Rainbow:
			GenerateRainbowPattern(OutPickups, PickupCount, FMath::RandBool());
			break;
			
		case EPickupPatternType::BouncingArcs:
			GenerateBouncingArcsPattern(OutPickups, PickupCount / 3);
			break;
			
		case EPickupPatternType::Random:
		default:
			GeneratePickupLayout(OutPickups, PickupCount);
			break;
	}
}

void UPickupSpawnerComponent::GenerateTrailPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, ELane Lane)
{
	// Line of pickups in a single lane
	OutPickups.Reserve(Count);
	
	float StartX = GetEffectiveMinSpawnOffset();
	float EndX = GetEffectiveMaxSpawnOffset();
	float Spacing = (EndX - StartX) / FMath::Max(1, Count - 1);
	
	for (int32 i = 0; i < Count; i++)
	{
		FPickupSpawnData Data;
		Data.Lane = Lane;
		Data.RelativeXOffset = StartX + (i * Spacing);
		Data.ZOffset = 0.0f;
		OutPickups.Add(Data);
	}
}

void UPickupSpawnerComponent::GenerateArcPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, bool bLeftToRight)
{
	// Parabolic curve across lanes (L-C-R-C-L)
	OutPickups.Reserve(Count);
	
	float StartX = GetEffectiveMinSpawnOffset();
	float EndX = GetEffectiveMaxSpawnOffset();
	float Spacing = (EndX - StartX) / FMath::Max(1, Count - 1);
	
	for (int32 i = 0; i < Count; i++)
	{
		FPickupSpawnData Data;
		
		float t = (float)i / FMath::Max(1, Count - 1);
		float ParabolicValue = 4.0f * t * (1.0f - t);
		
		int32 LaneIndex = FMath::RoundToInt(ParabolicValue * 2.0f);
		if (!bLeftToRight)
		{
			LaneIndex = 2 - LaneIndex;
		}
		
		switch (LaneIndex)
		{
			case 0: Data.Lane = ELane::Left; break;
			case 1: Data.Lane = ELane::Center; break;
			default: Data.Lane = ELane::Right; break;
		}
		
		Data.RelativeXOffset = StartX + (i * Spacing);
		Data.ZOffset = 0.0f;
		OutPickups.Add(Data);
	}
}

void UPickupSpawnerComponent::GenerateZigzagPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count)
{
	// Zigzag: L-C-R-C-L-C-R...
	OutPickups.Reserve(Count);
	
	float StartX = GetEffectiveMinSpawnOffset();
	float EndX = GetEffectiveMaxSpawnOffset();
	float Spacing = (EndX - StartX) / FMath::Max(1, Count - 1);
	
	const ELane ZigzagSequence[] = { ELane::Left, ELane::Center, ELane::Right, ELane::Center };
	const int32 SequenceLength = 4;
	
	for (int32 i = 0; i < Count; i++)
	{
		FPickupSpawnData Data;
		Data.Lane = ZigzagSequence[i % SequenceLength];
		Data.RelativeXOffset = StartX + (i * Spacing);
		Data.ZOffset = 0.0f;
		OutPickups.Add(Data);
	}
}

void UPickupSpawnerComponent::GenerateClusterPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, float CenterX)
{
	// Tight cluster around a center point
	OutPickups.Reserve(Count);
	
	const float ClusterRadius = 0.15f;
	const ELane Lanes[] = { ELane::Left, ELane::Center, ELane::Right };
	
	for (int32 i = 0; i < Count; i++)
	{
		FPickupSpawnData Data;
		
		if (i % 3 == 0)
		{
			Data.Lane = ELane::Center;
		}
		else
		{
			Data.Lane = Lanes[FMath::RandRange(0, 2)];
		}
		
		float RandomOffset = FMath::FRandRange(-ClusterRadius, ClusterRadius);
		Data.RelativeXOffset = FMath::Clamp(CenterX + RandomOffset, GetEffectiveMinSpawnOffset(), GetEffectiveMaxSpawnOffset());
		Data.ZOffset = 0.0f;
		OutPickups.Add(Data);
	}
}

void UPickupSpawnerComponent::GenerateDiamondPattern(TArray<FPickupSpawnData>& OutPickups, float CenterX)
{
	// Diamond shape: 5 pickups
	//      C
	//    L   R
	//      C
	//      C
	
	const float Spacing = 0.08f;
	const float EffectiveMin = GetEffectiveMinSpawnOffset();
	const float EffectiveMax = GetEffectiveMaxSpawnOffset();
	
	FPickupSpawnData Top;
	Top.Lane = ELane::Center;
	Top.RelativeXOffset = FMath::Clamp(CenterX - Spacing * 1.5f, EffectiveMin, EffectiveMax);
	OutPickups.Add(Top);
	
	FPickupSpawnData Left;
	Left.Lane = ELane::Left;
	Left.RelativeXOffset = FMath::Clamp(CenterX, EffectiveMin, EffectiveMax);
	OutPickups.Add(Left);
	
	FPickupSpawnData Right;
	Right.Lane = ELane::Right;
	Right.RelativeXOffset = FMath::Clamp(CenterX, EffectiveMin, EffectiveMax);
	OutPickups.Add(Right);
	
	FPickupSpawnData Center;
	Center.Lane = ELane::Center;
	Center.RelativeXOffset = FMath::Clamp(CenterX + Spacing, EffectiveMin, EffectiveMax);
	OutPickups.Add(Center);
	
	FPickupSpawnData Bottom;
	Bottom.Lane = ELane::Center;
	Bottom.RelativeXOffset = FMath::Clamp(CenterX + Spacing * 2.5f, EffectiveMin, EffectiveMax);
	OutPickups.Add(Bottom);
}

void UPickupSpawnerComponent::GenerateWavePattern(TArray<FPickupSpawnData>& OutPickups, int32 Count)
{
	// Sine wave across lanes
	OutPickups.Reserve(Count);
	
	float StartX = GetEffectiveMinSpawnOffset();
	float EndX = GetEffectiveMaxSpawnOffset();
	float Spacing = (EndX - StartX) / FMath::Max(1, Count - 1);
	
	const float Frequency = 2.0f * PI;
	
	for (int32 i = 0; i < Count; i++)
	{
		FPickupSpawnData Data;
		
		float t = (float)i / FMath::Max(1, Count - 1);
		float SineValue = FMath::Sin(t * Frequency);
		
		int32 LaneIndex = FMath::RoundToInt((SineValue + 1.0f) * 0.5f * 2.0f);
		LaneIndex = FMath::Clamp(LaneIndex, 0, 2);
		
		switch (LaneIndex)
		{
			case 0: Data.Lane = ELane::Left; break;
			case 1: Data.Lane = ELane::Center; break;
			default: Data.Lane = ELane::Right; break;
		}
		
		Data.RelativeXOffset = StartX + (i * Spacing);
		Data.ZOffset = 0.0f;
		OutPickups.Add(Data);
	}
}

void UPickupSpawnerComponent::GenerateDiagonalPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, bool bLeftToRight)
{
	// Diagonal line from one corner to the other
	OutPickups.Reserve(Count);
	
	float StartX = GetEffectiveMinSpawnOffset();
	float EndX = GetEffectiveMaxSpawnOffset();
	float Spacing = (EndX - StartX) / FMath::Max(1, Count - 1);
	
	for (int32 i = 0; i < Count; i++)
	{
		FPickupSpawnData Data;
		
		float t = (float)i / FMath::Max(1, Count - 1);
		int32 LaneIndex = FMath::RoundToInt(t * 2.0f);
		
		if (!bLeftToRight)
		{
			LaneIndex = 2 - LaneIndex;
		}
		
		switch (LaneIndex)
		{
			case 0: Data.Lane = ELane::Left; break;
			case 1: Data.Lane = ELane::Center; break;
			default: Data.Lane = ELane::Right; break;
		}
		
		Data.RelativeXOffset = StartX + (i * Spacing);
		Data.ZOffset = 0.0f;
		OutPickups.Add(Data);
	}
}

void UPickupSpawnerComponent::GenerateTripleLinePattern(TArray<FPickupSpawnData>& OutPickups, float XPosition)
{
	// All three lanes at the same X - reward wall
	const float EffectiveMin = GetEffectiveMinSpawnOffset();
	const float EffectiveMax = GetEffectiveMaxSpawnOffset();
	
	for (ELane Lane : { ELane::Left, ELane::Center, ELane::Right })
	{
		FPickupSpawnData Data;
		Data.Lane = Lane;
		Data.RelativeXOffset = FMath::Clamp(XPosition, EffectiveMin, EffectiveMax);
		Data.ZOffset = 0.0f;
		OutPickups.Add(Data);
	}
}

void UPickupSpawnerComponent::GenerateScatterPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count)
{
	// Golden ratio spacing for visual interest
	OutPickups.Reserve(Count);
	
	const float GoldenRatio = 1.618033988749895f;
	const float EffectiveMin = GetEffectiveMinSpawnOffset();
	const float EffectiveMax = GetEffectiveMaxSpawnOffset();
	float CurrentX = EffectiveMin;
	
	for (int32 i = 0; i < Count; i++)
	{
		FPickupSpawnData Data;
		
		int32 LaneIndex = (int32)(i * GoldenRatio) % 3;
		switch (LaneIndex)
		{
			case 0: Data.Lane = ELane::Left; break;
			case 1: Data.Lane = ELane::Center; break;
			default: Data.Lane = ELane::Right; break;
		}
		
		float BaseSpacing = (EffectiveMax - EffectiveMin) / Count;
		float Variation = BaseSpacing * 0.3f * FMath::Sin(i * GoldenRatio);
		CurrentX += BaseSpacing + Variation;
		
		Data.RelativeXOffset = FMath::Clamp(CurrentX, EffectiveMin, EffectiveMax);
		Data.ZOffset = 0.0f;
		OutPickups.Add(Data);
	}
}

// --- Aerial Pattern Generators ---

void UPickupSpawnerComponent::GenerateVerticalArcPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, ELane Lane)
{
	// Vertical arc: ground -> apex -> ground in one lane
	// Collect the whole arc with a well-timed jump
	OutPickups.Reserve(Count);
	
	float StartX, EndX;
	GetAerialPatternBounds(StartX, EndX);
	float Spacing = (EndX - StartX) / FMath::Max(1, Count - 1);
	
	const TArray<EPickupHeight> ArcHeights = {
		EPickupHeight::Ground,
		EPickupHeight::LowAir,
		EPickupHeight::MidAir,
		EPickupHeight::HighAir,
		EPickupHeight::Apex,
		EPickupHeight::HighAir,
		EPickupHeight::MidAir,
		EPickupHeight::LowAir,
		EPickupHeight::Ground
	};
	
	for (int32 i = 0; i < Count; i++)
	{
		FPickupSpawnData Data;
		Data.Lane = Lane;
		Data.RelativeXOffset = StartX + (i * Spacing);
		
		float t = (float)i / FMath::Max(1, Count - 1);
		float ParabolicValue = 4.0f * t * (1.0f - t);
		
		if (ParabolicValue < 0.2f)
			Data.HeightLevel = EPickupHeight::Ground;
		else if (ParabolicValue < 0.4f)
			Data.HeightLevel = EPickupHeight::LowAir;
		else if (ParabolicValue < 0.6f)
			Data.HeightLevel = EPickupHeight::MidAir;
		else if (ParabolicValue < 0.8f)
			Data.HeightLevel = EPickupHeight::HighAir;
		else
			Data.HeightLevel = EPickupHeight::Apex;
		
		OutPickups.Add(Data);
	}
}

void UPickupSpawnerComponent::GenerateAscendingPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, ELane Lane)
{
	// Ground to apex, rewards sustained jump hold
	OutPickups.Reserve(Count);
	
	float StartX, EndX;
	GetAerialPatternBounds(StartX, EndX);
	float Spacing = (EndX - StartX) / FMath::Max(1, Count - 1);
	
	const EPickupHeight Heights[] = {
		EPickupHeight::Ground,
		EPickupHeight::LowAir,
		EPickupHeight::MidAir,
		EPickupHeight::HighAir,
		EPickupHeight::Apex
	};
	const int32 NumHeights = 5;
	
	for (int32 i = 0; i < Count; i++)
	{
		FPickupSpawnData Data;
		Data.Lane = Lane;
		Data.RelativeXOffset = StartX + (i * Spacing);
		
		float t = (float)i / FMath::Max(1, Count - 1);
		int32 HeightIndex = FMath::Clamp(FMath::RoundToInt(t * (NumHeights - 1)), 0, NumHeights - 1);
		Data.HeightLevel = Heights[HeightIndex];
		
		OutPickups.Add(Data);
	}
}

void UPickupSpawnerComponent::GenerateDescendingPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, ELane Lane)
{
	// Apex to ground, catch during fall
	OutPickups.Reserve(Count);
	
	float StartX, EndX;
	GetAerialPatternBounds(StartX, EndX);
	float Spacing = (EndX - StartX) / FMath::Max(1, Count - 1);
	
	const EPickupHeight Heights[] = {
		EPickupHeight::Apex,
		EPickupHeight::HighAir,
		EPickupHeight::MidAir,
		EPickupHeight::LowAir,
		EPickupHeight::Ground
	};
	const int32 NumHeights = 5;
	
	for (int32 i = 0; i < Count; i++)
	{
		FPickupSpawnData Data;
		Data.Lane = Lane;
		Data.RelativeXOffset = StartX + (i * Spacing);
		
		float t = (float)i / FMath::Max(1, Count - 1);
		int32 HeightIndex = FMath::Clamp(FMath::RoundToInt(t * (NumHeights - 1)), 0, NumHeights - 1);
		Data.HeightLevel = Heights[HeightIndex];
		
		OutPickups.Add(Data);
	}
}

void UPickupSpawnerComponent::GenerateHelixPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count)
{
	// 3D corkscrew: zigzag lanes + height oscillation
	OutPickups.Reserve(Count);
	
	float StartX, EndX;
	GetAerialPatternBounds(StartX, EndX);
	float Spacing = (EndX - StartX) / FMath::Max(1, Count - 1);
	
	const ELane LaneSequence[] = { ELane::Left, ELane::Center, ELane::Right, ELane::Center };
	const EPickupHeight HeightSequence[] = { 
		EPickupHeight::Ground, 
		EPickupHeight::LowAir, 
		EPickupHeight::MidAir, 
		EPickupHeight::HighAir,
		EPickupHeight::MidAir,
		EPickupHeight::LowAir
	};
	
	for (int32 i = 0; i < Count; i++)
	{
		FPickupSpawnData Data;
		Data.Lane = LaneSequence[i % 4];
		Data.RelativeXOffset = StartX + (i * Spacing);
		Data.HeightLevel = HeightSequence[i % 6];
		
		OutPickups.Add(Data);
	}
}

void UPickupSpawnerComponent::GenerateJumpArcPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, ELane Lane, bool bHighJump)
{
	// Follows natural parabolic jump trajectory
	OutPickups.Reserve(Count);
	
	float StartX, EndX;
	GetAerialPatternBounds(StartX, EndX);
	float Spacing = (EndX - StartX) / FMath::Max(1, Count - 1);
	
	float PeakHeight = bHighJump ? 350.0f : 200.0f;
	
	for (int32 i = 0; i < Count; i++)
	{
		FPickupSpawnData Data;
		Data.Lane = Lane;
		Data.RelativeXOffset = StartX + (i * Spacing);
		
		float t = (float)i / FMath::Max(1, Count - 1);
		float HeightValue = 4.0f * PeakHeight * t * (1.0f - t);
		
		Data.HeightLevel = EPickupHeight::Ground;
		Data.ZOffset = HeightValue;
		
		OutPickups.Add(Data);
	}
}

void UPickupSpawnerComponent::GenerateStaircasePattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, bool bLeftToRight)
{
	// Ascending diagonally across lanes, each step changes lane + height
	OutPickups.Reserve(Count);
	
	float StartX, EndX;
	GetAerialPatternBounds(StartX, EndX);
	float Spacing = (EndX - StartX) / FMath::Max(1, Count - 1);
	
	const ELane LanesLR[] = { ELane::Left, ELane::Center, ELane::Right };
	const ELane LanesRL[] = { ELane::Right, ELane::Center, ELane::Left };
	const ELane* Lanes = bLeftToRight ? LanesLR : LanesRL;
	
	const EPickupHeight Heights[] = {
		EPickupHeight::Ground,
		EPickupHeight::LowAir,
		EPickupHeight::MidAir,
		EPickupHeight::HighAir,
		EPickupHeight::Apex
	};
	
	for (int32 i = 0; i < Count; i++)
	{
		FPickupSpawnData Data;
		Data.Lane = Lanes[i % 3];
		Data.RelativeXOffset = StartX + (i * Spacing);
		
		int32 HeightIndex = FMath::Min(i, 4);
		Data.HeightLevel = Heights[HeightIndex];
		
		OutPickups.Add(Data);
	}
}

void UPickupSpawnerComponent::GenerateSkyTrailPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, ELane Lane)
{
	// High altitude line, requires sustained max jump
	OutPickups.Reserve(Count);
	
	float StartX, EndX;
	GetAerialPatternBounds(StartX, EndX);
	float Spacing = (EndX - StartX) / FMath::Max(1, Count - 1);
	
	for (int32 i = 0; i < Count; i++)
	{
		FPickupSpawnData Data;
		Data.Lane = Lane;
		Data.RelativeXOffset = StartX + (i * Spacing);
		Data.HeightLevel = EPickupHeight::HighAir;
		
		OutPickups.Add(Data);
	}
}

void UPickupSpawnerComponent::GenerateTowerPattern(TArray<FPickupSpawnData>& OutPickups, float CenterX, ELane Lane)
{
	// Vertical stack at same X, one pickup per height level
	const EPickupHeight Heights[] = {
		EPickupHeight::Ground,
		EPickupHeight::LowAir,
		EPickupHeight::MidAir,
		EPickupHeight::HighAir,
		EPickupHeight::Apex
	};
	
	float AerialStartX, AerialEndX;
	GetAerialPatternBounds(AerialStartX, AerialEndX);
	float SafeCenterX = FMath::Clamp(CenterX, AerialStartX, AerialEndX);
	
	for (int32 i = 0; i < 5; i++)
	{
		FPickupSpawnData Data;
		Data.Lane = Lane;
		Data.RelativeXOffset = SafeCenterX;
		Data.HeightLevel = Heights[i];
		
		OutPickups.Add(Data);
	}
}

void UPickupSpawnerComponent::GenerateRainbowPattern(TArray<FPickupSpawnData>& OutPickups, int32 Count, bool bLeftToRight)
{
	// 3D arc across lanes AND height
	OutPickups.Reserve(Count);
	
	float StartX, EndX;
	GetAerialPatternBounds(StartX, EndX);
	float Spacing = (EndX - StartX) / FMath::Max(1, Count - 1);
	
	for (int32 i = 0; i < Count; i++)
	{
		FPickupSpawnData Data;
		Data.RelativeXOffset = StartX + (i * Spacing);
		
		float t = (float)i / FMath::Max(1, Count - 1);
		float ParabolicValue = 4.0f * t * (1.0f - t);
		
		int32 LaneIndex = FMath::RoundToInt(ParabolicValue * 2.0f);
		if (!bLeftToRight)
		{
			LaneIndex = 2 - LaneIndex;
		}
		
		switch (LaneIndex)
		{
			case 0: Data.Lane = ELane::Left; break;
			case 1: Data.Lane = ELane::Center; break;
			default: Data.Lane = ELane::Right; break;
		}
		
		if (ParabolicValue < 0.25f)
			Data.HeightLevel = EPickupHeight::LowAir;
		else if (ParabolicValue < 0.5f)
			Data.HeightLevel = EPickupHeight::MidAir;
		else if (ParabolicValue < 0.75f)
			Data.HeightLevel = EPickupHeight::HighAir;
		else
			Data.HeightLevel = EPickupHeight::Apex;
		
		OutPickups.Add(Data);
	}
}

void UPickupSpawnerComponent::GenerateBouncingArcsPattern(TArray<FPickupSpawnData>& OutPickups, int32 NumBounds)
{
	// Alternating L->R and R->L aerial arcs, creates a bouncing ball effect
	// Each bound = 3 pickups across all lanes at jump arc heights
	
	NumBounds = FMath::Max(2, NumBounds);
	int32 TotalPickups = NumBounds * 3;
	OutPickups.Reserve(TotalPickups);
	
	float StartX, EndX;
	GetAerialPatternBounds(StartX, EndX);
	
	float TotalRange = EndX - StartX;
	float BoundWidth = TotalRange / NumBounds;
	float PickupSpacing = BoundWidth / 3.0f;
	
	// Low -> High -> Low per bound
	const EPickupHeight ArcHeights[] = {
		EPickupHeight::LowAir,
		EPickupHeight::HighAir,
		EPickupHeight::LowAir
	};
	
	float CurrentX = StartX;
	bool bLeftToRight = FMath::RandBool();
	
	for (int32 Bound = 0; Bound < NumBounds; Bound++)
	{
		ELane Lanes[3];
		if (bLeftToRight)
		{
			Lanes[0] = ELane::Left;
			Lanes[1] = ELane::Center;
			Lanes[2] = ELane::Right;
		}
		else
		{
			Lanes[0] = ELane::Right;
			Lanes[1] = ELane::Center;
			Lanes[2] = ELane::Left;
		}
		
		for (int32 i = 0; i < 3; i++)
		{
			FPickupSpawnData Data;
			Data.Lane = Lanes[i];
			Data.RelativeXOffset = FMath::Clamp(CurrentX, StartX, EndX);
			Data.HeightLevel = ArcHeights[i];
			
			OutPickups.Add(Data);
			CurrentX += PickupSpacing;
		}
		
		bLeftToRight = !bLeftToRight;
	}
}

bool UPickupSpawnerComponent::IsAerialPattern(EPickupPatternType PatternType)
{
	// Aerial patterns have height variation and could lead players into jumps.
	// Skip these when FullWalls or HighBarriers are in the segment.
	switch (PatternType)
	{
		case EPickupPatternType::VerticalArc:
		case EPickupPatternType::Ascending:
		case EPickupPatternType::Descending:
		case EPickupPatternType::Helix:
		case EPickupPatternType::JumpArc:
		case EPickupPatternType::Staircase:
		case EPickupPatternType::SkyTrail:
		case EPickupPatternType::Tower:
		case EPickupPatternType::Rainbow:
		case EPickupPatternType::BouncingArcs:
			return true;
		default:
			return false;
	}
}

const FPickupPattern* UPickupSpawnerComponent::SelectPattern() const
{
	// Filter by difficulty and skip aerial patterns if blocking obstacles are present
	TArray<const FPickupPattern*> ValidPatterns;
	float TotalWeight = 0.0f;

	for (const FPickupPattern& Pattern : PredefinedPatterns)
	{
		if (Pattern.MinDifficultyLevel <= CurrentDensityLevel)
		{
			// Don't use aerial patterns when there are walls/barriers to dodge
			if (bSegmentHasBlockingObstacles && IsAerialPattern(Pattern.PatternType))
			{
				continue;
			}
			
			ValidPatterns.Add(&Pattern);
			TotalWeight += Pattern.SelectionWeight;
		}
	}

	if (ValidPatterns.Num() == 0 || TotalWeight <= 0.0f)
	{
		return nullptr;
	}

	// Weighted random selection
	float RandomValue = FMath::FRand() * TotalWeight;
	float AccumulatedWeight = 0.0f;

	for (const FPickupPattern* Pattern : ValidPatterns)
	{
		AccumulatedWeight += Pattern->SelectionWeight;
		if (RandomValue <= AccumulatedWeight)
		{
			return Pattern;
		}
	}

	return ValidPatterns.Last();
}

void UPickupSpawnerComponent::CreateDefaultPatterns()
{
	// --- Difficulty 0 Patterns (Early game) ---

	// Center Trail - Easy to collect
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Center Trail");
		Pattern.PatternType = EPickupPatternType::Trail;
		Pattern.PickupCount = 5;
		Pattern.MinDifficultyLevel = 0;
		Pattern.SelectionWeight = 1.5f;
		PredefinedPatterns.Add(Pattern);
	}

	// Simple Zigzag
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Simple Zigzag");
		Pattern.PatternType = EPickupPatternType::Zigzag;
		Pattern.PickupCount = 4;
		Pattern.MinDifficultyLevel = 0;
		Pattern.SelectionWeight = 1.3f;
		PredefinedPatterns.Add(Pattern);
	}

	// Triple Reward
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Triple Reward");
		Pattern.PatternType = EPickupPatternType::TripleLine;
		Pattern.PickupCount = 3;
		Pattern.MinDifficultyLevel = 0;
		Pattern.SelectionWeight = 1.0f;
		PredefinedPatterns.Add(Pattern);
	}

	// Small Cluster
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Small Cluster");
		Pattern.PatternType = EPickupPatternType::Cluster;
		Pattern.PickupCount = 4;
		Pattern.MinDifficultyLevel = 0;
		Pattern.SelectionWeight = 1.2f;
		PredefinedPatterns.Add(Pattern);
	}

	// --- Difficulty 1 Patterns (Building up) ---

	// Arc Sweep
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Arc Sweep");
		Pattern.PatternType = EPickupPatternType::Arc;
		Pattern.PickupCount = 6;
		Pattern.MinDifficultyLevel = 1;
		Pattern.SelectionWeight = 1.1f;
		PredefinedPatterns.Add(Pattern);
	}

	// Wave Ride
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Wave Ride");
		Pattern.PatternType = EPickupPatternType::Wave;
		Pattern.PickupCount = 6;
		Pattern.MinDifficultyLevel = 1;
		Pattern.SelectionWeight = 1.0f;
		PredefinedPatterns.Add(Pattern);
	}

	// Diagonal Dash
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Diagonal Dash");
		Pattern.PatternType = EPickupPatternType::Diagonal;
		Pattern.PickupCount = 5;
		Pattern.MinDifficultyLevel = 1;
		Pattern.SelectionWeight = 1.0f;
		PredefinedPatterns.Add(Pattern);
	}

	// Diamond Bonus
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Diamond Bonus");
		Pattern.PatternType = EPickupPatternType::Diamond;
		Pattern.PickupCount = 5;
		Pattern.MinDifficultyLevel = 1;
		Pattern.SelectionWeight = 0.9f;
		PredefinedPatterns.Add(Pattern);
	}

	// --- Difficulty 2 Patterns (Challenging) ---

	// Long Trail
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Long Trail");
		Pattern.PatternType = EPickupPatternType::Trail;
		Pattern.PickupCount = 8;
		Pattern.MinDifficultyLevel = 2;
		Pattern.SelectionWeight = 0.9f;
		PredefinedPatterns.Add(Pattern);
	}

	// Extended Zigzag
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Extended Zigzag");
		Pattern.PatternType = EPickupPatternType::Zigzag;
		Pattern.PickupCount = 8;
		Pattern.MinDifficultyLevel = 2;
		Pattern.SelectionWeight = 1.0f;
		PredefinedPatterns.Add(Pattern);
	}

	// Golden Scatter
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Golden Scatter");
		Pattern.PatternType = EPickupPatternType::Scatter;
		Pattern.PickupCount = 7;
		Pattern.MinDifficultyLevel = 2;
		Pattern.SelectionWeight = 0.85f;
		PredefinedPatterns.Add(Pattern);
	}

	// Dense Wave
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Dense Wave");
		Pattern.PatternType = EPickupPatternType::Wave;
		Pattern.PickupCount = 9;
		Pattern.MinDifficultyLevel = 2;
		Pattern.SelectionWeight = 0.8f;
		PredefinedPatterns.Add(Pattern);
	}

	// --- Difficulty 3+ Patterns (Expert) ---

	// Mega Trail
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Mega Trail");
		Pattern.PatternType = EPickupPatternType::Trail;
		Pattern.PickupCount = 10;
		Pattern.MinDifficultyLevel = 3;
		Pattern.SelectionWeight = 0.75f;
		PredefinedPatterns.Add(Pattern);
	}

	// Big Cluster
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Big Cluster");
		Pattern.PatternType = EPickupPatternType::Cluster;
		Pattern.PickupCount = 8;
		Pattern.MinDifficultyLevel = 3;
		Pattern.SelectionWeight = 0.7f;
		PredefinedPatterns.Add(Pattern);
	}

	// Chaos Scatter
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Chaos Scatter");
		Pattern.PatternType = EPickupPatternType::Scatter;
		Pattern.PickupCount = 10;
		Pattern.MinDifficultyLevel = 3;
		Pattern.SelectionWeight = 0.65f;
		PredefinedPatterns.Add(Pattern);
	}

	// Triple Wall Bonus (manual layout)
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Triple Wall Bonus");
		Pattern.MinDifficultyLevel = 3;
		Pattern.SelectionWeight = 0.5f;
		
		// Two triple lines spaced apart
		for (float X : { 0.3f, 0.6f })
		{
			for (ELane Lane : { ELane::Left, ELane::Center, ELane::Right })
			{
				FPickupSpawnData Data;
				Data.Lane = Lane;
				Data.RelativeXOffset = X;
				Data.ZOffset = 0.0f;
				Pattern.Pickups.Add(Data);
			}
		}
		
		PredefinedPatterns.Add(Pattern);
	}

	// --- Aerial Patterns (height variation, lowered difficulty thresholds) ---

	// Jump Arc - follows natural jump trajectory (difficulty 0)
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Jump Arc");
		Pattern.PatternType = EPickupPatternType::JumpArc;
		Pattern.PickupCount = 5;
		Pattern.MinDifficultyLevel = 0;
		Pattern.SelectionWeight = 1.0f;
		PredefinedPatterns.Add(Pattern);
	}

	// Vertical Arc - up and down in one lane (difficulty 0)
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Vertical Arc");
		Pattern.PatternType = EPickupPatternType::VerticalArc;
		Pattern.PickupCount = 7;
		Pattern.MinDifficultyLevel = 0;
		Pattern.SelectionWeight = 0.9f;
		PredefinedPatterns.Add(Pattern);
	}

	// Ascending Line (difficulty 1)
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Ascending Line");
		Pattern.PatternType = EPickupPatternType::Ascending;
		Pattern.PickupCount = 5;
		Pattern.MinDifficultyLevel = 1;
		Pattern.SelectionWeight = 0.85f;
		PredefinedPatterns.Add(Pattern);
	}

	// Descending Line (difficulty 1)
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Descending Line");
		Pattern.PatternType = EPickupPatternType::Descending;
		Pattern.PickupCount = 5;
		Pattern.MinDifficultyLevel = 1;
		Pattern.SelectionWeight = 0.85f;
		PredefinedPatterns.Add(Pattern);
	}

	// Staircase (difficulty 1)
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Staircase");
		Pattern.PatternType = EPickupPatternType::Staircase;
		Pattern.PickupCount = 6;
		Pattern.MinDifficultyLevel = 1;
		Pattern.SelectionWeight = 0.8f;
		PredefinedPatterns.Add(Pattern);
	}

	// Helix Spiral (difficulty 2)
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Helix Spiral");
		Pattern.PatternType = EPickupPatternType::Helix;
		Pattern.PickupCount = 8;
		Pattern.MinDifficultyLevel = 2;
		Pattern.SelectionWeight = 0.7f;
		PredefinedPatterns.Add(Pattern);
	}

	// Sky Trail (difficulty 2)
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Sky Trail");
		Pattern.PatternType = EPickupPatternType::SkyTrail;
		Pattern.PickupCount = 5;
		Pattern.MinDifficultyLevel = 2;
		Pattern.SelectionWeight = 0.65f;
		PredefinedPatterns.Add(Pattern);
	}

	// Tower (difficulty 2)
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Tower");
		Pattern.PatternType = EPickupPatternType::Tower;
		Pattern.PickupCount = 5;
		Pattern.MinDifficultyLevel = 2;
		Pattern.SelectionWeight = 0.6f;
		PredefinedPatterns.Add(Pattern);
	}

	// Rainbow (difficulty 2)
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Rainbow");
		Pattern.PatternType = EPickupPatternType::Rainbow;
		Pattern.PickupCount = 9;
		Pattern.MinDifficultyLevel = 2;
		Pattern.SelectionWeight = 0.55f;
		PredefinedPatterns.Add(Pattern);
	}

	// High Jump Arc (difficulty 3)
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("High Jump Arc");
		Pattern.PatternType = EPickupPatternType::JumpArc;
		Pattern.PickupCount = 7;
		Pattern.MinDifficultyLevel = 3;
		Pattern.SelectionWeight = 0.5f;
		PredefinedPatterns.Add(Pattern);
	}

	// Double Helix (difficulty 3)
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Double Helix");
		Pattern.PatternType = EPickupPatternType::Helix;
		Pattern.PickupCount = 12;
		Pattern.MinDifficultyLevel = 3;
		Pattern.SelectionWeight = 0.45f;
		PredefinedPatterns.Add(Pattern);
	}

	// Epic Rainbow (difficulty 3)
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Epic Rainbow");
		Pattern.PatternType = EPickupPatternType::Rainbow;
		Pattern.PickupCount = 12;
		Pattern.MinDifficultyLevel = 3;
		Pattern.SelectionWeight = 0.4f;
		PredefinedPatterns.Add(Pattern);
	}

	// --- Difficulty 4+ Patterns (INSANE Combo Enablers, 10+ pickups) ---

	// Super Trail - 12 pickups in a line
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Super Trail");
		Pattern.PatternType = EPickupPatternType::Trail;
		Pattern.PickupCount = 12;
		Pattern.MinDifficultyLevel = 4;
		Pattern.SelectionWeight = 1.2f;
		PredefinedPatterns.Add(Pattern);
	}

	// Mega Zigzag - 11 pickups
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Mega Zigzag");
		Pattern.PatternType = EPickupPatternType::Zigzag;
		Pattern.PickupCount = 11;
		Pattern.MinDifficultyLevel = 4;
		Pattern.SelectionWeight = 1.0f;
		PredefinedPatterns.Add(Pattern);
	}

	// Wave Marathon - 12 pickups
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Wave Marathon");
		Pattern.PatternType = EPickupPatternType::Wave;
		Pattern.PickupCount = 12;
		Pattern.MinDifficultyLevel = 4;
		Pattern.SelectionWeight = 0.9f;
		PredefinedPatterns.Add(Pattern);
	}

	// Ultra Scatter - 11 pickups
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Ultra Scatter");
		Pattern.PatternType = EPickupPatternType::Scatter;
		Pattern.PickupCount = 11;
		Pattern.MinDifficultyLevel = 4;
		Pattern.SelectionWeight = 0.8f;
		PredefinedPatterns.Add(Pattern);
	}

	// Bouncing Arcs - 4 bounds x 3 pickups = 12
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Bouncing Arcs");
		Pattern.PatternType = EPickupPatternType::BouncingArcs;
		Pattern.PickupCount = 12;
		Pattern.MinDifficultyLevel = 4;
		Pattern.SelectionWeight = 1.1f;
		PredefinedPatterns.Add(Pattern);
	}

	// Extended Bouncing Arcs - 5 bounds = 15 pickups
	{
		FPickupPattern Pattern;
		Pattern.PatternName = TEXT("Extended Bouncing Arcs");
		Pattern.PatternType = EPickupPatternType::BouncingArcs;
		Pattern.PickupCount = 15;
		Pattern.MinDifficultyLevel = 5;
		Pattern.SelectionWeight = 0.7f;
		PredefinedPatterns.Add(Pattern);
	}
}

// --- Debug Functions ---

bool UPickupSpawnerComponent::TrySpawnDebugPickupRow(float SegmentStartX, float SegmentEndX)
{
	if (bDebugPickupsSpawned)
	{
		return false;
	}

	float SpawnX = SegmentStartX + DebugPickupRowOffset;
	float SpawnZ = PickupSpawnZ;

	// Spawn EMP in left lane
	// Activate immediately so the next pool request doesn't return the same actor
	ABasePickup* EMP = GetPickupFromPool(EPickupType::EMP);
	if (!EMP)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("Pickup Showcase: Failed to get EMP from pool!"));
		return false;
	}
	
	FVector EMPPos(SpawnX, GetLaneYPosition(ELane::Left), SpawnZ);
	EMP->Activate(EMPPos, ELane::Left);
	ActivePickups.Add(EMP);
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("Pickup Showcase: EMP LEFT spawned at (%.0f, %.0f, %.0f)"), 
		EMPPos.X, EMPPos.Y, EMPPos.Z);

	// Spawn Magnet in center lane (falls back to 1-Up if MagnetClass isn't set)
	if (MagnetClass)
	{
		ABasePickup* MagnetPickup = GetPickupFromPool(EPickupType::Magnet);
		if (MagnetPickup)
		{
			FVector MagnetPos(SpawnX, GetLaneYPosition(ELane::Center), SpawnZ);
			MagnetPickup->Activate(MagnetPos, ELane::Center);
			ActivePickups.Add(MagnetPickup);
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("Pickup Showcase: Magnet CENTER spawned at (%.0f, %.0f, %.0f)"), 
				MagnetPos.X, MagnetPos.Y, MagnetPos.Z);
		}
		else
		{
			UE_LOG(LogStateRunner_Arcade, Warning, TEXT("Pickup Showcase: Failed to get Magnet from pool!"));
			bDebugPickupsSpawned = true;
			return false;
		}
	}
	else
	{
		// Fallback: 1-Up center if Magnet isn't configured
		ABasePickup* OneUpCenter = GetPickupFromPool(EPickupType::OneUp);
		if (OneUpCenter)
		{
			FVector OneUpCenterPos(SpawnX, GetLaneYPosition(ELane::Center), SpawnZ);
			OneUpCenter->Activate(OneUpCenterPos, ELane::Center);
			ActivePickups.Add(OneUpCenter);
			UE_LOG(LogStateRunner_Arcade, Log, TEXT("Pickup Showcase: 1-Up CENTER (fallback) spawned at (%.0f, %.0f, %.0f)"), 
				OneUpCenterPos.X, OneUpCenterPos.Y, OneUpCenterPos.Z);
		}
	}

	// Spawn 1-Up in right lane
	ABasePickup* OneUpRight = GetPickupFromPool(EPickupType::OneUp);
	if (!OneUpRight)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("Pickup Showcase: Failed to get 1-Up RIGHT from pool!"));
		bDebugPickupsSpawned = true;
		return false;
	}
	
	FVector OneUpRightPos(SpawnX, GetLaneYPosition(ELane::Right), SpawnZ);
	OneUpRight->Activate(OneUpRightPos, ELane::Right);
	ActivePickups.Add(OneUpRight);
	UE_LOG(LogStateRunner_Arcade, Log, TEXT("Pickup Showcase: 1-Up RIGHT spawned at (%.0f, %.0f, %.0f)"), 
		OneUpRightPos.X, OneUpRightPos.Y, OneUpRightPos.Z);

	bDebugPickupsSpawned = true;

	if (UGameDebugSubsystem* Debug = UGameDebugSubsystem::Get(this))
	{
		Debug->LogEvent(EDebugCategory::Spawning, 
			MagnetClass ? TEXT("Pickup Showcase: EMP | Magnet | 1-Up spawned") 
			            : TEXT("Pickup Showcase: EMP | 1-Up | 1-Up spawned"));
	}

	return true;
}
