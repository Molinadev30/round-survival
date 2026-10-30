// Round Survival

#include "Interaction/RSBarricade.h"

#include "Interaction/RSBarricadeWindowLayout.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "RoundSurvival"

namespace
{
	/** Declared in `DefaultEngine.ini`; see `RS_ObjectChannel_Barricade`. */
	const FName BarricadeCollisionProfile(TEXT("RSBarricade"));
}

namespace RSBarricadeLayout
{
	/** Board meshes are built once, at the ceiling of MaxBoards; data decides how many are in play. */
	static constexpr int32 BoardMeshCount = 6;

	/** Mesh components kept for the wall itself. Spare ones hide; the layout asset decides. */
	static constexpr int32 FramePieceCount = 8;

	/** Three enemies at a window. More than that and they stand inside one another. */
	static constexpr int32 SlotCount = 3;

	/**
	 * Centre-to-centre gap between neighbouring slots.
	 *
	 * Fifty-two: a capsule is 48 across, so neighbours stand four centimetres apart instead of
	 * overlapping. At 45 they shoved, and the one in the middle, squeezed from both sides, could
	 * be held further from its slot than the walk in accepts as arrived — and then it never tore
	 * or reached at all. The side slots now stand a little outside their gaps; each slot's
	 * `YawOffset` in the layout turns its enemy back towards its own.
	 */
	static constexpr float SlotSeparation = 52.0f;

	/** How far out from the wall's centre plane they stand: against the outside face, a
	 *  centimetre clear of the capsule, so the reach animation's arm gets as far through its gap
	 *  as it can. It is off the navmesh, which ends further out, so the last stretch of the walk
	 *  in is a straight line. */
	static constexpr float SlotStandoff = 35.0f;

	// The wall, measured off the window meshes it is drawn with.
	static constexpr float FrameHalfWidth = 157.5f;
	static constexpr float FrameHeight = 300.0f;
	static constexpr float FrameHalfDepth = 10.0f;
}

ARSBarricade::ARSBarricade(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	// Board state is world state: every client has to agree on whether a window is open. It moves
	// a handful of times a round, so it rides on the actor rather than through an RPC.
	bReplicates = true;
	SetReplicateMovement(false);

	// Ground zero. Everything this class talks about — sill height, where an enemy stands, where
	// a vault lands — is a height off the floor, so the actor's origin is the floor.
	Origin = CreateDefaultSubobject<USceneComponent>(TEXT("Origin"));
	SetRootComponent(Origin);

	Frame = CreateDefaultSubobject<UBoxComponent>(TEXT("Frame"));
	Frame->SetupAttachment(Origin);
	Frame->InitBoxExtent(FVector(RSBarricadeLayout::FrameHalfDepth, RSBarricadeLayout::FrameHalfWidth, RSBarricadeLayout::FrameHeight * 0.5f));
	Frame->SetRelativeLocation(FVector(0.0f, 0.0f, RSBarricadeLayout::FrameHeight * 0.5f));
	Frame->SetCollisionProfileName(TEXT("BlockAll"));
	// Movement and the interaction trace, across the whole opening whatever is left of the boards.
	// Not shots: those are the boards' and the wall's business, so a bullet through a gap goes
	// through and one into a plank stops in it.
	Frame->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

	// Greybox fallback. Real window meshes come in through the layout asset; nothing in this
	// class reads a mesh, so what it is drawn with is content's business and not the code's.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	// CubeMesh is a function-local static, so it is reachable inside the lambda without capture.
	auto MakeMesh = [this](const TCHAR* Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Piece->SetupAttachment(Origin);
		// Solid to shots and to physics bodies such as a ragdoll, never to movement: `Frame`
		// still decides who can walk up to and over the window, so a piece can be hidden or
		// torn down without the navigation or the vault having to know.
		Piece->SetCollisionProfileName(BarricadeCollisionProfile);
		Piece->SetCanEverAffectNavigation(false);
		return Piece;
	};

	FramePieces.Reserve(RSBarricadeLayout::FramePieceCount);
	for (int32 Index = 0; Index < RSBarricadeLayout::FramePieceCount; ++Index)
	{
		FramePieces.Add(MakeMesh(*FString::Printf(TEXT("FramePiece%d"), Index)));
	}

	Boards.Reserve(RSBarricadeLayout::BoardMeshCount);
	for (int32 Index = 0; Index < RSBarricadeLayout::BoardMeshCount; ++Index)
	{
		UStaticMeshComponent* Board = MakeMesh(*FString::Printf(TEXT("Board%d"), Index));
		if (CubeMesh.Succeeded())
		{
			Board->SetStaticMesh(CubeMesh.Object);
		}
		Boards.Add(Board);
	}

	AttackSlots.Reserve(RSBarricadeLayout::SlotCount);
	for (int32 Index = 0; Index < RSBarricadeLayout::SlotCount; ++Index)
	{
		USceneComponent* Slot = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("AttackSlot%d"), Index));
		Slot->SetupAttachment(Origin);
		AttackSlots.Add(Slot);
	}

