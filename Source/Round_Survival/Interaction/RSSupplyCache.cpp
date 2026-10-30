// Round Survival

#include "Interaction/RSSupplyCache.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Interaction/RSSupplyCacheSpot.h"
#include "Interaction/RSSupplyCacheTable.h"
#include "NavAreas/NavArea_Null.h"
#include "Net/UnrealNetwork.h"
#include "RSGameplayTags.h"
#include "RSLogChannels.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/RSWeaponAssemblyComponent.h"
#include "Weapons/RSWeaponItemDefinition.h"
#include "Weapons/RSWeaponSlotsComponent.h"

#define LOCTEXT_NAMESPACE "RoundSurvival"

ARSSupplyCache::ARSSupplyCache()
{
	DisplayName = LOCTEXT("SupplyCacheName", "Supply Cache");
	Cost = 950;

	SetReplicateMovement(false);

	// Ground zero, like the stations: placed on the floor, with the crate standing on it.
	Origin = CreateDefaultSubobject<USceneComponent>(TEXT("Origin"));
	SetRootComponent(Origin);

	Mesh->SetupAttachment(Origin);
	Mesh->SetMobility(EComponentMobility::Movable);

	// Kept out of the navigation, which NavBlocker answers for: baked into the navmesh, the crate
	// would leave a hole on its first spot after it moved and none on the next.
	Mesh->SetCanEverAffectNavigation(false);

	// Greybox fallback, a crate 140 x 70 x 80 cm with its long side along Y. A Blueprint swaps the mesh.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
		Mesh->SetRelativeScale3D(FVector(0.7f, 1.4f, 0.8f));
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	}

	// The middle of the floating weapon, 110 cm over the centre of the crate. Each weapon the roll
	// shows centres itself on it, so a long one does not hang further off one end than a short one.
	DisplayAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("DisplayAnchor"));
	DisplayAnchor->SetupAttachment(Origin);
	DisplayAnchor->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));

	DisplayMesh = CreateDefaultSubobject<URSWeaponAssemblyComponent>(TEXT("DisplayMesh"));
	DisplayMesh->SetupAttachment(DisplayAnchor);
	DisplayMesh->bCentreOnAttachParent = true;

	// A prop in its reference pose, like the weapon on a Wall Locker.
	DisplayMesh->PrimaryComponentTick.bCanEverTick = false;
	DisplayMesh->bNoSkeletonUpdate = true;

	// No collision: the crate is what the player aims at.
	DisplayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DisplayMesh->SetGenerateOverlapEvents(false);
	DisplayMesh->CanCharacterStepUpOn = ECB_No;

	NavBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("NavBlocker"));
	NavBlocker->SetupAttachment(Origin);
	NavBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NavBlocker->SetGenerateOverlapEvents(false);
	NavBlocker->SetCanEverAffectNavigation(true);
	// The Sector Gate's arrangement: an area modifier rather than geometry, so it moves with the
	// cache and leaves with it without rebuilding anything but its own tiles, and Null, because the
	// system's default obstacle area only makes a path expensive.
	NavBlocker->bDynamicObstacle = true;
	NavBlocker->SetAreaClassOverride(UNavArea_Null::StaticClass());

	Beam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam"));
	Beam->SetupAttachment(Origin);
	Beam->SetMobility(EComponentMobility::Movable);
	Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beam->SetGenerateOverlapEvents(false);
	Beam->SetCanEverAffectNavigation(false);
	Beam->CanCharacterStepUpOn = ECB_No;
	Beam->CastShadow = false;

	// Greybox fallback, a column 60 cm wide rising 40 m from the top of the crate.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		Beam->SetStaticMesh(CylinderMesh.Object);
		Beam->SetRelativeScale3D(FVector(0.6f, 0.6f, 40.0f));
		Beam->SetRelativeLocation(FVector(0.0f, 0.0f, 2080.0f));
	}
}

void ARSSupplyCache::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	FitNavBlockerToMesh();
}

void ARSSupplyCache::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// OnConstruction is the editor's preview: a cooked game does not run it for a cache placed in
	// the level, and what it did is not saved with it.
	FitNavBlockerToMesh();
}

void ARSSupplyCache::FitNavBlockerToMesh()
{
	const UStaticMesh* CrateMesh = Mesh->GetStaticMesh();
	if (CrateMesh == nullptr)
	{
		return;
	}

	const FBoxSphereBounds LocalBounds = CrateMesh->GetBounds();
	const FVector Scale = Mesh->GetRelativeScale3D();
	const FQuat Rotation = Mesh->GetRelativeRotation().Quaternion();

	NavBlocker->SetRelativeLocationAndRotation(Mesh->GetRelativeLocation() + Rotation.RotateVector(LocalBounds.Origin * Scale), Rotation);
	NavBlocker->SetBoxExtent(LocalBounds.BoxExtent * Scale.GetAbs(), /*bUpdateOverlaps=*/ false);
}

