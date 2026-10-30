// Round Survival

#pragma once

#include "Components/Image.h"

#include "RSDamageVignetteImage.generated.h"

/**
 * An image whose material is driven by two numbers: how far the vignette has crept in, and how
 * strong the flash of the last hit still is.
 *
 * The two live on the image rather than on the widget that owns it so that a UMG animation can
 * key Pulse directly — animations bind to widgets in the tree, and the setter below is what the
 * animation calls on each frame it plays. Both are pushed into a dynamic instance of the brush's
 * material; the look itself is M_RS_DamageVignette's.
 *
 * Collapses itself at runtime while both are zero, so a healthy player pays nothing for a
 * full-screen material.
 */
UCLASS()
class ROUND_SURVIVAL_API URSDamageVignetteImage : public UImage
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "RS|Damage Vignette")
	void SetIntensity(float InIntensity);

	UFUNCTION(BlueprintCallable, Category = "RS|Damage Vignette")
	void SetPulse(float InPulse);

	//~UWidget interface
	virtual void SynchronizeProperties() override;
	//~End of UWidget interface

protected:
	/** 0 shows nothing, 1 is the vignette at its deepest. Set from the owner's health thresholds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetIntensity, Interp, Category = "RS|Damage Vignette", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float Intensity = 0.0f;

	/** The flash of a hit: keyed by the owner's HitPulse animation, from 1 back to 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetPulse, Interp, Category = "RS|Damage Vignette", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float Pulse = 0.0f;

	UPROPERTY(EditAnywhere, Category = "RS|Damage Vignette")
	FName IntensityParameterName = TEXT("Intensity");

	UPROPERTY(EditAnywhere, Category = "RS|Damage Vignette")
	FName PulseParameterName = TEXT("Pulse");

private:
	void PushParameters();
};
