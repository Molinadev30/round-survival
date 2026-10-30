// Round Survival

#pragma once

#include "AbilitySystem/RSAbilitySourceInterface.h"
#include "Curves/CurveFloat.h"
#include "Equipment/RSWeaponInstance.h"
#include "GameplayTagContainer.h"

#include "RSRangedWeaponInstance.generated.h"

class UPhysicalMaterial;

/** How the trigger behaves. */
UENUM(BlueprintType)
enum class ERSWeaponFireMode : uint8
{
	/** One shot per press, never faster than the weapon's cadence. */
	SemiAutomatic,

	/** Keeps firing at the weapon's cadence for as long as the trigger is held. */
	Automatic,
};

/**
 * A ranged weapon: spread, falloff and hit-zone multipliers.
 *
 * Spread is heat-based. Firing adds heat, heat maps to a cone angle through a curve, and the heat
 * cools off on its own after a short delay. Moving adds a flat angle on top, the same at a walk
 * as at a run. Player state — aiming, crouching, standing still, in the air — then multiplies the
 * sum, every part blended rather than switched so the reticle never jumps.
 *
 * Everything here is data. Retuning a weapon is editing curves on a Blueprint subclass, never a
 * code change, which is the whole reason the numbers are curves instead of floats.
 *
 * So is how fast it fires. The fire mode and the cadence are read by the fire ability — which
 * spaces its own activations by the cadence — and by the server, which keeps a small budget of
 * shots per weapon so a client cannot fire faster than the weapon allows by simply asking more
 * often.
 */
UCLASS()
class ROUND_SURVIVAL_API URSRangedWeaponInstance : public URSWeaponInstance, public IRSAbilitySourceInterface
{
	GENERATED_BODY()

public:
	URSRangedWeaponInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~URSEquipmentInstance interface
	virtual void OnEquipped() override;
	//~End of URSEquipmentInstance interface

	/** Advances heat decay and multiplier blending. Driven by the equipment manager's tick. */
	void Tick(float DeltaSeconds);

	/** Called by the fire ability once per cartridge. */
	void AddSpread();

	/** Current cone angle in degrees — heat plus the moving bonus — before the player-state multiplier. */
	float GetCalculatedSpreadAngle() const { return CurrentSpreadAngle + MovingSpreadBonus; }

	float GetCalculatedSpreadAngleMultiplier() const { return bHasFirstShotAccuracy ? 0.0f : CurrentSpreadAngleMultiplier; }

	bool HasFirstShotAccuracy() const { return bHasFirstShotAccuracy; }

	float GetSpreadExponent() const { return SpreadExponent; }

	int32 GetBulletsPerCartridge() const { return BulletsPerCartridge; }

	float GetMaxDamageRange() const { return MaxDamageRange; }

	float GetBaseDamage() const { return BaseDamage; }

	float GetBulletTraceSweepRadius() const { return BulletTraceSweepRadius; }

	ERSWeaponFireMode GetFireMode() const { return FireMode; }

	bool IsAutomatic() const { return FireMode == ERSWeaponFireMode::Automatic; }

	/**
	 * Seconds from one shot to the next: this weapon's cadence, divided by its holder's
	 * `FireRateMultiplier`.
	 *
	 * The one place the cadence is worked out. The shooter's shot clock and the server's shot budget
	 * both ask here, so a fire rate buff speeds both up together and the server never refuses a shot
	 * the shooter was entitled to. Neither the fire ability nor this weapon knows what raised it.
	 */
	float GetTimeBetweenShots() const;

	/**
	 * World time at which the shooter's machine may fire this weapon next. Shooter only — the server
	 * holds its own budget below.
	 *
	 * Kept on the weapon, not on the fire ability, because the cadence belongs to the weapon and has
	 * to outlive an activation: letting go and pulling again must not fire sooner than holding would.
	 * The fire ability advances it by exactly one interval per shot, from when the shot was due rather
	 * than when it went out, so the time a shot waits for the next frame is never added to the next.
	 */
	double GetNextShotTime() const { return NextShotTime; }
	void SetNextShotTime(double InNextShotTime) { NextShotTime = InNextShotTime; }

	/**
	 * Whether the server would accept a shot now. Server only.
	 *
	 * A small token bucket rather than a strict interval: shots arrive over the network in bunches,
	 * and a server that demanded a full interval between any two arrivals would reject honest shots
	 * that a jittery connection delivered together. The bucket refills at the weapon's cadence and
	 * holds ServerShotBurstTolerance shots, so the long-run rate can never beat the cadence while
	 * two shots landing in the same packet still both count.
	 */
	bool HasServerShotBudget(double WorldTime) const;

