// Round Survival

#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "Interaction/RSInteractable.h"

#include "RSPlayerCharacter.generated.h"

class UAnimInstance;
class UCameraComponent;
class UGameplayEffect;
class URSFirstPersonRecoilComponent;
struct FGameplayEffectSpec;
class UInputMappingContext;
class USoundBase;
class URSAbilitySystemComponent;
class URSEquipmentManagerComponent;
class URSInputConfig;
class URSInteractorComponent;
class URSWeaponItemDefinition;
class URSWeaponSlotsComponent;
struct FInputActionValue;

/** What the camera height follows, from highest to lowest. */
enum class ERSCameraPosture : uint8
{
	Standing,
	Crouched,
	Sliding,
	Downed,
};

/**
 * The player pawn.
 *
 * It does not own an ability system component. The ASC lives on ARSPlayerState so that scrap and
 * buffs survive going down, and this pawn is only its avatar — which is why every ASC lookup
 * here goes through the player state and why InitAbilityActorInfo is called from two places.
 *
 * It is also what a teammate revives: a downed player is an interactable, so reviving is the same
 * held interaction a barricade or a wall locker uses, and only while downed does it answer.
 */
UCLASS()
class ROUND_SURVIVAL_API ARSPlayerCharacter : public ACharacter, public IAbilitySystemInterface, public IRSInteractable
{
	GENERATED_BODY()

public:
	ARSPlayerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~End of IAbilitySystemInterface

	//~IRSInteractable
	virtual FText GetPrompt_Implementation(AActor* InteractingActor) const override;
	virtual float GetHoldSeconds_Implementation(AActor* InteractingActor) const override;
	virtual bool CanInteract_Implementation(AActor* InteractingActor) const override;
	virtual bool InterruptsWeapon_Implementation(AActor* InteractingActor) const override { return true; }
	virtual void OnInteractionHoldStarted_Implementation(AActor* InteractingActor) override;
	virtual void OnInteractionHoldEnded_Implementation(AActor* InteractingActor) override;
	virtual void Interact_Implementation(AActor* InteractingActor) override;
	//~End of IRSInteractable

	//~AActor interface
	/** Also aims the camera for a dead teammate watching through these eyes. See the definition. */
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
	//~End of AActor interface

	/** Whether this player is down and waiting to be revived. Read from the ability system's tag. */
	UFUNCTION(BlueprintPure, Category = "RS|Player")
	bool IsDowned() const;

	UFUNCTION(BlueprintCallable, Category = "RS|Player")
	URSAbilitySystemComponent* GetRSAbilitySystemComponent() const;

	/** The viewmodel arms, with the viewmodel copy of the drawn weapon attached. */
	UFUNCTION(BlueprintPure, Category = "RS|Player")
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/**
	 * Whether a view on this machine is looking through this player's eyes: their own, or a dead
	 * teammate's watching them in first person. Asked by the cosmetics that come in two copies, so
	 * a shot flashes and kicks on the copy that is actually on screen here.
	 */
	UFUNCTION(BlueprintPure, Category = "RS|Player")
	bool IsViewedInFirstPerson() const;

	UFUNCTION(BlueprintPure, Category = "RS|Player")
	UCameraComponent* GetCameraComponent() const { return CameraComponent; }

	UFUNCTION(BlueprintPure, Category = "RS|Player")
	URSEquipmentManagerComponent* GetEquipmentManager() const { return EquipmentManager; }

	UFUNCTION(BlueprintPure, Category = "RS|Player")
	URSInteractorComponent* GetInteractor() const { return Interactor; }

	UFUNCTION(BlueprintPure, Category = "RS|Player")
	URSWeaponSlotsComponent* GetWeaponSlots() const { return WeaponSlots; }

	/** Which input actions exist and what tag each one means. The HUD asks it which key a prompt should name. */
	const URSInputConfig* GetInputConfig() const { return InputConfig; }

	/**
	 * How far into the sights this player is, from 0 at the hip to 1 fully aimed, eased over the drawn
	 * weapon's AimDownSightsSeconds. Follows `RS.State.ADS` on the machines that have it — the aiming
	 * player's and the server — and the server's replicated copy of it everywhere else. The camera
	 * zooms by it, the crosshair fades by it, and both meshes and the weapon blend their aimed poses by it.
	 */
	UFUNCTION(BlueprintPure, Category = "RS|Player")
	float GetAimDownSightsAlpha() const { return AimDownSightsAlpha; }

