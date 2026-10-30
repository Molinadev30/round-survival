// Round Survival

#include "Debug/RSDebugSubsystem.h"

#include "AbilitySystem/Abilities/RSGameplayAbility_Downed.h"
#include "AbilitySystem/Attributes/RSEconomySet.h"
#include "AbilitySystem/Attributes/RSHealthSet.h"
#include "AbilitySystem/RSAbilitySystemComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Core/RSCharacterMovementComponent.h"
#include "Core/RSPlayerCharacter.h"
#include "Core/RSGameMode.h"
#include "Core/RSPlayerState.h"
#include "Enemies/RSDismembermentComponent.h"
#include "Enemies/RSDismembermentProfile.h"
#include "Enemies/RSEnemyDefinition.h"
#include "Enemies/RSZombieCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/RSSupplyCache.h"
#include "PowerUps/RSPowerUpDefinition.h"
#include "PowerUps/RSPowerUpDirector.h"
#include "PowerUps/RSPowerUpSettings.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"
#include "Rounds/RSRoundDirector.h"
#include "Rounds/RSSpawnSubsystem.h"
#include "Rounds/RSZombieSpawner.h"
#include "Rounds/RSZone.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include "Weapons/RSWeaponItemDefinition.h"
#include "Weapons/RSWeaponItemInstance.h"
#include "Weapons/RSWeaponSkin.h"
#include "Weapons/RSWeaponSlotsComponent.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#endif

namespace RSDebug
{
	/** How often the spawn point read-out redraws itself. */
	static constexpr float SpawnerDrawInterval = 0.25f;
}

URSDebugSubsystem* URSDebugSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<URSDebugSubsystem>() : nullptr;
}

bool URSDebugSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URSDebugSubsystem::Deinitialize()
{
	SetSpawnerDrawingEnabled(false);

	Super::Deinitialize();
}

URSRoundDirector* URSDebugSubsystem::GetRoundDirector() const
{
	const UWorld* World = GetWorld();
	ARSGameMode* GameMode = World ? World->GetAuthGameMode<ARSGameMode>() : nullptr;

	return GameMode ? GameMode->GetRoundDirector() : nullptr;
}

void URSDebugSubsystem::SetRound(int32 RoundNumber)
{
	URSRoundDirector* Director = GetRoundDirector();
	if (!Director)
	{
		UE_LOG(LogRSRounds, Warning, TEXT("RS.SetRound: no round director here. A client, or a map with no round settings."));
		return;
	}

	// Through the director's own entry rather than by writing the round number: a forced round has
	// to exercise the same path a played one does, or it proves nothing about the curve.
	Director->SkipToRound(RoundNumber);
}

void URSDebugSubsystem::AddScrap(float Amount)
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		ARSPlayerState* RSPlayerState = Cast<ARSPlayerState>(PlayerState);
		URSAbilitySystemComponent* ASC = RSPlayerState ? RSPlayerState->GetRSAbilitySystemComponent() : nullptr;
		if (!ASC)
		{
			continue;
		}

		// Straight onto the attribute, deliberately: a reward effect would pass through
		// ScrapGainMultiplier and hand back some other number than the one that was typed. A cheat
		// that cannot give you exactly 5,000 scrap is useless for pricing a wall locker.
		ASC->ApplyModToAttribute(URSEconomySet::GetScrapAttribute(), EGameplayModOp::Additive, Amount);

		UE_LOG(LogRSRounds, Log, TEXT("RS.AddScrap: %+.0f to [%s]."), Amount, *GetNameSafe(PlayerState));
	}
}

int32 URSDebugSubsystem::KillAll()
{
	const int32 Killed = ARSZombieCharacter::KillAllUncredited(GetWorld());

	UE_LOG(LogRSRounds, Log, TEXT("RS.KillAll: killed %d."), Killed);

	return Killed;
}

