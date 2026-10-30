// Round Survival

#include "Enemies/RSZombieCharacter.h"

#include "AbilitySystem/Attributes/RSCombatSet.h"
#include "AbilitySystem/Attributes/RSHealthSet.h"
#include "AbilitySystem/RSAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/RSPlayerState.h"
#include "EngineUtils.h"
#include "Enemies/RSDismembermentComponent.h"
#include "Enemies/RSEnemyDefinition.h"
#include "Enemies/RSZombieController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "Interaction/RSBarricade.h"
#include "MotionWarpingComponent.h"
#include "Net/UnrealNetwork.h"
#include "Physics/RSPhysicalMaterialWithTags.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"
#include "Performance/RSSignificanceManager.h"
#include "Rounds/RSSpawnSubsystem.h"

namespace RSZombie
{
	/** A living enemy's collision, as named profiles in DefaultEngine.ini. */
	static const FName CapsuleProfile(TEXT("RSEnemyCapsule"));
	static const FName MeshProfile(TEXT("RSEnemyMesh"));
}

ARSZombieCharacter::ARSZombieCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystemComponent = CreateDefaultSubobject<URSAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// Minimal: no gameplay effect replicates to any client, only attribute values, tags and cues.
	// Nobody predicts on an enemy, so the effect list would be bandwidth spent 64 times over on
	// data no client ever reads.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	HealthSet = CreateDefaultSubobject<URSHealthSet>(TEXT("HealthSet"));
	CombatSet = CreateDefaultSubobject<URSCombatSet>(TEXT("CombatSet"));

	// Well below the default 100 Hz. Movement between updates is smoothed by the character
	// movement component, and a shambling enemy does not need a player's precision.
	SetNetUpdateFrequency(20.0f);

	// The AI controller only ever exists on the server; placed and spawned enemies both need one.
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Set here rather than on the Blueprint: an enemy without its brain is not a variant of an
	// enemy, it is a bug. Which behaviour tree that brain runs is the part that stays content.
	AIControllerClass = ARSZombieController::StaticClass();

	// Movement is driven by the movement component and the animations are in place. Root motion
	// would move the pawn from the animation instead, which does not replicate.
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->bUseControllerDesiredRotation = false;
	bUseControllerRotationYaw = false;

	// Slower than the default 360 degrees a second. A body that pivots instantly is what makes a
	// chasing enemy feel like a homing missile; at this rate stepping around one buys a moment,
	// which is the same moment the melee cone is built around.
	Movement->RotationRate = FRotator(0.0f, 250.0f, 0.0f);

	// Crawling is the engine's crouch: a shorter capsule with the mesh kept on the floor, a speed of
	// its own, and `bIsCrouched` replicated to every client. URSGameplayAbility_FallToCrawl starts
	// it; nothing in this game crouches an enemy for any other reason. The height is the crawl
	// pose's, the pelvis 15 cm off the floor and the back not much above it; the radius stays, so
	// the capsule is never shorter than it is wide.
	Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
	Movement->SetCrouchedHalfHeight(30.0f);
	// A crawler stopped at the top of a step for good would read as a bug, not as a crawl.
	Movement->bCanWalkOffLedgesWhenCrouching = true;

	MotionWarping = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarping"));

	Dismemberment = CreateDefaultSubobject<URSDismembermentComponent>(TEXT("Dismemberment"));

	// Narrower than a person, deliberately. Three attackers share a 115 cm window opening at 45 cm
	// centres, and at the 34 cm `ACharacter` default their capsules overlapped by 23 cm — enough
	// that they spent the fight shoving one another out of a slot instead of tearing boards. At
	// 24 they stand shoulder to shoulder with 3 cm to spare.
	//
	// This is only affordable because of the two lines below: the capsule is not what a bullet
	// hits, so shrinking it does not make an enemy harder to kill or move a single hit zone. What
	// it does cost is that a crowd is less of a wall — a player squeezes between two enemies more
	// easily than before, which is a deliberate trade and not a side effect.
	//
	// Path queries still run against a navmesh generated for the project's 34 cm agent, so this
	// does not let an enemy route through a gap the navmesh never opened.
	GetCapsuleComponent()->InitCapsuleSize(24.0f, 92.0f);

	// Bullets hit the mesh, not the capsule. The capsule is a coarse movement shape; hit zones
	// live on the physics asset bodies, and a capsule that blocked the weapon trace would stop
	// every shot before it reached a head. Both are named profiles (DefaultEngine.ini) so that
	// bringing a pooled body back sets each in one call: on a mesh with a body per bone, every
	// separate collision change is a separate physics update.
	GetCapsuleComponent()->SetCollisionProfileName(RSZombie::CapsuleProfile);

	USkeletalMeshComponent* MeshComponent = GetMesh();
	MeshComponent->SetCollisionProfileName(RSZombie::MeshProfile);

	// A bullet hole on the wall behind would otherwise print onto every enemy walking through it.
	MeshComponent->bReceivesDecals = false;

	// Update rate optimisation: distant enemies evaluate animation less often and interpolate.
	// This is the largest per-enemy animation saving available without touching the graph.
	MeshComponent->bEnableUpdateRateOptimizations = true;

	// Bones must stay current when the enemy is off screen. The server validates hits against the
	// physics asset, and on a listen server most enemies are not on the host's screen; a pose
	// frozen at the last rendered frame would put the head somewhere it no longer is. This is the
	// High tier; URSSignificanceManager scales it back only for an enemy no player can see.
	MeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

