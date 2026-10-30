// Round Survival

#include "AbilitySystem/Abilities/RSGameplayAbility_Downed.h"

#include "AbilitySystem/Attributes/RSEconomySet.h"
#include "AbilitySystem/Attributes/RSHealthSet.h"
#include "AbilitySystemComponent.h"
#include "Core/RSGameState.h"
#include "Core/RSPlayerState.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Interaction/RSInteractable.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"
#include "TimerManager.h"

URSGameplayAbility_Downed::URSGameplayAbility_Downed(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// One instance for the whole life of the ability system: it holds a binding to the health set.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Health only reaches zero on the server, and the effect it applies replicates on its own.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	ActivationPolicy = ERSAbilityActivationPolicy::OnSpawn;

	CancelAbilitiesWhenDowned.AddTag(RSGameplayTags::Ability_Interact);

	// A burst or a reload under way stops here, on the server, and RS.State.GoingDown keeps the weapon
	// from starting again until it comes back up. What comes back up is the weapon at whatever the
	// player has left: anything lost going down — a station's fire rate or reload speed — has reached
	// the client by then, so the first shot after is at the same cadence on both machines.
	CancelAbilitiesWhenDowned.AddTag(RSGameplayTags::Ability_Fire);
	CancelAbilitiesWhenDowned.AddTag(RSGameplayTags::Ability_Reload);

	// Aiming too. A downed player can aim again once the weapon is back up, like firing.
	CancelAbilitiesWhenDowned.AddTag(RSGameplayTags::Ability_ADS);

	// A downed player crawls.
	CancelAbilitiesWhenDowned.AddTag(RSGameplayTags::Ability_Sprint);

	// And does not strike: a blow under way when they went down lands on nobody.
	CancelAbilitiesWhenDowned.AddTag(RSGameplayTags::Ability_Melee);
}

void URSGameplayAbility_Downed::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const URSHealthSet* HealthSet = ASC ? ASC->GetSet<URSHealthSet>() : nullptr;

	if (!HealthSet || !DownedEffect || !DeadEffect)
	{
		UE_LOG(LogRSAbility, Error, TEXT("[%s] needs a health set on its owner, and DownedEffect and DeadEffect set."), *GetNameSafe(this));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	OutOfHealthHandle = HealthSet->OnOutOfHealth.AddUObject(this, &ThisClass::HandleOutOfHealth);

	UAbilitySystemComponent* MutableASC = ActorInfo->AbilitySystemComponent.Get();
	DownedTagHandle = MutableASC->RegisterGameplayTagEvent(RSGameplayTags::State_Downed, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleDownedTagChanged);
	BeingRevivedTagHandle = MutableASC->RegisterGameplayTagEvent(RSGameplayTags::State_BeingRevived, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleBeingRevivedTagChanged);
}

void URSGameplayAbility_Downed::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		if (const URSHealthSet* HealthSet = ASC->GetSet<URSHealthSet>())
		{
			HealthSet->OnOutOfHealth.Remove(OutOfHealthHandle);
		}

		ASC->RegisterGameplayTagEvent(RSGameplayTags::State_Downed, EGameplayTagEventType::NewOrRemoved).Remove(DownedTagHandle);
		ASC->RegisterGameplayTagEvent(RSGameplayTags::State_BeingRevived, EGameplayTagEventType::NewOrRemoved).Remove(BeingRevivedTagHandle);

		if (bSelfReviving)
		{
			ASC->RemoveLooseGameplayTag(RSGameplayTags::State_SelfReviving, 1, EGameplayTagReplicationState::TagOnly);
		}
	}

	bSelfReviving = false;

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BleedOutTimerHandle);
		World->GetTimerManager().ClearTimer(SelfReviveTimerHandle);
		PublishBleedOutClock();
	}

	OutOfHealthHandle.Reset();
	DownedTagHandle.Reset();
	BeingRevivedTagHandle.Reset();
	DownedEffectHandle.Invalidate();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URSGameplayAbility_Downed::HandleOutOfHealth(AActor* EffectInstigator, AActor* EffectCauser,
	const FGameplayEffectSpec* EffectSpec, float Magnitude, float OldValue, float NewValue)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || ASC->HasMatchingGameplayTag(RSGameplayTags::State_Downed) || ASC->HasMatchingGameplayTag(RSGameplayTags::State_Dead))
	{
		return;
	}

	// Before the downed effect, though the order does not matter to the loss: it goes to Scrap
	// directly, and the multiplier the effect zeroes only scales what is earned.
	const int32 ScrapLost = TakeScrapFraction(*ASC, DownedScrapLossFraction, RSGameplayTags::Economy_Penalty_Downed);

	if (ARSPlayerState* PlayerState = Cast<ARSPlayerState>(GetOwningActorFromActorInfo()))
	{
		PlayerState->SetScrapLostToDown(ScrapLost);
	}

	// Asked before the downed effect, which takes Second Wind off with every other station. And the
	// tag goes on first, so whoever checks whether anyone is left standing sees this player as able
	// to carry on at every instant of the down.
	bSelfReviving = ShouldSelfRevive(*ASC);
	if (bSelfReviving)
	{
		ASC->AddLooseGameplayTag(RSGameplayTags::State_SelfReviving, 1, EGameplayTagReplicationState::TagOnly);
	}

	DownedEffectHandle = ApplyGameplayEffectToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
		DownedEffect->GetDefaultObject<UGameplayEffect>(), GetAbilityLevel());

	ASC->CancelAbilities(&CancelAbilitiesWhenDowned);

	FTimerManager& TimerManager = GetWorld()->GetTimerManager();

	if (bSelfReviving)
	{
		TimerManager.SetTimer(SelfReviveTimerHandle, this, &ThisClass::HandleSelfRevived, SelfReviveSeconds, /*bLoop=*/ false);
		PublishBleedOutClock();

		UE_LOG(LogRSAbility, Log, TEXT("[%s] is down with Second Wind, up in %.1f s."), *GetNameSafe(GetAvatarActorFromActorInfo()), SelfReviveSeconds);
		return;
	}

	TimerManager.SetTimer(BleedOutTimerHandle, this, &ThisClass::HandleBledOut, BleedOutSeconds, /*bLoop=*/ false);
	PublishBleedOutClock();

	UE_LOG(LogRSAbility, Log, TEXT("[%s] is down, %.0f s to bleed out."), *GetNameSafe(GetAvatarActorFromActorInfo()), BleedOutSeconds);
}

