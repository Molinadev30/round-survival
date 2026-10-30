// Round Survival

#include "Weapons/RSWeaponStateComponent.h"

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Enemies/RSZombieCharacter.h"
#include "Engine/World.h"
#include "Equipment/RSEquipmentManagerComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "RSLogChannels.h"
#include "TimerManager.h"
#include "Weapons/RSRangedWeaponInstance.h"

URSWeaponStateComponent::URSWeaponStateComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Markers expire when they are read, not on a timer.
	PrimaryComponentTick.bCanEverTick = false;
}

void URSWeaponStateComponent::BeginPlay()
{
	Super::BeginPlay();

	// Only the player looking through this controller has a crosshair to colour.
	const APlayerController* PC = GetController<APlayerController>();
	UWorld* World = GetWorld();
	if (!PC || !PC->IsLocalController() || !World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(AimTargetTimerHandle, this, &ThisClass::RefreshAimTarget, AimTargetRefreshInterval,
		/*bLoop=*/ true, FMath::FRandRange(KINDA_SMALL_NUMBER, AimTargetRefreshInterval));
}

void URSWeaponStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AimTargetTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void URSWeaponStateComponent::RefreshAimTarget()
{
	bool bNewAimingAtEnemy = false;

	APlayerController* PC = GetController<APlayerController>();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UWorld* World = GetWorld();

	if (Pawn && World)
	{
		// Down the centre of the screen and on the channel the weapon's own aim trace uses, so
		// "red" means the same thing the shot will find.
		FVector ViewLocation;
		FRotator ViewRotation;
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

		const URSEquipmentManagerComponent* Equipment = Pawn->FindComponentByClass<URSEquipmentManagerComponent>();
		const URSRangedWeaponInstance* Weapon = Equipment ? Equipment->GetFirstInstanceOfType<URSRangedWeaponInstance>() : nullptr;
		const float Range = Weapon ? Weapon->GetMaxDamageRange() : DefaultAimTargetRange;

		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(RSAimTarget), /*bTraceComplex=*/ false, /*IgnoreActor=*/ Pawn);

		// The weapons hanging off the pawn would otherwise be the first thing the trace meets.
		TArray<AActor*> AttachedActors;
		Pawn->GetAttachedActors(AttachedActors, /*bResetArray=*/ true, /*bRecursivelyIncludeAttachedActors=*/ true);
		TraceParams.AddIgnoredActors(AttachedActors);

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * Range, ECC_Visibility, TraceParams))
		{
			const ARSZombieCharacter* Enemy = Cast<ARSZombieCharacter>(Hit.GetActor());
			bNewAimingAtEnemy = Enemy && !Enemy->IsDead();
		}
	}

	if (bNewAimingAtEnemy != bAimingAtEnemy)
	{
		bAimingAtEnemy = bNewAimingAtEnemy;
		OnAimingAtEnemyChanged.Broadcast(bAimingAtEnemy);
	}
}

void URSWeaponStateComponent::AddUnconfirmedHitMarkers(const FGameplayAbilityTargetDataHandle& TargetData, const TArray<FHitResult>& Hits)
{
	APlayerController* PC = GetController<APlayerController>();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	FRSHitMarkerBatch Batch(static_cast<uint16>(TargetData.UniqueId));

	for (const FHitResult& Hit : Hits)
	{
		FRSScreenSpaceHitMarker Marker;
		Marker.bSuccess = ShouldShowHitAsSuccess(Hit);

		// Projected here, where the impact is still in world space. The HUD only ever deals in
		// screen coordinates and never needs to know what was hit or where it was.
		PC->ProjectWorldLocationToScreen(Hit.ImpactPoint, Marker.ScreenLocation, /*bPlayerViewportRelative=*/ true);

		Batch.Markers.Add(Marker);
	}

	UnconfirmedBatches.Add(MoveTemp(Batch));
}

void URSWeaponStateComponent::ClientConfirmTargetData_Implementation(uint16 UniqueId, bool bSuccess, const TArray<uint8>& HitReplaces)
{
	for (int32 BatchIndex = 0; BatchIndex < UnconfirmedBatches.Num(); ++BatchIndex)
	{
		FRSHitMarkerBatch& Batch = UnconfirmedBatches[BatchIndex];

		if (Batch.UniqueId != UniqueId)
		{
			continue;
		}

		if (bSuccess && HitReplaces.Num() < Batch.Markers.Num())
		{
			TArray<FRSScreenSpaceHitMarker> Confirmed;

			for (int32 MarkerIndex = 0; MarkerIndex < Batch.Markers.Num(); ++MarkerIndex)
			{
				// A replaced index is a hit the server disagreed with — the shot stopped somewhere
				// else on its machine, so the marker the player already saw was wrong.
				if (!HitReplaces.Contains(static_cast<uint8>(MarkerIndex)))
				{
					Confirmed.Add(Batch.Markers[MarkerIndex]);
				}
			}

			if (Confirmed.Num() > 0)
			{
				if (const UWorld* World = GetWorld())
				{
					LastConfirmedHitTime = World->GetTimeSeconds();
				}

				ConfirmedMarkers.Append(Confirmed);
				OnHitMarkersConfirmed.Broadcast(Confirmed);
			}
		}
		else
		{
			UE_LOG(LogRSAbility, Verbose, TEXT("Hit marker batch %u discarded: the server rejected the shot."), UniqueId);
		}

		UnconfirmedBatches.RemoveAt(BatchIndex);
		return;
	}
}

const TArray<FRSScreenSpaceHitMarker>& URSWeaponStateComponent::GetConfirmedHitMarkers()
{
	ExpireOldMarkers();
	return ConfirmedMarkers;
}

int32 URSWeaponStateComponent::GetConfirmedHitMarkerCount() const
{
	// Answers the same question the array getter does, without mutating anything: a caller that
	// only wants the count should not be the reason the list gets cleared.
	return (GetTimeSinceLastConfirmedHit() > HitMarkerDisplayDuration) ? 0 : ConfirmedMarkers.Num();
}

float URSWeaponStateComponent::GetTimeSinceLastConfirmedHit() const
{
	const UWorld* World = GetWorld();
	return World ? static_cast<float>(World->GetTimeSeconds() - LastConfirmedHitTime) : 0.0f;
}

bool URSWeaponStateComponent::ShouldShowHitAsSuccess(const FHitResult& Hit) const
{
	// Any pawn other than the shooter counts.
	const AActor* HitActor = Hit.GetActor();
	const AController* OwningController = GetController<AController>();
	const APawn* OwnPawn = OwningController ? OwningController->GetPawn() : nullptr;

	return HitActor && HitActor->IsA<APawn>() && HitActor != OwnPawn;
}

void URSWeaponStateComponent::ExpireOldMarkers()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if ((World->GetTimeSeconds() - LastConfirmedHitTime) > HitMarkerDisplayDuration)
	{
		ConfirmedMarkers.Reset();
	}
}
