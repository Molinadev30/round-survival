// Round Survival

#include "Core/RSPlayerCharacter.h"

#include "AbilitySystem/Attributes/RSCombatSet.h"
#include "AbilitySystem/Attributes/RSHandlingSet.h"
#include "AbilitySystem/RSAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/CameraComponent.h"
#include "Camera/RSPlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/RSCharacterMovementComponent.h"
#include "Core/RSPlayerState.h"
#include "EnhancedInputSubsystems.h"
#include "Equipment/RSEquipmentDefinition.h"
#include "Equipment/RSEquipmentInstance.h"
#include "Equipment/RSEquipmentManagerComponent.h"
#include "GameplayEffect.h"
#include "Weapons/RSFirstPersonRecoilComponent.h"
#include "Weapons/RSRangedWeaponInstance.h"
#include "Weapons/RSWeaponSlotsComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Input/RSInputComponent.h"
#include "Interaction/RSInteractorComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"

#define LOCTEXT_NAMESPACE "RoundSurvival"

ARSPlayerCharacter::ARSPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<URSCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Ticks only while the camera is moving between postures, in and out of the sights or under a
	// slide's tilt, and switches itself off when all of them arrive. Nothing is polled.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// On the body mesh, not the capsule: on every machine but the owner's the engine's network
	// smoothing moves only the mesh, and the capsule jumps to each update. A teammate watching
	// through these eyes gets the smoothed view this way, with nothing extra. The owner's mesh is
	// never smoothed, so their camera — the view point their shots and interactions start from — is
	// where it always was. Epic's first-person template hangs its camera off the mesh as well.
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(GetMesh());
	CameraComponent->SetRelativeLocation(FVector(0.0f, 0.0f, CameraHeight));
	CameraComponent->bUsePawnControlRotation = true;

	// The viewmodel gets its own FOV and is scaled towards the camera, which is what stops the
	// weapon intersecting walls without any gameplay-side near-wall handling.
	// Because these primitives are not rendered where they live, the muzzle socket
	// on this mesh is not a usable world position and is never used as a shot origin.
	CameraComponent->SetEnableFirstPersonFieldOfView(true);
	CameraComponent->SetEnableFirstPersonScale(true);
	// 100 is what the first-person animations are framed for: the arms are authored at a
	// world FOV of 100. At 70 the weapon was magnified ~1.7x and cropped by the screen edges.
	CameraComponent->SetFirstPersonFieldOfView(100.0f);
	CameraComponent->SetFirstPersonScale(0.6f);

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(CameraComponent);
	FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonMesh->bOnlyOwnerSee = true;
	FirstPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonMesh->SetCollisionProfileName(TEXT("NoCollision"));
	FirstPersonMesh->bReceivesDecals = false;
	// Every machine has the arms, and only a view through this player's eyes draws them: on the
	// others they keep their montages in step, so a teammate switching to first person finds them
	// mid-reload, and skip the rest of the graph.
	FirstPersonMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;

	FirstPersonRecoil = CreateDefaultSubobject<URSFirstPersonRecoilComponent>(TEXT("FirstPersonRecoil"));

	// WorldSpaceRepresentation sets bOwnerNoSee internally: the owner does not see this mesh,
	// but it still casts the shadow the owner does see, and it is what other players see.
	USkeletalMeshComponent* BodyMesh = GetMesh();
	BodyMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	BodyMesh->bReceivesDecals = false;

	EquipmentManager = CreateDefaultSubobject<URSEquipmentManagerComponent>(TEXT("EquipmentManager"));
	WeaponSlots = CreateDefaultSubobject<URSWeaponSlotsComponent>(TEXT("WeaponSlots"));

	Interactor = CreateDefaultSubobject<URSInteractorComponent>(TEXT("Interactor"));

	// Strafing keeps the pawn facing where the camera looks; rotating the mesh towards movement
	// would fight the aim direction in a shooter.
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
	Movement->MaxWalkSpeedCrouched = 300.0f;
}

UAbilitySystemComponent* ARSPlayerCharacter::GetAbilitySystemComponent() const
{
	return GetRSAbilitySystemComponent();
}

URSAbilitySystemComponent* ARSPlayerCharacter::GetRSAbilitySystemComponent() const
{
	const ARSPlayerState* RSPS = GetPlayerState<ARSPlayerState>();
	return RSPS ? RSPS->GetRSAbilitySystemComponent() : nullptr;
}

void ARSPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitAbilitySystem();

	// After the ability system, never before: drawing a weapon grants an ability set, and there is
	// nothing to grant it to until the avatar is wired up.
	GiveDefaultWeapons();
}

void ARSPlayerCharacter::GiveDefaultWeapons()
{
	if (!HasAuthority() || !WeaponSlots)
	{
		return;
	}

	// Possession can happen more than once for the same pawn; giving again would fail on the
	// duplicate check anyway, but it would also redraw.
	if (WeaponSlots->HasAnyWeapon())
	{
		return;
	}

	for (const URSWeaponItemDefinition* Weapon : DefaultWeapons)
	{
		WeaponSlots->GiveWeapon(Weapon, /*bDraw=*/ false);
	}

	// GiveWeapon draws the first one it is given into an empty pair of hands; this only makes the
	// rule explicit, so the order of DefaultWeapons is the order of the slots and slot 0 is in hand.
	WeaponSlots->SetActiveSlot(0);
}

void ARSPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitAbilitySystem();
}

void ARSPlayerCharacter::InitAbilitySystem()
{
	ARSPlayerState* RSPS = GetPlayerState<ARSPlayerState>();
	if (RSPS == nullptr)
	{
		// Expected on the client when the pawn replicates before its player state does.
		// OnRep_PlayerState will bring us back here.
		return;
	}

	URSAbilitySystemComponent* RSASC = RSPS->GetRSAbilitySystemComponent();
	if (RSASC == nullptr)
	{
		UE_LOG(LogRSAbility, Error, TEXT("Player state [%s] has no ability system component."), *GetNameSafe(RSPS));
		return;
	}

	// Owner stays the player state, avatar becomes this pawn. The component itself guards
	// against re-running the spawn pass when the avatar has not actually changed.
	RSASC->InitAbilityActorInfo(RSPS, this);

	BindToAbilitySystem(RSASC);
}

void ARSPlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Before anything is equipped: the switch reinitialises the body's graph, which would drop a
	// weapon's linked layers.
	if (BodyPostProcessAnimClass)
	{
		GetMesh()->SetOverridePostProcessAnimBP(BodyPostProcessAnimClass);
	}

	// What the Blueprint set, captured once: everything after this scales or moves away from it.
	BaseMaxWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
	StandingCameraHeight = CameraComponent->GetRelativeLocation().Z;
	TargetCameraHeight = StandingCameraHeight;
}

void ARSPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The ability system lives on the player state and outlives this pawn: a player who died has no
	// avatar until the next pawn makes itself one, rather than one that no longer exists. Read from
	// the binding, since an unpossessed pawn no longer has the player state to ask.
	if (URSAbilitySystemComponent* RSASC = BoundAbilitySystem.Get())
	{
		if (RSASC->GetAvatarActor_Direct() == this)
		{
			RSASC->SetAvatarActor(nullptr);
		}
	}

	UnbindFromAbilitySystem();

	Super::EndPlay(EndPlayReason);
}

void ARSPlayerCharacter::BindToAbilitySystem(URSAbilitySystemComponent* RSASC)
{
	if (BoundAbilitySystem.Get() == RSASC)
	{
		return;
	}

	UnbindFromAbilitySystem();
	BoundAbilitySystem = RSASC;

	DownedTagChangedHandle = RSASC->RegisterGameplayTagEvent(RSGameplayTags::State_Downed, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleDownedTagChanged);
	AimDownSightsTagChangedHandle = RSASC->RegisterGameplayTagEvent(RSGameplayTags::State_ADS, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleAimDownSightsTagChanged);

	if (const URSCombatSet* CombatSet = RSASC->GetSet<URSCombatSet>())
	{
		MoveSpeedChangedHandle = CombatSet->OnMoveSpeedMultiplierChanged.AddUObject(this, &ThisClass::HandleMoveSpeedMultiplierChanged);
	}

	// The ability system's own delegate fires on the server when an effect changes it and on the
	// owning client when the change arrives: the two machines that spend the sprint.
	SprintDurationChangedHandle = RSASC->GetGameplayAttributeValueChangeDelegate(URSHandlingSet::GetSprintDurationMultiplierAttribute())
		.AddUObject(this, &ThisClass::HandleSprintDurationMultiplierChanged);

	// Whatever the player state already carries — a client whose pawn arrives while its player is
	// down — applies at once rather than waiting for the next change.
	HandleDownedTagChanged(RSGameplayTags::State_Downed, RSASC->GetTagCount(RSGameplayTags::State_Downed));
	RefreshMovementSpeed();
	RefreshSprintDuration();
}