bool URSGameplayAbility_Downed::ShouldSelfRevive(const UAbilitySystemComponent& ASC) const
{
	if (!ASC.HasMatchingGameplayTag(RSGameplayTags::Station_SecondWind))
	{
		return false;
	}

	// Alone now, not when it was bought: someone who joined since takes the self-revive away, and the
	// station is lost to this down either way.
	const ARSGameState* GameState = GetWorld()->GetGameState<ARSGameState>();
	return GameState && GameState->IsSoloMatch();
}

void URSGameplayAbility_Downed::HandleSelfRevived()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->Implements<URSInteractable>())
	{
		UE_LOG(LogRSAbility, Error, TEXT("[%s] cannot get [%s] back up: the avatar is not revivable."), *GetNameSafe(this), *GetNameSafe(Avatar));
		return;
	}

	UE_LOG(LogRSAbility, Log, TEXT("[%s] gets back up on Second Wind."), *GetNameSafe(Avatar));

	// The pawn's own revive, with nobody reviving: the downed effect comes off — which ends this
	// down, here included — the revived effect heals, and the scrap owed to a reviver is dropped.
	IRSInteractable::Execute_Interact(Avatar, nullptr);
}

void URSGameplayAbility_Downed::HandleDownedTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		return;
	}

	FTimerManager& TimerManager = GetWorld()->GetTimerManager();
	TimerManager.ClearTimer(BleedOutTimerHandle);
	TimerManager.ClearTimer(SelfReviveTimerHandle);

	if (bSelfReviving)
	{
		bSelfReviving = false;

		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(RSGameplayTags::State_SelfReviving, 1, EGameplayTagReplicationState::TagOnly);
		}
	}

	PublishBleedOutClock();
	DownedEffectHandle.Invalidate();
}

void URSGameplayAbility_Downed::HandleBeingRevivedTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	FTimerManager& TimerManager = GetWorld()->GetTimerManager();
	if (!TimerManager.TimerExists(BleedOutTimerHandle))
	{
		return;
	}

	// Counted, so two teammates reviving at once and one letting go leaves the clock stopped.
	if (NewCount > 0)
	{
		TimerManager.PauseTimer(BleedOutTimerHandle);
	}
	else
	{
		TimerManager.UnPauseTimer(BleedOutTimerHandle);
	}

	PublishBleedOutClock();

	UE_LOG(LogRSAbility, Verbose, TEXT("[%s] bleed-out %s with %.1f s left."), *GetNameSafe(GetAvatarActorFromActorInfo()),
		NewCount > 0 ? TEXT("paused") : TEXT("resumed"), TimerManager.GetTimerRemaining(BleedOutTimerHandle));
}

void URSGameplayAbility_Downed::BleedOutNow()
{
	FTimerManager& TimerManager = GetWorld()->GetTimerManager();
	if (!TimerManager.TimerExists(BleedOutTimerHandle))
	{
		return;
	}

	TimerManager.ClearTimer(BleedOutTimerHandle);
	HandleBledOut();
}

