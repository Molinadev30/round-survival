// Round Survival

#include "Animation/RSZombieAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "Enemies/RSEnemyDefinition.h"
#include "Enemies/RSZombieCharacter.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"

URSZombieAnimInstance::URSZombieAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URSZombieAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwningZombie = Cast<ARSZombieCharacter>(TryGetPawnOwner());

	CacheSpeedThresholds();

	if (UAbilitySystemComponent* Previous = CrawlTagSource.Get())
	{
		Previous->RegisterGameplayTagEvent(RSGameplayTags::State_Crawling, EGameplayTagEventType::NewOrRemoved).Remove(CrawlTagHandle);
	}
	CrawlTagSource.Reset();
	CrawlTagHandle.Reset();

	ARSZombieCharacter* Zombie = OwningZombie.Get();
	UAbilitySystemComponent* ASC = Zombie ? Zombie->GetAbilitySystemComponent() : nullptr;
	if (ASC)
	{
		CrawlTagSource = ASC;
		CrawlTagHandle = ASC->RegisterGameplayTagEvent(RSGameplayTags::State_Crawling, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleCrawlingTagChanged);
		bIsCrawling = ASC->HasMatchingGameplayTag(RSGameplayTags::State_Crawling);
	}
}

void URSZombieAnimInstance::HandleCrawlingTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	bIsCrawling = NewCount > 0;
}

void URSZombieAnimInstance::CacheSpeedThresholds()
{
	bSpeedThresholdsValid = false;

	// No owner is the animation editor's preview, which has nothing to read and nothing to warn about.
	const ARSZombieCharacter* Zombie = OwningZombie.Get();
	if (!Zombie)
	{
		return;
	}

	const URSEnemyDefinition* Definition = Zombie->GetEnemyDefinition();
	const float* Shamble = Definition ? Definition->FindTierSpeed(ERSMoveTier::Shamble) : nullptr;
	const float* Walk = Definition ? Definition->FindTierSpeed(ERSMoveTier::Walk) : nullptr;
	const float* Run = Definition ? Definition->FindTierSpeed(ERSMoveTier::Run) : nullptr;
	const float* Sprint = Definition ? Definition->FindTierSpeed(ERSMoveTier::Sprint) : nullptr;

	if (!Shamble || !Walk || !Run || !Sprint)
	{
		// The legs would stay on the slowest loop at any speed. Wrong, but visibly so, and the pawn
		// logs the same missing data when it sets its own speed.
		UE_LOG(LogRSRounds, Warning, TEXT("[%s] cannot place its locomotion switch points: definition [%s] is missing a tier speed."),
			*GetNameSafe(Zombie), *GetNameSafe(Definition));
		return;
	}

	WalkSpeedThreshold = (*Shamble + *Walk) * 0.5f;
	RunSpeedThreshold = (*Walk + *Run) * 0.5f;
	SprintSpeedThreshold = (*Run + *Sprint) * 0.5f;
	bSpeedThresholdsValid = true;
}

void URSZombieAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// On the game thread, where montage state lives. The montage itself is replicated by the
	// ability that plays it, so this reads the same on every machine.
	bMirroredReachActive = IsSlotActive(MirroredReachSlot);
}

void URSZombieAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	const ARSZombieCharacter* Zombie = OwningZombie.Get();
	if (!Zombie)
	{
		return;
	}

	// Horizontal only, and read off the pawn rather than the movement component: on a client this
	// enemy is a simulated proxy whose velocity arrives over the network, and the movement component
	// there is not the thing deciding where it goes.
	Speed = Zombie->GetVelocity().Size2D();
	bIsMoving = Speed > MovingSpeedThreshold;

	bFastEnoughToWalk = bSpeedThresholdsValid && Speed > WalkSpeedThreshold;
	bFastEnoughToRun = bSpeedThresholdsValid && Speed > RunSpeedThreshold;
	bFastEnoughToSprint = bSpeedThresholdsValid && Speed > SprintSpeedThreshold;

	// A plain enum read. The tier is replicated, so this is the same value on every machine.
	MoveTier = Zombie->GetMoveTier();

	CrawlPlayRate = FMath::Min(Speed / CrawlLoopSpeed, MaxCrawlPlayRate);
}