#if WITH_EDITORONLY_DATA
	// Editor furniture for tuning the reach gaps by eye. Stripped from cooked builds, hidden in PIE.
	ReachGapArrows.Reserve(RSBarricadeLayout::SlotCount);
	for (int32 Index = 0; Index < RSBarricadeLayout::SlotCount; ++Index)
	{
		UArrowComponent* Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(*FString::Printf(TEXT("ReachGapArrow%d"), Index));
		if (Arrow)
		{
			Arrow->SetupAttachment(Origin);
			Arrow->ArrowColor = FColor::Orange;
			Arrow->ArrowSize = 0.4f;
			Arrow->bIsScreenSizeScaled = false;
			ReachGapArrows.Add(Arrow);
		}

		UArrowComponent* Facing = CreateEditorOnlyDefaultSubobject<UArrowComponent>(*FString::Printf(TEXT("ReachFacingArrow%d"), Index));
		if (Facing)
		{
			Facing->SetupAttachment(Origin);
			Facing->ArrowColor = FColor::Cyan;
			Facing->ArrowSize = 0.6f;
			Facing->bIsScreenSizeScaled = false;
			ReachFacingArrows.Add(Facing);
		}
	}
#endif

	// Where each of them actually goes is `ApplyWindowLayout`'s business, so that the constructor,
	// the editor viewport and a runtime spawn all end up with the same window.
	ApplyWindowLayout();
}

void ARSBarricade::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARSBarricade, BoardCount);
}

void ARSBarricade::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// What OnConstruction did is not saved with the level, and a cooked game does not run it for
	// a barricade placed in one: without this pass the window would show none of the layout's
	// meshes. Here it runs before the world begins play, while a static mesh can still change.
	ApplyWindowLayout();
}

void ARSBarricade::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		BoardCount = FMath::Clamp(StartingBoards, 0, MaxBoards);
		SlotHolders.SetNum(AttackSlots.Num());
	}

	// Both sides: the client shows whatever count arrived with the actor, the server shows the one
	// it just decided on.
	ApplyBoardVisuals();
	OnBoardsChanged.Broadcast(this, BoardCount);
}

void ARSBarricade::OnRep_BoardCount()
{
	ApplyBoardVisuals();

	OnBoardsChanged.Broadcast(this, BoardCount);
}

void ARSBarricade::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// So that a designer editing SillHeight or swapping the layout asset sees the window move in
	// the viewport rather than at the next play. The game itself gets it from PostInitializeComponents.
	ApplyWindowLayout();
}