	/**
	 * Whether this player is sprinting, on any machine. Where the moves are made or checked — the
	 * player's own machine and the server — the movement component decides it move by move
	 * (URSCharacterMovementComponent::IsSprinting); everywhere else it is the server's answer,
	 * replicated. Animation reads this.
	 */
	bool IsSprinting() const;

	/** The server's answer after each move it runs, passed on to everybody else. Server only. */
	void SetServerSprinting(bool bInSprinting) { bSprinting = bInSprinting; }

	/** Whether this player is sliding, on any machine: it is a movement mode, and the engine replicates those. */
	bool IsSliding() const;

	//~ACharacter interface
	/** Moves the camera into or out of a slide. */
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;
	//~End of ACharacter interface

protected:
	//~AActor interface
	virtual void PostInitializeComponents() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End of AActor interface

	//~ACharacter interface
	virtual bool CanJumpInternal_Implementation() const override;
	virtual bool CanCrouch() const override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	//~End of ACharacter interface

	//~APawn interface
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_PlayerState() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	//~End of APawn interface

	/**
	 * Points the ability system at this pawn as its avatar.
	 *
	 * Runs on possession on the server and on PlayerState replication on the client, because
	 * neither path alone covers both: PossessedBy never fires on a simulated proxy's owner, and
	 * the player state can arrive before or after the pawn does.
	 */
	void InitAbilitySystem();