void ARSPlayerCharacter::UnbindFromAbilitySystem()
{
	URSAbilitySystemComponent* RSASC = BoundAbilitySystem.Get();
	if (!RSASC)
	{
		return;
	}

	RSASC->RegisterGameplayTagEvent(RSGameplayTags::State_Downed, EGameplayTagEventType::NewOrRemoved).Remove(DownedTagChangedHandle);
	RSASC->RegisterGameplayTagEvent(RSGameplayTags::State_ADS, EGameplayTagEventType::NewOrRemoved).Remove(AimDownSightsTagChangedHandle);

	if (const URSCombatSet* CombatSet = RSASC->GetSet<URSCombatSet>())
	{
		CombatSet->OnMoveSpeedMultiplierChanged.Remove(MoveSpeedChangedHandle);
	}

	RSASC->GetGameplayAttributeValueChangeDelegate(URSHandlingSet::GetSprintDurationMultiplierAttribute()).Remove(SprintDurationChangedHandle);

	DownedTagChangedHandle.Reset();
	AimDownSightsTagChangedHandle.Reset();
	MoveSpeedChangedHandle.Reset();
	SprintDurationChangedHandle.Reset();
	BoundAbilitySystem.Reset();
}

void ARSPlayerCharacter::HandleDownedTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// On every machine, the server's included: the shot and the interaction trace start from the
	// view point, and the server checks a client's shot from its own copy of it.
	RefreshCameraHeight();

	// A downed player is not crouching. Crouching is the controlling machine's to change; the
	// server and everybody else follow its moves.
	if (NewCount > 0 && bIsCrouched && IsLocallyControlled())
	{
		UnCrouch();
	}

	// A downed player uses nothing, so nothing in front of them offers a prompt. The interaction
	// ability's blocked tag still refuses on the server; this is what keeps the HUD quiet.
	Interactor->SetFocusEnabled(NewCount == 0);
}

void ARSPlayerCharacter::HandleAimDownSightsTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	const bool bAiming = NewCount > 0;

	// The server passes on what its copy of the ability did. One bit, and only when it changes.
	if (HasAuthority())
	{
		bAimingDownSights = bAiming;
	}

	SetTargetAimDownSights(bAiming);
}

void ARSPlayerCharacter::OnRep_AimingDownSights()
{
	SetTargetAimDownSights(bAimingDownSights);
}

bool ARSPlayerCharacter::IsSprinting() const
{
	if (HasAuthority() || IsLocallyControlled())
	{
		const URSCharacterMovementComponent* Movement = Cast<URSCharacterMovementComponent>(GetCharacterMovement());
		return Movement && Movement->IsSprinting();
	}

	return bSprinting;
}

void ARSPlayerCharacter::SetTargetAimDownSights(bool bAiming)
{
	// Eased from here exactly as on the aiming player's machine, so everybody else sees the same
	// 0.2 s, only later by the time the change took to reach them.
	TargetAimDownSightsAlpha = bAiming ? 1.0f : 0.0f;
	RefreshTickEnabled();
}

void ARSPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, bAimingDownSights, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ThisClass, bSprinting, COND_SkipOwner);
}

const URSRangedWeaponInstance* ARSPlayerCharacter::GetDrawnRangedWeapon() const
{
	return EquipmentManager ? EquipmentManager->GetFirstInstanceOfType<URSRangedWeaponInstance>() : nullptr;
}

void ARSPlayerCharacter::HandleMoveSpeedMultiplierChanged(AActor* EffectInstigator, AActor* Causer, const FGameplayEffectSpec* Spec,
	float Magnitude, float OldValue, float NewValue)
{
	RefreshMovementSpeed();
}

void ARSPlayerCharacter::HandleSprintDurationMultiplierChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshSprintDuration();
}

void ARSPlayerCharacter::RefreshSprintDuration()
{
	const URSAbilitySystemComponent* RSASC = BoundAbilitySystem.Get();
	bool bFound = false;
	const float Multiplier = RSASC ? RSASC->GetGameplayAttributeValue(URSHandlingSet::GetSprintDurationMultiplierAttribute(), bFound) : 1.0f;

	if (URSCharacterMovementComponent* Movement = Cast<URSCharacterMovementComponent>(GetCharacterMovement()))
	{
		Movement->SetSprintDurationMultiplier(bFound ? Multiplier : 1.0f);
	}
}

