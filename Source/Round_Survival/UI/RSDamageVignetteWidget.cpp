// Round Survival

#include "UI/RSDamageVignetteWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Engine/LocalPlayer.h"
#include "UI/RSDamageVignetteImage.h"
#include "UI/RSHUDSubsystem.h"

URSDamageVignetteWidget::URSDamageVignetteWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FRichCurve* Curve = IntensityByHealth.GetRichCurve();
	for (const FVector2f& Key : { FVector2f(0.0f, 1.0f), FVector2f(0.2f, 0.33f), FVector2f(0.6f, 0.15f), FVector2f(0.8f, 0.0f) })
	{
		Curve->SetKeyInterpMode(Curve->AddKey(Key.X, Key.Y), RCIM_Linear);
	}
}

void URSDamageVignetteWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (URSHUDSubsystem* HUDSubsystem = ULocalPlayer::GetSubsystem<URSHUDSubsystem>(GetOwningLocalPlayer()))
	{
		DamagedHandle = HUDSubsystem->OnViewedPlayerDamaged.AddUObject(this, &ThisClass::HandleViewedPlayerDamaged);
	}
}

void URSDamageVignetteWidget::NativeDestruct()
{
	if (URSHUDSubsystem* HUDSubsystem = ULocalPlayer::GetSubsystem<URSHUDSubsystem>(GetOwningLocalPlayer()))
	{
		HUDSubsystem->OnViewedPlayerDamaged.Remove(DamagedHandle);
	}
	DamagedHandle.Reset();

	Super::NativeDestruct();
}

void URSDamageVignetteWidget::SetMissingHealth(float InMissingHealth)
{
	MissingHealth = FMath::Clamp(InMissingHealth, 0.0f, 1.0f);

	if (Vignette)
	{
		Vignette->SetIntensity(IntensityByHealth.GetRichCurveConst()->Eval(1.0f - MissingHealth, 0.0f));
	}
}

void URSDamageVignetteWidget::HandleViewedPlayerDamaged()
{
	if (HitPulse)
	{
		PlayAnimation(HitPulse);
	}
}