void ARSBarricade::ApplyWindowLayout()
{
	using namespace RSBarricadeLayout;

	// The layout asset is authored in opening space, whose origin is the middle of the sill's top
	// edge. The actor's origin is the ground directly below it, so lifting one onto the other is
	// a single offset — and it is what lets `SillHeight` move the whole window without the asset
	// being touched.
	const FVector ToOpening(0.0f, 0.0f, SillHeight);

	const int32 PieceCount = WindowLayout ? WindowLayout->FramePieces.Num() : 0;
	for (int32 Index = 0; Index < FramePieces.Num(); ++Index)
	{
		UStaticMeshComponent* Piece = FramePieces[Index];
		if (Piece == nullptr)
		{
			continue;
		}

		if (Index < PieceCount)
		{
			const FRSBarricadeFramePiece& Data = WindowLayout->FramePieces[Index];
			FTransform Local = Data.Transform;
			Local.AddToTranslation(ToOpening);
			Piece->SetStaticMesh(Data.Mesh);
			Piece->SetRelativeTransform(Local);
			Piece->SetVisibility(Data.Mesh != nullptr);
		}
		else
		{
			// Spare components. An unconfigured barricade shows no wall rather than a stack of
			// cubes at the origin, and the boards below still say what it is.
			Piece->SetStaticMesh(nullptr);
			Piece->SetVisibility(false);
		}
	}

	// Attack slots are placed here rather than in the constructor for the same reason the boards
	// are: a component that already exists in a saved Blueprint keeps the transform it was saved
	// with, and only a pass like this one moves it to where the current code says it belongs.
	for (int32 Index = 0; Index < AttackSlots.Num(); ++Index)
	{
		USceneComponent* Slot = AttackSlots[Index];
		if (Slot == nullptr)
		{
			continue;
		}

		const float Across = (Index - (AttackSlots.Num() - 1) * 0.5f) * SlotSeparation;
		Slot->SetRelativeLocation(FVector(SlotStandoff, Across, 0.0f));
		Slot->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	}

#if WITH_EDITORONLY_DATA
	// Each gap's arrow comes in from outside and ends on its reach point, where the blow starts.
	// The length is only drawing, and is set here on every rebuild.
	static constexpr float ReachArrowLength = 30.0f;
	for (int32 Index = 0; Index < ReachGapArrows.Num(); ++Index)
	{
		UArrowComponent* Arrow = ReachGapArrows[Index];
		if (Arrow == nullptr)
		{
			continue;
		}

		const bool bHasGap = WindowLayout && WindowLayout->ReachPoints.IsValidIndex(Index);
		Arrow->SetVisibility(bHasGap);
		if (bHasGap)
		{
			Arrow->ArrowLength = ReachArrowLength;
			Arrow->SetRelativeLocation(WindowLayout->ReachPoints[Index].Location + ToOpening + FVector(ReachArrowLength, 0.0f, 0.0f));
			Arrow->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
			Arrow->MarkRenderStateDirty();
		}
	}

	// Each slot's facing arrow stands on the slot and points the way the enemy there faces while
	// it reaches: at the middle of the window, as `FRSSTTask_FaceLocation` turns it, then
	// `YawOffset` further. For lining the reach animation's arm up with its gap by eye.
	static constexpr float FacingArrowHeight = 120.0f;
	for (int32 Index = 0; Index < ReachFacingArrows.Num(); ++Index)
	{
		UArrowComponent* Facing = ReachFacingArrows[Index];
		if (Facing == nullptr)
		{
			continue;
		}

		const bool bHasSlot = AttackSlots.IsValidIndex(Index) && AttackSlots[Index] != nullptr;
		Facing->SetVisibility(bHasSlot);
		if (bHasSlot)
		{
			const FVector SlotLocation = AttackSlots[Index]->GetRelativeLocation();
			const float TowardsWindow = (-SlotLocation).GetSafeNormal2D().Rotation().Yaw;
			Facing->ArrowLength = 60.0f;
			Facing->SetRelativeLocation(SlotLocation + FVector(0.0f, 0.0f, FacingArrowHeight));
			Facing->SetRelativeRotation(FRotator(0.0f, TowardsWindow + GetReachYawOffset(Index), 0.0f));
			Facing->MarkRenderStateDirty();
		}
	}
#endif

	const int32 LayoutCount = WindowLayout ? WindowLayout->Boards.Num() : 0;
	const float FallbackSpacing = 110.0f / (Boards.Num() + 1);

	for (int32 Index = 0; Index < Boards.Num(); ++Index)
	{
		UStaticMeshComponent* Board = Boards[Index];
		if (Board == nullptr)
		{
			continue;
		}

		if (BoardMesh)
		{
			Board->SetStaticMesh(BoardMesh);
		}

		if (BoardMaterial)
		{
			Board->SetMaterial(0, BoardMaterial);
		}

		if (Index < LayoutCount)
		{
			FTransform Local = WindowLayout->Boards[Index];
			Local.AddToTranslation(ToOpening);
			Board->SetRelativeTransform(Local);
		}
		else
		{
			// Even spacing above the sill, so a barricade nobody configured is still a barricade.
			Board->SetRelativeLocation(ToOpening + FVector(0.0f, 0.0f, FallbackSpacing * (Index + 1)));
			Board->SetRelativeRotation(FRotator::ZeroRotator);
			Board->SetRelativeScale3D(FVector(0.3f, 1.9f, 0.25f));
		}
	}
}

