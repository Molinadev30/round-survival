// Round Survival

#include "Core/RSPlayerState.h"

#include "AbilitySystem/Attributes/RSCombatSet.h"
#include "AbilitySystem/Attributes/RSEconomySet.h"
#include "AbilitySystem/Attributes/RSHandlingSet.h"
#include "AbilitySystem/Attributes/RSHealthSet.h"
#include "AbilitySystem/RSAbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"

ARSPlayerState::ARSPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystemComponent = CreateDefaultSubobject<URSAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// Mixed: the owning client receives full gameplay effect data so it can predict its own
	// abilities, while other clients receive only tags and cues. Full replication to everyone
	// would send every player's effect list to every connection for no visible benefit.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	HealthSet = CreateDefaultSubobject<URSHealthSet>(TEXT("HealthSet"));
	CombatSet = CreateDefaultSubobject<URSCombatSet>(TEXT("CombatSet"));
	EconomySet = CreateDefaultSubobject<URSEconomySet>(TEXT("EconomySet"));
	HandlingSet = CreateDefaultSubobject<URSHandlingSet>(TEXT("HandlingSet"));

	// The default 1 Hz is fine for a scoreboard and far too slow for an attribute the HUD
	// renders. Attributes on this actor drive health and scrap readouts.
	SetNetUpdateFrequency(100.0f);
}

UAbilitySystemComponent* ARSPlayerState::GetAbilitySystemComponent() const
{
	return GetRSAbilitySystemComponent();
}

void ARSPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, BleedOutClock);
	DOREPLIFETIME(ThisClass, MatchStats);
}

void ARSPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	check(AbilitySystemComponent);

	// Owner is the player state, avatar is the pawn. The pawn does not exist yet at this point;
	// the character calls InitAbilityActorInfo again on possession to fill the avatar in.
	AbilitySystemComponent->InitAbilityActorInfo(this, GetPawn());

	GrantDefaultAbilitySets();

	if (HasAuthority())
	{
		ScrapChangedHandle = EconomySet->OnScrapChanged.AddUObject(this, &ThisClass::HandleScrapChanged);
		ScrapAwardedHandle = EconomySet->OnScrapAwarded.AddUObject(this, &ThisClass::HandleScrapAwarded);
	}
}

void ARSPlayerState::OnRep_PlayerName()
{
	Super::OnRep_PlayerName();

	// The engine calls this from SetPlayerName on a listen server and in standalone as well as
	// from replication, so one broadcast here covers every machine with a HUD.
	OnPlayerNameChanged.Broadcast(this);
}

void ARSPlayerState::SetBleedOutClock(const FRSBleedOutClock& InClock)
{
	if (!HasAuthority())
	{
		return;
	}

	BleedOutClock = InClock;

	// A listen server's own HUD gets no rep notify.
	OnBleedOutClockChanged.Broadcast(this);
}

void ARSPlayerState::OnRep_BleedOutClock()
{
	OnBleedOutClockChanged.Broadcast(this);
}

void ARSPlayerState::RecordKill(bool bHeadshot)
{
	if (!HasAuthority())
	{
		return;
	}

	++MatchStats.Kills;
	MatchStats.Headshots += bHeadshot ? 1 : 0;
	MatchStatsChanged();
}

void ARSPlayerState::RecordDown()
{
	if (!HasAuthority())
	{
		return;
	}

	++MatchStats.Downs;
	MatchStatsChanged();
}

void ARSPlayerState::RecordRevive()
{
	if (!HasAuthority())
	{
		return;
	}

	++MatchStats.Revives;
	MatchStatsChanged();
}

void ARSPlayerState::MatchStatsChanged()
{
	ForceNetUpdate();
	OnMatchStatsChanged.Broadcast(this);
}

void ARSPlayerState::OnRep_MatchStats()
{
	OnMatchStatsChanged.Broadcast(this);
}

void ARSPlayerState::OnRep_Score()
{
	Super::OnRep_Score();

	OnMatchStatsChanged.Broadcast(this);
}

void ARSPlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilitySystemComponent)
	{
		DefaultAbilityHandles.TakeFromAbilitySystem(AbilitySystemComponent);
	}

	EconomySet->OnScrapChanged.Remove(ScrapChangedHandle);
	ScrapChangedHandle.Reset();
	EconomySet->OnScrapAwarded.Remove(ScrapAwardedHandle);
	ScrapAwardedHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void ARSPlayerState::HandleScrapChanged(AActor* /*Instigator*/, AActor* /*Causer*/, const FGameplayEffectSpec* EffectSpec,
	float /*Magnitude*/, float OldValue, float NewValue)
{
	// The set also broadcasts from replication, with no spec. Only an executed effect on the server
	// is a change worth telling anyone about, and it is told exactly once, from here.
	const int32 Delta = FMath::RoundToInt(NewValue - OldValue);
	if (!EffectSpec || Delta == 0)
	{
		return;
	}

	// Awards and purchases tag their spec with what they were for. The scrap effects themselves
	// carry no RS.Economy tag, so whatever is found here is the reason.
	static const FGameplayTag EconomyTag = FGameplayTag::RequestGameplayTag(TEXT("RS.Economy"));

	FGameplayTagContainer AssetTags;
	EffectSpec->GetAllAssetTags(AssetTags);

	FGameplayTag Reason;
	for (const FGameplayTag& Tag : AssetTags)
	{
		if (Tag.MatchesTag(EconomyTag))
		{
			Reason = Tag;
			break;
		}
	}

	ClientScrapChanged(Delta, Reason);
}

void ARSPlayerState::HandleScrapAwarded(AActor* /*Instigator*/, AActor* /*Causer*/, const FGameplayEffectSpec* EffectSpec,
	float Magnitude, float /*OldValue*/, float /*NewValue*/)
{
	if (!EffectSpec || Magnitude <= 0.0f)
	{
		return;
	}

	FGameplayTagContainer AssetTags;
	EffectSpec->GetAllAssetTags(AssetTags);
	if (AssetTags.HasTagExact(RSGameplayTags::Economy_Reward_Revive))
	{
		return;
	}

	// Magnitude is what was granted, after the multiplier. The listen server's own scoreboard gets no
	// OnRep_Score, so it hears the change from here.
	SetScore(GetScore() + Magnitude);
	MatchStatsChanged();
}

void ARSPlayerState::ClientScrapChanged_Implementation(int32 Delta, FGameplayTag Reason)
{
	OnScrapDeltaReceived.Broadcast(Delta, Reason);
}

void ARSPlayerState::GrantDefaultAbilitySets()
{
	// Granting is server-only; clients receive the resulting specs by replication. Without this
	// guard a listen server's client would try to give itself abilities it does not own.
	if (!HasAuthority())
	{
		return;
	}

	for (const URSAbilitySet* AbilitySet : DefaultAbilitySets)
	{
		if (!AbilitySet)
		{
			UE_LOG(LogRSAbility, Warning, TEXT("Null entry in DefaultAbilitySets on [%s]."), *GetNameSafe(this));
			continue;
		}

		// SourceObject is the player state: an ability granted here belongs to the player, not to
		// a piece of equipment. Weapon abilities pass the weapon instance instead.
		AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &DefaultAbilityHandles, this);
	}
}
