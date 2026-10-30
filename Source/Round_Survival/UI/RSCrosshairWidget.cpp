// Round Survival

#include "UI/RSCrosshairWidget.h"

#include "Brushes/SlateColorBrush.h"
#include "Camera/PlayerCameraManager.h"
#include "Core/RSPlayerCharacter.h"
#include "Equipment/RSEquipmentManagerComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Weapons/RSRangedWeaponInstance.h"

void URSCrosshairWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	SpreadRadius = -1.0f;

	const APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const URSEquipmentManagerComponent* Equipment = Pawn ? Pawn->FindComponentByClass<URSEquipmentManagerComponent>() : nullptr;
	const URSRangedWeaponInstance* Weapon = Equipment ? Equipment->GetFirstInstanceOfType<URSRangedWeaponInstance>() : nullptr;

	if (!Weapon || !PC->PlayerCameraManager)
	{
		return;
	}

	// The same angle the fire ability spreads its bullets over, and halved the same way: the
	// ability draws each bullet inside a cone of half this angle around the aim.
	const float SpreadAngle = Weapon->GetCalculatedSpreadAngle() * Weapon->GetCalculatedSpreadAngleMultiplier();
	const float HalfSpread = FMath::DegreesToRadians(SpreadAngle * 0.5f);

	// The world camera's horizontal field of view, not the viewmodel's: the bullets fly in the
	// world. The widget covers the screen, so half its width is half the view.
	const float HalfFOV = FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle() * 0.5f);
	const float HalfWidth = MyGeometry.GetLocalSize().X * 0.5f;

	SpreadRadius = FMath::Tan(HalfSpread) / FMath::Max(FMath::Tan(HalfFOV), KINDA_SMALL_NUMBER) * HalfWidth;

	const ARSPlayerCharacter* Character = Cast<ARSPlayerCharacter>(Pawn);
	Opacity = Character ? 1.0f - Character->GetAimDownSightsAlpha() : 1.0f;
}

int32 URSCrosshairWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (SpreadRadius < 0.0f || Opacity <= 0.0f)
	{
		return LayerId;
	}

	static const FSlateColorBrush WhiteBrush(FLinearColor::White);

	const FVector2D Centre = AllottedGeometry.GetLocalSize() * 0.5f;
	const float Gap = MinimumGap + SpreadRadius;
	const FLinearColor Fade(1.0f, 1.0f, 1.0f, Opacity);
	const FLinearColor TickColor = (bAimingAtEnemy ? EnemyColor : Color) * InWidgetStyle.GetColorAndOpacityTint() * Fade;

	// Right, left, down, up: each tick starts Gap from the centre and runs outwards.
	const FVector2D Directions[] = { FVector2D(1, 0), FVector2D(-1, 0), FVector2D(0, 1), FVector2D(0, -1) };

	for (const FVector2D& Direction : Directions)
	{
		const bool bHorizontal = Direction.Y == 0.0;
		const FVector2D Size = bHorizontal ? FVector2D(TickLength, TickThickness) : FVector2D(TickThickness, TickLength);
		const FVector2D TickCentre = Centre + Direction * (Gap + TickLength * 0.5f);
		const FVector2D TopLeft = TickCentre - Size * 0.5f;

		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(Size + FVector2D(2.0f), FSlateLayoutTransform(TopLeft - FVector2D(1.0f))),
			&WhiteBrush, ESlateDrawEffect::None, OutlineColor * InWidgetStyle.GetColorAndOpacityTint() * Fade);

		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
			AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft)),
			&WhiteBrush, ESlateDrawEffect::None, TickColor);
	}

	return LayerId + 1;
}