UAbilitySystemComponent* ARSZombieCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

int32 ARSZombieCharacter::KillAllUncredited(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return 0;
	}

	// An effect built here rather than an asset. It exists for one frame and has one modifier, and
	// an asset for it would be content nobody tunes.
	UGameplayEffect* KillEffect = NewObject<UGameplayEffect>(GetTransientPackage(), FName(TEXT("RSUncreditedKill")));
	KillEffect->DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo& Modifier = KillEffect->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = URSHealthSet::GetDamageAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FScalableFloat(TNumericLimits<float>::Max() * 0.5f);

	int32 Killed = 0;
	for (TActorIterator<ARSZombieCharacter> Iterator(World); Iterator; ++Iterator)
	{
		ARSZombieCharacter* Enemy = *Iterator;
		URSAbilitySystemComponent* ASC = Enemy && !Enemy->IsDead() ? Enemy->GetRSAbilitySystemComponent() : nullptr;
		if (!ASC)
		{
			continue;
		}

		// Through the Damage meta attribute, so the real death runs: the tag, the event, the
		// ragdoll and the round's count of what is still alive. The instigator is the enemy
		// itself, so nobody is paid scrap for a kill they did not make.
		ASC->ApplyGameplayEffectToSelf(KillEffect, 1.0f, ASC->MakeEffectContext());
		++Killed;
	}

	return Killed;
}

float ARSZombieCharacter::GetHealth() const
{
	return HealthSet ? HealthSet->GetHealth() : 0.0f;
}

float ARSZombieCharacter::GetMaxHealth() const
{
	return HealthSet ? HealthSet->GetMaxHealth() : 0.0f;
}

void ARSZombieCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	check(AbilitySystemComponent);

	// Owner and avatar are both this pawn, and both exist already. That is the whole reason the
	// player needs two call sites for this and the enemy needs one: there is no player state to
	// wait for, on the server or on any client.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	// Bound on every machine. The server pays out scrap from these callbacks; a client reaches the
	// same OnOutOfHealth through OnRep_Health and only plays the ragdoll.
	check(HealthSet);
	HealthSet->OnHealthChanged.AddUObject(this, &ThisClass::HandleHealthChanged);
	HealthSet->OnOutOfHealth.AddUObject(this, &ThisClass::HandleOutOfHealth);

	// The set reports the change before it reports running out, so a part is decided before the
	// death starts: the killing hit can still take one.
	check(Dismemberment);
	HealthSet->OnHealthChanged.AddUObject(Dismemberment.Get(), &URSDismembermentComponent::HandleHealthChanged);

	check(CombatSet);
	CombatSet->OnMoveSpeedMultiplierChanged.AddUObject(this, &ThisClass::HandleMoveSpeedMultiplierChanged);

	// The tier a placed enemy was authored with, or the default one a spawned enemy starts on until
	// the round director says otherwise. Either way movement must not run one frame at the engine
	// default of 600, which is the player's speed.
	RefreshMovementSpeed();
}

void ARSZombieCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		// Spawned straight into the world. A prewarmed body is put away again in the same frame, which lets go of it.
		if (URSSignificanceManager* Significance = URSSignificanceManager::Get(this))
		{
			Significance->RegisterEnemy(this);
		}
	}
	else
	{
		// A client only shoots what it renders, so the engine's rendered test is the right saving for its copies —
		// unlike the server's, whose renderer is only the host's view (URSSignificanceManager).
		GetMesh()->VisibilityBasedAnimTickOption = GetDefault<URSSignificanceSettings>()->ClientAnimTickOption;
	}
}

void ARSZombieCharacter::SetSignificanceTier(ERSSignificanceTier NewTier)
{
	if (NewTier == SignificanceTier)
	{
		return;
	}
	SignificanceTier = NewTier;

	const ARSZombieCharacter* Defaults = GetClass()->GetDefaultObject<ARSZombieCharacter>();
	const URSSignificanceSettings* Settings = GetDefault<URSSignificanceSettings>();

	// High is not a setting: an enemy a player can see or is close to is one whose shots are validated against its
	// pose here, so it animates and refreshes its bones exactly as the class says.
	GetMesh()->VisibilityBasedAnimTickOption = NewTier == ERSSignificanceTier::High
		? Defaults->GetMesh()->VisibilityBasedAnimTickOption
		: Settings->UnseenAnimTickOption;
}

void ARSZombieCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARSZombieCharacter, MoveTier);
	DOREPLIFETIME(ARSZombieCharacter, PoolState);
}

bool ARSZombieCharacter::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	// A pooled body is hidden and collides with nothing, which the engine reads as irrelevant to
	// everyone: its channel would close, every client would destroy its copy, and spawn it again
	// when the body came back. Kept relevant, the channel goes dormant instead, and the copies wait
	// in each client's own pool. Dormant, it costs nothing to keep.
	if (PoolState.bInPool)
	{
		return true;
	}

	return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

void ARSZombieCharacter::OnRep_PoolState()
{
	const bool bNewLife = PoolState.Life != AppliedPoolLife;
	AppliedPoolLife = PoolState.Life;

	if (PoolState.bInPool)
	{
		StowBody();
		return;
	}

	// A new life, and the last one may still be ragdolling here: undone. The pool hands the most
	// recently pooled body out first, so a client can miss `bInPool` altogether and only see the
	// count move. Living, too: the server writes the round's health in the same frame it brings
	// the body back, so it arrives in the same update and is applied before this runs. That keeps
	// a corpse a client sees for the first time, whose count has moved on, lying down.
	if (bNewLife && GetHealth() > 0.0f)
	{
		bDead = false;
		UnstowBody();
	}
}

ARSBarricade* ARSZombieCharacter::GetEntryBarricade() const
{
	return EntryBarricade.Get();
}

void ARSZombieCharacter::SetEntryBarricade(ARSBarricade* NewBarricade)
{
	if (!HasAuthority())
	{
		UE_LOG(LogRSRounds, Error, TEXT("SetEntryBarricade ran without authority on [%s]."), *GetName());
		return;
	}

	// Not replicated: a client has no use for which window a body climbed through, and sixty-four
	// of these would be channel traffic for something only the behaviour tree ever reads.
	EntryBarricade = NewBarricade;
}

void ARSZombieCharacter::SetMoveTier(ERSMoveTier NewTier)
{
	if (!HasAuthority() || MoveTier == NewTier)
	{
		return;
	}

	MoveTier = NewTier;

	// OnRep never runs on the server, and on a listen server the host moves its own enemies.
	RefreshMovementSpeed();
}

void ARSZombieCharacter::OnRep_MoveTier()
{
	RefreshMovementSpeed();
}

void ARSZombieCharacter::HandleMoveSpeedMultiplierChanged(AActor* EffectInstigator, AActor* Causer, const FGameplayEffectSpec* Spec,
	float Magnitude, float OldValue, float NewValue)
{
	RefreshMovementSpeed();
}

