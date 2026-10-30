// Round Survival

#pragma once

#include "Animation/AnimInstance.h"
#include "Enemies/RSEnemyTypes.h"

#include "RSZombieAnimInstance.generated.h"

class ARSZombieCharacter;
class UAbilitySystemComponent;
struct FGameplayTag;

/**
 * Animation instance for the enemy.
 *
 * Deliberately smaller than the player's: an enemy walks forward, stands still or swings, and the
 * swing arrives as a montage on the default slot. There is no aim and no strafe, so there is one
 * number worth computing, and everything else the graph reads is derived from it. The one other
 * thing the graph needs is whether the enemy is on its feet at all: one that has lost a leg
 * crawls, on loops of its own (`bIsCrawling`).
 *
 * Speed is the axis of `BS_RS_Zombie_Locomotion`, whose samples sit at the speed each loop really
 * travels at. That is why the tier is not what drives the graph: a slowed runner and a walker move
 * at the same speed and should look the same, and they do, without the graph knowing either exists.
 */
UCLASS()
class ROUND_SURVIVAL_API URSZombieAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	URSZombieAnimInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~UAnimInstance interface
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	//~End of UAnimInstance interface

	/**
	 * Whether a montage is playing on `MirroredReachSlot`, blending out included.
	 *
	 * The graph mirrors the pose on the way into that slot and again on the way out, so the two
	 * cancel on everything but the montage: only the reach comes out mirrored, with the other arm.
	 * This gates that detour, so the rest of the time, which is nearly always, the graph does not
	 * pay for two mirrors of a pose that would come out the same.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Reach")
	bool bMirroredReachActive = false;

	/** The slot the mirrored reach montage plays on. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Reach")
	FName MirroredReachSlot = TEXT("ReachMirrored");

	/** Horizontal speed in cm/s. The locomotion blend space's only axis. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Locomotion")
	float Speed = 0.0f;

	/**
	 * Whether the enemy is going anywhere.
	 *
	 * Its own bool rather than a comparison in the graph, and the threshold is under the slowest
	 * tier: a shambler moves at 45 cm/s, so anything that tests against a round number like 50
	 * would leave the slowest enemies in the game standing still while they walk at you.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Locomotion")
	bool bIsMoving = false;

	/**
	 * What the AI decided this enemy is. Nothing in the graph needs it today — the blend space
	 * reads speed — but a per-tier swing or scream is a content change with it exposed.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Locomotion")
	ERSMoveTier MoveTier = ERSMoveTier::Walk;

	/** Below this, in cm/s, the enemy counts as stopped. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Locomotion", meta = (ForceUnits = "cm/s"))
	float MovingSpeedThreshold = 5.0f;

	/**
	 * Whether the enemy moves fast enough for the walk loop, the run loop and the sprint loop.
	 *
	 * What the state machine's transitions read, one bool each way, instead of comparing Speed
	 * against a number typed into the rule. A comparison in a transition is a Blueprint thunk that
	 * takes the graph off the fast path; a member read is not. It also means the switch points are
	 * no longer a second copy of the tier speeds that can drift away from the first.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Locomotion")
	bool bFastEnoughToWalk = false;

	UPROPERTY(BlueprintReadOnly, Category = "RS|Locomotion")
	bool bFastEnoughToRun = false;

	UPROPERTY(BlueprintReadOnly, Category = "RS|Locomotion")
	bool bFastEnoughToSprint = false;

	/**
	 * Whether the enemy has `RS.State.Crawling`: a leg gone, on its hands for the rest of its life.
	 *
	 * The tag rather than the movement component's crouch, which a climb through a window lifts
	 * while it flies the body over the sill: the graph under the climb stays on the crawl, so the
	 * landing blends into a crawl and not into a standing walk first. Kept by a tag event, so no
	 * frame asks the ability system. The tag is replicated, so a client reads what the server does.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Crawl")
	bool bIsCrawling = false;

	/**
	 * Playback rate of the crawl loop, so the hands keep pace with the body. The enemy's crawl speed
	 * is set for the game (URSEnemyDefinition::CrawlSpeed), not for the clip.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Crawl")
	float CrawlPlayRate = 1.0f;

	/**
	 * How fast the crawl loop travels at a rate of 1, in cm/s. Measured from the root motion
	 * version of the clip: 72 cm in 9.03 s.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Crawl", meta = (ClampMin = 0.1, ForceUnits = "cm/s"))
	float CrawlLoopSpeed = 8.0f;

	/** Where `CrawlPlayRate` stops. Past it the loop stops reading as a crawl at all. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Crawl", meta = (ClampMin = 0.1))
	float MaxCrawlPlayRate = 4.0f;

private:
	/**
	 * Reads the switch points off the enemy definition: halfway between consecutive tier speeds,
	 * so an enemy moving at a tier's speed sits squarely inside that tier's loop and a slowed or
	 * hastened one changes loop where it starts to look more like the neighbouring tier.
	 */
	void CacheSpeedThresholds();

	/** Halfway between Shamble and Walk, Walk and Run, Run and Sprint. In cm/s. */
	float WalkSpeedThreshold = 0.0f;
	float RunSpeedThreshold = 0.0f;
	float SprintSpeedThreshold = 0.0f;

	/** False until a definition supplied the thresholds; the bools stay false until then. */
	bool bSpeedThresholdsValid = false;

	/** Cached on init so the worker thread never walks the actor hierarchy looking for it. */
	TWeakObjectPtr<ARSZombieCharacter> OwningZombie;

	void HandleCrawlingTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** Where the crawl tag event is bound, so a second initialisation does not bind it twice. */
	TWeakObjectPtr<UAbilitySystemComponent> CrawlTagSource;
	FDelegateHandle CrawlTagHandle;
};