	/** Spends one shot from the server's budget. Server only, once per accepted shot. */
	void ConsumeServerShot(double WorldTime);

	float GetAimDownSightsSeconds() const { return AimDownSightsSeconds; }
	float GetAimDownSightsFieldOfView() const { return AimDownSightsFieldOfView; }
	float GetAimDownSightsFirstPersonFieldOfView() const { return AimDownSightsFirstPersonFieldOfView; }

	//~IRSAbilitySourceInterface interface
	virtual float GetDistanceAttenuation(float Distance, const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr) const override;
	virtual float GetPhysicalMaterialAttenuation(const UPhysicalMaterial* PhysicalMaterial,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr) const override;
	//~End of IRSAbilitySourceInterface interface

protected:
	// -- Spread, weapon side --------------------------------------------------------------------

	/**
	 * How tightly shots cluster inside the cone. 1.0 spreads them evenly across it; higher values
	 * pull them towards the centre line, so the cone is the worst case rather than the average.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spread|Weapon", meta = (ClampMin = 0.1))
	float SpreadExponent = 1.0f;

	/** Heat -> cone angle in degrees. Its X range is what defines the weapon's heat range. */
	UPROPERTY(EditAnywhere, Category = "Spread|Weapon")
	FRuntimeFloatCurve HeatToSpreadCurve;

	/** Heat -> heat added per shot. Flat for a predictable weapon, rising to punish holding fire. */
	UPROPERTY(EditAnywhere, Category = "Spread|Weapon")
	FRuntimeFloatCurve HeatToHeatPerShotCurve;

	/** Heat -> heat removed per second once the recovery delay has passed. */
	UPROPERTY(EditAnywhere, Category = "Spread|Weapon")
	FRuntimeFloatCurve HeatToCoolDownPerSecondCurve;

	/** Seconds after the last shot before heat starts coming down. */
	UPROPERTY(EditAnywhere, Category = "Spread|Weapon", meta = (ForceUnits = s))
	float SpreadRecoveryCooldownDelay = 0.0f;

	/** Perfect accuracy when heat and every multiplier are already at their minimum. */
	UPROPERTY(EditAnywhere, Category = "Spread|Weapon")
	bool bAllowFirstShotAccuracy = false;

	// -- Spread, player side ---------------------------------------------------------------------
	// Each of these blends towards its target rather than snapping, because the reticle is drawn
	// from the same number and a snap reads as a bug.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spread|Player", meta = (ForceUnits = x))
	float SpreadAngleMultiplier_Aiming = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spread|Player")
	float TransitionRate_Aiming = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spread|Player", meta = (ForceUnits = x))
	float SpreadAngleMultiplier_StandingStill = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spread|Player")
	float TransitionRate_StandingStill = 5.0f;