void ARSSupplyCache::BeginPlay()
{
	Super::BeginPlay();

	// Standing on one of its spots from the start counts as being on it, so the first move never
	// picks the place it already is.
	if (HasAuthority() && CurrentSpot == nullptr)
	{
		for (ARSSupplyCacheSpot* Spot : Spots)
		{
			if (Spot && FVector::DistSquared2D(Spot->GetActorLocation(), GetActorLocation()) < FMath::Square(100.0f))
			{
				CurrentSpot = Spot;
				break;
			}
		}
	}

	ApplyPhase();
	RefreshPresentation();
}

void ARSSupplyCache::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ARSSupplyCache::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, Phase);
	DOREPLIFETIME(ThisClass, OfferedItem);
	DOREPLIFETIME(ThisClass, Buyer);
	DOREPLIFETIME(ThisClass, CurrentSpot);
}

URSWeaponSlotsComponent* ARSSupplyCache::FindWeaponSlots(AActor* Interactor)
{
	return Interactor ? Interactor->FindComponentByClass<URSWeaponSlotsComponent>() : nullptr;
}

bool ARSSupplyCache::IsBuyer(const AActor* Interactor) const
{
	return Interactor != nullptr && Interactor == Buyer;
}

int32 ARSSupplyCache::GetScrapCost_Implementation(AActor* Interactor) const
{
	// The weapon on offer was paid for when the roll started.
	if (Phase != ERSSupplyCachePhase::Idle)
	{
		return 0;
	}

	// Asked of the buyer, whose tags replicate to their own machine, so the prompt reads the same
	// price the server charges.
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Interactor);
	return ASC && ASC->HasMatchingGameplayTag(RSGameplayTags::PowerUp_Markdown) ? MarkdownCost : Cost;
}

bool ARSSupplyCache::IsAvailable_Implementation(AActor* Interactor) const
{
	const URSWeaponSlotsComponent* Slots = FindWeaponSlots(Interactor);
	if (Table == nullptr || Slots == nullptr)
	{
		return false;
	}

	switch (Phase)
	{
	case ERSSupplyCachePhase::Idle:
		// Refused before paying: a draw with nothing this player can carry would take the scrap and
		// hand over nothing.
		return Table->HasAnythingFor(Slots);

	case ERSSupplyCachePhase::Offering:
		// Only the buyer, and not if they picked the same weapon up elsewhere while it rolled: their
		// slots would refuse a second copy.
		return IsBuyer(Interactor) && OfferedItem && Slots->FindItemByDefinition(OfferedItem) == nullptr;

	default:
		return false;
	}
}

FText ARSSupplyCache::GetPromptName(AActor* Interactor) const
{
	if (Phase == ERSSupplyCachePhase::Offering && OfferedItem)
	{
		return OfferedItem->DisplayName.IsEmpty() ? FText::FromString(OfferedItem->GetName()) : OfferedItem->DisplayName;
	}

	return Super::GetPromptName(Interactor);
}

FText ARSSupplyCache::GetUnavailablePrompt(AActor* Interactor) const
{
	// To everyone looking, not only the buyer: the cache is about to be gone for all of them.
	if (Phase == ERSSupplyCachePhase::Leaving)
	{
		return FText::Format(LOCTEXT("SupplyCacheMoving", "The {0} is moving"), DisplayName);
	}

	return Super::GetUnavailablePrompt(Interactor);
}

void ARSSupplyCache::OnPurchased_Implementation(AActor* Interactor)
{
	URSWeaponSlotsComponent* Slots = FindWeaponSlots(Interactor);
	if (Slots == nullptr)
	{
		// IsAvailable refuses this before the cost is committed; reaching here means it was bypassed.
		UE_LOG(LogRS, Error, TEXT("[%s] was used by [%s], which carries no weapons."), *GetName(), *GetNameSafe(Interactor));
		return;
	}

	// Asked again rather than remembered from the prompt: this is the server's phase at the moment
	// the price was charged, and the price was decided by the same question.
	switch (Phase)
	{
	case ERSSupplyCachePhase::Idle:
		StartRoll(Interactor, *Slots);
		break;

	case ERSSupplyCachePhase::Offering:
		HandOverOffer(Interactor, *Slots);
		break;

	default:
		UE_LOG(LogRS, Error, TEXT("[%s] was used by [%s] while it rolls."), *GetName(), *GetNameSafe(Interactor));
		break;
	}
}