int32 URSDebugSubsystem::SpawnHorde(int32 Count)
{
	URSRoundDirector* Director = GetRoundDirector();
	if (!Director)
	{
		UE_LOG(LogRSRounds, Warning, TEXT("RS.SpawnHorde: no round director here. A client, or a map with no round settings."));
		return 0;
	}

	// Through the director, so the horde is scaled to the round it is standing in. A horde spawned
	// past the director would be sixty-four enemies with the wrong health and the wrong speed, and
	// the frame time it produced would not be the frame time of a round.
	const int32 Released = Director->SpawnHorde(Count);

	UE_LOG(LogRSRounds, Log, TEXT("RS.SpawnHorde: asked for %d, released %d, %d now alive."),
		Count, Released, Director->GetAliveCount());

	return Released;
}

int32 URSDebugSubsystem::GiveWeapon(const FString& Name)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogRS, Warning, TEXT("RS.GiveWeapon: server only."));
		return 0;
	}

	// Found by name through the asset registry rather than by a hardcoded path, so a weapon added in
	// 7.5c is reachable from the console the moment its asset exists.
	const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	TArray<FAssetData> Candidates;
	AssetRegistry.GetAssetsByClass(URSWeaponItemDefinition::StaticClass()->GetClassPathName(), Candidates, /*bSearchSubClasses=*/ true);

	// No argument falls through to the listing below rather than matching everything.
	const FAssetData* Match = Name.IsEmpty() ? nullptr : Candidates.FindByPredicate([&Name](const FAssetData& Asset)
	{
		return Asset.AssetName.ToString().Contains(Name);
	});

	const URSWeaponItemDefinition* Definition = Match ? Cast<URSWeaponItemDefinition>(Match->GetAsset()) : nullptr;
	if (!Definition)
	{
		FString Known;
		for (const FAssetData& Asset : Candidates)
		{
			Known += TEXT(" ") + Asset.AssetName.ToString();
		}

		UE_LOG(LogRS, Warning, TEXT("RS.GiveWeapon: no weapon item matches '%s'. Known:%s"), *Name, *Known);
		return 0;
	}

	int32 Given = 0;

	for (TActorIterator<ARSPlayerCharacter> It(World); It; ++It)
	{
		if (URSWeaponSlotsComponent* WeaponSlots = It->GetWeaponSlots())
		{
			if (WeaponSlots->GiveWeapon(Definition))
			{
				++Given;
			}
		}
	}

	UE_LOG(LogRS, Log, TEXT("RS.GiveWeapon: [%s] given to %d player(s)."), *GetNameSafe(Definition), Given);
	return Given;
}

int32 URSDebugSubsystem::SetWeaponSkin(const FString& Name)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogRS, Warning, TEXT("RS.SetWeaponSkin: server only."));
		return 0;
	}

	const URSWeaponSkin* Skin = nullptr;

	// An empty name is a request, not a typo: it takes the override off and shows the item's own skin.
	if (!Name.IsEmpty())
	{
		const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		TArray<FAssetData> Candidates;
		AssetRegistry.GetAssetsByClass(URSWeaponSkin::StaticClass()->GetClassPathName(), Candidates, /*bSearchSubClasses=*/ true);

		const FAssetData* Match = Candidates.FindByPredicate([&Name](const FAssetData& Asset)
		{
			return Asset.AssetName.ToString().Contains(Name);
		});

		Skin = Match ? Cast<URSWeaponSkin>(Match->GetAsset()) : nullptr;
		if (!Skin)
		{
			FString Known;
			for (const FAssetData& Asset : Candidates)
			{
				Known += TEXT(" ") + Asset.AssetName.ToString();
			}

			UE_LOG(LogRS, Warning, TEXT("RS.SetWeaponSkin: no weapon skin matches '%s'. Known:%s"), *Name, *Known);
			return 0;
		}
	}

	int32 Changed = 0;

	for (TActorIterator<ARSPlayerCharacter> It(World); It; ++It)
	{
		const URSWeaponSlotsComponent* WeaponSlots = It->GetWeaponSlots();
		if (URSWeaponItemInstance* Item = WeaponSlots ? WeaponSlots->GetActiveItem() : nullptr)
		{
			Item->SetSkinOverride(Skin);
			++Changed;
		}
	}

	UE_LOG(LogRS, Log, TEXT("RS.SetWeaponSkin: [%s] on %d weapon(s)."), Skin ? *GetNameSafe(Skin) : TEXT("item default"), Changed);
	return Changed;
}

