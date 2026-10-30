// Round Survival

#include "Interaction/RSInteractorComponent.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/RSInteractable.h"
#include "RSCollisionChannels.h"
#include "RSLogChannels.h"
#include "TimerManager.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#endif

namespace RSInteractionDebug
{
#if ENABLE_DRAW_DEBUG
	static TAutoConsoleVariable<int32> CVarDrawInteraction(
		TEXT("RS.Interact.Debug"),
		0,
		TEXT("Draw the interaction focus trace and the actor it found."),
		ECVF_Cheat);

	static bool IsEnabled()
	{
		return CVarDrawInteraction.GetValueOnGameThread() > 0;
	}
#else
	static bool IsEnabled()
	{
		return false;
	}
#endif
}

URSInteractorComponent::URSInteractorComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The focus trace is a timer, not a tick: at ten a second it costs a fifth of what ticking
	// would, and a prompt that appears a frame later than it could is not something a player sees.
	PrimaryComponentTick.bCanEverTick = false;
}

bool URSInteractorComponent::ShouldTrack() const
{
	const APawn* Pawn = GetPawn<APawn>();
	if (Pawn == nullptr)
	{
		return false;
	}

	// The client traces to draw a prompt, the server traces because it decides what was used.
	// On a listen server the host is both, and one timer covers it.
	return Pawn->IsLocallyControlled() || Pawn->HasAuthority();
}

void URSInteractorComponent::BeginPlay()
{
	Super::BeginPlay();

	// The pawn can have switched the focus off already: a client whose pawn arrives while its player
	// is down binds to the ability system before this begins play.
	if (ShouldTrack() && bFocusEnabled)
	{
		StartFocusTimer();
	}
}

void URSInteractorComponent::StartFocusTimer()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// Staggered like the enemy targeting component: players joining on the same frame would
	// otherwise trace on the same frame for the rest of the match.
	const float FirstDelay = FMath::FRandRange(KINDA_SMALL_NUMBER, FocusRefreshInterval);
	World->GetTimerManager().SetTimer(FocusTimerHandle, this, &ThisClass::RefreshFocus, FocusRefreshInterval, /*bLoop=*/ true, FirstDelay);
}

void URSInteractorComponent::SetFocusEnabled(bool bEnabled)
{
	if (bFocusEnabled == bEnabled)
	{
		return;
	}

	bFocusEnabled = bEnabled;

	if (bEnabled)
	{
		// Before begin play the timer is started there instead, once the pawn knows who controls it.
		if (HasBegunPlay() && ShouldTrack())
		{
			StartFocusTimer();
		}

		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FocusTimerHandle);
	}

	// Cleared now rather than on a next trace that will not come: the prompt leaves the HUD at once.
	SetFocus(nullptr);
	RefreshPrompt();
}

void URSInteractorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FocusTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

AActor* URSInteractorComponent::ResolveInteractable(AActor* HitActor)
{
	// The trace can land on a child actor — a board on a barricade, a mesh actor a Blueprint
	// spawned — so the chain is walked up to whoever actually implements the interface.
	for (AActor* Actor = HitActor; Actor != nullptr; Actor = Actor->GetAttachParentActor())
	{
		if (Actor->Implements<URSInteractable>())
		{
			return Actor;
		}
	}

	return nullptr;
}

AActor* URSInteractorComponent::TraceForInteractable() const
{
	APawn* Pawn = GetPawn<APawn>();
	UWorld* World = GetWorld();

	if (Pawn == nullptr || World == nullptr)
	{
		return nullptr;
	}

	// The same view point the weapon traces from: on the server this is the replicated control
	// rotation, which is the input the client used, which is what makes the two agree.
	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;

	if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else if (const AController* Controller = Pawn->GetController())
	{
		ViewLocation = Pawn->GetPawnViewLocation();
		ViewRotation = Controller->GetControlRotation();
	}
	else
	{
		return nullptr;
	}

	const FVector TraceEnd = ViewLocation + (ViewRotation.Vector().GetSafeNormal() * MaxInteractionDistance);

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(RSInteractionFocus), /*bTraceComplex=*/ false, /*IgnoreActor=*/ Pawn);

	// One blocking hit, and if it is not an interactable there is no focus. That is the line of
	// sight rule for free: a locker behind a wall is not focusable because the wall is hit first.
	FHitResult Hit;
	World->SweepSingleByChannel(Hit, ViewLocation, TraceEnd, FQuat::Identity, RS_TraceChannel_Interaction,
		FCollisionShape::MakeSphere(TraceRadius), TraceParams);

	AActor* Interactable = Hit.bBlockingHit ? ResolveInteractable(Hit.GetActor()) : nullptr;