void ARSSupplyCache::StartRoll(AActor* InBuyer, URSWeaponSlotsComponent& Slots)
{
	// The phase is still Idle, so this is the price just charged, Markdown's if it was on.
	PaidCost = GetScrapCost_Implementation(InBuyer);

	// Decided with the draw and kept with it, on the server, until the roll ends.
	bLeavingThisRoll = ShouldMoveOnThisRoll();
	++RollsHere;

	DrawnItem = bLeavingThisRoll ? nullptr : Table ? Table->Roll(&Slots) : nullptr;
	if (DrawnItem == nullptr && !bLeavingThisRoll)
	{
		UE_LOG(LogRS, Error, TEXT("[%s] was paid by [%s] but had nothing to draw."), *GetName(), *GetNameSafe(InBuyer));
		return;
	}

	Buyer = InBuyer;
	SetPhase(ERSSupplyCachePhase::Rolling);

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ThisClass::HandleRollFinished, FMath::Max(RollSeconds, KINDA_SMALL_NUMBER));

	UE_LOG(LogRS, Log, TEXT("[%s] bought roll %d at [%s]: %s in %.1f s."), *GetNameSafe(InBuyer), RollsHere, *GetName(),
		bLeavingThisRoll ? TEXT("it moves") : *FString::Printf(TEXT("[%s]"), *GetNameSafe(DrawnItem)), RollSeconds);
}

bool ARSSupplyCache::ShouldMoveOnThisRoll()
{
	// Nothing moves during a sale, and a sale cache never does; a forced move waits for the sale to end.
	if (bSaleOn || bSaleCache)
	{
		return false;
	}

	const bool bForced = bForceMoveOnNextRoll;
	bForceMoveOnNextRoll = false;

	// A cache with nowhere else to stand never leaves.
	if (PickNextSpot() == nullptr)
	{
		return false;
	}

	return bForced || (RollsHere >= MinRollsBeforeMove && FMath::FRand() < MoveChance);
}

ARSSupplyCacheSpot* ARSSupplyCache::PickNextSpot() const
{
	TArray<ARSSupplyCacheSpot*, TInlineAllocator<8>> Candidates;

	for (ARSSupplyCacheSpot* Spot : Spots)
	{
		// Never where it is: not its spot, and not a spot under it that it was never told it was on.
		if (Spot && Spot != CurrentSpot && FVector::DistSquared2D(Spot->GetActorLocation(), GetActorLocation()) >= FMath::Square(100.0f))
		{
			Candidates.Add(Spot);
		}
	}

	return Candidates.IsEmpty() ? nullptr : Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
}

void ARSSupplyCache::HandleRollFinished()
{
	// A sale that started while this roll was under way keeps the cache where it is: the roll draws
	// now, like any other, instead of leaving.
	if (bLeavingThisRoll && bSaleOn)
	{
		bLeavingThisRoll = false;
		const URSWeaponSlotsComponent* Slots = FindWeaponSlots(Buyer);
		DrawnItem = Table && Slots ? Table->Roll(Slots) : nullptr;

		if (DrawnItem == nullptr)
		{
			RefundBuyer();
			ResetCycle();
			return;
		}
	}

	if (bLeavingThisRoll)
	{
		StartLeaving();
		return;
	}

	OfferedItem = DrawnItem;
	DrawnItem = nullptr;
	SetPhase(ERSSupplyCachePhase::Offering);

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ThisClass::HandleOfferExpired, OfferSeconds);
}

void ARSSupplyCache::StartLeaving()
{
	bLeavingThisRoll = false;

	RefundBuyer();
	SetPhase(ERSSupplyCachePhase::Leaving);

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ThisClass::HandleLeft, FMath::Max(LeaveSeconds, KINDA_SMALL_NUMBER));
}

void ARSSupplyCache::RefundBuyer()
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Buyer);
	if (PaidCost <= 0 || ASC == nullptr)
	{
		return;
	}

	if (RefundEffect == nullptr)
	{
		UE_LOG(LogRS, Error, TEXT("[%s] owes [%s] %d scrap but has no RefundEffect to pay it with."), *GetName(), *GetNameSafe(Buyer), PaidCost);
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(RefundEffect, 1.0f, ASC->MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return;
	}

	// Positive through the effect a purchase charges with, so it lands on the balance as it is.
	SpecHandle.Data->SetSetByCallerMagnitude(RSGameplayTags::SetByCaller_Scrap, static_cast<float>(PaidCost));
	SpecHandle.Data->AddDynamicAssetTag(RSGameplayTags::Economy_Refund);
	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);

	UE_LOG(LogRS, Log, TEXT("[%s] paid [%s] back %d scrap: it is moving."), *GetName(), *GetNameSafe(Buyer), PaidCost);
}

