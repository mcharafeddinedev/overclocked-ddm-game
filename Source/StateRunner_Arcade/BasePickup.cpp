#include "BasePickup.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "WorldScrollComponent.h"
#include "ScoreSystemComponent.h"
#include "LivesSystemComponent.h"
#include "OverclockSystemComponent.h"
#include "ObstacleSpawnerComponent.h"
#include "PickupSpawnerComponent.h"
#include "GameDebugSubsystem.h"
#include "StateRunner_Arcade.h"
#include "BaseObstacle.h"
#include "StateRunner_ArcadeGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundBase.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

ABasePickup::ABasePickup()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Root
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	SetRootComponent(RootSceneComponent);

	// Collision box
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetupAttachment(RootSceneComponent);
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionBox->SetGenerateOverlapEvents(true);
	CollisionBox->SetBoxExtent(CollisionExtent);

	// Visual mesh
	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(RootSceneComponent);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Particle component for collection effect (scrolls with pickup)
	CollectionParticleComponent = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("CollectionParticleComponent"));
	CollectionParticleComponent->SetupAttachment(RootSceneComponent);
	CollectionParticleComponent->bAutoActivate = false;

	// Start hidden (pooled)
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void ABasePickup::BeginPlay()
{
	Super::BeginPlay();

	CacheWorldScrollComponent();

	if (CollisionBox)
	{
		CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &ABasePickup::OnCollisionOverlapBegin);
	}
}

void ABasePickup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsActive)
	{
		return;
	}

	// Scroll with the world (always, even during collection effect)
	UpdateScrolling(DeltaTime);

	// Only spin/bob if not playing collection effect (mesh is hidden then anyway)
	if (!bIsPlayingCollectionEffect)
	{
		UpdateVisualEffects(DeltaTime);
	}

	// Debug collision visualization
	if (bDrawDebugCollision && CollisionBox)
	{
		FVector BoxCenter = CollisionBox->GetComponentLocation();
		FVector BoxExtent = CollisionBox->GetScaledBoxExtent();
		FColor DebugColor = FColor::White;
		
		switch (PickupType)
		{
			case EPickupType::DataPacket: DebugColor = FColor::White; break;
			case EPickupType::OneUp: DebugColor = FColor::Yellow; break;
			case EPickupType::EMP: DebugColor = FColor::Cyan; break;
		}
		
		DrawDebugBox(GetWorld(), BoxCenter, BoxExtent, DebugColor, false, 0.1f, 0, 4.0f);
	}

	if (ShouldDespawn())
	{
		Deactivate();
	}
}

// --- Pooling ---

void ABasePickup::Activate(const FVector& SpawnLocation, ELane Lane)
{
	bIsActive = true;
	bIsPlayingCollectionEffect = false;
	CurrentLane = Lane;
	BaseZ = SpawnLocation.Z;
	BobTime = FMath::FRand() * 2.0f * PI;

	// Randomize rotation speed
	float SpeedVariation = FMath::FRandRange(-RotationSpeedVariation, RotationSpeedVariation);
	CurrentRotationSpeed = RotationSpeed * (1.0f + SpeedVariation);

	SetActorLocation(SpawnLocation);

	// Pick a random mesh variant first (sets mesh position)
	SelectRandomMeshVariant();

	// Then set up collision to match
	SetupCollisionBox();

	// Random starting yaw so pickups don't all look the same
	if (PickupMesh)
	{
		FRotator StartRotation = PickupMesh->GetRelativeRotation();
		StartRotation.Yaw += FMath::FRandRange(0.0f, 360.0f);
		PickupMesh->SetRelativeRotation(StartRotation);
	}

	// Set up collection particle
	if (CollectionParticleComponent && CollectionParticleEffect)
	{
		CollectionParticleComponent->SetTemplate(CollectionParticleEffect);
		CollectionParticleComponent->SetWorldScale3D(FVector(CollectionParticleScale));
		CollectionParticleComponent->CustomTimeDilation = CollectionParticleSpeed;
		CollectionParticleComponent->Deactivate();
	}

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
}

void ABasePickup::Deactivate()
{
	bIsActive = false;
	bIsPlayingCollectionEffect = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CollectionEffectTimerHandle);
	}

	if (CollectionParticleComponent)
	{
		CollectionParticleComponent->Deactivate();
	}

	// Restore visibility on all components for next activation
	TArray<USceneComponent*> AllComponents;
	GetComponents<USceneComponent>(AllComponents);
	for (USceneComponent* Comp : AllComponents)
	{
		if (Comp != CollectionParticleComponent)
		{
			Comp->SetVisibility(true, true);
		}
	}

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
	SetActorLocation(FVector(0.0f, 0.0f, -10000.0f));
}

// --- Collection ---

void ABasePickup::OnCollisionOverlapBegin(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!bIsActive || bIsPlayingCollectionEffect)
	{
		return;
	}

	if (OtherActor && OtherActor->ActorHasTag(TEXT("Player")))
	{
		HandleCollection(OtherActor);
	}
}