void ARSZombieCharacter::RefreshMovementSpeed()
{
	const float* TierSpeed = EnemyDefinition ? EnemyDefinition->FindTierSpeed(MoveTier) : nullptr;
	if (!TierSpeed)
	{
		// A missing entry would leave the enemy on whatever speed it had last, which reads as a
		// tier that silently does nothing. Say so and leave the speed alone.
		UE_LOG(LogRSRounds, Warning, TEXT("No TierSpeeds entry for tier %d on [%s] (definition [%s])."),
			static_cast<int32>(MoveTier), *GetNameSafe(this), *GetNameSafe(EnemyDefinition));
		return;
	}

	const float Multiplier = CombatSet ? CombatSet->GetMoveSpeedMultiplier() : 1.0f;

	// Both, always: the movement component picks the crouched one while the enemy crawls, so
	// falling to a crawl needs no call here.
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = *TierSpeed * Multiplier;
	Movement->MaxWalkSpeedCrouched = EnemyDefinition->CrawlSpeed * Multiplier;
}

void ARSZombieCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	GrantEnemyAbilitySets();
}

void ARSZombieCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilitySystemComponent)
	{
		EnemyAbilityHandles.TakeFromAbilitySystem(AbilitySystemComponent);
	}

	bEnemyAbilitySetsGranted = false;

	if (URSSignificanceManager* Significance = URSSignificanceManager::Get(this))
	{
		Significance->UnregisterEnemy(this);
	}

	// A body destroyed rather than pooled — a full pool, pooling switched off — takes the brain it
	// was keeping with it. A possessed one is the engine's to clean up, as for any pawn.
	if (KeptController && EndPlayReason == EEndPlayReason::Destroyed)
	{
		KeptController->Destroy();
	}
	KeptController = nullptr;

	Super::EndPlay(EndPlayReason);
}

void ARSZombieCharacter::GrantEnemyAbilitySets()
{
	// PossessedBy only runs on the server, but the guard states the rule rather than relying on it.
	if (!HasAuthority() || bEnemyAbilitySetsGranted)
	{
		return;
	}

	if (!EnemyDefinition)
	{
		// No kit means no starting health either: the enemy would die to the first hit, or never,
		// depending on what the attribute defaults happen to be. Loud, because it is a content bug.
		UE_LOG(LogRSAbility, Error, TEXT("[%s] has no EnemyDefinition; it spawned without abilities or attributes."),
			*GetNameSafe(this));
		return;
	}

	for (const URSAbilitySet* AbilitySet : EnemyDefinition->AbilitySets)
	{
		if (!AbilitySet)
		{
			UE_LOG(LogRSAbility, Warning, TEXT("Null entry in AbilitySets of [%s] on [%s]."),
				*GetNameSafe(EnemyDefinition), *GetNameSafe(this));
			continue;
		}

		// SourceObject is the pawn itself: an enemy's abilities belong to the enemy, not to a
		// piece of equipment.
		AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &EnemyAbilityHandles, this);
	}

	bEnemyAbilitySetsGranted = true;

	// The avatar was assigned in PostInitializeComponents, before anything was granted, so the
	// spawn pass InitAbilityActorInfo runs found nothing to start. Passives start here instead.
	AbilitySystemComponent->TryActivateAbilitiesOnSpawn();
}

void ARSZombieCharacter::HandleHealthChanged(AActor* EffectInstigator, AActor* Causer, const FGameplayEffectSpec* Spec,
	float Magnitude, float OldValue, float NewValue)
{
	// Healing, replication echoes and anything the server did not cause pay nothing.
	if (!HasAuthority() || bDead || NewValue >= OldValue || Spec == nullptr)
	{
		return;
	}

	AwardScrap(EffectInstigator, SelectRewardTag(Spec, /*bKilling=*/ false));
}

