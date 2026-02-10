#include "ThemedMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ThemeSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "StateRunner_Arcade.h"

UThemedMeshComponent::UThemedMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UThemedMeshComponent::BeginPlay()
{
	Super::BeginPlay();

	// Get theme subsystem
	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(this))
	{
		ThemeSubsystem = GI->GetSubsystem<UThemeSubsystem>();
	}

	if (!ThemeSubsystem)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("ThemedMeshComponent: Could not get ThemeSubsystem!"));
		return;
	}

	// Auto-find mesh if configured
	if (bAutoFindMesh && !TargetMeshComponent)
	{
		if (AActor* Owner = GetOwner())
		{
			// Get all static mesh components on the owner
			TArray<UStaticMeshComponent*> MeshComponents;
			Owner->GetComponents<UStaticMeshComponent>(MeshComponents);

			// First, try to find a mesh matching the name filter
			if (!AutoFindNameFilter.IsEmpty())
			{
				for (UStaticMeshComponent* Mesh : MeshComponents)
				{
					if (Mesh && Mesh->GetName().Contains(AutoFindNameFilter, ESearchCase::IgnoreCase))
					{
						TargetMeshComponent = Mesh;
						UE_LOG(LogStateRunner_Arcade, Log, TEXT("ThemedMeshComponent: Auto-found mesh %s (matched filter '%s') on %s"),
							*TargetMeshComponent->GetName(), *AutoFindNameFilter, *Owner->GetName());
						break;
					}
				}
			}

			// Fall back to first mesh if no filter match
			if (!TargetMeshComponent && MeshComponents.Num() > 0)
			{
				TargetMeshComponent = MeshComponents[0];
				UE_LOG(LogStateRunner_Arcade, Log, TEXT("ThemedMeshComponent: Auto-found first mesh %s on %s (no filter match)"),
					*TargetMeshComponent->GetName(), *Owner->GetName());
			}
		}
	}

	// Register with theme subsystem
	if (TargetMeshComponent)
	{
		ThemeSubsystem->RegisterThemedMesh(TargetMeshComponent);
		bIsRegistered = true;
	}

	// Subscribe to theme changes
	ThemeSubsystem->OnThemeChanged.AddDynamic(this, &UThemedMeshComponent::OnThemeChanged);

	// Apply current theme if configured
	if (bApplyOnBeginPlay && TargetMeshComponent)
	{
		ThemeSubsystem->ApplyThemeToMesh(TargetMeshComponent);
	}

	UE_LOG(LogStateRunner_Arcade, Verbose, TEXT("ThemedMeshComponent: BeginPlay on %s (mesh: %s)"),
		*GetOwner()->GetName(), TargetMeshComponent ? *TargetMeshComponent->GetName() : TEXT("None"));
}

void UThemedMeshComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unsubscribe from theme changes
	if (ThemeSubsystem)
	{
		ThemeSubsystem->OnThemeChanged.RemoveDynamic(this, &UThemedMeshComponent::OnThemeChanged);

		// Unregister meshes
		if (bIsRegistered && TargetMeshComponent)
		{
			ThemeSubsystem->UnregisterThemedMesh(TargetMeshComponent);
		}

		// Unregister additional meshes
		for (const TWeakObjectPtr<UStaticMeshComponent>& Weak : AdditionalMeshes)
		{
			if (UStaticMeshComponent* Mesh = Weak.Get())
			{
				ThemeSubsystem->UnregisterThemedMesh(Mesh);
			}
		}
	}

	bIsRegistered = false;
	AdditionalMeshes.Empty();

	Super::EndPlay(EndPlayReason);
}

//=============================================================================
// PUBLIC FUNCTIONS
//=============================================================================

void UThemedMeshComponent::ApplyCurrentTheme()
{
	if (!ThemeSubsystem)
	{
		UE_LOG(LogStateRunner_Arcade, Warning, TEXT("ThemedMeshComponent: No theme subsystem available"));
		return;
	}

	if (TargetMeshComponent)
	{
		ThemeSubsystem->ApplyThemeToMesh(TargetMeshComponent);
	}

	// Apply to additional meshes
	for (const TWeakObjectPtr<UStaticMeshComponent>& Weak : AdditionalMeshes)
	{
		if (UStaticMeshComponent* Mesh = Weak.Get())
		{
			ThemeSubsystem->ApplyThemeToMesh(Mesh);
		}
	}
}

void UThemedMeshComponent::RegisterAdditionalMesh(UStaticMeshComponent* MeshComponent)
{
	if (!MeshComponent)
	{
		return;
	}

	// Check if already registered
	for (const TWeakObjectPtr<UStaticMeshComponent>& Weak : AdditionalMeshes)
	{
		if (Weak.Get() == MeshComponent)
		{
			return; // Already registered
		}
	}

	AdditionalMeshes.Add(MeshComponent);

	// Register with subsystem and apply theme
	if (ThemeSubsystem)
	{
		ThemeSubsystem->RegisterThemedMesh(MeshComponent);
	}

	UE_LOG(LogStateRunner_Arcade, Verbose, TEXT("ThemedMeshComponent: Registered additional mesh %s"),
		*MeshComponent->GetName());
}

EThemeType UThemedMeshComponent::GetCurrentThemeType() const
{
	if (ThemeSubsystem)
	{
		return ThemeSubsystem->GetCurrentThemeType();
	}
	return EThemeType::Cyan;
}

//=============================================================================
// THEME CHANGE CALLBACK
//=============================================================================

void UThemedMeshComponent::OnThemeChanged(EThemeType NewTheme)
{
	UE_LOG(LogStateRunner_Arcade, Verbose, TEXT("ThemedMeshComponent: Theme changed to %d on %s"),
		static_cast<int32>(NewTheme), *GetOwner()->GetName());

	// Apply new theme to all meshes
	ApplyCurrentTheme();
}
