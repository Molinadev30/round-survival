// Round Survival

#pragma once

#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"

#include "RSPowerUpEffect.generated.h"

class AActor;
class UGameplayEffect;
class URSPowerUpDefinition;
class UWorld;

/** Who picked a power-up up, where, and which one. Handed to every effect it has. */
struct FRSPowerUpContext
{
	UWorld* World = nullptr;

	/** The player's pawn that walked over it. Effects reach the whole team; this is the instigator. */
	AActor* Collector = nullptr;

	const URSPowerUpDefinition* PowerUp = nullptr;
};

/**
 * One thing a power-up does when it is picked up. Server only.
 *
 * A power-up is a list of these, instanced inside its definition, so a new power-up is assembled in
 * data from pieces that exist: "a timed effect on the team" and "fill everyone's ammo" are
 * two pieces, and a power-up that did both would list both. A new kind of piece is one small class.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories, Const)
class ROUND_SURVIVAL_API URSPowerUpEffect : public UObject
{
	GENERATED_BODY()

public:
	virtual void Apply(const FRSPowerUpContext& Context) const PURE_VIRTUAL(URSPowerUpEffect::Apply, );
};

/**
 * Applies a gameplay effect to every player in the match, the collector included.
 *
 * What timed power-ups are made of: the effect carries the duration, the modifiers, and how a
 * second pickup stacks — refreshing the duration, for the ones the genre restarts. A power-up the
 * game reads by tag names it in `GrantedTag`, put on the spec the way a station's is. Down or dead,
 * every player gets it: the team picked it up.
 */
UCLASS(meta = (DisplayName = "Gameplay Effect on Team"))
class ROUND_SURVIVAL_API URSPowerUpEffect_TeamGameplayEffect : public URSPowerUpEffect
{
	GENERATED_BODY()

public:
	virtual void Apply(const FRSPowerUpContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Power-Up")
	TSubclassOf<UGameplayEffect> Effect;

	/** Granted for as long as the effect lasts, if set. */
	UPROPERTY(EditAnywhere, Category = "Power-Up", meta = (Categories = "RS.PowerUp"))
	FGameplayTag GrantedTag;
};

/**
 * Fills every weapon every player carries: the magazine to its size and the reserve to full, the
 * upgraded sizes for an upgraded weapon. A dead player carries nothing and gets nothing.
 */
UCLASS(meta = (DisplayName = "Refill Team Ammo"))
class ROUND_SURVIVAL_API URSPowerUpEffect_RefillAmmo : public URSPowerUpEffect
{
	GENERATED_BODY()

public:
	virtual void Apply(const FRSPowerUpContext& Context) const override;
};

/**
 * Pays every player the same amount of scrap, through the award effect kills use. So an income buff
 * raises it, and a downed player, whose income is zero, gets nothing.
 */
UCLASS(meta = (DisplayName = "Scrap to Team"))
class ROUND_SURVIVAL_API URSPowerUpEffect_GrantScrap : public URSPowerUpEffect
{
	GENERATED_BODY()

public:
	virtual void Apply(const FRSPowerUpContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Power-Up", meta = (ClampMin = 0))
	int32 Amount = 0;

	/** `GE_RS_ScrapAward`: instant, `SetByCaller.Scrap` scaled by the gain multiplier. */
	UPROPERTY(EditAnywhere, Category = "Power-Up")
	TSubclassOf<UGameplayEffect> ScrapAwardEffect;
};

/** Kills every living enemy, crediting nobody: no scrap per kill, and no power-up drops from them. */
UCLASS(meta = (DisplayName = "Kill All Enemies"))
class ROUND_SURVIVAL_API URSPowerUpEffect_KillAllEnemies : public URSPowerUpEffect
{
	GENERATED_BODY()

public:
	virtual void Apply(const FRSPowerUpContext& Context) const override;
};

/**
 * Puts a Supply Cache on every spot the level has for one, for as long as `LastsAsLongAs` does, and
 * none of them moves while it lasts. The price is not here: the effect it lasts as long as marks it
 * down on every player.
 */
UCLASS(meta = (DisplayName = "Supply Cache Sale"))
class ROUND_SURVIVAL_API URSPowerUpEffect_SupplyCacheSale : public URSPowerUpEffect
{
	GENERATED_BODY()

public:
	virtual void Apply(const FRSPowerUpContext& Context) const override;

	/**
	 * The team effect listed with this one, whose duration the sale takes: one number, so the
	 * countdown on the HUD and the extra caches end together.
	 */
	UPROPERTY(EditAnywhere, Category = "Power-Up")
	TSubclassOf<UGameplayEffect> LastsAsLongAs;
};

/** Puts every board back on every barricade on the map. */
UCLASS(meta = (DisplayName = "Repair All Barricades"))
class ROUND_SURVIVAL_API URSPowerUpEffect_RepairBarricades : public URSPowerUpEffect
{
	GENERATED_BODY()

public:
	virtual void Apply(const FRSPowerUpContext& Context) const override;
};