bool URSDebugSubsystem::SpawnPowerUp(const FString& Name)
{
	const UWorld* World = GetWorld();
	const ARSGameMode* GameMode = World ? World->GetAuthGameMode<ARSGameMode>() : nullptr;
	URSPowerUpDirector* Director = GameMode ? GameMode->GetPowerUpDirector() : nullptr;
	const URSPowerUpSettings* Settings = Director ? Director->GetSettings() : nullptr;
	if (!Settings)
	{
		UE_LOG(LogRS, Warning, TEXT("RS.SpawnPowerUp: server only, and the game mode needs power-up settings."));
		return false;
	}

	const URSPowerUpDefinition* PowerUp = Name.IsEmpty() ? nullptr : Settings->FindPowerUp(Name);
	if (!PowerUp)
	{
		FString Known;
		for (const FRSPowerUpDrop& Drop : Settings->PowerUps)
		{
			Known += FString::Printf(TEXT(" %s"), *GetNameSafe(Drop.PowerUp));
		}

		UE_LOG(LogRS, Warning, TEXT("RS.SpawnPowerUp: no power-up matches '%s'. Known:%s"), *Name, *Known);
		return false;
	}

	// Three metres in front of the host, where they can walk onto it.
	const APlayerController* Host = World->GetFirstPlayerController();
	const APawn* Pawn = Host ? Host->GetPawn() : nullptr;
	if (!Pawn)
	{
		UE_LOG(LogRS, Warning, TEXT("RS.SpawnPowerUp: the host has no pawn to drop it in front of."));
		return false;
	}

	const FVector Forward = FRotator(0.0f, Host->GetControlRotation().Yaw, 0.0f).Vector();
	return Director->SpawnPowerUp(PowerUp, Pawn->GetActorLocation() + Forward * 300.0f) != nullptr;
}

int32 URSDebugSubsystem::SeverNearestEnemy(const FString& PartName)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogRS, Warning, TEXT("RS.Enemy.Sever: server only."));
		return 0;
	}

	const bool bAll = PartName.Equals(TEXT("All"), ESearchCase::IgnoreCase);
	const int64 PartValue = bAll ? INDEX_NONE : StaticEnum<ERSBodyPart>()->GetValueByNameString(PartName);
	if (!bAll && (PartValue == INDEX_NONE || PartValue >= static_cast<int64>(ERSBodyPart::MAX)))
	{
		UE_LOG(LogRS, Warning, TEXT("RS.Enemy.Sever: '%s' is not a part. Head, ArmLeft, ArmRight, LegLeft, LegRight or All."), *PartName);
		return 0;
	}

	const APlayerController* Host = World->GetFirstPlayerController();
	const APawn* HostPawn = Host ? Host->GetPawn() : nullptr;
	if (!HostPawn)
	{
		UE_LOG(LogRS, Warning, TEXT("RS.Enemy.Sever: the host has no pawn to measure from."));
		return 0;
	}

	ARSZombieCharacter* Nearest = nullptr;
	float NearestDistanceSq = TNumericLimits<float>::Max();
	for (TActorIterator<ARSZombieCharacter> It(World); It; ++It)
	{
		if (It->IsDead() || It->IsInPool())
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(It->GetActorLocation(), HostPawn->GetActorLocation());
		if (DistanceSq < NearestDistanceSq)
		{
			Nearest = *It;
			NearestDistanceSq = DistanceSq;
		}
	}

	URSDismembermentComponent* Dismemberment = Nearest ? Nearest->GetDismemberment() : nullptr;
	const URSDismembermentProfile* Profile = Nearest && Nearest->GetEnemyDefinition() ? Nearest->GetEnemyDefinition()->Dismemberment.Get() : nullptr;
	if (!Dismemberment || !Profile)
	{
		UE_LOG(LogRS, Warning, TEXT("RS.Enemy.Sever: no living enemy with a dismemberment profile."));
		return 0;
	}

	int32 Severed = 0;
	for (const FRSBodyPartRule& Rule : Profile->Parts)
	{
		if (!bAll && static_cast<int64>(Rule.Part) != PartValue)
		{
			continue;
		}

		if (Rule.bOnlyOnKillingBlow)
		{
			if (!bAll)
			{
				UE_LOG(LogRS, Warning, TEXT("RS.Enemy.Sever: %s only comes off with the killing blow. Kill it with a shot there."), *PartName);
			}
			continue;
		}

		Severed += Dismemberment->SeverPart(Rule.Part) ? 1 : 0;
	}

	UE_LOG(LogRS, Log, TEXT("RS.Enemy.Sever: %d part(s) off [%s], mask now 0x%02x."),
		Severed, *GetNameSafe(Nearest), Dismemberment->GetSeveredMask());
	return Severed;
}

