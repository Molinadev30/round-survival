// Round Survival

#pragma once

#include "AIController.h"

#include "RSZombieController.generated.h"

class URSZombieTargetingComponent;
class UStateTreeAIComponent;

/**
 * The brain of one enemy.
 *
 * Holds the two halves of the AI: a StateTree component running the behaviour, and a targeting
 * component answering the one question every state needs an answer to. Both start on possession
 * and stop on unpossession, which is also how death turns the AI off — the pawn detaches from its
 * controller when it dies, so nothing has to listen for a death event to stop pathing a corpse.
 *
 * Exists on the server only, like every AI controller.
 */
UCLASS()
class ROUND_SURVIVAL_API ARSZombieController : public AAIController
{
	GENERATED_BODY()

public:
	ARSZombieController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintPure, Category = "RS|Enemy")
	URSZombieTargetingComponent* GetTargetingComponent() const { return TargetingComponent; }

	UFUNCTION(BlueprintPure, Category = "RS|Enemy")
	UStateTreeAIComponent* GetStateTreeComponent() const { return StateTreeComponent; }

	/**
	 * Where this enemy's place at its window is, and whether it has one.
	 *
	 * Written by `FRSSTTask_ReserveBarricadeSlot` and read, through the controller as a context
	 * object, by whatever walks and whatever decides it has arrived. It lives here rather than
	 * passing task to task because a State Tree binds a task to a context object, not to its
	 * neighbour: one writer, one place, and any state in the tree can read it.
	 *
	 * Only meaningful while `bHasBarricadeSlot`: the walk in binds it, and that state is left the
	 * moment the place is. Where the queue stands is a separate value, below — one value with two
	 * meanings is how an enemy that lost its place once ended up still "walking in", to the queue.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Enemy")
	FVector BarricadeSlotLocation = FVector::ZeroVector;

	/**
	 * Where those without a place wait: `QueueStandoff` out from the window, so the surplus stands
	 * behind the attackers instead of in the places they are walking into. Written by the same
	 * task, bound by the queue's walk.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Enemy")
	FVector BarricadeQueueLocation = FVector::ZeroVector;

	/** True while this enemy holds a place at the window rather than queueing for one. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Enemy")
	bool bHasBarricadeSlot = false;

	/**
	 * Which of the window's slots is held, or -1 while queueing.
	 *
	 * Each slot owns one gap in the boards, so this is also which gap a reach goes through — the
	 * reach ability and its State Tree condition both read it from here, off the same task.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Enemy")
	int32 BarricadeSlotIndex = INDEX_NONE;

	/**
	 * Degrees the held slot turns its enemy while it reaches through, from the window's layout.
	 * Written by the same task; the reach state's `Face Location` adds it to facing the window.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Enemy")
	float BarricadeSlotReachYaw = 0.0f;

	/**
	 * The window itself, as opposed to the place at it this enemy stands in.
	 *
	 * Written by the same task and for the same reason, but it answers a different question: the
	 * slot is where to walk to, this is what to face. An enemy tearing at a window stands beside
	 * the middle of it, so turning towards its own slot would have it facing nothing.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Enemy")
	FVector BarricadeLocation = FVector::ZeroVector;

	/**
	 * Where this enemy stands to start climbing: its own slot, against the wall.
	 *
	 * Written by `FRSSTTask_ReserveTransit` once the crossing is this enemy's, and read by the walk
	 * that comes before the vault. Each slot climbs through its own lane, so this is where the
	 * enemy already is unless the crowd pushed it; the walk only squares it back up.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Enemy")
	FVector VaultStartLocation = FVector::ZeroVector;

	/**
	 * World time the last ability run by the tree's `Play Ability` ended, or a large negative
	 * number before any has. Lets the next one tell a swing that follows straight on from the last
	 * from one that starts from rest, which is the only one worth delaying.
	 */
	double LastPlayedAbilityEndTime = -UE_DOUBLE_BIG_NUMBER;

protected:
	//~AController interface
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	//~End of AController interface

	/**
	 * Runs the behaviour. Its State Tree asset is set on the Blueprint subclass rather than here:
	 * which tree an enemy runs is content, and a second enemy type is then a controller Blueprint
	 * with a different tree and no C++ at all.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Enemy", meta = (AllowPrivateAccess = true))
	TObjectPtr<UStateTreeAIComponent> StateTreeComponent;

	/** Decides who this enemy is chasing. See URSZombieTargetingComponent for why it is not in the tree. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Enemy", meta = (AllowPrivateAccess = true))
	TObjectPtr<URSZombieTargetingComponent> TargetingComponent;
};
