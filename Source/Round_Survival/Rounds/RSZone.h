// Round Survival

#pragma once

#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "RSZone.generated.h"

class ARSZone;
class UBillboardComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FRSZoneOpenChanged, ARSZone* /*Zone*/, bool /*bIsOpen*/);

/**
 * A region of the map that can be shut off from the round.
 *
 * A zone owns no geometry and no volume. It is a switch: its spawners are eligible while it is
 * open and invisible to the spawn selector while it is closed, which is the whole of what the
 * round system needs to know about map progression. Sector Gates open zones; nothing
 * else does.
 *
 * Replicated and always relevant. The flag is small, changes a handful of times per match and
 * every client needs it — for the gate's own prompt and for the HUD — so it rides on the actor
 * rather than through an RPC that arrives before the zone does.
 *
 * `OnZoneOpenChanged` fires on the server as well as on clients: `OnRep` never runs on the
 * server and on a listen server the host's own gate has to react like everyone else's.
 */
UCLASS(Blueprintable)
class ROUND_SURVIVAL_API ARSZone : public AActor
{
	GENERATED_BODY()

public:
	ARSZone(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintPure, Category = "RS|Zone")
	bool IsOpen() const { return bIsOpen; }

	/** Identifies the zone for data — spawn tables, gate costs, audio. Not used for lookup. */
	UFUNCTION(BlueprintPure, Category = "RS|Zone")
	FGameplayTag GetZoneTag() const { return ZoneTag; }

	/** Server only. Opening a zone is one-way; nothing closes one back. */
	UFUNCTION(BlueprintCallable, Category = "RS|Zone")
	void SetOpen(bool bNewOpen);

	/** Fires on every machine when the flag settles, the server included. */
	FRSZoneOpenChanged OnZoneOpenChanged;

protected:
	//~AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End of AActor interface

	UFUNCTION()
	void OnRep_IsOpen();

	/** Whether the round can use this zone from the first round. The starting area is the one. */
	UPROPERTY(EditAnywhere, Category = "RS|Zone")
	bool bStartsOpen = false;

	UPROPERTY(EditAnywhere, Category = "RS|Zone")
	FGameplayTag ZoneTag;

private:
	UPROPERTY(ReplicatedUsing = OnRep_IsOpen)
	bool bIsOpen = false;

#if WITH_EDITORONLY_DATA
	/** Something to click in the viewport. Editor only: the zone has no presence in game. */
	UPROPERTY()
	TObjectPtr<UBillboardComponent> BillboardComponent;
#endif
};