void ARSZombieCharacter::HandleOutOfHealth(AActor* EffectInstigator, AActor* Causer, const FGameplayEffectSpec* Spec,
	float Magnitude, float OldValue, float NewValue)
{
	if (bDead)
	{
		return;
	}

	bDead = true;

	// Cosmetics first and on every machine, so the corpse looks the same everywhere even if the
	// authority half below returns early.
	StartRagdoll();

	if (!HasAuthority())
	{
		return;
	}

	// TagOnly: the tag replicates to everyone without a count. The animation and the AI read it,
	// and a client that never saw the killing effect still has to know this one is finished.
	AbilitySystemComponent->AddLooseGameplayTag(RSGameplayTags::State_Dead, 1, EGameplayTagReplicationState::TagOnly);

	FGameplayEventData EventData;
	EventData.EventTag = RSGameplayTags::Event_Death;
	EventData.Instigator = EffectInstigator;
	EventData.Target = this;
	AbilitySystemComponent->HandleGameplayEvent(RSGameplayTags::Event_Death, &EventData);

	// A corpse is nothing to tier: the ragdoll needs its full pose, and the manager letting go puts it back.
	if (URSSignificanceManager* Significance = URSSignificanceManager::Get(this))
	{
		Significance->UnregisterEnemy(this);
	}

	// The AI stops before anything else notices: a dead pawn must not keep asking for paths.
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	SetReplicateMovement(false);
	ReleaseController();

	// The reward tag already tells a headshot kill apart, melee excluded, so the scoreboard counts
	// exactly the kills that paid the headshot rate.
	const FGameplayTag KillRewardTag = SelectRewardTag(Spec, /*bKilling=*/ true);
	AwardScrap(EffectInstigator, KillRewardTag);

	OnZombieDied.Broadcast(this, EffectInstigator, KillRewardTag == RSGameplayTags::Economy_Reward_Headshot);

	const float CorpseLifetime = EnemyDefinition ? EnemyDefinition->CorpseLifetime : 0.0f;
	if (CorpseLifetime > 0.0f)
	{
		GetWorldTimerManager().SetTimer(CorpseTimerHandle, this, &ThisClass::FinishDeath, CorpseLifetime, /*bLoop=*/ false);
	}
	else
	{
		FinishDeath();
	}
}

void ARSZombieCharacter::StartRagdoll()
{
	// The capsule stops colliding with anything: it is a movement shape and there is no movement.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	USkeletalMeshComponent* MeshComponent = GetMesh();

	// A corpse does not stop bullets. Left blocking the weapon channel, a body on the floor would
	// shield the enemies walking over it.
	MeshComponent->SetCollisionProfileName(TEXT("Ragdoll"));
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

	MeshComponent->SetAllBodiesSimulatePhysics(true);
	MeshComponent->WakeAllRigidBodies();
}

void ARSZombieCharacter::FinishDeath()
{
	// Out through the same door it came in by. The subsystem pools the body or destroys it, and
	// this class does not have to know which.
	if (URSSpawnSubsystem* SpawnSubsystem = URSSpawnSubsystem::Get(this))
	{
		SpawnSubsystem->ReleaseEnemy(this);
		return;
	}

	Destroy();
}

void ARSZombieCharacter::ReleaseController()
{
	if (AController* Brain = GetController())
	{
		KeptController = Brain;
		Brain->UnPossess();
	}
}

void ARSZombieCharacter::ReturnToPool()
{
	if (!HasAuthority() || PoolState.bInPool)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(CorpseTimerHandle);

	// A living enemy cleared by a forced round is still being tiered. Let go, it is back on full updates.
	if (URSSignificanceManager* Significance = URSSignificanceManager::Get(this))
	{
		Significance->UnregisterEnemy(this);
	}

	// Whatever was running ends first, while the body is still the one it ran on: the vault puts
	// back the capsule and the movement mode it changed as it ends.
	AbilitySystemComponent->CancelAllAbilities();

	// The next life starts on its feet, and every client follows `bIsCrouched` back up.
	StandUp();

	if (!bDead)
	{
		// A living enemy, cleared by a forced round. It stops the way a dying one does but without
		// the death: no event, no reward, no ragdoll, and nobody told it was killed.
		bDead = true;
		AbilitySystemComponent->AddLooseGameplayTag(RSGameplayTags::State_Dead, 1, EGameplayTagReplicationState::TagOnly);
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
		SetReplicateMovement(false);
	}

	ReleaseController();

	// Nothing of this life carries into the next. Every effect goes — the round's health, a slow,
	// the kit's passives — and the kit itself, which possession grants again. The attributes come
	// back with it: the kit's initial effect, then the round's health. The pooled body keeps its
	// zero health and its Dead tag, so to everything still asking it is a corpse.
	AbilitySystemComponent->RemoveActiveEffects(FGameplayEffectQuery());
	EnemyAbilityHandles.TakeFromAbilitySystem(AbilitySystemComponent);
	bEnemyAbilitySetsGranted = false;

	EntryBarricade.Reset();
	MotionWarping->RemoveAllWarpTargets();

	StowBody();
	PoolState.bInPool = true;

	// Dormant once the state above has reached every client: the channel stops replicating and
	// stays open, so each client keeps its copy, put away, until the body comes back.
	SetNetDormancy(DORM_DormantAll);

	OnReturnedToPool.Broadcast(this);
}