void ABasePickup::HandleCollection_Implementation(AActor* PlayerActor)
{
	// Get game systems
	UScoreSystemComponent* ScoreSystem = nullptr;
	ULivesSystemComponent* LivesSystem = nullptr;
	UOverclockSystemComponent* OverclockSystem = nullptr;
	UObstacleSpawnerComponent* ObstacleSpawner = nullptr;

	if (UWorld* World = GetWorld())
	{
		if (AStateRunner_ArcadeGameMode* GameMode = Cast<AStateRunner_ArcadeGameMode>(World->GetAuthGameMode()))
		{
			ScoreSystem = GameMode->GetScoreSystemComponent();
			LivesSystem = GameMode->GetLivesSystemComponent();
			OverclockSystem = GameMode->GetOverclockSystemComponent();
			ObstacleSpawner = GameMode->GetObstacleSpawnerComponent();
		}
	}

	if (PickupType == EPickupType::DataPacket)
	{
		if (ScoreSystem)
		{
			ScoreSystem->AddPickupBonus();
		}
		if (OverclockSystem)
		{
			OverclockSystem->AddPickupBonus();
		}

		// Streak-based pitch (computed after AddPickupBonus updates timestamps)
		float DataPacketPitch = 1.0f;
		if (ScoreSystem)
		{
			DataPacketPitch = ScoreSystem->GetCurrentPickupPitchMultiplier();
		}

		PlayCollectionEffect(DataPacketPitch);
		OnCollected.Broadcast(this);
		return;
	}
	else if (PickupType == EPickupType::OneUp)
	{
		bool bWasAtMax = false;
		if (LivesSystem)
		{
			bWasAtMax = LivesSystem->Collect1Up();
		}
		
		if (ScoreSystem)
		{
			ScoreSystem->Add1UpBonus(bWasAtMax);
		}
		
		// Bonus sound when already at max lives
		if (bWasAtMax && BonusCollectionSound)
		{
			UGameplayStatics::PlaySoundAtLocation(GetWorld(), BonusCollectionSound, GetActorLocation(), CollectionSoundVolume);
		}
		
		PlayCollectionEffectNoSound();
		OnCollected.Broadcast(this);
		return;
	}
	else if (PickupType == EPickupType::EMP)
	{
		// Screen nuke: deactivate all active obstacles
		// Only deactivates obstacles -- no side effects on spawning or other systems
		int32 DestroyedCount = 0;
		
		if (ObstacleSpawner)
		{
			DestroyedCount = ObstacleSpawner->DeactivateAllActiveObstacles();
		}
		
		if (ScoreSystem)
		{
			ScoreSystem->AddEMPBonus(DestroyedCount);
		}
		
		// Triple OVERCLOCK boost for the dramatic moment
		if (OverclockSystem)
		{
			OverclockSystem->AddPickupBonus();
			OverclockSystem->AddPickupBonus();
			OverclockSystem->AddPickupBonus();
		}
		
		PlayCollectionEffect();
		OnCollected.Broadcast(this);
		return;
	}
	else if (PickupType == EPickupType::Magnet)
	{
		UE_LOG(LogStateRunner_Arcade, Log, TEXT("*** MAGNET COLLECTED! ***"));
		
		// Activate the magnet pull effect
		if (UWorld* World = GetWorld())
		{
			if (AStateRunner_ArcadeGameMode* GameMode = Cast<AStateRunner_ArcadeGameMode>(World->GetAuthGameMode()))
			{
				if (UPickupSpawnerComponent* PickupSpawner = GameMode->GetPickupSpawnerComponent())
				{
					PickupSpawner->ActivateMagnet();
				}
				else
				{
					UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MAGNET collection: PickupSpawnerComponent is NULL on GameMode!"));
				}
				
				if (ScoreSystem)
				{
					ScoreSystem->NotifyMagnetCollected();
				}
			}
			else
			{
				UE_LOG(LogStateRunner_Arcade, Warning, TEXT("MAGNET collection: Failed to cast GameMode!"));
			}
		}
		
		PlayCollectionEffect();
		OnCollected.Broadcast(this);
		return;
	}

	// Fallback for any unhandled pickup types
	PlayCollectionEffect();
	OnCollected.Broadcast(this);
}

// --- Collection Effects ---

void ABasePickup::PlayCollectionEffect(float PitchMultiplier)
{
	PlayCollectionEffectNoSound();

	// Play sound with optional pitch scaling (data packets use streak pitch)
	if (CollectionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			GetWorld(),
			CollectionSound,
			GetActorLocation(),
			CollectionSoundVolume,
			PitchMultiplier
		);
	}
}