void ARSPlayerCharacter::RefreshMovementSpeed()
{
	const URSAbilitySystemComponent* RSASC = BoundAbilitySystem.Get();
	const URSCombatSet* CombatSet = RSASC ? RSASC->GetSet<URSCombatSet>() : nullptr;

	// The multiplier replicates to everyone, so the owning client predicts its movement at the same
	// speed the server checks it against.
	const float Multiplier = CombatSet ? CombatSet->GetMoveSpeedMultiplier() : 1.0f;

	GetCharacterMovement()->MaxWalkSpeed = BaseMaxWalkSpeed * Multiplier;
}

void ARSPlayerCharacter::RefreshCameraHeight()
{
	const URSAbilitySystemComponent* RSASC = BoundAbilitySystem.Get();
	const bool bDowned = RSASC && RSASC->HasMatchingGameplayTag(RSGameplayTags::State_Downed);

	ERSCameraPosture NewPosture = ERSCameraPosture::Standing;
	if (bDowned)
	{
		NewPosture = ERSCameraPosture::Downed;
	}
	else if (IsSliding())
	{
		NewPosture = ERSCameraPosture::Sliding;
	}
	else if (bIsCrouched)
	{
		NewPosture = ERSCameraPosture::Crouched;
	}

	if (NewPosture == CameraPosture)
	{
		return;
	}

	// The rate of the whole change between the two postures, not of what is left of it: a change
	// turned back halfway goes back at the same speed it came.
	const float Seconds = GetCameraTransitionSeconds(CameraPosture, NewPosture);
	const float Distance = FMath::Abs(GetCameraHeight(NewPosture) - GetCameraHeight(CameraPosture));
	CameraHeightSpeed = Seconds > KINDA_SMALL_NUMBER && Distance > KINDA_SMALL_NUMBER ? Distance / Seconds : UE_BIG_NUMBER;

	CameraPosture = NewPosture;
	TargetCameraHeight = GetCameraHeight(NewPosture);
	RefreshTickEnabled();
}

float ARSPlayerCharacter::GetCameraHeight(ERSCameraPosture Posture) const
{
	switch (Posture)
	{
	case ERSCameraPosture::Crouched:
		return CrouchedCameraHeight;
	case ERSCameraPosture::Sliding:
		return SlidingCameraHeight;
	case ERSCameraPosture::Downed:
		return DownedCameraHeight;
	default:
		return StandingCameraHeight;
	}
}

float ARSPlayerCharacter::GetCameraTransitionSeconds(ERSCameraPosture From, ERSCameraPosture To) const
{
	if (To == ERSCameraPosture::Sliding)
	{
		return SlideCameraSeconds;
	}

	if (From == ERSCameraPosture::Sliding && To != ERSCameraPosture::Downed)
	{
		return To == ERSCameraPosture::Standing ? SlideToStandingCameraSeconds : SlideToCrouchedCameraSeconds;
	}

	if (From == ERSCameraPosture::Downed || To == ERSCameraPosture::Downed)
	{
		return DownedCameraSeconds;
	}

	return CrouchCameraSeconds;
}

void ARSPlayerCharacter::RefreshTickEnabled()
{
	const bool bCameraMoving = !FMath::IsNearlyEqual(CameraComponent->GetRelativeLocation().Z, TargetCameraHeight);
	const bool bAimMoving = AimDownSightsAlpha != TargetAimDownSightsAlpha;
	const bool bSlideViewMoving = IsSliding() || !SlideViewOffset.IsZero();
	SetActorTickEnabled(bCameraMoving || bAimMoving || bSlideViewMoving);
}

void ARSPlayerCharacter::UpdateSlideView(float DeltaSeconds)
{
	if (IsSliding())
	{
		SlideViewSeconds += DeltaSeconds;

		const float In = SlideViewInSeconds > KINDA_SMALL_NUMBER ? FMath::Min(SlideViewSeconds / SlideViewInSeconds, 1.0f) : 1.0f;
		const float Ease = FMath::Sin(In * UE_HALF_PI);
		const float Wobble = SlideViewWobblePeriod > KINDA_SMALL_NUMBER
			? SlideViewWobble * FMath::Sin(UE_TWO_PI * SlideViewSeconds / SlideViewWobblePeriod)
			: 0.0f;

		SlideViewOffset = FRotator(SlideViewPitch * Ease + Wobble, 0.0f, SlideViewRoll * Ease + Wobble);
		return;
	}

	// In proportion to what is left, so it comes back fast and settles softly.
	SlideViewSeconds = 0.0f;
	SlideViewOffset *= FMath::Max(0.0f, 1.0f - SlideViewRecoveryRate * DeltaSeconds);
	if (SlideViewOffset.IsNearlyZero(0.01f))
	{
		SlideViewOffset = FRotator::ZeroRotator;
	}
}

void ARSPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// At a constant rate, with no easing: a posture change takes the same time wherever it starts.
	FVector CameraLocation = CameraComponent->GetRelativeLocation();
	CameraLocation.Z = FMath::FInterpConstantTo(CameraLocation.Z, TargetCameraHeight, DeltaSeconds, CameraHeightSpeed);
	CameraComponent->SetRelativeLocation(CameraLocation);

	// At a constant rate, so going in takes exactly the weapon's time and coming out takes the same.
	const URSRangedWeaponInstance* Weapon = GetDrawnRangedWeapon();
	const float AimSeconds = Weapon ? Weapon->GetAimDownSightsSeconds() : 0.0f;
	AimDownSightsAlpha = AimSeconds > KINDA_SMALL_NUMBER
		? FMath::FInterpConstantTo(AimDownSightsAlpha, TargetAimDownSightsAlpha, DeltaSeconds, 1.0f / AimSeconds)
		: TargetAimDownSightsAlpha;

	UpdateSlideView(DeltaSeconds);

	RefreshTickEnabled();
}

void ARSPlayerCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	// The camera turns with the controller only where that controller is a local player's: that is
	// the engine's rule in UCameraComponent::GetCameraView. Asked anywhere else, the view is a dead
	// teammate's watching in first person, and the rotation to look along is the one the engine
	// keeps for a spectator for exactly this — GetViewRotation hands back the watched player's view
	// rotation as replicated to them and smoothed. The viewmodel hangs off the camera, so it turns too.
	const AController* CurrentController = GetController();
	if (CameraComponent->bUsePawnControlRotation && !(CurrentController && CurrentController->IsLocalPlayerController()))
	{
		CameraComponent->SetWorldRotation(GetViewRotation());
	}

	Super::CalcCamera(DeltaTime, OutResult);

	// The slide's tilt, in the view's own axes: up, and rolled. Put on the camera component as well as
	// the view, so the arms hanging from it tilt with the screen rather than against it; the camera
	// takes the control rotation back on the next view, so nothing builds up. The shot is aimed along
	// this same view.
	if (!SlideViewOffset.IsZero())
	{
		OutResult.Rotation = (OutResult.Rotation.Quaternion() * SlideViewOffset.Quaternion()).Rotator();
		CameraComponent->SetWorldRotation(OutResult.Rotation);
	}

	// Zoomed in by the weapon's sights. The crosshair projects the spread through this same field of
	// view, so the cone it draws is still the cone the bullets come from.
	const URSRangedWeaponInstance* Weapon = AimDownSightsAlpha > 0.0f ? GetDrawnRangedWeapon() : nullptr;
	if (Weapon)
	{
		OutResult.FOV = FMath::Lerp(OutResult.FOV, Weapon->GetAimDownSightsFieldOfView(), AimDownSightsAlpha);

		if (OutResult.bUseFirstPersonParameters)
		{
			OutResult.FirstPersonFOV = FMath::Lerp(OutResult.FirstPersonFOV, Weapon->GetAimDownSightsFirstPersonFieldOfView(), AimDownSightsAlpha);
		}
	}
}

bool ARSPlayerCharacter::CanCrouch() const
{
	// Native input rather than an ability, like jumping, so the downed tag is checked here too.
	const URSAbilitySystemComponent* RSASC = GetRSAbilitySystemComponent();
	if (RSASC && RSASC->HasMatchingGameplayTag(RSGameplayTags::State_Downed))
	{
		return false;
	}

	// Nor in the air: the movement component would otherwise keep the wish and crouch on landing.
	if (!GetCharacterMovement()->IsMovingOnGround())
	{
		return false;
	}

	return Super::CanCrouch();
}

void ARSPlayerCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	// The engine moves the mesh up inside the shorter capsule so the feet stay on the floor; the
	// camera hangs from the mesh, so without this the eyes would stay where they were. On every
	// machine, because the server checks shots from its own copy of the view point.
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	RefreshCameraHeight();
}

void ARSPlayerCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	RefreshCameraHeight();
}

bool ARSPlayerCharacter::CanJumpInternal_Implementation() const
{
	// Native input rather than an ability, so the downed tag is checked here.
	const URSAbilitySystemComponent* RSASC = GetRSAbilitySystemComponent();
	if (RSASC && RSASC->HasMatchingGameplayTag(RSGameplayTags::State_Downed))
	{
		return false;
	}

	// The engine refuses a crouched jump, and a slide is crouched: jumping is how a slide is left upwards.
	if (IsSliding())
	{
		return JumpIsAllowedInternal();
	}

	return Super::CanJumpInternal_Implementation();
}

void ARSPlayerCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	// On every machine, like crouching: the server checks shots from its own copy of the view point.
	RefreshCameraHeight();

	// Heard on every machine: they all see the mode change. Not again when the owner replays its moves
	// after a correction, which runs through the same change.
	const URSCharacterMovementComponent* Movement = Cast<URSCharacterMovementComponent>(GetCharacterMovement());
	if (SlideStartSound && Movement && Movement->IsSliding() && !bClientUpdating)
	{
		UGameplayStatics::SpawnSoundAttached(SlideStartSound, GetRootComponent());
	}
}

bool ARSPlayerCharacter::IsSliding() const
{
	const URSCharacterMovementComponent* Movement = Cast<URSCharacterMovementComponent>(GetCharacterMovement());
	return Movement && Movement->IsSliding();
}

void ARSPlayerCharacter::UnPossessed()
{
	if (URSAbilitySystemComponent* RSASC = GetRSAbilitySystemComponent())
	{
		// Held input must not survive the pawn. Otherwise a WhileInputActive ability would keep
		// re-activating on the next pawn because the key was never seen to come up.
		RSASC->ClearAbilityInput();

		// Nor do the weapons. Their abilities were granted to the player state's ability system,
		// which outlives this pawn, and this is the last moment it can be reached through the
		// player state: torn down later with the pawn, the equipment would find nothing to revoke
		// them from, and a dead player would keep the fire and reload abilities into the next life.
		// A pawn is only unpossessed on its way out — dying, or the map going.
		if (HasAuthority() && WeaponSlots)
		{
			WeaponSlots->RemoveAllWeapons();
		}
	}

	Super::UnPossessed();
}

void ARSPlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC == nullptr || DefaultMappingContext == nullptr)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		// Only this pawn's own context, taken off and put back: PawnClientRestart runs again on
		// respawn, and the controller keeps contexts of its own that are not the pawn's to clear.
		Subsystem->RemoveMappingContext(DefaultMappingContext);
		Subsystem->AddMappingContext(DefaultMappingContext, DefaultMappingPriority);
	}
}

void ARSPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (InputConfig == nullptr)
	{
		UE_LOG(LogRS, Error, TEXT("[%s] has no InputConfig assigned; no input will be bound."), *GetNameSafe(this));
		return;
	}

	URSInputComponent* RSInput = Cast<URSInputComponent>(PlayerInputComponent);
	if (RSInput == nullptr)
	{
		UE_LOG(LogRS, Error, TEXT("[%s] expects URSInputComponent. Set it as the default input component class in DefaultInput.ini."),
			*GetNameSafe(this));
		return;
	}

	using namespace RSGameplayTags;

	RSInput->BindNativeAction(InputConfig, InputTag_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
	RSInput->BindNativeAction(InputConfig, InputTag_Look, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);
	// The press edge: Triggered repeats every frame the key is down, so a press that stands a crouching
	// player up would jump on the next frame. A held jump still holds, the engine keeps it until StopJumping.
	RSInput->BindNativeAction(InputConfig, InputTag_Jump, ETriggerEvent::Started, this, &ThisClass::Input_Jump);
	RSInput->BindNativeAction(InputConfig, InputTag_Jump, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

	// Native, like jumping: the engine already predicts and replicates crouching. A toggle, so the press edge only.
	RSInput->BindNativeAction(InputConfig, InputTag_Crouch, ETriggerEvent::Started, this, &ThisClass::Input_Crouch);
	RSInput->BindNativeAction(InputConfig, InputTag_Crouch, ETriggerEvent::Completed, this, &ThisClass::Input_CrouchReleased);

	// Everything else goes through the ability system by tag. Nothing here names Fire or Reload.
	// Started, not Triggered: with the default Down trigger, Triggered fires on every frame the
	// key is held, which would re-report the press each frame and re-activate an OnInputTriggered
	// ability the moment it ended. Started is the press edge and fires exactly once; the held
	// state that WhileInputActive needs is tracked on the ability system between press and release.
	RSInput->BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityInputTagPressed,
		&ThisClass::Input_AbilityInputTagReleased, AbilityBindHandles);
}

