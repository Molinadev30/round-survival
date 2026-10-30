// Round Survival

#include "Weapons/RSImpactEffectsSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/DecalComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "RSLogChannels.h"
#include "Weapons/RSImpactEffects.h"
#include "Weapons/RSImpactSettings.h"

URSImpactEffectsSubsystem* URSImpactEffectsSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<URSImpactEffectsSubsystem>() : nullptr;
}

bool URSImpactEffectsSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URSImpactEffectsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (InWorld.GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	ImpactEffects = GetDefault<URSImpactSettings>()->ImpactEffects.LoadSynchronous();
	if (!ImpactEffects)
	{
		UE_LOG(LogRS, Warning, TEXT("No impact effects set in Project Settings > Round Survival > Impacts: shots will leave nothing where they land."));
	}
}

void URSImpactEffectsSubsystem::PlayImpact(const FVector& Location, const FVector& Normal, const UPhysicalMaterial* PhysicalMaterial)
{
	UWorld* World = GetWorld();
	if (!ImpactEffects || !World || !IsWithinReachOfLocalView(Location))
	{
		return;
	}

	const FRSImpactEffect& Impact = ImpactEffects->FindEffect(PhysicalMaterial);

	if (Impact.Effect)
	{
		// Pre-culled and pooled by Niagara itself: a burst of shots reuses the components of the last one.
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Impact.Effect, Location, Normal.Rotation(), FVector::OneVector,
			/*bAutoDestroy=*/ true, /*bAutoActivate=*/ true, ENCPoolMethod::AutoRelease, /*bPreCullCheck=*/ true);
	}

	if (Impact.Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, Impact.Sound, Location);
	}

	if (Impact.Decal)
	{
		PlaceDecal(Impact.Decal, Impact.DecalSize, Location, Normal);
	}
}

bool URSImpactEffectsSubsystem::IsWithinReachOfLocalView(const FVector& Location) const
{
	const float MaxDistanceSquared = FMath::Square(GetDefault<URSImpactSettings>()->MaxDistance);

	// Every local view, not only the first: a split screen has one per player, and a dead player
	// watching a teammate sees through that teammate's camera from their own camera manager.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		if (PlayerController && PlayerController->IsLocalController() && PlayerController->PlayerCameraManager)
		{
			if (FVector::DistSquared(PlayerController->PlayerCameraManager->GetCameraLocation(), Location) <= MaxDistanceSquared)
			{
				return true;
			}
		}
	}

	return false;
}

void URSImpactEffectsSubsystem::PlaceDecal(UMaterialInterface* Material, float Size, const FVector& Location, const FVector& Normal)
{
	const URSImpactSettings* Settings = GetDefault<URSImpactSettings>();
	UWorld* World = GetWorld();
	if (Settings->MaxDecals <= 0)
	{
		return;
	}

	// A new one while there is room, else the oldest one out.
	UDecalComponent* Decal = nullptr;
	if (Decals.Num() < Settings->MaxDecals)
	{
		// Owned by the world settings, as the engine's own spawned decals are: there is no actor
		// for them to belong to, and they last as long as the level does.
		Decal = NewObject<UDecalComponent>(World->GetWorldSettings(), NAME_None, RF_Transient);
		Decal->SetUsingAbsoluteScale(true);
		Decal->SetFadeScreenSize(Settings->DecalFadeScreenSize);
		Decal->RegisterComponentWithWorld(World);
		Decals.Add(Decal);
	}
	else
	{
		NextDecal %= Decals.Num();
		Decal = Decals[NextDecal++];
	}

	if (!Decal)
	{
		return;
	}

	// A decal projects along its X axis, so X goes into the surface. The roll is random, or every
	// hole on a wall would be the same hole.
	FRotator Rotation = (-Normal).Rotation();
	Rotation.Roll = FMath::FRandRange(-180.0f, 180.0f);

	// DecalSize is the box's half extent on each axis.
	Decal->DecalSize = FVector(Settings->DecalDepth, Size, Size) * 0.5f;
	Decal->SetWorldLocationAndRotation(Location, Rotation);

	// Last: it rebuilds the decal's render state, which picks up the new size as well.
	Decal->SetDecalMaterial(Material);
}