void ABasePickup::PlayCollectionEffectNoSound()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("PlayCollectionEffectNoSound: No World!"));
		Deactivate();
		return;
	}

	bIsPlayingCollectionEffect = true;

	// Hide all visual components except particle (we want that to stay visible)
	TArray<USceneComponent*> AllComponents;
	GetComponents<USceneComponent>(AllComponents);
	for (USceneComponent* Comp : AllComponents)
	{
		if (Comp != CollectionParticleComponent && Comp != RootSceneComponent)
		{
			Comp->SetVisibility(false, true);
		}
	}

	// Disable collision so player can't re-collect
	SetActorEnableCollision(false);

	// Fire off the particle effect
	if (CollectionParticleComponent && CollectionParticleEffect)
	{
		CollectionParticleComponent->Activate(true);
	}
	else if (!CollectionParticleEffect)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("PlayCollectionEffectNoSound: CollectionParticleEffect is NULL for %s"), *GetName());
	}

	// Timer to deactivate after effect finishes
	World->GetTimerManager().SetTimer(
		CollectionEffectTimerHandle,
		this,
		&ABasePickup::OnCollectionEffectFinished,
		CollectionEffectDuration,
		false
	);
}

void ABasePickup::OnCollectionEffectFinished()
{
	Deactivate();
}

// --- Mesh Variants ---

void ABasePickup::SelectRandomMeshVariant()
{
	if (MeshVariants.Num() == 0)
	{
		return;
	}

	const int32 RandomIndex = FMath::RandRange(0, MeshVariants.Num() - 1);
	ApplyMeshVariant(RandomIndex);
}

void ABasePickup::ApplyMeshVariant(int32 VariantIndex)
{
	if (!MeshVariants.IsValidIndex(VariantIndex))
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("BasePickup::ApplyMeshVariant - Invalid index %d (array size: %d)"),
			VariantIndex, MeshVariants.Num());
		return;
	}

	if (!PickupMesh)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("BasePickup::ApplyMeshVariant - PickupMesh component is null!"));
		return;
	}

	const FPickupMeshVariantData& VariantData = MeshVariants[VariantIndex];

	if (VariantData.Mesh)
	{
		PickupMesh->SetStaticMesh(VariantData.Mesh);
		
		// Clear material overrides so the mesh uses its own materials
		for (int32 i = 0; i < PickupMesh->GetNumMaterials(); ++i)
		{
			PickupMesh->SetMaterial(i, nullptr);
		}
		
		PickupMesh->SetRelativeScale3D(VariantData.Scale);
		PickupMesh->SetRelativeLocation(VariantData.LocationOffset);
		
		// Optional random yaw flip for variety
		MeshBaseRotation = VariantData.RotationOffset;
		if (VariantData.bRandomizeYawFlip && FMath::RandBool())
		{
			MeshBaseRotation.Yaw += 180.0f;
		}
		
		PickupMesh->SetRelativeRotation(MeshBaseRotation);
	}
	else
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("BasePickup::ApplyMeshVariant - MeshVariants[%d].Mesh is null!"), VariantIndex);
	}
}

// --- Movement ---

void ABasePickup::UpdateScrolling(float DeltaTime)
{
	if (!WorldScrollComponent)
	{
		CacheWorldScrollComponent();
		if (!WorldScrollComponent)
		{
			return;
		}
	}

	float ScrollSpeed = WorldScrollComponent->GetCurrentScrollSpeed();
	
	FVector CurrentLocation = GetActorLocation();
	CurrentLocation.X -= ScrollSpeed * DeltaTime;
	SetActorLocation(CurrentLocation);
}

void ABasePickup::UpdateVisualEffects(float DeltaTime)
{
	// Spin
	if (PickupMesh && CurrentRotationSpeed > 0.0f)
	{
		FRotator CurrentRotation = PickupMesh->GetRelativeRotation();
		CurrentRotation.Yaw += CurrentRotationSpeed * DeltaTime;
		PickupMesh->SetRelativeRotation(CurrentRotation);
	}

	// Bob
	if (BobAmplitude > 0.0f)
	{
		BobTime += DeltaTime * BobFrequency * 2.0f * PI;
		float BobOffset = FMath::Sin(BobTime) * BobAmplitude;
		
		FVector CurrentLocation = GetActorLocation();
		CurrentLocation.Z = BaseZ + BobOffset;
		SetActorLocation(CurrentLocation);
	}
}

bool ABasePickup::ShouldDespawn() const
{
	return GetActorLocation().X < DespawnXThreshold;
}

void ABasePickup::CacheWorldScrollComponent()
{
	if (UWorld* World = GetWorld())
	{
		if (AStateRunner_ArcadeGameMode* GameMode = Cast<AStateRunner_ArcadeGameMode>(World->GetAuthGameMode()))
		{
			WorldScrollComponent = GameMode->GetWorldScrollComponent();
		}
	}

	if (!WorldScrollComponent)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("BasePickup: Could not find WorldScrollComponent!"));
	}
}

void ABasePickup::SetupCollisionBox()
{
	if (!CollisionBox)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("SetupCollisionBox: Missing CollisionBox!"));
		return;
	}

	// Only set extent -- Blueprint-configured position is preserved
	CollisionBox->SetBoxExtent(CollisionExtent);
	CollisionBox->SetWorldScale3D(FVector::OneVector);
}