void ARSPlayerCharacter::Input_Move(const FInputActionValue& Value)
{
	const AController* CurrentController = GetController();
	if (CurrentController == nullptr)
	{
		return;
	}

	const FVector2D MoveValue = Value.Get<FVector2D>();

	// Yaw only: pitching the movement basis would let the player walk into the ground by
	// looking down.
	const FRotator MovementRotation(0.0f, CurrentController->GetControlRotation().Yaw, 0.0f);

	if (MoveValue.X != 0.0f)
	{
		AddMovementInput(MovementRotation.RotateVector(FVector::RightVector), MoveValue.X);
	}

	if (MoveValue.Y != 0.0f)
	{
		AddMovementInput(MovementRotation.RotateVector(FVector::ForwardVector), MoveValue.Y);
	}
}

void ARSPlayerCharacter::Input_Jump()
{
	if (bIsCrouched && !IsSliding())
	{
		UnCrouch();
		return;
	}

	Jump();
}

void ARSPlayerCharacter::Input_Crouch()
{
	// Standing up from a crouch, or out of a slide, which cancels it.
	if (bIsCrouched)
	{
		UnCrouch();
		return;
	}

	// Mid-sprint, or in the air with the sprint key down, the key asks for a slide. The movement
	// component starts it — on landing, from the air — when the move allows one.
	URSCharacterMovementComponent* Movement = Cast<URSCharacterMovementComponent>(GetCharacterMovement());
	if (Movement && (Movement->IsSprinting() || (Movement->IsFalling() && Movement->WantsToSprint())))
	{
		Movement->SetWantsToSlide(true);
		return;
	}

	Crouch();
}

void ARSPlayerCharacter::Input_CrouchReleased()
{
	if (URSCharacterMovementComponent* Movement = Cast<URSCharacterMovementComponent>(GetCharacterMovement()))
	{
		Movement->SetWantsToSlide(false);
	}
}

void ARSPlayerCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D LookValue = Value.Get<FVector2D>();

	// No delta-time scaling: mouse input is already a delta, and multiplying it by frame time
	// makes sensitivity depend on framerate. Gamepad scaling belongs in the input modifiers.
	if (LookValue.X != 0.0f)
	{
		AddControllerYawInput(LookValue.X);
	}

	if (LookValue.Y != 0.0f)
	{
		AddControllerPitchInput(-LookValue.Y);
	}
}

void ARSPlayerCharacter::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (URSAbilitySystemComponent* RSASC = GetRSAbilitySystemComponent())
	{
		RSASC->AbilityInputTagPressed(InputTag);
	}
}

void ARSPlayerCharacter::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (URSAbilitySystemComponent* RSASC = GetRSAbilitySystemComponent())
	{
		RSASC->AbilityInputTagReleased(InputTag);
	}
}

bool ARSPlayerCharacter::IsViewedInFirstPerson() const
{
	// The same question URSLocalPlayer answers for the renderer, asked of the same camera, so the
	// copy that flashes is the copy that is drawn.
	const APlayerController* Viewer = GetLocalViewingPlayerController();
	if (!Viewer)
	{
		return false;
	}

	const ARSPlayerCameraManager* Camera = Cast<ARSPlayerCameraManager>(Viewer->PlayerCameraManager);
	return !(Camera && Camera->IsViewingTargetFromOutside());
}

bool ARSPlayerCharacter::IsDowned() const
{
	const URSAbilitySystemComponent* RSASC = GetRSAbilitySystemComponent();
	return RSASC && RSASC->HasMatchingGameplayTag(RSGameplayTags::State_Downed);
}

FText ARSPlayerCharacter::GetPrompt_Implementation(AActor* InteractingActor) const
{
	// A teammate on their feet is not something to use, so looking at one shows nothing.
	if (!IsDowned())
	{
		return FText::GetEmpty();
	}

	const APlayerState* DownedPlayer = GetPlayerState();
	const FText Name = DownedPlayer ? FText::FromString(DownedPlayer->GetPlayerName()) : LOCTEXT("ReviveUnknownPlayer", "teammate");

	return IRSInteractable::MakeActionPrompt(FText::Format(LOCTEXT("Revive", "to revive {0}"), Name),
		IRSInteractable::Execute_GetHoldSeconds(this, InteractingActor));
}