int32 URSDebugSubsystem::ForceSupplyCacheMove()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogRS, Warning, TEXT("RS.SupplyCacheMove: server only."));
		return 0;
	}

	int32 Caches = 0;
	for (TActorIterator<ARSSupplyCache> It(World); It; ++It)
	{
		It->ForceMoveOnNextRoll();
		++Caches;
	}

	UE_LOG(LogRS, Log, TEXT("RS.SupplyCacheMove: the next roll moves %d Supply Cache(s)."), Caches);
	return Caches;
}

bool URSDebugSubsystem::DownPlayer(int32 PlayerIndex)
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogRS, Warning, TEXT("RS.DownPlayer: server only."));
		return false;
	}

	const ARSPlayerState* PlayerState = GameState->PlayerArray.IsValidIndex(PlayerIndex)
		? Cast<ARSPlayerState>(GameState->PlayerArray[PlayerIndex]) : nullptr;
	URSAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetRSAbilitySystemComponent() : nullptr;
	const URSHealthSet* HealthSet = ASC ? ASC->GetSet<URSHealthSet>() : nullptr;
	if (!HealthSet)
	{
		UE_LOG(LogRS, Warning, TEXT("RS.DownPlayer: no player %d (%d in the game)."), PlayerIndex, GameState->PlayerArray.Num());
		return false;
	}

	if (ASC->HasMatchingGameplayTag(RSGameplayTags::State_Downed) || ASC->HasMatchingGameplayTag(RSGameplayTags::State_Dead))
	{
		UE_LOG(LogRS, Warning, TEXT("RS.DownPlayer: [%s] is already down."), *GetNameSafe(PlayerState));
		return false;
	}

	// Exactly the health they have, written to the Damage meta attribute: the same path an enemy's
	// blow takes from the health set onwards, with the damage execution's rules left out because
	// they are about who may hurt whom, and this is nobody.
	UGameplayEffect* DownEffect = NewObject<UGameplayEffect>(GetTransientPackage(), FName(TEXT("RSDebugDown")));
	DownEffect->DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo& Modifier = DownEffect->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = URSHealthSet::GetDamageAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FScalableFloat(HealthSet->GetHealth());

	ASC->ApplyGameplayEffectToSelf(DownEffect, 1.0f, ASC->MakeEffectContext());

	UE_LOG(LogRS, Log, TEXT("RS.DownPlayer: [%s]."), *GetNameSafe(PlayerState));
	return true;
}

