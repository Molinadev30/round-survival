// Round Survival

#include "UI/RSDownedMarkerLayer.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Core/RSPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UI/RSDownedMarker.h"
#include "UI/ViewModels/RSDownedTeammateViewModel.h"

void URSDownedMarkerLayer::NativeConstruct()
{
	Super::NativeConstruct();

	// The designer keeps it visible to lay it out.
	if (!IsDesignTime())
	{
		SetVisibility(Teammates.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void URSDownedMarkerLayer::SetTeammates(const TArray<URSDownedTeammateViewModel*>& InTeammates)
{
	Teammates = TArray<TObjectPtr<URSDownedTeammateViewModel>>(InTeammates);

	if (!MarkerCanvas || IsDesignTime())
	{
		return;
	}

	// A marker for each teammate that has none; markers whose teammate is no longer down go.
	for (int32 Index = Markers.Num() - 1; Index >= 0; --Index)
	{
		if (!Teammates.Contains(Markers[Index]->GetTeammate()))
		{
			Markers[Index]->RemoveFromParent();
			Markers.RemoveAt(Index);
		}
	}

	for (URSDownedTeammateViewModel* Teammate : Teammates)
	{
		const bool bHasMarker = Markers.ContainsByPredicate([Teammate](const URSDownedMarker* Marker) { return Marker->GetTeammate() == Teammate; });
		if (bHasMarker || !Teammate || !MarkerClass)
		{
			continue;
		}

		URSDownedMarker* Marker = CreateWidget<URSDownedMarker>(this, MarkerClass);
		UCanvasPanelSlot* MarkerSlot = MarkerCanvas->AddChildToCanvas(Marker);
		MarkerSlot->SetAutoSize(true);
		MarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		Marker->SetTeammate(Teammate);
		Markers.Add(Marker);
	}

	SetVisibility(Markers.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	// Placed now rather than on the next tick, so a new marker never shows a frame at the corner.
	PlaceMarkers();
}

void URSDownedMarkerLayer::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	PlaceMarkers();
}

void URSDownedMarkerLayer::PlaceMarkers()
{
	const APlayerController* PC = GetOwningPlayer();
	const APlayerCameraManager* Camera = PC ? PC->PlayerCameraManager.Get() : nullptr;
	if (!Camera || !MarkerCanvas || Markers.IsEmpty())
	{
		return;
	}

	// The layer covers the viewport, so its own space is the one the projection answers in.
	const FVector2D Size = MarkerCanvas->GetCachedGeometry().GetLocalSize();
	if (Size.X <= 1.0 || Size.Y <= 1.0)
	{
		return;
	}

	const FVector2D Center = Size * 0.5;
	const FVector2D HalfExtent(FMath::Max(Center.X - EdgeMargin, 1.0), FMath::Max(Center.Y - EdgeMargin, 1.0));

	const FVector CameraLocation = Camera->GetCameraLocation();
	const FRotator CameraRotation = Camera->GetCameraRotation();

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	const double ServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);

	for (URSDownedMarker* Marker : Markers)
	{
		const URSDownedTeammateViewModel* Teammate = Marker->GetTeammate();
		const ARSPlayerState* PlayerState = Teammate ? Teammate->GetPlayerState() : nullptr;
		const APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;

		// A teammate whose pawn has not replicated yet has nowhere to be.
		if (!Pawn)
		{
			Marker->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		const FVector Target = Pawn->GetActorLocation() + FVector(0.0, 0.0, HeightAboveTeammate);

		FVector2D Position;
		const bool bInFront = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Target, Position, /*bPlayerViewportRelative=*/ false);
		const FVector2D FromCenter = Position - Center;

		if (!bInFront)
		{
			// Behind the camera the projection means nothing; which side the teammate is on does.
			const FVector InView = CameraRotation.UnrotateVector(Target - CameraLocation);
			Position = PinToEdge(Center, HalfExtent, FVector2D(InView.Y, -InView.Z));
		}
		else if (FMath::Abs(FromCenter.X) > HalfExtent.X || FMath::Abs(FromCenter.Y) > HalfExtent.Y)
		{
			Position = PinToEdge(Center, HalfExtent, FromCenter);
		}

		if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(Marker->Slot))
		{
			MarkerSlot->SetPosition(Position);
		}

		Marker->SetVisibility(ESlateVisibility::HitTestInvisible);
		Marker->Redraw(ServerTime);
	}
}

FVector2D URSDownedMarkerLayer::PinToEdge(const FVector2D& Center, const FVector2D& HalfExtent, FVector2D Direction)
{
	// Straight behind, with nothing to say which side: the bottom edge, where "behind you" reads.
	if (Direction.IsNearlyZero())
	{
		Direction = FVector2D(0.0, 1.0);
	}

	const double ScaleX = FMath::IsNearlyZero(Direction.X) ? UE_BIG_NUMBER : HalfExtent.X / FMath::Abs(Direction.X);
	const double ScaleY = FMath::IsNearlyZero(Direction.Y) ? UE_BIG_NUMBER : HalfExtent.Y / FMath::Abs(Direction.Y);

	return Center + Direction * FMath::Min(ScaleX, ScaleY);
}
