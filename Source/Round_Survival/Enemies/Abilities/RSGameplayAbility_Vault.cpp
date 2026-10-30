// Round Survival

#include "Enemies/Abilities/RSGameplayAbility_Vault.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "AnimNotifyState_MotionWarping.h"
#include "Components/CapsuleComponent.h"
#include "Enemies/RSZombieCharacter.h"
#include "Enemies/RSZombieController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interaction/RSBarricade.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"

URSGameplayAbility_Vault::URSGameplayAbility_Vault(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// One instance per actor: the crossing holds a transit claim and a movement ignore between
	// activation and the end of the montage, and an enemy is only ever in one window at a time.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// No prediction. Where a body ends up after a warped root-motion montage is the server's call.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	ActivationPolicy = ERSAbilityActivationPolicy::OnInputTriggered;

	// For the length of the climb, so anything that would change the body under the warp — falling
	// to a crawl — knows to wait for the far side.
	ActivationOwnedTags.AddTag(RSGameplayTags::State_TraversingWindow);
}

bool URSGameplayAbility_Vault::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// A window still holding boards is not a way in, it is something to tear at. The State Tree
	// tests this before entering the state; the ability answers for itself so that the tree is not
	// the only thing standing between an enemy and a climb through solid planks.
	const ARSBarricade* Barricade = GetTargetBarricade();

	// The same goes for where the climb starts: from a place at the window, held, in its own lane. The
	// warp reaches the sill from wherever the enemy stands, so an enemy without a slot would be dragged
	// over from however far out it happened to be.
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const APawn* Pawn = Cast<APawn>(Avatar);
	const ARSZombieController* Controller = Pawn ? Cast<ARSZombieController>(Pawn->GetController()) : nullptr;
	if (!Controller || Controller->BarricadeSlotIndex == INDEX_NONE)
	{
		return false;
	}

	// Busy means someone else is already in the opening. Answering no here rather than letting
	// `ActivateAbility` reserve and fail is what stops an enemy in the queue from trying, and
	// failing, on every single tick of the state it is waiting in. A claim the enemy took itself,
	// stepping up to the sill, is not busy.
	return Barricade && Barricade->IsBreached() && !Barricade->IsTransitHeldByOther(Avatar);
}

void URSGameplayAbility_Vault::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ARSBarricade* Barricade = GetTargetBarricade();
	ARSZombieCharacter* Enemy = Cast<ARSZombieCharacter>(GetAvatarActorFromActorInfo());
	UMotionWarpingComponent* Warping = Enemy ? Enemy->GetMotionWarping() : nullptr;
	UAnimMontage* Montage = GetVaultMontage();

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo) || !Barricade || !Enemy || !Warping || !Montage)
	{
		UE_LOG(LogRSAbility, Warning, TEXT("[%s] cannot vault: barricade=%s warping=%s montage=%s."),
			*GetNameSafe(GetAvatarActorFromActorInfo()), *GetNameSafe(Barricade), *GetNameSafe(Warping), *GetNameSafe(Montage));
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
		return;
	}

	// One body in the window at a time. Losing the race here is normal and not an error: the
	// enemy goes back to queueing and tries again when whoever is inside has finished.
	if (!Barricade->TryReserveTransit(Enemy))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
		return;
	}

	CrossingBarricade = Barricade;
	bCrossing = true;

	// Both targets before the montage starts. The first notify state can sit on frame zero, and a
	// warp target added after its window has opened is a warp that never happens.
	// With a rotation, not just a location: the notify states warp rotation as well, and a target
	// built from a location alone faces world +X, which turns the enemy side-on in any window that
	// is not itself facing +X. Both targets face the way through, outside to inside.
	// In the lane of the slot this enemy climbs from; see `ARSBarricade::GetVaultFrontLedge`.
	const ARSZombieController* Controller = Cast<ARSZombieController>(Enemy->GetController());
	const int32 SlotIndex = Controller ? Controller->BarricadeSlotIndex : INDEX_NONE;

	const FRotator Crossing = Barricade->GetInwardDirection().Rotation();
	// Clear of the wall whatever the tuning says: a capsule put down closer than its own radius
	// would land inside the brickwork.
	const float Landing = FMath::Max(LandingDistance, Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius() + 2.0f);
	const FVector BackLedge = Barricade->GetVaultBackLedge(SlotIndex, Landing);
	const FVector FrontLedge = Barricade->GetVaultFrontLedge(SlotIndex) - Crossing.Vector() * FrontLedgeSetback;
	Warping->AddOrUpdateWarpTargetFromLocationAndRotation(FrontLedgeTargetName, FrontLedge, Crossing);
	Warping->AddOrUpdateWarpTargetFromLocationAndRotation(BackLedgeTargetName, BackLedge, Crossing);

	// Movement only. The barricade keeps every trace response it had, so an enemy halfway through
	// a window is exactly as shootable as one standing in front of it.
	Enemy->MoveIgnoreActorAdd(Barricade);

	// Walking discards the vertical part of root motion — the capsule would slide through the wall
	// at floor height while the mesh climbed over the sill. Flying lets the warp lift it.
	Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Flying);

	// Other bodies, for the length of the climb. The window is where the crowd is — the enemies at
	// the neighbouring slots, whoever just landed, a player standing at the glass — and a capsule
	// stopped by one of them mid-warp finishes the montage where it was stopped, outside. Movement
	// only, like the frame: the mesh still takes every shot, and the bodies push apart on landing.
	UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
	PawnResponseBeforeCrossing = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	// How far the enemy still has to go to the plant, measured along the way through: sideways
	// offset is the warp's to correct, and no frame of the clip would match it anyway.
	const float DistanceToPlant = FMath::Max(0.0f, static_cast<float>(FVector::DotProduct(
		FrontLedge - Enemy->GetActorLocation(), Crossing.Vector())));
	const float StartTime = FindDistanceMatchedStartTime(Montage, DistanceToPlant);

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this,
		NAME_None, Montage, MontageRate, /*StartSection=*/ NAME_None, /*bStopWhenAbilityEnds=*/ true,
		/*AnimRootMotionTranslationScale=*/ 1.0f, StartTime);

	// Only a montage that ran to the end put the enemy on the far side. Anything else leaves it
	// outside, and the difference decides whether the window stops being its way in.
	// Blend out is the landing: the montage's `BlendOutTriggerTime` is zero, so the blend starts on
	// the clip's last frame, with the feet on the floor, and the root motion before it plays at full
	// weight — which the landing warp needs to reach its target. Waiting for completion instead
	// would leave the enemy flying for the length of the blend, and flying has no braking: it
	// slides on with whatever speed the landing had. Completion still ends it if blend out is missed.
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageCompleted);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->ReadyForActivation();

	UE_LOG(LogRSAbility, Verbose, TEXT("[%s] is climbing through [%s] from %.0f cm out, montage from %.2f s."),
		*Enemy->GetName(), *Barricade->GetName(), DistanceToPlant, StartTime);
}

