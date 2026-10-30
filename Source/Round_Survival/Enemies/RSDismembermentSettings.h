// Round Survival

#pragma once

#include "Engine/DeveloperSettings.h"

#include "RSDismembermentSettings.generated.h"

/**
 * What every machine allows the pieces that come off enemies, whatever enemy they came off.
 *
 * Per machine, not per enemy: the pieces are cosmetic, never replicated, and each machine keeps its
 * own. What a part does and how it bleeds is the enemy's, in its URSDismembermentProfile.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Dismemberment"))
class ROUND_SURVIVAL_API URSDismembermentSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URSDismembermentSettings();

	//~UDeveloperSettings interface
	virtual FName GetCategoryName() const override { return TEXT("Round Survival"); }
	//~End of UDeveloperSettings interface

	/**
	 * Pieces in the world at once. A cut past it takes the oldest piece, so a horde cut apart
	 * costs no more than this many simulated bodies. Zero turns pieces off.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Pieces", meta = (ClampMin = 0))
	int32 MaxSeveredPieces = 16;

	/** How long a piece lies there before it goes. About the corpse's own lifetime: a piece that outlives its body by much reads as a mistake. */
	UPROPERTY(Config, EditAnywhere, Category = "Pieces", meta = (ClampMin = 0.0, ForceUnits = s))
	float SeveredPieceLifetime = 6.0f;
};