bool URSDebugSubsystem::RefillSprint(int32 PlayerIndex)
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogRS, Warning, TEXT("RS.RefillSprint: server only."));
		return false;
	}

	const APlayerState* PlayerState = GameState->PlayerArray.IsValidIndex(PlayerIndex) ? GameState->PlayerArray[PlayerIndex].Get() : nullptr;
	const ACharacter* Character = PlayerState ? Cast<ACharacter>(PlayerState->GetPawn()) : nullptr;
	URSCharacterMovementComponent* Movement = Character ? Cast<URSCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	if (!Movement)
	{
		UE_LOG(LogRS, Warning, TEXT("RS.RefillSprint: no player %d with a pawn (%d in the game)."), PlayerIndex, GameState->PlayerArray.Num());
		return false;
	}

	Movement->SetSprintStamina(FRSSprintStamina());

	UE_LOG(LogRS, Log, TEXT("RS.RefillSprint: [%s], on the server's copy only."), *GetNameSafe(PlayerState));
	return true;
}

bool URSDebugSubsystem::RevivePlayer(int32 PlayerIndex)
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogRS, Warning, TEXT("RS.RevivePlayer: server only."));
		return false;
	}

	const APlayerState* PlayerState = GameState->PlayerArray.IsValidIndex(PlayerIndex) ? GameState->PlayerArray[PlayerIndex].Get() : nullptr;
	ARSPlayerCharacter* Player = PlayerState ? PlayerState->GetPawn<ARSPlayerCharacter>() : nullptr;
	if (!Player || !Player->IsDowned())
	{
		UE_LOG(LogRS, Warning, TEXT("RS.RevivePlayer: player %d is not down (%d in the game)."), PlayerIndex, GameState->PlayerArray.Num());
		return false;
	}

	IRSInteractable::Execute_Interact(Player, nullptr);

	UE_LOG(LogRS, Log, TEXT("RS.RevivePlayer: [%s]."), *GetNameSafe(PlayerState));
	return true;
}

bool URSDebugSubsystem::BleedOut(int32 PlayerIndex)
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogRS, Warning, TEXT("RS.BleedOut: server only."));
		return false;
	}

	const ARSPlayerState* PlayerState = GameState->PlayerArray.IsValidIndex(PlayerIndex)
		? Cast<ARSPlayerState>(GameState->PlayerArray[PlayerIndex]) : nullptr;
	URSAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetRSAbilitySystemComponent() : nullptr;
	if (!ASC || !ASC->HasMatchingGameplayTag(RSGameplayTags::State_Downed))
	{
		UE_LOG(LogRS, Warning, TEXT("RS.BleedOut: player %d is not down (%d in the game)."), PlayerIndex, GameState->PlayerArray.Num());
		return false;
	}

	// The passive's one instance, which is the one holding the clock. Granted as a Blueprint child,
	// so matched by IsA rather than by FindAbilitySpecFromClass's exact class.
	URSGameplayAbility_Downed* Downed = nullptr;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->IsA<URSGameplayAbility_Downed>())
		{
			Downed = Cast<URSGameplayAbility_Downed>(Spec.GetPrimaryInstance());
			break;
		}
	}
	if (!Downed)
	{
		UE_LOG(LogRS, Warning, TEXT("RS.BleedOut: [%s] has no downed ability."), *GetNameSafe(PlayerState));
		return false;
	}

	Downed->BleedOutNow();

	UE_LOG(LogRS, Log, TEXT("RS.BleedOut: [%s]."), *GetNameSafe(PlayerState));
	return true;
}

void URSDebugSubsystem::RestartMatch()
{
	const UWorld* World = GetWorld();
	ARSGameMode* GameMode = World ? World->GetAuthGameMode<ARSGameMode>() : nullptr;
	if (!GameMode)
	{
		UE_LOG(LogRS, Warning, TEXT("RS.RestartMatch: server only."));
		return;
	}

	GameMode->RestartMatch();
}

