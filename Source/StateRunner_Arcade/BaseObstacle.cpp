#include "BaseObstacle.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "WorldScrollComponent.h"
#include "StateRunner_Arcade.h"
#include "StateRunner_ArcadeGameMode.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

ABaseObstacle::ABaseObstacle()
{
	// Enable ticking (but we'll disable when inactive for pooling)
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // Start disabled, enable on Activate()

	// Create root component
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	SetRootComponent(RootSceneComponent);

	// Create collision box
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetupAttachment(RootSceneComponent);
	
	// Configure collision for overlap detection (NOT blocking)
	// Player should pass through obstacles, not be stopped by them
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionBox->SetGenerateOverlapEvents(true);
	
	// Initial box extent (overridden by SetupCollisionBox on Activate)
	CollisionBox->SetBoxExtent(FVector(50.0f, 150.0f, 40.0f));

	// Create visual mesh component
	ObstacleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ObstacleMesh"));
	ObstacleMesh->SetupAttachment(RootSceneComponent);
	
	// Mesh doesn't need collision - collision handled by CollisionBox
	ObstacleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Start hidden (pooled state)
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void ABaseObstacle::BeginPlay()
{
	Super::BeginPlay();

	// Cache reference to WorldScrollComponent for scroll speed queries
	CacheWorldScrollComponent();

	// Bind collision overlap event
	if (CollisionBox)
	{
		CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &ABaseObstacle::OnCollisionOverlapBegin);
	}

	// Setup collision box based on configured extent
	SetupCollisionBox();
}

void ABaseObstacle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Only tick if active
	if (!bIsActive)
	{
		return;
	}

	// Update position based on scroll speed
	UpdateScrolling(DeltaTime);

	// Debug: Draw collision box (persistent so visible when paused)
	if (bDrawDebugCollision && CollisionBox)
	{
		FVector BoxCenter = CollisionBox->GetComponentLocation();
		FVector BoxExtent = CollisionBox->GetScaledBoxExtent();
		FColor DebugColor = FColor::Red;
		
		switch (ObstacleType)
		{
			case EObstacleType::LowWall: DebugColor = FColor::Orange; break;
			case EObstacleType::HighBarrier: DebugColor = FColor::Purple; break;
			case EObstacleType::FullWall: DebugColor = FColor::Red; break;
		}
		
		// Use 0.1f duration so it persists when paused, and draw in front of geometry
		DrawDebugBox(GetWorld(), BoxCenter, BoxExtent, DebugColor, false, 0.1f, 0, 4.0f);
	}

	// Check if should despawn
	if (ShouldDespawn())
	{
		Deactivate();
	}
}

// --- Pooling Functions ---

void ABaseObstacle::Activate(const FVector& SpawnLocation, ELane Lane, EObstacleType Type)
{
	// Set state
	bIsActive = true;
	bHasTriggeredDamage = false;
	CurrentLane = Lane;
	ObstacleType = Type;

	// Position the obstacle
	SetActorLocation(SpawnLocation);

	// Select random mesh variant (if variants are configured)
	SelectRandomMeshVariant();

	// Make visible and enable collision
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	// Enable ticking
	SetActorTickEnabled(true);

	// Setup collision based on type (in case type changed)
	SetupCollisionBox();

}

void ABaseObstacle::Deactivate()
{
	// Set state
	bIsActive = false;
	bHasTriggeredDamage = false;

	// Hide and disable collision
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	// Disable ticking for performance
	SetActorTickEnabled(false);

	// Move to a safe pooled position
	SetActorLocation(FVector(0.0f, 0.0f, -10000.0f));

}

// --- Collision ---

void ABaseObstacle::OnCollisionOverlapBegin(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	// Ignore if not active
	if (!bIsActive)
	{
		return;
	}

	// Ignore if already triggered damage this activation
	if (bHasTriggeredDamage)
	{
		return;
	}

	// Check if it's the player (using tag-based detection)
	if (OtherActor && OtherActor->ActorHasTag(TEXT("Player")))
	{
		bHasTriggeredDamage = true;
		HandlePlayerCollision(OtherActor);
	}
}

