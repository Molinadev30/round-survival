// Round Survival

#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "RSImpactEffectsSubsystem.generated.h"

class UDecalComponent;
class UPhysicalMaterial;
class URSImpactEffects;

/**
 * Shows where bullets land, in one world: the effect, the sound and the hole.
 *
 * Cosmetic and per machine. The fire cue brings each impact here on every machine it reaches, so
 * the shooter's comes from its own trace and everybody else's from the server's validated one.
 * Nothing here is shown on a dedicated server.
 *
 * Holes are a fixed number of decal components, made as they are first needed and then reused
 * oldest first, so they are never created or destroyed in play. The effects use Niagara's own
 * component pool. URSImpactSettings sets the numbers.
 */
UCLASS()
class ROUND_SURVIVAL_API URSImpactEffectsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static URSImpactEffectsSubsystem* Get(const UObject* WorldContextObject);

	/** Shows an impact at Location on a surface facing Normal, chosen by the physical material the shot touched. */
	void PlayImpact(const FVector& Location, const FVector& Normal, const UPhysicalMaterial* PhysicalMaterial);

protected:
	//~UWorldSubsystem interface
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~End of UWorldSubsystem interface

private:
	/** Whether a local camera is near enough to Location to show anything. */
	bool IsWithinReachOfLocalView(const FVector& Location) const;

	void PlaceDecal(UMaterialInterface* Material, float Size, const FVector& Location, const FVector& Normal);

	UPROPERTY(Transient)
	TObjectPtr<const URSImpactEffects> ImpactEffects;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDecalComponent>> Decals;

	/** The decal the next hole takes once all of them are out: the oldest. */
	int32 NextDecal = 0;
};
