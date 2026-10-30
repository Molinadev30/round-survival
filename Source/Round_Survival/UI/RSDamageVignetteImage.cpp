// Round Survival

#include "UI/RSDamageVignetteImage.h"

#include "Materials/MaterialInstanceDynamic.h"

void URSDamageVignetteImage::SetIntensity(float InIntensity)
{
	Intensity = FMath::Clamp(InIntensity, 0.0f, 1.0f);
	PushParameters();
}

void URSDamageVignetteImage::SetPulse(float InPulse)
{
	Pulse = FMath::Clamp(InPulse, 0.0f, 1.0f);
	PushParameters();
}

void URSDamageVignetteImage::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	PushParameters();
}

void URSDamageVignetteImage::PushParameters()
{
	if (UMaterialInstanceDynamic* Material = GetDynamicMaterial())
	{
		Material->SetScalarParameterValue(IntensityParameterName, Intensity);
		Material->SetScalarParameterValue(PulseParameterName, Pulse);
	}

	// In the designer it stays visible, so the material can be looked at with the sliders above.
	if (!IsDesignTime())
	{
		SetVisibility(Intensity > 0.0f || Pulse > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