void ABaseObstacle::HandlePlayerCollision_Implementation(AActor* PlayerActor)
{
	// Base implementation - child classes can override for special behavior
	// Called when player overlaps the obstacle
	
	// Get obstacle type name for debug display
	FString ObstacleTypeName;
	switch (ObstacleType)
	{
		case EObstacleType::LowWall:    ObstacleTypeName = TEXT("LowWall (JUMP)"); break;
		case EObstacleType::HighBarrier: ObstacleTypeName = TEXT("HighBarrier (SLIDE)"); break;
		case EObstacleType::FullWall:   ObstacleTypeName = TEXT("FullWall (DODGE)"); break;
		default:                        ObstacleTypeName = TEXT("Unknown"); break;
	}
	
}

// --- Scrolling ---

void ABaseObstacle::UpdateScrolling(float DeltaTime)
{
	// Get current scroll speed
	if (!WorldScrollComponent)
	{
		// Try to get it again if we lost the reference
		CacheWorldScrollComponent();
		if (!WorldScrollComponent)
		{
			return;
		}
	}

	float ScrollSpeed = WorldScrollComponent->GetCurrentScrollSpeed();
	
	// Move in NEGATIVE X direction (world scrolls toward player)
	FVector CurrentLocation = GetActorLocation();
	CurrentLocation.X -= ScrollSpeed * DeltaTime;
	SetActorLocation(CurrentLocation);
}

bool ABaseObstacle::ShouldDespawn() const
{
	return GetActorLocation().X < DespawnXThreshold;
}

// --- Setup Functions ---

void ABaseObstacle::SetupCollisionBox()
{
	if (!CollisionBox)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("SetupCollisionBox: Missing CollisionBox!"));
		return;
	}

	// Only set extent (size) based on type - respect Blueprint-configured position
	// The collision box position set in the Blueprint editor is preserved
	CollisionBox->SetWorldScale3D(FVector::OneVector);

	switch (ObstacleType)
	{
		case EObstacleType::LowWall:
			CollisionBox->SetBoxExtent(LowWallCollisionExtent);
			break;

		case EObstacleType::HighBarrier:
			CollisionBox->SetBoxExtent(HighBarrierCollisionExtent);
			break;

		case EObstacleType::FullWall:
			CollisionBox->SetBoxExtent(FullWallCollisionExtent);
			break;
	}

}

// --- Mesh Variants ---

void ABaseObstacle::SelectRandomMeshVariant()
{
	// Skip if no variants configured - keep whatever mesh is already set
	if (MeshVariants.Num() == 0)
	{
		return;
	}

	// Select a random index from available variants
	const int32 RandomIndex = FMath::RandRange(0, MeshVariants.Num() - 1);
	ApplyMeshVariant(RandomIndex);
}

void ABaseObstacle::ApplyMeshVariant(int32 VariantIndex)
{
	if (!MeshVariants.IsValidIndex(VariantIndex) || !ObstacleMesh)
	{
		return;
	}

	const FMeshVariantData& VariantData = MeshVariants[VariantIndex];

	if (VariantData.Mesh)
	{
		ObstacleMesh->SetStaticMesh(VariantData.Mesh);
		
		// Clear material overrides so mesh uses its own materials
		for (int32 i = 0; i < ObstacleMesh->GetNumMaterials(); ++i)
		{
			ObstacleMesh->SetMaterial(i, nullptr);
		}
		
		// Calculate rotation with optional random yaw flip
		FRotator FinalRotation = VariantData.RotationOffset;
		if (VariantData.bRandomizeYawFlip && FMath::RandBool())
		{
			FinalRotation.Yaw += 180.0f;
		}
		
		ObstacleMesh->SetRelativeRotation(FinalRotation);
		ObstacleMesh->SetRelativeLocation(VariantData.LocationOffset);
		ObstacleMesh->SetRelativeScale3D(VariantData.Scale);
	}
}

void ABaseObstacle::CacheWorldScrollComponent()
{
	// Get GameMode and cache WorldScrollComponent reference
	if (UWorld* World = GetWorld())
	{
		if (AStateRunner_ArcadeGameMode* GameMode = Cast<AStateRunner_ArcadeGameMode>(World->GetAuthGameMode()))
		{
			WorldScrollComponent = GameMode->GetWorldScrollComponent();
		}
	}

	if (!WorldScrollComponent)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("BaseObstacle: Could not find WorldScrollComponent!"));
	}
}