FVector ARSBarricade::GetVaultFrontLedge(int32 SlotIndex) const
{
	using namespace RSBarricadeLayout;

	// The outside face, at the top of the sill: where the animation's feet are when it plants.
	return GetActorTransform().TransformPosition(FVector(FrameHalfDepth, GetVaultLaneOffset(SlotIndex), SillHeight));
}

FVector ARSBarricade::GetVaultBackLedge(int32 SlotIndex, float DistanceInside) const
{
	using namespace RSBarricadeLayout;

	return GetActorTransform().TransformPosition(FVector(-(FrameHalfDepth + DistanceInside), GetVaultLaneOffset(SlotIndex), 0.0f));
}

float ARSBarricade::GetVaultLaneOffset(int32 SlotIndex) const
{
	if (!AttackSlots.IsValidIndex(SlotIndex) || AttackSlots[SlotIndex] == nullptr)
	{
		return 0.0f;
	}

	// Where the slot stands across the window, pulled in to what a body fits through.
	const float SlotAcross = AttackSlots[SlotIndex]->GetRelativeLocation().Y;
	return FMath::Clamp(SlotAcross, -VaultLaneLimit, VaultLaneLimit);
}

bool ARSBarricade::GetReachPoint(int32 SlotIndex, FVector& OutLocation) const
{
	if (!WindowLayout || !WindowLayout->ReachPoints.IsValidIndex(SlotIndex))
	{
		return false;
	}

	// Opening space, like the boards: lifted onto this window's sill, then into the world.
	const FVector InOpening = WindowLayout->ReachPoints[SlotIndex].Location + FVector(0.0f, 0.0f, SillHeight);
	OutLocation = GetActorTransform().TransformPosition(InOpening);
	return true;
}

float ARSBarricade::GetReachYawOffset(int32 SlotIndex) const
{
	return WindowLayout && WindowLayout->ReachPoints.IsValidIndex(SlotIndex) ? WindowLayout->ReachPoints[SlotIndex].YawOffset : 0.0f;
}

bool ARSBarricade::IsReachMirrored(int32 SlotIndex) const
{
	return WindowLayout && WindowLayout->ReachPoints.IsValidIndex(SlotIndex) && WindowLayout->ReachPoints[SlotIndex].bMirrored;
}