UAnimMontage* URSGameplayAbility_Vault::GetVaultMontage() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const bool bCrawling = ASC && ASC->HasMatchingGameplayTag(RSGameplayTags::State_Crawling);
	return bCrawling && CrawlingVaultMontage ? CrawlingVaultMontage.Get() : VaultMontage.Get();
}

float URSGameplayAbility_Vault::FindDistanceMatchedStartTime(const UAnimMontage* Montage, float DistanceToPlant) const
{
	const float PlantTime = FindWarpWindowEnd(Montage, FrontLedgeTargetName);
	if (PlantTime <= 0.0f)
	{
		UE_LOG(LogRSAbility, Warning, TEXT("[%s] has no '%s' warp window on [%s]; the climb starts from the top."),
			*GetName(), *FrontLedgeTargetName.ToString(), *GetNameSafe(Montage));
		return 0.0f;
	}

	// Ground the clip still covers between a start time and the plant. It only ever shrinks as the
	// start moves later, which is what lets a bisection find the match.
	const auto RemainingAt = [Montage, PlantTime](float Time)
	{
		return static_cast<float>(Montage->ExtractRootMotionFromTrackRange(Time, PlantTime, FAnimExtractContext())
			.GetTranslation().Size2D());
	};

	const float LatestStart = FMath::Min(MaxMontageStartTime, PlantTime);

	// Further out than the whole run-up: start from the top and let the warp stretch the rest.
	if (RemainingAt(0.0f) <= DistanceToPlant)
	{
		return 0.0f;
	}

	// Closer than the cap allows for: start at the cap and let the warp shorten the rest.
	if (RemainingAt(LatestStart) >= DistanceToPlant)
	{
		return LatestStart;
	}

	// A dozen halvings of a third of a second is well under a frame of animation.
	float Early = 0.0f;
	float Late = LatestStart;
	for (int32 Step = 0; Step < 12; ++Step)
	{
		const float Mid = 0.5f * (Early + Late);
		if (RemainingAt(Mid) > DistanceToPlant)
		{
			Early = Mid;
		}
		else
		{
			Late = Mid;
		}
	}

	return 0.5f * (Early + Late);
}