void ARSSupplyCache::HandleLeft()
{
	ARSSupplyCacheSpot* NextSpot = PickNextSpot();
	if (NextSpot == nullptr)
	{
		// The spots were there when the roll was paid for; nothing removes one in play.
		UE_LOG(LogRS, Error, TEXT("[%s] was leaving but has nowhere to go."), *GetName());
		ResetCycle();
		return;
	}

	UE_LOG(LogRS, Log, TEXT("[%s] left [%s] for [%s], back in %.1f s."), *GetName(), *GetNameSafe(CurrentSpot), *GetNameSafe(NextSpot), AwaySeconds);

	// Moved as it goes out of sight, so every machine has it on the new spot long before it shows.
	CurrentSpot = NextSpot;
	RollsHere = 0;
	MoveToCurrentSpot();
	SetPhase(ERSSupplyCachePhase::Away);

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ThisClass::HandleArrived, FMath::Max(AwaySeconds, KINDA_SMALL_NUMBER));
}

void ARSSupplyCache::HandleArrived()
{
	UE_LOG(LogRS, Log, TEXT("[%s] is back, at [%s]."), *GetName(), *GetNameSafe(CurrentSpot));

	ResetCycle();
}

void ARSSupplyCache::StartSale(float Seconds)
{
	if (!HasAuthority() || bSaleCache)
	{
		return;
	}

	bSaleOn = true;
	GetWorldTimerManager().SetTimer(SaleTimerHandle, this, &ThisClass::EndSale, Seconds);

	SaleCaches.RemoveAll([](const ARSSupplyCache* Cache) { return !IsValid(Cache); });

	int32 Spawned = 0;
	for (ARSSupplyCacheSpot* Spot : Spots)
	{
		if (Spot == nullptr || IsSpotTaken(Spot))
		{
			continue;
		}

		// The same class as this one, so it looks, prices and draws the same; tuned like this one.
		ARSSupplyCache* SaleCache = GetWorld()->SpawnActorDeferred<ARSSupplyCache>(GetClass(), Spot->GetActorTransform());
		if (SaleCache == nullptr)
		{
			continue;
		}

		SaleCache->bSaleCache = true;
		SaleCache->CurrentSpot = Spot;
		SaleCache->Table = Table;
		SaleCache->Cost = Cost;
		SaleCache->MarkdownCost = MarkdownCost;
		SaleCache->RollSeconds = RollSeconds;
		SaleCache->OfferSeconds = OfferSeconds;
		SaleCache->FinishSpawning(Spot->GetActorTransform());

		SaleCaches.Add(SaleCache);
		++Spawned;
	}

	UE_LOG(LogRS, Log, TEXT("[%s] is on sale for %.1f s: %d more cache(s), %d in all."), *GetName(), Seconds, Spawned, SaleCaches.Num() + 1);
}

bool ARSSupplyCache::IsSpotTaken(const ARSSupplyCacheSpot* Spot) const
{
	// This cache's own spot is taken even while it is away: it is coming back there.
	if (Spot == CurrentSpot || FVector::DistSquared2D(Spot->GetActorLocation(), GetActorLocation()) < FMath::Square(100.0f))
	{
		return true;
	}

	return SaleCaches.ContainsByPredicate([Spot](const ARSSupplyCache* Cache) { return IsValid(Cache) && Cache->CurrentSpot == Spot && !Cache->bRetiring; });
}

void ARSSupplyCache::EndSale()
{
	bSaleOn = false;

	for (ARSSupplyCache* Cache : SaleCaches)
	{
		if (IsValid(Cache))
		{
			Cache->Retire();
		}
	}
	SaleCaches.Reset();

	UE_LOG(LogRS, Log, TEXT("[%s]'s sale is over."), *GetName());
}

void ARSSupplyCache::Retire()
{
	if (Phase == ERSSupplyCachePhase::Idle)
	{
		Destroy();
		return;
	}

	// Its buyer paid for the roll under way and gets to see it through; it goes when that cycle ends.
	bRetiring = true;
}