void URSDebugSubsystem::SetSpawnerDrawingEnabled(bool bEnabled)
{
	bDrawSpawners = bEnabled;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (bEnabled)
	{
		World->GetTimerManager().SetTimer(DrawTimerHandle, this, &ThisClass::DrawSpawners,
			RSDebug::SpawnerDrawInterval, /*bLoop=*/ true, /*InFirstDelay=*/ 0.0f);
	}
	else
	{
		World->GetTimerManager().ClearTimer(DrawTimerHandle);
	}
}

void URSDebugSubsystem::DrawSpawners()
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	const URSSpawnSubsystem* SpawnSubsystem = URSSpawnSubsystem::Get(this);
	if (!World || !SpawnSubsystem)
	{
		return;
	}

	// Slightly longer than the interval, so the drawing never blinks between refreshes.
	const float Lifetime = RSDebug::SpawnerDrawInterval * 1.1f;

	for (const TObjectPtr<ARSZombieSpawner>& Spawner : SpawnSubsystem->GetSpawners())
	{
		if (!IsValid(Spawner))
		{
			continue;
		}

		const FVector Ground = Spawner->GetGroundSpawnTransform().GetLocation();
		const bool bEligible = Spawner->IsEligible();
		const FColor Color = bEligible ? FColor::Green : FColor(255, 80, 80);

		DrawDebugSphere(World, Ground + FVector(0.0f, 0.0f, 45.0f), 45.0f, 12, Color, /*bPersistentLines=*/ false, Lifetime);
		DrawDebugDirectionalArrow(World, Ground, Ground + Spawner->GetActorForwardVector() * 150.0f, 40.0f, Color,
			/*bPersistentLines=*/ false, Lifetime);

		// Each of the three reasons a point can be unavailable, named. They look identical from
		// outside — the point simply never produces anything — and they are fixed differently.
		const ARSZone* Zone = Spawner->GetZone();
		const FString Label = FString::Printf(TEXT("%s\nzone %s\n%s%s"),
			*Spawner->GetActorNameOrLabel(),
			Zone ? *FString::Printf(TEXT("%s (%s)"), *Zone->GetActorNameOrLabel(), Zone->IsOpen() ? TEXT("open") : TEXT("closed")) : TEXT("none"),
			Spawner->IsOnCooldown() ? TEXT("cooling down\n") : TEXT(""),
			Spawner->IsBlocked() ? TEXT("blocked") : TEXT(""));

		DrawDebugString(World, Ground + FVector(0.0f, 0.0f, 140.0f), Label, /*TestBaseActor=*/ nullptr, Color,
			/*Duration=*/ RSDebug::SpawnerDrawInterval, /*bDrawShadow=*/ true);
	}
#endif
}

#if !UE_BUILD_SHIPPING

namespace RSDebugCommands
{
	static int32 ParseInt(const TArray<FString>& Args, int32 Index, int32 Fallback)
	{
		return Args.IsValidIndex(Index) ? FCString::Atoi(*Args[Index]) : Fallback;
	}

	static void SetRound(const TArray<FString>& Args, UWorld* World)
	{
		if (URSDebugSubsystem* Debug = World ? World->GetSubsystem<URSDebugSubsystem>() : nullptr)
		{
			Debug->SetRound(ParseInt(Args, 0, 1));
		}
	}

	static void AddScrap(const TArray<FString>& Args, UWorld* World)
	{
		if (URSDebugSubsystem* Debug = World ? World->GetSubsystem<URSDebugSubsystem>() : nullptr)
		{
			Debug->AddScrap(static_cast<float>(ParseInt(Args, 0, 1000)));
		}
	}

	static void KillAll(const TArray<FString>& Args, UWorld* World)
	{
		if (URSDebugSubsystem* Debug = World ? World->GetSubsystem<URSDebugSubsystem>() : nullptr)
		{
			Debug->KillAll();
		}
	}