	/** At or below this speed the pawn counts as standing still. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spread|Player", meta = (ForceUnits = "cm/s"))
	float StandingStillSpeedThreshold = 80.0f;

	/** Over this much extra speed the standing-still bonus has faded out completely. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spread|Player", meta = (ForceUnits = "cm/s"))
	float StandingStillToMovingSpeedRange = 20.0f;

	/**
	 * Degrees added to the cone while moving, faded in over the same speed band the standing-still
	 * bonus fades out over. Added rather than multiplied: a multiplier scales with heat, so it is
	 * invisible on a cold weapon and would mute firing while standing still. Past the band it is
	 * flat, so walking and running cost the same.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spread|Player", meta = (ForceUnits = deg, ClampMin = 0.0))
	float SpreadAngleBonus_Moving = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spread|Player")
	float TransitionRate_Moving = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spread|Player", meta = (ForceUnits = x))
	float SpreadAngleMultiplier_Crouching = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spread|Player")
	float TransitionRate_Crouching = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spread|Player", meta = (ForceUnits = x))
	float SpreadAngleMultiplier_JumpingOrFalling = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spread|Player")
	float TransitionRate_JumpingOrFalling = 5.0f;

	// -- Aim down sights -------------------------------------------------------------------------
	// The weapon's, not the player's: a scope zooms further than iron sights, and the camera asks the
	// weapon in hand. The spread side of aiming is SpreadAngleMultiplier_Aiming above.

	/** Seconds to go all the way into the sights, and back out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim Down Sights", meta = (ForceUnits = s, ClampMin = 0.0))
	float AimDownSightsSeconds = 0.2f;

	/** The world camera's horizontal field of view fully aimed. The player's camera sets it otherwise. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim Down Sights", meta = (ForceUnits = deg, ClampMin = 5.0, ClampMax = 170.0))
	float AimDownSightsFieldOfView = 80.0f;

	/**
	 * The viewmodel's field of view fully aimed. The same as the camera's first-person field of view
	 * until the sights are lined up with the centre of the screen, which is when it is worth changing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim Down Sights", meta = (ForceUnits = deg, ClampMin = 5.0, ClampMax = 170.0))
	float AimDownSightsFirstPersonFieldOfView = 100.0f;

	// -- Damage and traces -----------------------------------------------------------------------

	/**
	 * Damage per bullet before falloff, hit zone and either side's multipliers.
	 *
	 * Per bullet, not per trigger pull: a shotgun's pellets each deal this. It reaches the damage
	 * execution as a SetByCaller on the spec rather than as an attribute, because it belongs to the
	 * weapon and not to whoever is holding it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Config", meta = (ClampMin = 0.0))
	float BaseDamage = 30.0f;

	/** More than one only for a weapon that fires a spread of pellets per trigger pull. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Config", meta = (ClampMin = 1))
	int32 BulletsPerCartridge = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Config", meta = (ForceUnits = cm))
	float MaxDamageRange = 25000.0f;

	/** Sweep radius for the bullet trace. Zero traces a line. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Config", meta = (ForceUnits = cm))
	float BulletTraceSweepRadius = 0.0f;

	// -- Fire rate ---------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Config|Fire")
	ERSWeaponFireMode FireMode = ERSWeaponFireMode::SemiAutomatic;

	/**
	 * The weapon's cadence. Automatic weapons fire at exactly this rate while the trigger is held;
	 * semi-automatic ones can never be clicked faster than it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Config|Fire", meta = (ClampMin = 1.0))
	float RoundsPerMinute = 600.0f;

	/**
	 * How many shots the server lets arrive closer together than the cadence allows. One is
	 * strict and rejects shots a jittery connection bunched together; much more than two would let
	 * a client fire a short burst faster than the weapon can.
	 */
	UPROPERTY(EditAnywhere, Category = "Weapon Config|Fire", meta = (ClampMin = 1.0))
	float ServerShotBurstTolerance = 2.0f;

	/** Distance in cm -> damage multiplier. An empty curve means no falloff at all. */
	UPROPERTY(EditAnywhere, Category = "Weapon Config")
	FRuntimeFloatCurve DistanceDamageFalloff;

	/**
	 * Hit-zone tag -> damage multiplier, read off the physical material that was hit.
	 *
	 * This is the headshot, and it lives on the weapon rather than the enemy so two weapons can
	 * disagree about how much a head is worth.
	 */
	UPROPERTY(EditAnywhere, Category = "Weapon Config", meta = (Categories = "RS.Damage.Zone"))
	TMap<FGameplayTag, float> HitZoneDamageMultiplier;

private:
	void ComputeHeatRange(float& MinHeat, float& MaxHeat) const;
	void ComputeSpreadRange(float& MinSpread, float& MaxSpread) const;

	float ClampHeat(float NewHeat) const
	{
		float MinHeat = 0.0f;
		float MaxHeat = 0.0f;
		ComputeHeatRange(MinHeat, MaxHeat);

		return FMath::Clamp(NewHeat, MinHeat, MaxHeat);
	}

	/** Returns true when spread is back at its minimum. */
	bool UpdateSpread(float DeltaSeconds);

	/** Blends the multipliers and the moving bonus. Returns true when every one is at its best value. */
	bool UpdateMultipliers(float DeltaSeconds);

	float CurrentHeat = 0.0f;
	float CurrentSpreadAngle = 0.0f;
	float CurrentSpreadAngleMultiplier = 1.0f;
	float MovingSpreadBonus = 0.0f;

	float AimingMultiplier = 1.0f;
	float StandingStillMultiplier = 1.0f;
	float CrouchingMultiplier = 1.0f;
	float JumpFallMultiplier = 1.0f;

	bool bHasFirstShotAccuracy = false;

	/** The server's shot budget, refilled lazily from the time it was last spent. See HasServerShotBudget. */
	float GetServerShotBudget(double WorldTime) const;

	float ServerShotBudget = 0.0f;
	double ServerShotBudgetTime = -1.0;

	double NextShotTime = 0.0;
};
