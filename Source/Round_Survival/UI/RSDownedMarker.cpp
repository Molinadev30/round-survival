// Round Survival

#include "UI/RSDownedMarker.h"

#include "Components/Image.h"
#include "MVVMSubsystem.h"
#include "RSLogChannels.h"
#include "UI/ViewModels/RSDownedTeammateViewModel.h"
#include "View/MVVMView.h"

URSDownedMarker::URSDownedMarker(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// White just down, amber at half, red as it runs out.
	const FLinearColor Keys[] = { FLinearColor(1.0f, 0.15f, 0.1f), FLinearColor(1.0f, 0.7f, 0.15f), FLinearColor(1.0f, 1.0f, 1.0f) };
	const float Times[] = { 0.0f, 0.5f, 1.0f };

	for (int32 Channel = 0; Channel < 4; ++Channel)
	{
		FRichCurve& Curve = ColorByTimeLeft.ColorCurves[Channel];
		for (int32 Key = 0; Key < UE_ARRAY_COUNT(Times); ++Key)
		{
			const float Value = Channel == 0 ? Keys[Key].R : Channel == 1 ? Keys[Key].G : Channel == 2 ? Keys[Key].B : 1.0f;
			Curve.SetKeyInterpMode(Curve.AddKey(Times[Key], Value), RCIM_Linear);
		}
	}
}

void URSDownedMarker::SetTeammate(URSDownedTeammateViewModel* InTeammate)
{
	Teammate = InTeammate;

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	if (!InTeammate || !View || !View->SetViewModelByClass(InTeammate))
	{
		UE_LOG(LogRS, Warning, TEXT("[%s] could not take %s as its teammate viewmodel."), *GetNameSafe(this), *GetNameSafe(InTeammate));
	}
}

void URSDownedMarker::SetBleedOutClock(const FRSBleedOutClock& InClock)
{
	BleedOutClock = InClock;
}

void URSDownedMarker::SetBeingRevived(bool bInBeingRevived)
{
	bBeingRevived = bInBeingRevived;

	if (RevivingLabel)
	{
		RevivingLabel->SetVisibility(bBeingRevived ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void URSDownedMarker::Redraw(double ServerTime)
{
	if (!Icon)
	{
		return;
	}

	if (bBeingRevived)
	{
		Icon->SetColorAndOpacity(RevivingColor);
		return;
	}

	const float TimeLeft = BleedOutClock.IsActive() ? BleedOutClock.GetSecondsLeft(ServerTime) / BleedOutClock.Duration : 1.0f;
	Icon->SetColorAndOpacity(ColorByTimeLeft.GetLinearColorValue(TimeLeft));
}