void ARSSupplyCache::HandleOfferExpired()
{
	UE_LOG(LogRS, Log, TEXT("[%s] took back [%s]: [%s] left it."), *GetName(), *GetNameSafe(OfferedItem), *GetNameSafe(Buyer));

	ResetCycle();
}

void ARSSupplyCache::HandOverOffer(AActor* Interactor, URSWeaponSlotsComponent& Slots)
{
	if (!IsBuyer(Interactor) || OfferedItem == nullptr)
	{
		UE_LOG(LogRS, Error, TEXT("[%s] refused [%s]: not the buyer, or nothing on offer."), *GetName(), *GetNameSafe(Interactor));
		return;
	}

	if (!Slots.GiveWeapon(OfferedItem, /*bDraw=*/ true))
	{
		UE_LOG(LogRS, Error, TEXT("[%s] could not hand [%s] to [%s]: GiveWeapon refused it."), *GetName(), *GetNameSafe(OfferedItem), *GetNameSafe(Interactor));
		return;
	}

	UE_LOG(LogRS, Log, TEXT("[%s] took [%s] from [%s]."), *GetNameSafe(Interactor), *GetNameSafe(OfferedItem), *GetName());

	ResetCycle();
}

void ARSSupplyCache::ResetCycle()
{
	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);

	DrawnItem = nullptr;
	OfferedItem = nullptr;
	Buyer = nullptr;
	bLeavingThisRoll = false;
	PaidCost = 0;
	SetPhase(ERSSupplyCachePhase::Idle);

	if (bRetiring)
	{
		Destroy();
	}
}

void ARSSupplyCache::SetPhase(ERSSupplyCachePhase NewPhase)
{
	Phase = NewPhase;

	// The rep notify does not run where the value was set; a listen server's host applies it here.
	ApplyPhase();
	RefreshPresentation();
}

void ARSSupplyCache::ApplyPhase()
{
	// On every machine, not only those that draw: the server's interaction trace and its enemies'
	// paths have to agree with what the players see.
	const bool bAway = Phase == ERSSupplyCachePhase::Away;

	SetActorHiddenInGame(bAway);
	SetActorEnableCollision(!bAway);

	if (NavBlocker->CanEverAffectNavigation() == bAway)
	{
		NavBlocker->SetCanEverAffectNavigation(!bAway);
	}
}

void ARSSupplyCache::MoveToCurrentSpot()
{
	if (CurrentSpot)
	{
		SetActorLocationAndRotation(CurrentSpot->GetActorLocation(), CurrentSpot->GetActorRotation(), /*bSweep=*/ false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void ARSSupplyCache::OnRep_Phase()
{
	ApplyPhase();
	RefreshPresentation();
}

void ARSSupplyCache::OnRep_CurrentSpot()
{
	MoveToCurrentSpot();
}

void ARSSupplyCache::OnRep_OfferedItem()
{
	// Phase and item change in the same frame and usually arrive together, but neither waits for
	// the other: whichever lands second finds both.
	RefreshPresentation();
}

void ARSSupplyCache::RefreshPresentation()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	FTimerManager& TimerManager = GetWorldTimerManager();

	switch (Phase)
	{
	case ERSSupplyCachePhase::Rolling:
		if (!TimerManager.IsTimerActive(RollDisplayTimerHandle))
		{
			ShowNextRollWeapon();
			TimerManager.SetTimer(RollDisplayTimerHandle, this, &ThisClass::ShowNextRollWeapon, RollDisplayInterval, /*bLoop=*/ true);
		}
		break;

	case ERSSupplyCachePhase::Offering:
		TimerManager.ClearTimer(RollDisplayTimerHandle);
		ShowWeapon(OfferedItem);
		break;

	default:
		TimerManager.ClearTimer(RollDisplayTimerHandle);
		ShowWeapon(nullptr);
		break;
	}
}

void ARSSupplyCache::ShowNextRollWeapon()
{
	ShowWeapon(Table ? Table->PickForDisplay(ShownItem) : nullptr);
}

void ARSSupplyCache::ShowWeapon(const URSWeaponItemDefinition* Item)
{
	if (Item == ShownItem)
	{
		return;
	}

	// Nothing shown is an empty assembly rather than a hidden one: hiding would have to propagate to
	// the parts, and unhiding them would show the parts a weapon keeps hidden, its spare magazine.
	ShownItem = Item;
	DisplayMesh->SetAssembly(Item ? Item->Assembly.Get() : nullptr);
	DisplayMesh->ApplySkin(Item ? Item->Skin.Get() : nullptr);
}

#undef LOCTEXT_NAMESPACE