	static void SpawnHorde(const TArray<FString>& Args, UWorld* World)
	{
		if (URSDebugSubsystem* Debug = World ? World->GetSubsystem<URSDebugSubsystem>() : nullptr)
		{
			Debug->SpawnHorde(FMath::Max(ParseInt(Args, 0, 64), 1));
		}
	}

	static void SeverEnemy(const TArray<FString>& Args, UWorld* World)
	{
		if (URSDebugSubsystem* Debug = World ? World->GetSubsystem<URSDebugSubsystem>() : nullptr)
		{
			Debug->SeverNearestEnemy(Args.IsValidIndex(0) ? Args[0] : FString(TEXT("All")));
		}
	}

	static void ShowSpawners(const TArray<FString>& Args, UWorld* World)
	{
		URSDebugSubsystem* Debug = World ? World->GetSubsystem<URSDebugSubsystem>() : nullptr;
		if (!Debug)
		{
			return;
		}

		// No argument toggles, an argument sets. Typed mid-round, the toggle is what you want.
		const bool bEnabled = Args.Num() > 0 ? ParseInt(Args, 0, 1) != 0 : !Debug->IsSpawnerDrawingEnabled();
		Debug->SetSpawnerDrawingEnabled(bEnabled);
	}

	static void GiveWeapon(const TArray<FString>& Args, UWorld* World)
	{
		if (URSDebugSubsystem* Debug = World ? World->GetSubsystem<URSDebugSubsystem>() : nullptr)
		{
			Debug->GiveWeapon(Args.IsValidIndex(0) ? Args[0] : FString());
		}
	}

	static void SetWeaponSkin(const TArray<FString>& Args, UWorld* World)
	{
		if (URSDebugSubsystem* Debug = World ? World->GetSubsystem<URSDebugSubsystem>() : nullptr)
		{
			Debug->SetWeaponSkin(Args.IsValidIndex(0) ? Args[0] : FString());
		}
	}

	static void DownPlayer(const TArray<FString>& Args, UWorld* World)
	{
		if (URSDebugSubsystem* Debug = World ? World->GetSubsystem<URSDebugSubsystem>() : nullptr)
		{
			Debug->DownPlayer(ParseInt(Args, 0, 0));
		}
	}

	static void SpawnPowerUp(const TArray<FString>& Args, UWorld* World)
	{
		if (URSDebugSubsystem* Debug = World ? World->GetSubsystem<URSDebugSubsystem>() : nullptr)
		{
			Debug->SpawnPowerUp(Args.IsValidIndex(0) ? Args[0] : FString());
		}
	}

	static void SupplyCacheMove(const TArray<FString>& Args, UWorld* World)
	{
		if (URSDebugSubsystem* Debug = World ? World->GetSubsystem<URSDebugSubsystem>() : nullptr)
		{
			Debug->ForceSupplyCacheMove();
		}
	}

	static void RevivePlayer(const TArray<FString>& Args, UWorld* World)
	{
		if (URSDebugSubsystem* Debug = World ? World->GetSubsystem<URSDebugSubsystem>() : nullptr)
		{
			Debug->RevivePlayer(ParseInt(Args, 0, 0));
		}
	}

	static void BleedOut(const TArray<FString>& Args, UWorld* World)
	{
		if (URSDebugSubsystem* Debug = World ? World->GetSubsystem<URSDebugSubsystem>() : nullptr)
		{
			Debug->BleedOut(ParseInt(Args, 0, 0));
		}
	}

	static void RefillSprint(const TArray<FString>& Args, UWorld* World)
	{
		if (URSDebugSubsystem* Debug = World ? World->GetSubsystem<URSDebugSubsystem>() : nullptr)
		{
			Debug->RefillSprint(ParseInt(Args, 0, 0));
		}
	}

	static void RestartMatch(const TArray<FString>& Args, UWorld* World)
	{
		if (URSDebugSubsystem* Debug = World ? World->GetSubsystem<URSDebugSubsystem>() : nullptr)
		{
			Debug->RestartMatch();
		}
	}

