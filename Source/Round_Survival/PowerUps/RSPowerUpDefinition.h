// Round Survival

#pragma once

#include "Engine/DataAsset.h"

#include "RSPowerUpDefinition.generated.h"

class USoundBase;
class UStaticMesh;
class URSPowerUpEffect;
struct FRSPowerUpContext;

/**
 * A power-up as data: what it is called, what it looks like on the floor, and what it does.
 *
 * What it does is a list of `URSPowerUpEffect`s instanced in this asset, run in order on the server
 * when a player picks it up, and always for the whole team. Resupply is "refill team ammo"; Scrap
 * Surge is "gameplay effect on team" naming a 30 s effect. Neither has a class of its own.
 */
UCLASS(BlueprintType, Const)
class ROUND_SURVIVAL_API URSPowerUpDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Runs every effect, in order. Server only. */
	void Activate(const FRSPowerUpContext& Context) const;

	/** What the HUD calls it when someone picks it up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Power-Up")
	FText DisplayName;

	/** What lies on the floor. The engine's basic shapes until the art exists. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Power-Up")
	TObjectPtr<UStaticMesh> Mesh;

	/** Tints the pickup, so two power-ups are told apart at a glance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Power-Up")
	FLinearColor Color = FLinearColor::White;

	/**
	 * Heard by everyone in the match when someone picks it up, wherever they are: the team learns it
	 * has the power-up by ear, not by a message on screen.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Power-Up")
	TObjectPtr<USoundBase> PickupSound;

	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "Power-Up")
	TArray<TObjectPtr<URSPowerUpEffect>> Effects;
};