void ARSBarricade::ApplyBoardVisuals()
{
	for (int32 Index = 0; Index < Boards.Num(); ++Index)
	{
		if (UStaticMeshComponent* Board = Boards[Index])
		{
			const bool bIsUp = Index < FMath::Min(BoardCount, MaxBoards);
			Board->SetVisibility(bIsUp);
			// A torn-off board stops nothing: no shot, no arm, no interaction trace.
			Board->SetCollisionEnabled(bIsUp ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		}
	}
}

FText ARSBarricade::GetPrompt_Implementation(AActor* Interactor) const
{
	// A full barricade says nothing: there is nothing to do here, and a prompt with no action in it
	// would only cover the window the player is watching.
	if (BoardCount >= MaxBoards)
	{
		return FText::GetEmpty();
	}

	return IRSInteractable::MakeActionPrompt(LOCTEXT("BarricadeRepair", "to rebuild Barricade"),
		IRSInteractable::Execute_GetHoldSeconds(this, Interactor));
}

float ARSBarricade::GetHoldSeconds_Implementation(AActor* Interactor) const
{
	return SecondsPerBoard;
}

bool ARSBarricade::CanInteract_Implementation(AActor* Interactor) const
{
	// Going false at the ceiling is what stops a held key: the ability asks this before every
	// repeat, so the player never has to watch a number to know when to let go.
	return BoardCount < MaxBoards;
}

bool ARSBarricade::IsRepeatable_Implementation() const
{
	// One press, board after board. This is the case `IsRepeatable` exists for.
	return true;
}

void ARSBarricade::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority())
	{
		UE_LOG(LogRS, Error, TEXT("ARSBarricade::Interact ran without authority on [%s]."), *GetName());
		return;
	}

	if (BoardCount >= MaxBoards)
	{
		return;
	}

	++BoardCount;
	OnRep_BoardCount();

	// Paid here, for this board, now. Waiting until the hold ended would mean a player driven off
	// the window by a hit walked away with nothing for the boards they did put back.
	AwardRepairScrap(Interactor);

	UE_LOG(LogRS, Verbose, TEXT("[%s] repaired [%s] to %d/%d boards."),
		*GetNameSafe(Interactor), *GetName(), BoardCount, MaxBoards);
}

void ARSBarricade::RepairFully()
{
	if (!HasAuthority())
	{
		UE_LOG(LogRS, Error, TEXT("ARSBarricade::RepairFully ran without authority on [%s]."), *GetName());
		return;
	}

	if (BoardCount >= MaxBoards)
	{
		return;
	}

	BoardCount = MaxBoards;
	OnRep_BoardCount();
}

void ARSBarricade::AwardRepairScrap(AActor* Interactor)
{
	if (ScrapPerBoard <= 0.0f || !ScrapAwardEffect)
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Interactor);
	if (ASC == nullptr)
	{
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(ScrapAwardEffect, 1.0f, ASC->MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Scrap, ScrapPerBoard);

	// The reward tag rides on the spec, the same way a kill's does, so the HUD popup can label a
	// repair without a second notification path.
	SpecHandle.Data->AddDynamicAssetTag(RSGameplayTags::Economy_Reward_Repair);

	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
}

bool ARSBarricade::RemoveBoard(AActor* Attacker)
{
	if (!HasAuthority())
	{
		UE_LOG(LogRS, Error, TEXT("ARSBarricade::RemoveBoard ran without authority on [%s]."), *GetName());
		return false;
	}

	if (BoardCount <= 0)
	{
		return false;
	}

	--BoardCount;
	OnRep_BoardCount();

	UE_LOG(LogRS, Verbose, TEXT("[%s] tore a board off [%s]: %d/%d left."),
		*GetNameSafe(Attacker), *GetName(), BoardCount, MaxBoards);

	return true;
}

void ARSBarricade::JoinQueue(AActor* Claimant)
{
	if (!HasAuthority())
	{
		UE_LOG(LogRS, Error, TEXT("ARSBarricade::JoinQueue ran without authority on [%s]."), *GetName());
		return;
	}

	// Holding a place already is not waiting for one. A task that re-enters its state asks again,
	// and must keep its own place rather than queue for a second and starve the crowd behind it.
	if (Claimant == nullptr || GetSlotHeldBy(Claimant) != INDEX_NONE)
	{
		return;
	}

	QueuedClaimants.AddUnique(Claimant);
	HandFreeSlotsToQueue();
}

void ARSBarricade::LeaveQueue(AActor* Claimant)
{
	QueuedClaimants.Remove(Claimant);
}

