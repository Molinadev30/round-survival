// Round Survival

#include "PowerUps/RSPowerUpEffect.h"

#include "AbilitySystemComponent.h"
#include "Core/RSPlayerState.h"
#include "Enemies/RSZombieCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayEffect.h"
#include "Interaction/RSBarricade.h"
#include "Interaction/RSSupplyCache.h"
#include "PowerUps/RSPowerUpDefinition.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"
#include "Weapons/RSWeaponItemInstance.h"
#include "Weapons/RSWeaponSlotsComponent.h"

namespace RSPowerUpEffect
{
	/** Every player in the match: the team a power-up is picked up for. */
	template <typename Func>
	void ForEachPlayer(const FRSPowerUpContext& Context, Func&& Callback)
	{
		const AGameStateBase* GameState = Context.World ? Context.World->GetGameState() : nullptr;
		if (!GameState)
		{
			return;
		}

		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			ARSPlayerState* RSPlayerState = Cast<ARSPlayerState>(PlayerState);
			if (IsValid(RSPlayerState) && !RSPlayerState->IsOnlyASpectator())
			{
				Callback(*RSPlayerState);
			}
		}
	}
}

void URSPowerUpEffect_TeamGameplayEffect::Apply(const FRSPowerUpContext& Context) const
{
	if (!Effect)
	{
		UE_LOG(LogRS, Error, TEXT("[%s] in [%s] has no effect to apply."), *GetName(), *GetNameSafe(Context.PowerUp));
		return;
	}

	RSPowerUpEffect::ForEachPlayer(Context, [this, &Context](ARSPlayerState& PlayerState)
	{
		UAbilitySystemComponent* ASC = PlayerState.GetAbilitySystemComponent();
		if (!ASC)
		{
			return;
		}

		FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
		EffectContext.AddInstigator(Context.Collector, Context.Collector);
		EffectContext.AddSourceObject(Context.PowerUp);

		const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(Effect, 1.0f, EffectContext);
		if (!SpecHandle.IsValid())
		{
			return;
		}

		// It replicates with the spec and leaves with the effect, as a station's does.
		if (GrantedTag.IsValid())
		{
			SpecHandle.Data->DynamicGrantedTags.AddTag(GrantedTag);
		}

		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
	});
}

void URSPowerUpEffect_RefillAmmo::Apply(const FRSPowerUpContext& Context) const
{
	RSPowerUpEffect::ForEachPlayer(Context, [](ARSPlayerState& PlayerState)
	{
		const APawn* Pawn = PlayerState.GetPawn();
		const URSWeaponSlotsComponent* Slots = Pawn ? Pawn->FindComponentByClass<URSWeaponSlotsComponent>() : nullptr;
		if (!Slots)
		{
			return;
		}

		for (int32 SlotIndex = 0; SlotIndex < Slots->GetNumSlots(); ++SlotIndex)
		{
			if (URSWeaponItemInstance* Item = Slots->GetItemInSlot(SlotIndex))
			{
				Item->RefillAllAmmo();
			}
		}
	});
}

void URSPowerUpEffect_GrantScrap::Apply(const FRSPowerUpContext& Context) const
{
	if (!ScrapAwardEffect)
	{
		UE_LOG(LogRS, Error, TEXT("[%s] in [%s] has no scrap award effect."), *GetName(), *GetNameSafe(Context.PowerUp));
		return;
	}

	if (Amount <= 0)
	{
		return;
	}

	RSPowerUpEffect::ForEachPlayer(Context, [this, &Context](ARSPlayerState& PlayerState)
	{
		UAbilitySystemComponent* ASC = PlayerState.GetAbilitySystemComponent();
		if (!ASC)
		{
			return;
		}

		FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
		EffectContext.AddSourceObject(Context.PowerUp);

		const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(ScrapAwardEffect, 1.0f, EffectContext);
		if (!SpecHandle.IsValid())
		{
			return;
		}

		SpecHandle.Data->SetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Scrap, static_cast<float>(Amount));

		// The reason the HUD popup reads, as a kill's or a repair's does.
		SpecHandle.Data->AddDynamicAssetTag(RSGameplayTags::Economy_Reward_PowerUp);

		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
	});
}

void URSPowerUpEffect_KillAllEnemies::Apply(const FRSPowerUpContext& Context) const
{
	const int32 Killed = ARSZombieCharacter::KillAllUncredited(Context.World);

	UE_LOG(LogRS, Log, TEXT("[%s] killed %d enemies."), *GetNameSafe(Context.PowerUp), Killed);
}

void URSPowerUpEffect_SupplyCacheSale::Apply(const FRSPowerUpContext& Context) const
{
	float Seconds = 0.0f;
	const UGameplayEffect* Duration = LastsAsLongAs ? LastsAsLongAs->GetDefaultObject<UGameplayEffect>() : nullptr;
	if (!Context.World || !Duration || !Duration->DurationMagnitude.GetStaticMagnitudeIfPossible(1.0f, Seconds) || Seconds <= 0.0f)
	{
		UE_LOG(LogRS, Error, TEXT("[%s]: the sale needs LastsAsLongAs, an effect with a fixed duration."), *GetNameSafe(Context.PowerUp));
		return;
	}

	// Gathered first: a sale spawns caches, which must not join the iteration that started it.
	TArray<ARSSupplyCache*> Caches;
	for (TActorIterator<ARSSupplyCache> Iterator(Context.World); Iterator; ++Iterator)
	{
		if (!Iterator->IsSaleCache())
		{
			Caches.Add(*Iterator);
		}
	}

	for (ARSSupplyCache* Cache : Caches)
	{
		Cache->StartSale(Seconds);
	}
}

void URSPowerUpEffect_RepairBarricades::Apply(const FRSPowerUpContext& Context) const
{
	if (!Context.World)
	{
		return;
	}

	for (TActorIterator<ARSBarricade> Iterator(Context.World); Iterator; ++Iterator)
	{
		Iterator->RepairFully();
	}
}