float ARSPlayerCharacter::GetHoldSeconds_Implementation(AActor* InteractingActor) const
{
	// The reviver's speed, not this player's: whatever this player had went with the down. Owner-only
	// replication is enough, since only the reviver's client and the server run the hold.
	const UAbilitySystemComponent* ReviverASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InteractingActor);
	bool bFound = false;
	const float Multiplier = ReviverASC ? ReviverASC->GetGameplayAttributeValue(URSHandlingSet::GetReviveSpeedMultiplierAttribute(), bFound) : 1.0f;

	return ReviveHoldSeconds / (bFound ? Multiplier : 1.0f);
}

bool ARSPlayerCharacter::CanInteract_Implementation(AActor* InteractingActor) const
{
	// The reviver being on their feet is the interaction ability's own rule (it is blocked while
	// downed), so here it is only whether there is anybody to revive.
	return InteractingActor != this && IsDowned();
}

void ARSPlayerCharacter::OnInteractionHoldStarted_Implementation(AActor* InteractingActor)
{
	URSAbilitySystemComponent* RSASC = GetRSAbilitySystemComponent();
	if (!RSASC || !IsDowned() || Revivers.Contains(InteractingActor))
	{
		return;
	}

	// Replicated with the tag only, so the downed player's HUD can say they are being revived.
	Revivers.Add(InteractingActor);
	RSASC->AddLooseGameplayTag(RSGameplayTags::State_BeingRevived, 1, EGameplayTagReplicationState::TagOnly);
}

void ARSPlayerCharacter::OnInteractionHoldEnded_Implementation(AActor* InteractingActor)
{
	URSAbilitySystemComponent* RSASC = GetRSAbilitySystemComponent();
	if (!RSASC || Revivers.Remove(InteractingActor) == 0)
	{
		return;
	}

	RSASC->RemoveLooseGameplayTag(RSGameplayTags::State_BeingRevived, 1, EGameplayTagReplicationState::TagOnly);
}

void ARSPlayerCharacter::Interact_Implementation(AActor* InteractingActor)
{
	URSAbilitySystemComponent* RSASC = GetRSAbilitySystemComponent();
	if (!HasAuthority() || !RSASC || !IsDowned())
	{
		return;
	}

	// Whatever made this player downed comes off — the downed ability stops the bleed-out when the
	// tag goes — and with it the crawl, the low camera and the refusals, each of which reads the tag.
	RSASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(RSGameplayTags::State_Downed));

	if (RevivedEffect)
	{
		FGameplayEffectContextHandle Context = RSASC->MakeEffectContext();
		Context.AddInstigator(InteractingActor, InteractingActor);

		RSASC->ApplyGameplayEffectToSelf(RevivedEffect->GetDefaultObject<UGameplayEffect>(), 1.0f, Context);
	}

	PayReviver(InteractingActor);

	// A teammate's hands only. Second Wind and the debug command revive with nobody interacting.
	if (const APawn* ReviverPawn = Cast<APawn>(InteractingActor))
	{
		if (ARSPlayerState* ReviverState = ReviverPawn->GetPlayerState<ARSPlayerState>())
		{
			ReviverState->RecordRevive();
		}
	}

	UE_LOG(LogRSAbility, Log, TEXT("[%s] revived by [%s]."), *GetNameSafe(this), *GetNameSafe(InteractingActor));
}

void ARSPlayerCharacter::PayReviver(AActor* Reviver)
{
	// Taken whether or not anyone is paid, so a revive with no reviver — the debug command — does
	// not leave it owed to the next one.
	ARSPlayerState* RSPS = GetPlayerState<ARSPlayerState>();
	const int32 Amount = RSPS ? RSPS->TakeScrapLostToDown() : 0;

	UAbilitySystemComponent* ReviverASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Reviver);
	if (Amount <= 0 || !ReviverASC || !ReviveRewardEffect)
	{
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = ReviverASC->MakeOutgoingSpec(ReviveRewardEffect, 1.0f, ReviverASC->MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Scrap, static_cast<float>(Amount));
	SpecHandle.Data->AddDynamicAssetTag(RSGameplayTags::Economy_Reward_Revive);

	ReviverASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
}

#undef LOCTEXT_NAMESPACE