	/** Gives the pawn its starting weapons and draws the first. Server only, on possession. */
	void GiveDefaultWeapons();

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);

	/**
	 * Jumps, or, when crouching, only stands up: the jump key gets a crouching player off the floor, not
	 * into the air. A slide is crouched too, and jumps.
	 */
	void Input_Jump();

	/**
	 * Crouches or stands up. Mid-sprint, or in the air with the sprint key down, it asks for a slide
	 * instead; mid-slide, standing up is what cancels it.
	 */
	void Input_Crouch();

	/** The crouch key coming up: a slide not yet started is no longer asked for. */
	void Input_CrouchReleased();
	void Input_AbilityInputTagPressed(FGameplayTag InputTag);
	void Input_AbilityInputTagReleased(FGameplayTag InputTag);

	/** First-person arms. Owner only, and rendered with the camera's first-person settings. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Player", meta = (AllowPrivateAccess = true))
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Player", meta = (AllowPrivateAccess = true))
	TObjectPtr<UCameraComponent> CameraComponent;

	/** Kicks the first-person arms when a weapon fires. Cosmetic and local. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Player", meta = (AllowPrivateAccess = true))
	TObjectPtr<URSFirstPersonRecoilComponent> FirstPersonRecoil;

	/** What this pawn has in its hands. Lives on the pawn, not the player state: equipment is lost on death. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Equipment", meta = (AllowPrivateAccess = true))
	TObjectPtr<URSEquipmentManagerComponent> EquipmentManager;

	/** The weapons this pawn carries and which one is drawn. Also lost on death, for the same reason. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Equipment", meta = (AllowPrivateAccess = true))
	TObjectPtr<URSWeaponSlotsComponent> WeaponSlots;

	/** What this pawn is looking at. On the pawn rather than the controller: the trace starts
	 * from the pawn's own view point, and a spectator with no pawn has nothing to interact with. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Interaction", meta = (AllowPrivateAccess = true))
	TObjectPtr<URSInteractorComponent> Interactor;

	/** Which input actions exist and what tag each one means. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Input")
	TObjectPtr<const URSInputConfig> InputConfig;

	/** Added on the owning client when the pawn is ready for input. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "RS|Input")
	int32 DefaultMappingPriority = 0;

	/**
	 * What this pawn spawns carrying, in slot order; the first is drawn.
	 *
	 * Data on the pawn rather than a rule in the game mode, because what a player starts with is a
	 * property of the character — and it goes through the same GiveWeapon a Wall Locker uses.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Equipment")
	TArray<TObjectPtr<const URSWeaponItemDefinition>> DefaultWeapons;

	/**
	 * The body's post-process graph: the helper bones that keep the skin from folding at the joints.
	 * Set from here rather than on the mesh asset, which belongs to the imported art and names its
	 * own graph.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Animation")
	TSubclassOf<UAnimInstance> BodyPostProcessAnimClass;

	/** Height of the camera above the feet: the body mesh's origin, which the camera hangs from. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Player")
	float CameraHeight = 148.0f;

	/** Height of the camera above the feet while downed: close to the floor. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Player")
	float DownedCameraHeight = 48.0f;

	/** Height of the camera above the feet while crouching. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Player")
	float CrouchedCameraHeight = 98.0f;

	/** Height of the camera above the feet while sliding: lower than crouching. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Player")
	float SlidingCameraHeight = 68.0f;

	/**
	 * How long the camera takes to go from one posture's height to another's, at a constant rate.
	 * Standing and crouching, either way.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Player", meta = (ForceUnits = s, ClampMin = 0.0))
	float CrouchCameraSeconds = 0.2f;

	/** Into a slide, from whatever posture. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Player", meta = (ForceUnits = s, ClampMin = 0.0))
	float SlideCameraSeconds = 0.21f;

	/** Out of a slide, standing up or staying down. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Player", meta = (ForceUnits = s, ClampMin = 0.0))
	float SlideToStandingCameraSeconds = 0.22f;

	UPROPERTY(EditDefaultsOnly, Category = "RS|Player", meta = (ForceUnits = s, ClampMin = 0.0))
	float SlideToCrouchedCameraSeconds = 0.12f;

	/** Going down or getting up. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Player", meta = (ForceUnits = s, ClampMin = 0.0))
	float DownedCameraSeconds = 0.3f;

	/**
	 * How far the view tilts while sliding: up, and rolled to the right. Eased in over SlideViewInSeconds
	 * on a sine, with a wobble on top on both axes. The arms hang from the camera and tilt with it, and
	 * the shot follows the view, so the crosshair stays true.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Slide", meta = (ForceUnits = deg))
	float SlideViewPitch = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "RS|Slide", meta = (ForceUnits = deg))
	float SlideViewRoll = 7.0f;

	UPROPERTY(EditDefaultsOnly, Category = "RS|Slide", meta = (ForceUnits = s, ClampMin = 0.0))
	float SlideViewInSeconds = 0.2f;

	/** The wobble: how far either way, and how long one swing takes. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Slide", meta = (ForceUnits = deg, ClampMin = 0.0))
	float SlideViewWobble = 0.47f;

	UPROPERTY(EditDefaultsOnly, Category = "RS|Slide", meta = (ForceUnits = s, ClampMin = 0.0))
	float SlideViewWobblePeriod = 0.3f;

	/** Once the slide is over, the fraction of the tilt left that goes each second. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Slide", meta = (ClampMin = 0.0))
	float SlideViewRecoveryRate = 10.0f;

	/** Played at the pawn as a slide starts, on every machine. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Slide")
	TObjectPtr<USoundBase> SlideStartSound;

	/**
	 * How long a teammate holds the interact key to revive this player, divided by the reviver's
	 * ReviveSpeedMultiplier (Second Wind halves it).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Revive", meta = (ForceUnits = s, ClampMin = 0.0))
	float ReviveHoldSeconds = 4.0f;

	/**
	 * Applied to this player when a revive completes, after the downed state is taken off: how much
	 * health they get up with. Regeneration brings the rest.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Revive")
	TSubclassOf<UGameplayEffect> RevivedEffect;

	/**
	 * Pays the reviver what this player lost going down: the scrap award effect, `GE_RS_ScrapAward`,
	 * so it is income like any other and shows on the reviver's HUD.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Revive")
	TSubclassOf<UGameplayEffect> ReviveRewardEffect;

private:
	/**
	 * Listens to the ability system for what this pawn shows of the player's state: the downed tag
	 * and the movement speed multiplier. Runs on every machine, from InitAbilitySystem, and binds
	 * once per ability system — the ASC lives on the player state and outlives this pawn.
	 */
	void BindToAbilitySystem(URSAbilitySystemComponent* RSASC);
	void UnbindFromAbilitySystem();

	void HandleDownedTagChanged(const FGameplayTag Tag, int32 NewCount);
	void HandleAimDownSightsTagChanged(const FGameplayTag Tag, int32 NewCount);

	UFUNCTION()
	void OnRep_AimingDownSights();

	/** Moves the aim towards the hip or the sights. */
	void SetTargetAimDownSights(bool bAiming);
	void HandleMoveSpeedMultiplierChanged(AActor* EffectInstigator, AActor* Causer, const FGameplayEffectSpec* Spec,
		float Magnitude, float OldValue, float NewValue);

	/** Walk speed is the Blueprint's, scaled by URSCombatSet::MoveSpeedMultiplier. */
	void RefreshMovementSpeed();

	void HandleSprintDurationMultiplierChanged(const struct FOnAttributeChangeData& ChangeData);

	/** Pushes URSHandlingSet::SprintDurationMultiplier into the movement component, which spends the sprint. */
	void RefreshSprintDuration();

	/** Hands the reviver what this player lost going down. Nothing when the revive had no reviver. */
	void PayReviver(AActor* Reviver);

	/**
	 * The player's posture — downed over sliding over crouching over standing — and, when it has changed,
	 * starts the camera moving to its height at the rate that change takes.
	 */
	void RefreshCameraHeight();

	float GetCameraHeight(ERSCameraPosture Posture) const;
	float GetCameraTransitionSeconds(ERSCameraPosture From, ERSCameraPosture To) const;

	/** Ticks while the camera height, the aim or the slide's tilt is moving, and not otherwise. */
	void RefreshTickEnabled();

	/** Moves the slide's view tilt on: in and wobbling while sliding, easing back to nothing after. */
	void UpdateSlideView(float DeltaSeconds);

	/** The ranged weapon in this pawn's hands, or null. */
	const class URSRangedWeaponInstance* GetDrawnRangedWeapon() const;

	/**
	 * Teammates holding the revive on this player right now. Server only. Each one put one count of
	 * RS.State.BeingRevived on, which is what pauses the bleed-out, and takes exactly that one off.
	 */
	TArray<TWeakObjectPtr<AActor>> Revivers;

	TWeakObjectPtr<URSAbilitySystemComponent> BoundAbilitySystem;
	FDelegateHandle DownedTagChangedHandle;
	FDelegateHandle AimDownSightsTagChangedHandle;
	FDelegateHandle MoveSpeedChangedHandle;
	FDelegateHandle SprintDurationChangedHandle;

	/**
	 * RS.State.ADS as the server sees it, for the machines that never get the tag: everyone's but the
	 * aiming player's own. The tag is owned by the aim ability's activation, and an activation exists
	 * only where the ability runs — the aiming player's machine and the server. The aiming player
	 * skips this copy: their own prediction is already ahead of it.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_AimingDownSights)
	bool bAimingDownSights = false;

	/**
	 * URSCharacterMovementComponent::IsSprinting as the server last found it, for the machines that
	 * never see the sprint flag: it only travels in the owner's moves to the server. One bit, sent
	 * only when it changes; the owner skips it, being ahead of it.
	 */
	UPROPERTY(Replicated)
	bool bSprinting = false;

	/** See GetAimDownSightsAlpha. The target is 1 while aiming, 0 otherwise. */
	float AimDownSightsAlpha = 0.0f;
	float TargetAimDownSightsAlpha = 0.0f;

	/** MaxWalkSpeed as the Blueprint set it, before any multiplier. */
	float BaseMaxWalkSpeed = 0.0f;

	/** Camera height as the Blueprint set it, and the one the camera is moving to. */
	float StandingCameraHeight = 0.0f;
	float TargetCameraHeight = 0.0f;

	/** The posture the camera is at or moving to, and how fast it moves there, in cm/s. */
	ERSCameraPosture CameraPosture = ERSCameraPosture::Standing;
	float CameraHeightSpeed = 0.0f;

	/** The slide's tilt on the view as it stands, and how long the current slide has been on screen. */
	FRotator SlideViewOffset = FRotator::ZeroRotator;
	float SlideViewSeconds = 0.0f;

	/** Handles for the generic ability bindings, so they can be torn down on unpossess. */
	TArray<uint32> AbilityBindHandles;
};