float URSGameplayAbility_Vault::FindWarpWindowEnd(const UAnimMontage* Montage, FName WarpTargetName) const
{
	if (!Montage)
	{
		return -1.0f;
	}

	// Read off the montage rather than kept as a number here, so retiming the clip in the editor
	// cannot leave the distance matching aiming at a plant the animation no longer has.
	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		const UAnimNotifyState_MotionWarping* WarpNotify = Cast<UAnimNotifyState_MotionWarping>(NotifyEvent.NotifyStateClass);
		const URootMotionModifier_Warp* Modifier = WarpNotify ? Cast<URootMotionModifier_Warp>(WarpNotify->RootMotionModifier) : nullptr;
		if (Modifier && Modifier->WarpTargetName == WarpTargetName)
		{
			return NotifyEvent.GetEndTriggerTime();
		}
	}

	return -1.0f;
}

void URSGameplayAbility_Vault::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// The backstop. Every ordinary path has already called this, but an ability cancelled from
	// outside — death, a cleared ability set, the window destroyed under it — arrives only here,
	// and an enemy left ignoring a wall is worse than one that never crossed.
	StopCrossing(/*bArrivedInside=*/ false);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

ARSBarricade* URSGameplayAbility_Vault::GetTargetBarricade() const
{
	const ARSZombieCharacter* Enemy = Cast<ARSZombieCharacter>(GetAvatarActorFromActorInfo());
	ARSBarricade* Barricade = Enemy ? Enemy->GetEntryBarricade() : nullptr;

	// IsValid rather than a null check: a window can be torn out of the level mid-climb, and the
	// pointer handed over at birth would outlive it by a frame.
	return IsValid(Barricade) ? Barricade : nullptr;
}

void URSGameplayAbility_Vault::HandleMontageCompleted()
{
	// A montage that ran to the end has not necessarily carried the body with it: root motion that
	// was blocked still plays out. Only a body past the window's middle is inside; anything short
	// of that keeps its way in and tries again, rather than being told it arrived while standing
	// outside with no window left to go through.
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const ARSBarricade* Barricade = CrossingBarricade.Get();
	const bool bArrivedInside = Avatar && Barricade && FVector::DotProduct(
		Avatar->GetActorLocation() - Barricade->GetActorLocation(), Barricade->GetInwardDirection()) > 0.0;

	if (!bArrivedInside)
	{
		UE_LOG(LogRSAbility, Warning, TEXT("[%s] finished its climb without getting through [%s]; it keeps the window and tries again."),
			*GetNameSafe(Avatar), *GetNameSafe(Barricade));
	}

	StopCrossing(bArrivedInside);

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		/*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
}

void URSGameplayAbility_Vault::HandleMontageInterrupted()
{
	StopCrossing(/*bArrivedInside=*/ false);

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		/*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ true);
}

void URSGameplayAbility_Vault::StopCrossing(bool bArrivedInside)
{
	if (!bCrossing)
	{
		return;
	}

	bCrossing = false;

	ARSZombieCharacter* Enemy = Cast<ARSZombieCharacter>(GetAvatarActorFromActorInfo());
	ARSBarricade* Barricade = CrossingBarricade.Get();
	CrossingBarricade.Reset();

	if (Enemy && Barricade)
	{
		Enemy->MoveIgnoreActorRemove(Barricade);
	}

	// Back to pushing and being pushed. Harmless on a corpse: death turns the capsule's collision
	// off outright, and a response on a disabled component changes nothing.
	if (Enemy)
	{
		Enemy->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, PawnResponseBeforeCrossing);
	}

	// Falling rather than walking: a climb cut short can leave the enemy on the sill, and falling
	// finds the floor from wherever it is. Only out of flying — death has already set the movement
	// to none, and waking it up here would drop a corpse with no capsule through the floor.
	UCharacterMovementComponent* Movement = Enemy ? Enemy->GetCharacterMovement() : nullptr;
	if (Movement && Movement->MovementMode == MOVE_Flying)
	{
		Movement->SetMovementMode(MOVE_Falling);
	}

	if (Barricade && Enemy)
	{
		Barricade->ReleaseTransit(Enemy);
	}

	// Inside, so the window stops being the way in. Clearing it is what sends the enemy back to
	// the chase: `SeekBarricade` is gated on having an entry barricade, so the State Tree picks
	// the chase on its next tick without a second thing having to tell it the climb is over.
	// An interrupted climb leaves the barricade in place, because the enemy is still outside.
	if (bArrivedInside && Enemy)
	{
		Enemy->SetEntryBarricade(nullptr);
		UE_LOG(LogRSAbility, Verbose, TEXT("[%s] is inside; [%s] is no longer its way in."),
			*Enemy->GetName(), *GetNameSafe(Barricade));
	}
}
