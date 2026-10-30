// Round Survival

#include "UI/RSPlayerScrapEntry.h"

#include "Components/PanelWidget.h"
#include "Engine/LocalPlayer.h"
#include "MVVMSubsystem.h"
#include "RSLogChannels.h"
#include "UI/RSHUDSubsystem.h"
#include "UI/RSScrapPopup.h"
#include "UI/ViewModels/RSPlayerScrapViewModel.h"
#include "View/MVVMView.h"

void URSPlayerScrapEntry::NativeConstruct()
{
	Super::NativeConstruct();

	if (URSHUDSubsystem* HUDSubsystem = ULocalPlayer::GetSubsystem<URSHUDSubsystem>(GetOwningLocalPlayer()))
	{
		ScrapDeltaHandle = HUDSubsystem->OnLocalScrapDelta.AddUObject(this, &ThisClass::HandleLocalScrapDelta);
	}
}

void URSPlayerScrapEntry::NativeDestruct()
{
	if (URSHUDSubsystem* HUDSubsystem = ULocalPlayer::GetSubsystem<URSHUDSubsystem>(GetOwningLocalPlayer()))
	{
		HUDSubsystem->OnLocalScrapDelta.Remove(ScrapDeltaHandle);
	}
	ScrapDeltaHandle.Reset();

	Super::NativeDestruct();
}

void URSPlayerScrapEntry::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	URSPlayerScrapViewModel* NewViewModel = Cast<URSPlayerScrapViewModel>(ListItemObject);
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);

	if (!NewViewModel || !View || !View->SetViewModelByClass(NewViewModel))
	{
		UE_LOG(LogRS, Warning, TEXT("[%s] could not take %s as its scrap viewmodel."), *GetNameSafe(this), *GetNameSafe(ListItemObject));
	}

	ViewModel = NewViewModel;
}

void URSPlayerScrapEntry::HandleLocalScrapDelta(int32 Delta, const FGameplayTag& Reason)
{
	const URSPlayerScrapViewModel* RowViewModel = ViewModel.Get();
	if (!RowViewModel || !RowViewModel->IsLocalPlayer() || !PopupLayer || !PopupClass)
	{
		return;
	}

	URSScrapPopup* Popup = nullptr;
	for (UWidget* Child : PopupLayer->GetAllChildren())
	{
		URSScrapPopup* Candidate = Cast<URSScrapPopup>(Child);
		if (Candidate && !Candidate->IsShowing())
		{
			Popup = Candidate;
			break;
		}
	}

	if (!Popup)
	{
		Popup = CreateWidget<URSScrapPopup>(this, PopupClass);
		PopupLayer->AddChild(Popup);
	}

	const double Now = FPlatformTime::Seconds();
	BurstIndex = (LastPopupTime >= 0.0 && Now - LastPopupTime < BurstWindow) ? BurstIndex + 1 : 0;
	LastPopupTime = Now;

	// On the popup itself, not its text: the Rise animation owns the text's position.
	Popup->SetRenderTranslation(FVector2D(0.0f, -BurstIndex * BurstSpacing));
	Popup->Show(Delta, Reason);
}
