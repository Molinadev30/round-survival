// Round Survival

#pragma once

#include "Components/ControllerComponent.h"
#include "GameplayTagContainer.h"

#include "RSWeaponStateComponent.generated.h"

struct FGameplayAbilityTargetDataHandle;
struct FHitResult;

/** One impact the shooter is showing on screen, in viewport space. */
struct FRSScreenSpaceHitMarker
{
	FVector2D ScreenLocation = FVector2D::ZeroVector;

	/** Which zone was hit, so the HUD can mark a headshot differently. */
	FGameplayTag HitZone;

	/** True when it landed on something worth a hit marker rather than on the world. */
	bool bSuccess = false;
};

/** The impacts from one trigger pull, still waiting for the server to agree with them. */
struct FRSHitMarkerBatch
{
	FRSHitMarkerBatch() = default;

	explicit FRSHitMarkerBatch(uint16 InUniqueId)
		: UniqueId(InUniqueId)
	{
	}

	TArray<FRSScreenSpaceHitMarker> Markers;

	uint16 UniqueId = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FRSHitMarkersConfirmed, const TArray<FRSScreenSpaceHitMarker>& /*Markers*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FRSAimingAtEnemyChanged, bool /*bAimingAtEnemy*/);

/**
 * Holds the shooter's own hit feedback between firing and the server agreeing.
 *
 * A hit marker has to appear on the frame the player clicks — waiting a round trip for it feels
 * broken even on a good connection. So the shooter records its impacts immediately and this
 * component holds them as *unconfirmed*: the server re-runs the shot, then tells this component
 * which of them survived. Confirmed markers are shown, rejected ones are dropped.
 *
 * It lives on the controller rather than the pawn because it is per-player feedback that has to
 * outlive the pawn, and because the server addresses it with a client RPC.
 */
UCLASS(BlueprintType)
class ROUND_SURVIVAL_API URSWeaponStateComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	URSWeaponStateComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Called on the shooter right after its local trace, before the server has seen anything. */
	void AddUnconfirmedHitMarkers(const FGameplayAbilityTargetDataHandle& TargetData, const TArray<FHitResult>& Hits);

	/**
	 * Server -> shooter: the verdict on one batch.
	 *
	 * @param UniqueId     Which batch, matching the target data handle the shot was sent with.
	 * @param bSuccess     False when the shot as a whole was thrown out.
	 * @param HitReplaces  Indices the server replaced with its own result; those markers are dropped.
	 */
	UFUNCTION(Client, Reliable)
	void ClientConfirmTargetData(uint16 UniqueId, bool bSuccess, const TArray<uint8>& HitReplaces);

	/** Markers the server confirmed, already expired by age. For the HUD to draw. */
	const TArray<FRSScreenSpaceHitMarker>& GetConfirmedHitMarkers();

	/** Batches still waiting for the server. Exposed so the HUD and debug tooling can read it. */
	UFUNCTION(BlueprintPure, Category = "RS|Weapon")
	int32 GetUnconfirmedBatchCount() const { return UnconfirmedBatches.Num(); }

	/** Markers still inside the display window. Zero once they have aged out. */
	UFUNCTION(BlueprintPure, Category = "RS|Weapon")
	int32 GetConfirmedHitMarkerCount() const;

	UFUNCTION(BlueprintPure, Category = "RS|Weapon")
	float GetTimeSinceLastConfirmedHit() const;

	/** Fired when a batch comes back confirmed. The HUD listens instead of polling. */
	FRSHitMarkersConfirmed OnHitMarkersConfirmed;

	/** Whether the centre of the screen is on a living enemy, as of the last check. Local controller only. */
	bool IsAimingAtEnemy() const { return bAimingAtEnemy; }

	/** Fired when IsAimingAtEnemy changes, so the crosshair turns red without asking. */
	FRSAimingAtEnemyChanged OnAimingAtEnemyChanged;

	//~UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

protected:
	/**
	 * Seconds between checks of what the crosshair is over. A timer rather than a tick: a reticle
	 * that turns red a fraction of a frame late is not something a player can see, and it keeps
	 * the trace off the frames that do not need it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Weapon", meta = (ForceUnits = s, ClampMin = 0.01))
	float AimTargetRefreshInterval = 1.0f / 15.0f;

	/** How far the check looks when no ranged weapon is drawn to say. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Weapon", meta = (ForceUnits = cm))
	float DefaultAimTargetRange = 10000.0f;
	/** A hit worth a marker. Overridden once enemies exist to mean "hit something damageable". */
	virtual bool ShouldShowHitAsSuccess(const FHitResult& Hit) const;

	/** How long a confirmed marker stays in the list before it is dropped. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Weapon", meta = (ForceUnits = s))
	float HitMarkerDisplayDuration = 0.4f;

private:
	/** Expires stale markers. Called from the getters, so nothing has to tick to keep this honest. */
	void ExpireOldMarkers();

	/** One trace down the centre of the screen; updates bAimingAtEnemy and broadcasts on a change. */
	void RefreshAimTarget();

	bool bAimingAtEnemy = false;

	FTimerHandle AimTargetTimerHandle;

	TArray<FRSHitMarkerBatch> UnconfirmedBatches;

	TArray<FRSScreenSpaceHitMarker> ConfirmedMarkers;

	double LastConfirmedHitTime = 0.0;
};