	static void PoolStats(const TArray<FString>& Args, UWorld* World)
	{
		URSSpawnSubsystem* SpawnSubsystem = URSSpawnSubsystem::Get(World);
		if (!SpawnSubsystem)
		{
			return;
		}

		SpawnSubsystem->LogPoolStats();
		if (Args.IsValidIndex(0) && Args[0].Equals(TEXT("reset"), ESearchCase::IgnoreCase))
		{
			SpawnSubsystem->ResetPoolStats();
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs GSetRound(
		TEXT("RS.SetRound"),
		TEXT("Jumps to a round (default 1), clearing whatever is alive. Goes through the round director."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetRound), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs GAddScrap(
		TEXT("RS.AddScrap"),
		TEXT("Adds scrap to every player (default 1000). Exact amount: income multipliers are not applied."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AddScrap), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs GKillAll(
		TEXT("RS.KillAll"),
		TEXT("Kills every living enemy through the damage pipeline. Pays no scrap."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KillAll), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs GSpawnHorde(
		TEXT("RS.SpawnHorde"),
		TEXT("Releases N enemies at once (default 64), scaled to the current round, outside the quota."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SpawnHorde), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs GGiveWeapon(
		TEXT("RS.GiveWeapon"),
		TEXT("Gives every player the weapon item whose asset name contains the argument, as a Wall Locker would. No argument lists them."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GiveWeapon), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs GSetWeaponSkin(
		TEXT("RS.SetWeaponSkin"),
		TEXT("Puts the weapon skin whose asset name contains the argument on every player's drawn weapon, as the Weapon Forge will. No argument goes back to the weapon's own."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetWeaponSkin), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs GDownPlayer(
		TEXT("RS.DownPlayer"),
		TEXT("Puts a player down through the damage pipeline: by index in the player list, default 0, the host. Type it on the server."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DownPlayer), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs GSpawnPowerUp(
		TEXT("RS.SpawnPowerUp"),
		TEXT("Drops the power-up whose asset name contains the argument three metres in front of the host, as a kill would. No argument lists them. Type it on the server."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SpawnPowerUp), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs GRevivePlayer(
		TEXT("RS.RevivePlayer"),
		TEXT("Revives a downed player as a completed revive would: by index in the player list, default 0, the host. Type it on the server."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RevivePlayer), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs GBleedOut(
		TEXT("RS.BleedOut"),
		TEXT("Ends a downed player's bleed-out now, as the clock running out would: by index in the player list, default 0, the host. Type it on the server."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BleedOut), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs GRefillSprint(
		TEXT("RS.RefillSprint"),
		TEXT("Fills a player's sprint on the server's copy only, to put it out of step with their client: by index in the player list, default 0, the host. Type it on the server."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RefillSprint), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs GSupplyCacheMove(
		TEXT("RS.SupplyCacheMove"),
		TEXT("Makes the next roll paid for at each Supply Cache draw nothing and move it to another of its spots, refunded. Type it on the server."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SupplyCacheMove), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs GRestartMatch(
		TEXT("RS.RestartMatch"),
		TEXT("Starts the match over from round one, as the host's restart after a game over does. Type it on the server."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RestartMatch), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs GPoolStats(
		TEXT("RS.PoolStats"),
		TEXT("Logs the enemy pool's contents and what reusing, spawning, pooling and destroying an enemy has cost. 'reset' then starts the figures over. Type it on the server."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PoolStats), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs GSeverEnemy(
		TEXT("RS.Enemy.Sever"),
		TEXT("Takes a part off the living enemy nearest the host: Head, ArmLeft, ArmRight, LegLeft, LegRight, or All (default). The head only comes off with a killing blow and is refused. Type it on the server."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SeverEnemy), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs GShowSpawners(
		TEXT("RS.ShowSpawners"),
		TEXT("Draws every spawn point and why it is or is not available. No argument toggles."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ShowSpawners), ECVF_Cheat);
}

#endif