#if ENABLE_DRAW_DEBUG
	if (RSInteractionDebug::IsEnabled())
	{
		const FColor Colour = Interactable ? FColor::Green : FColor::Silver;
		const FVector DrawEnd = Hit.bBlockingHit ? Hit.ImpactPoint : TraceEnd;

		DrawDebugLine(World, ViewLocation, DrawEnd, Colour, /*bPersistent=*/ false, FocusRefreshInterval, 0, 1.0f);
		DrawDebugSphere(World, DrawEnd, TraceRadius, 12, Colour, /*bPersistent=*/ false, FocusRefreshInterval);

		if (Interactable)
		{
			DrawDebugString(World, Interactable->GetActorLocation(),
				FString::Printf(TEXT("%s%s"), *GetNameSafe(Interactable),
					IRSInteractable::Execute_CanInteract(Interactable, Pawn) ? TEXT("") : TEXT(" (blocked)")),
				nullptr, Colour, FocusRefreshInterval, /*bDrawShadow=*/ true);
		}
	}
#endif

	return Interactable;
}

void URSInteractorComponent::RefreshFocus()
{
	// Also the server's one-off trace on an activation with an empty cache: switched off, there is
	// nothing to find.
	if (!bFocusEnabled)
	{
		return;
	}

	SetFocus(TraceForInteractable());
	RefreshPrompt();
}

void URSInteractorComponent::SetFocus(AActor* NewFocus)
{
	if (NewFocus == FocusedActor.Get())
	{
		return;
	}

	FocusedActor = NewFocus;

	UE_LOG(LogRS, Verbose, TEXT("Interaction focus on [%s] is now [%s]."),
		*GetNameSafe(GetOwner()), *GetNameSafe(NewFocus));

	OnFocusChanged.Broadcast(NewFocus);
}

void URSInteractorComponent::RefreshPrompt()
{
	// Only the player looking through this pawn reads a prompt; the server's copy traces to rule
	// on interactions and has nothing to draw.
	const APawn* Pawn = GetPawn<APawn>();
	if (Pawn == nullptr || !Pawn->IsLocallyControlled())
	{
		return;
	}

	// Asked again on every pass, not only when the focus moves: a prompt changes with the focus
	// held still — scrap arrives, a board goes back up, a weapon bought turns Buy into Refill — and
	// none of those owe the interactor a notification. Compared by content, because the prompts
	// are formatted fresh each time and a fresh FText is never identical to the last one.
	const FText NewPrompt = GetFocusPrompt();
	if (!NewPrompt.ToString().Equals(FocusPrompt.ToString(), ESearchCase::CaseSensitive))
	{
		FocusPrompt = NewPrompt;
		OnFocusPromptChanged.Broadcast(FocusPrompt);
	}
}

FText URSInteractorComponent::GetFocusPrompt() const
{
	AActor* Focus = FocusedActor.Get();
	if (Focus == nullptr)
	{
		return FText::GetEmpty();
	}

	return IRSInteractable::Execute_GetPrompt(Focus, GetPawn<APawn>());
}

void URSInteractorComponent::SetHoldProgress(float NewProgress)
{
	const float Clamped = FMath::Clamp(NewProgress, 0.0f, 1.0f);

	if (FMath::IsNearlyEqual(Clamped, HoldProgress))
	{
		return;
	}

	HoldProgress = Clamped;
	OnHoldProgressChanged.Broadcast(HoldProgress);
}