void ARSZombieCharacter::Restart()
{
	Super::Restart();

	// The engine stands every pawn up on possession. A crawler stays one for the whole life,
	// whoever is driving it: a controller swapped mid-life must not put it back on a leg it lost.
	if (AbilitySystemComponent->HasMatchingGameplayTag(RSGameplayTags::State_Crawling))
	{
		Crouch();
	}
}

bool ARSZombieCharacter::CanCrouch() const
{
	// The engine turns down a request to crouch from a body that already is, which loses nothing for
	// a player pressing a key and loses the crawl here: `Restart` drops the wish to crouch while the
	// body is still down, and only a fresh request, granted, keeps it there.
	return Super::CanCrouch()
		|| (IsCrouched() && GetCharacterMovement()->CanEverCrouch() && !GetRootComponent()->IsSimulatingPhysics());
}

void ARSZombieCharacter::StandUp()
{
	// Stops the movement component crouching it again on its next update.
	UnCrouch();

	if (bIsCrouched)
	{
		// Now, and without testing for room. The movement component's own uncrouch gives up when the
		// standing capsule would not fit, and a crawler that died under a table would come back
		// crawling. The proxy's path is the one that skips the test; it leaves the flag to the caller.
		GetCharacterMovement()->UnCrouch(/*bClientSimulation=*/ true);
		SetIsCrouched(false);
	}
}

void ARSZombieCharacter::StowBody()
{
	// Out of the world: nothing to see, nothing to hit, nothing to update, and on a client a
	// ragdoll that stops simulating.
	GetMesh()->SetAllBodiesSimulatePhysics(false);
	SetActorHiddenInGame(true);
	// The actor's switch, not each component's. Turning the mesh's own collision off destroys the
	// physics state of a body per bone, and bringing it back rebuilds them all: measured, that made
	// a reuse cost more than the original restore did. The switch leaves them in place.
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
	GetMesh()->SetComponentTickEnabled(false);
	GetCharacterMovement()->SetComponentTickEnabled(false);
}

void ARSZombieCharacter::UnstowBody()
{
	RestoreLivingBody();

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
	GetMesh()->SetComponentTickEnabled(true);
	GetCharacterMovement()->SetComponentTickEnabled(true);
}

void ARSZombieCharacter::LeavePool(const FTransform& SpawnTransform)
{
	if (!HasAuthority() || !PoolState.bInPool)
	{
		return;
	}

	// Awake before anything changes, so every change below reaches the clients.
	SetNetDormancy(DORM_Awake);

	++PoolState.Life;
	PoolState.bInPool = false;

	SetActorLocationAndRotation(SpawnTransform.GetLocation(), SpawnTransform.GetRotation(), /*bSweep=*/ false, nullptr, ETeleportType::ResetPhysics);
	UnstowBody();

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->Velocity = FVector::ZeroVector;
	Movement->SetMovementMode(MOVE_Walking);
	SetReplicateMovement(true);

	bDead = false;
	AbilitySystemComponent->SetLooseGameplayTagCount(RSGameplayTags::State_Dead, 0, EGameplayTagReplicationState::TagOnly);
	Dismemberment->ResetForNewLife();

	// The kit comes back with possession, exactly as it came the first time.
	if (AController* Brain = KeptController)
	{
		KeptController = nullptr;
		Brain->Possess(this);
	}
	else
	{
		SpawnDefaultController();
	}

	RefreshMovementSpeed();
	ForceNetUpdate();

	if (URSSignificanceManager* Significance = URSSignificanceManager::Get(this))
	{
		Significance->RegisterEnemy(this);
	}
}

