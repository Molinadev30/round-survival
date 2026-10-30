// Round Survival

#pragma once

#include "Engine/DeveloperSettings.h"

#include "RSImpactSettings.generated.h"

class URSImpactEffects;

/**
 * What every machine allows bullet impacts.
 *
 * Per machine, like the pieces in URSDismembermentSettings: impacts are cosmetic and each machine
 * keeps its own. Which effect goes with which surface is URSImpactEffects.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Impacts"))
class ROUND_SURVIVAL_API URSImpactSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URSImpactSettings();

	//~UDeveloperSettings interface
	virtual FName GetCategoryName() const override { return TEXT("Round Survival"); }
	//~End of UDeveloperSettings interface

	/** Loaded as the world begins play, not on the first shot, so nobody's first shot hitches. */
	UPROPERTY(Config, EditAnywhere, Category = "Impacts")
	TSoftObjectPtr<URSImpactEffects> ImpactEffects;

	/**
	 * Impacts farther than this from every local camera are not shown at all: no effect, no sound,
	 * no hole. Another player's shots across the map would otherwise cost the same as one's own.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Impacts", meta = (ClampMin = 0.0, ForceUnits = cm))
	float MaxDistance = 5000.0f;

	/**
	 * Holes in the world at once. Past it, the next hole takes the oldest one's place, so a long
	 * match costs no more than this many decals. Zero turns holes off.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Decals", meta = (ClampMin = 0))
	int32 MaxDecals = 64;

	/** How thick a hole is along the shot. Thin, so that it prints on the surface it was shot into and not on whatever stands near it later. */
	UPROPERTY(Config, EditAnywhere, Category = "Decals", meta = (ClampMin = 0.0, ForceUnits = cm))
	float DecalDepth = 4.0f;

	/** A hole stops drawing once it covers less of the screen than this, so far ones cost nothing. */
	UPROPERTY(Config, EditAnywhere, Category = "Decals", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float DecalFadeScreenSize = 0.002f;
};
