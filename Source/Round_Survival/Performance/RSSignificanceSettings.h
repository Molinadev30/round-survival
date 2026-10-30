// Round Survival

#pragma once

#include "Components/SkinnedMeshComponent.h"
#include "Engine/DeveloperSettings.h"

#include "RSSignificanceSettings.generated.h"

/**
 * How much an enemy matters to the players right now, worked out on the server by URSSignificanceManager.
 * Ordered, so a higher value matters more.
 */
UENUM(BlueprintType)
enum class ERSSignificanceTier : uint8
{
	/** Seen by nobody and close to nobody. */
	Low,

	/** Inside some player's view, or close to some player. Always updated in full. */
	High,
};

/**
 * The significance manager's numbers: where the tiers fall and what each one costs an enemy.
 *
 * What High gets is not here on purpose. An enemy any player can see or is close to is one whose shots are validated
 * against its pose on the server, so it always animates and refreshes its bones in full. That is a rule of the code,
 * not a setting someone could turn down.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Significance"))
class ROUND_SURVIVAL_API URSSignificanceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URSSignificanceSettings();

	//~UDeveloperSettings interface
	virtual FName GetCategoryName() const override { return TEXT("Round Survival"); }
	//~End of UDeveloperSettings interface

	/** How often the tiers are worked out again. The view cone's margin covers how far a player can turn in between. */
	UPROPERTY(Config, EditAnywhere, Category = "Significance", meta = (ClampMin = 0.0, ForceUnits = s))
	float UpdateInterval = 0.1f;

	/**
	 * Half the angle of the cone, around a player's view, inside which an enemy is seen. Wider than the camera's own
	 * half field of view (about 52 degrees) by the turn a player can make in one update interval.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Significance", meta = (ClampMin = 0.0, ClampMax = 180.0, ForceUnits = deg))
	float ViewHalfAngle = 75.0f;

	/** Within this distance of a player an enemy is High wherever it is: it is about to be in the player's face. */
	UPROPERTY(Config, EditAnywhere, Category = "Significance", meta = (ClampMin = 0.0, ForceUnits = cm))
	float NearRadius = 600.0f;

	/**
	 * How the server animates a Low enemy. The default keeps montages ticking — attacks, tearing boards, the vault:
	 * their notifies and root motion are gameplay — and refreshes bones only while one plays.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Significance")
	EVisibilityBasedAnimTickOption UnseenAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesAndRefreshBonesWhenPlayingMontages;

	/**
	 * How a client animates its copies of the enemies. A client only shoots what it renders, so the engine's own
	 * rendered test is the right one there — unlike the server's, whose renderer is only the host's view.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Significance")
	EVisibilityBasedAnimTickOption ClientAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesAndRefreshBonesWhenPlayingMontages;
};