void URSGameplayAbility_Downed::HandleBledOut()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !ASC->HasMatchingGameplayTag(RSGameplayTags::State_Downed))
	{
		return;
	}

	// A lost match holds as it is until the host restarts it. Nobody dies into a spectator camera
	// with nobody left to watch.
	const ARSGameState* GameState = GetWorld()->GetGameState<ARSGameState>();
	if (GameState && GameState->GetRoundPhase() == ERSRoundPhase::GameOver)
	{
		UE_LOG(LogRSAbility, Log, TEXT("[%s] ran out of bleed-out after the match was lost; stays down."),
			*GetNameSafe(GetAvatarActorFromActorInfo()));

		// The published clock is left as it is: run out, it reads zero, which is the truth.
		return;
	}

	UE_LOG(LogRSAbility, Log, TEXT("[%s] bled out."), *GetNameSafe(GetAvatarActorFromActorInfo()));

	// Dead before no longer downed, so that there is no instant in which this player counts as
	// standing to whoever is checking whether anyone is.
	ApplyGameplayEffectToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
		DeadEffect->GetDefaultObject<UGameplayEffect>(), GetAbilityLevel());

	ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(RSGameplayTags::State_Downed));

	// Nobody revived them, so nobody is paid what they lost going down.
	if (ARSPlayerState* PlayerState = Cast<ARSPlayerState>(GetOwningActorFromActorInfo()))
	{
		PlayerState->TakeScrapLostToDown();
	}

	// Everyone still alive pays for the teammate who was left on the floor — downed ones included.
	if (GameState)
	{
		for (APlayerState* Other : GameState->PlayerArray)
		{
			const ARSPlayerState* OtherPlayer = Cast<ARSPlayerState>(Other);
			UAbilitySystemComponent* OtherASC = OtherPlayer ? OtherPlayer->GetAbilitySystemComponent() : nullptr;

			if (OtherASC && OtherASC != ASC && !OtherASC->HasMatchingGameplayTag(RSGameplayTags::State_Dead))
			{
				TakeScrapFraction(*OtherASC, BleedOutScrapLossFraction, RSGameplayTags::Economy_Penalty_TeammateBledOut);
			}
		}
	}
}

void URSGameplayAbility_Downed::PublishBleedOutClock() const
{
	ARSPlayerState* PlayerState = Cast<ARSPlayerState>(GetOwningActorFromActorInfo());
	const UWorld* World = GetWorld();
	if (!PlayerState || !World)
	{
		return;
	}

	FRSBleedOutClock Clock;

	// At most one of the two runs: a down either bleeds out or gets back up on its own.
	const FTimerManager& TimerManager = World->GetTimerManager();
	const bool bSelfRevive = TimerManager.TimerExists(SelfReviveTimerHandle);
	const FTimerHandle& Timer = bSelfRevive ? SelfReviveTimerHandle : BleedOutTimerHandle;

	if (TimerManager.TimerExists(Timer))
	{
		Clock.Duration = bSelfRevive ? SelfReviveSeconds : BleedOutSeconds;
		Clock.bSelfRevive = bSelfRevive;

		// A paused timer reports what it had left when it stopped.
		const float SecondsLeft = TimerManager.GetTimerRemaining(Timer);
		if (TimerManager.IsTimerPaused(Timer))
		{
			Clock.PausedSecondsLeft = SecondsLeft;
		}
		else
		{
			// The server's own clock, by the same call a client measures it with.
			const AGameStateBase* GameState = World->GetGameState();
			Clock.EndServerTime = (GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds()) + SecondsLeft;
		}
	}

	PlayerState->SetBleedOutClock(Clock);
}

int32 URSGameplayAbility_Downed::TakeScrapFraction(UAbilitySystemComponent& ASC, float Fraction, const FGameplayTag& Reason) const
{
	const URSEconomySet* EconomySet = ASC.GetSet<URSEconomySet>();
	if (!EconomySet || !ScrapLossEffect || Fraction <= 0.0f)
	{
		return 0;
	}

	const int32 Amount = FMath::FloorToInt(EconomySet->GetScrap() * Fraction);
	if (Amount <= 0)
	{
		return 0;
	}

	FGameplayEffectSpecHandle SpecHandle = ASC.MakeOutgoingSpec(ScrapLossEffect, 1.0f, ASC.MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return 0;
	}

	// Negative, because the effect is additive on Scrap — the same one a purchase charges through.
	SpecHandle.Data->SetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Scrap, -static_cast<float>(Amount));
	SpecHandle.Data->AddDynamicAssetTag(Reason);

	ASC.ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);

	return Amount;
}