int32 ARSBarricade::GetSlotHeldBy(const AActor* Claimant) const
{
	if (Claimant == nullptr)
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < SlotHolders.Num(); ++Index)
	{
		if (SlotHolders[Index].Get() == Claimant)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void ARSBarricade::HandFreeSlotsToQueue()
{
	SlotHolders.SetNum(AttackSlots.Num());

	// Whoever left the world without leaving the queue — destroyed, travelled — is no longer in it.
	QueuedClaimants.RemoveAll([](const TWeakObjectPtr<AActor>& Queued) { return !Queued.IsValid(); });

	for (int32 Index = 0; Index < SlotHolders.Num() && QueuedClaimants.Num() > 0; ++Index)
	{
		// A stale weak pointer is a free slot: an enemy that died at the window releases it by
		// ceasing to exist, so nothing has to clean up after a body.
		if (SlotHolders[Index].Get() != nullptr)
		{
			continue;
		}

		// The queued enemy nearest this place, not the first to ask. Handing places out by who asked
		// first sends enemies across each other's paths to reach a spot somebody else was standing
		// beside, and a crossing pair in a crowd is a pair that arrives late or not at all.
		const FVector SlotLocation = GetSlotTransform(Index).GetLocation();
		int32 Nearest = INDEX_NONE;
		double NearestDistanceSquared = TNumericLimits<double>::Max();

		for (int32 QueueIndex = 0; QueueIndex < QueuedClaimants.Num(); ++QueueIndex)
		{
			const double DistanceSquared = FVector::DistSquared2D(QueuedClaimants[QueueIndex]->GetActorLocation(), SlotLocation);
			if (DistanceSquared < NearestDistanceSquared)
			{
				NearestDistanceSquared = DistanceSquared;
				Nearest = QueueIndex;
			}
		}

		SlotHolders[Index] = QueuedClaimants[Nearest];
		QueuedClaimants.RemoveAtSwap(Nearest);
	}
}

bool ARSBarricade::TryReserveTransit(AActor* Claimant)
{
	if (!HasAuthority())
	{
		UE_LOG(LogRS, Error, TEXT("ARSBarricade::TryReserveTransit ran without authority on [%s]."), *GetName());
		return false;
	}

	if (Claimant == nullptr)
	{
		return false;
	}

	// Already ours, or free. A stale weak pointer counts as free, so an enemy killed mid-vault
	// does not close the window behind it.
	AActor* Current = TransitHolder.Get();
	if (Current == Claimant)
	{
		return true;
	}

	if (Current != nullptr)
	{
		return false;
	}

	TransitHolder = Claimant;
	return true;
}

void ARSBarricade::ReleaseTransit(AActor* Claimant)
{
	if (!HasAuthority())
	{
		return;
	}

	// Only the holder gives it up, for the same reason a slot works that way: an ability ending
	// late must not hand the crossing away from whoever is already halfway through it.
	if (TransitHolder.Get() == Claimant)
	{
		TransitHolder.Reset();
	}
}

void ARSBarricade::ReleaseSlot(int32 SlotIndex, AActor* Claimant)
{
	if (!HasAuthority() || !SlotHolders.IsValidIndex(SlotIndex))
	{
		return;
	}

	// Only the holder can give it up. Otherwise a task ending late would hand away a slot that
	// has already been taken by the next enemy in the queue. A holder being destroyed reads as
	// nobody by now, and gives it up too, so the queue is not left waiting for the next arrival.
	if (SlotHolders[SlotIndex].Get() == Claimant || !SlotHolders[SlotIndex].IsValid())
	{
		SlotHolders[SlotIndex].Reset();
		HandFreeSlotsToQueue();
	}
}

FTransform ARSBarricade::GetSlotTransform(int32 SlotIndex) const
{
	if (!AttackSlots.IsValidIndex(SlotIndex) || AttackSlots[SlotIndex] == nullptr)
	{
		return GetActorTransform();
	}

	return AttackSlots[SlotIndex]->GetComponentTransform();
}

#undef LOCTEXT_NAMESPACE