void ARSZombieCharacter::RestoreLivingBody()
{
	const ARSZombieCharacter* Defaults = GetClass()->GetDefaultObject<ARSZombieCharacter>();
	const UCapsuleComponent* DefaultCapsule = Defaults->GetCapsuleComponent();
	const USkeletalMeshComponent* DefaultMesh = Defaults->GetMesh();

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	USkeletalMeshComponent* MeshComponent = GetMesh();
	MeshComponent->SetAllBodiesSimulatePhysics(false);
	Capsule->ClearMoveIgnoreActors();

	// One call each, whatever the ragdoll, the vault or the pool left: the class's own named profiles.
	// On a mesh with a body per bone every separate collision change is a separate physics update,
	// and the type, the switch and the responses set one by one were three of them for the mesh alone.
	Capsule->SetCollisionProfileName(DefaultCapsule->GetCollisionProfileName());
	MeshComponent->SetCollisionProfileName(DefaultMesh->GetCollisionProfileName());

	MeshComponent->AttachToComponent(Capsule, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	MeshComponent->SetRelativeLocationAndRotation(DefaultMesh->GetRelativeLocation(), DefaultMesh->GetRelativeRotation());

	// The death may have left a montage playing under the ragdoll — a swing, a vault — and it ends
	// here. The rest of the animation carries on as it is: the instance keeps no state of a death,
	// only speed and direction the next frame writes again. Reinitialising it instead was the
	// largest single cost of a reuse, a third of it.
	if (UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance())
	{
		AnimInstance->StopAllMontages(/*BlendOut=*/ 0.0f);
	}
}

FGameplayTag ARSZombieCharacter::SelectRewardTag(const FGameplayEffectSpec* Spec, bool bKilling) const
{
	bool bMelee = false;
	bool bHeadshot = false;

	if (Spec)
	{
		FGameplayTagContainer SpecTags;
		Spec->GetAllAssetTags(SpecTags);
		bMelee = SpecTags.HasTag(RSGameplayTags::Damage_Type_Melee);

		if (const FHitResult* HitResult = Spec->GetContext().GetHitResult())
		{
			const URSPhysicalMaterialWithTags* Material = Cast<const URSPhysicalMaterialWithTags>(HitResult->PhysMaterial.Get());
			bHeadshot = Material && Material->Tags.HasTag(RSGameplayTags::Damage_Zone_Head);
		}
	}

	if (!bKilling)
	{
		return bMelee ? RSGameplayTags::Economy_Reward_Hit_Melee : RSGameplayTags::Economy_Reward_Hit;
	}

	// A melee kill pays its own rate whatever it landed on: there is no melee headshot bonus.
	if (bMelee)
	{
		return RSGameplayTags::Economy_Reward_Kill_Melee;
	}

	return bHeadshot ? RSGameplayTags::Economy_Reward_Headshot : RSGameplayTags::Economy_Reward_Kill;
}

void ARSZombieCharacter::AwardScrap(AActor* RewardedActor, const FGameplayTag& RewardTag)
{
	// Only players hold scrap. An enemy killed by another enemy, or by the world, pays nobody.
	if (!Cast<ARSPlayerState>(RewardedActor) || !EnemyDefinition || !EnemyDefinition->ScrapAwardEffect)
	{
		return;
	}

	const float Amount = EnemyDefinition->GetScrapReward(RewardTag);
	if (Amount <= 0.0f)
	{
		return;
	}

	UAbilitySystemComponent* RewardedASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(RewardedActor);
	if (!RewardedASC)
	{
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = RewardedASC->MakeOutgoingSpec(EnemyDefinition->ScrapAwardEffect, 1.0f, RewardedASC->MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Scrap, Amount);

	// The reward tag rides on the spec so the HUD popup can tell a headshot bonus from a plain
	// kill without a second notification path.
	SpecHandle.Data->AddDynamicAssetTag(RewardTag);

	RewardedASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);

	UE_LOG(LogRSAbility, Verbose, TEXT("Awarded %.0f scrap to [%s] for [%s]."),
		Amount, *GetNameSafe(RewardedActor), *RewardTag.ToString());
}
