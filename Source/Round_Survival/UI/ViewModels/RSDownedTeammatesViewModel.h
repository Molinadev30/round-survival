// Round Survival

#pragma once

#include "MVVMViewModelBase.h"

#include "RSDownedTeammatesViewModel.generated.h"

class URSDownedTeammateViewModel;

/**
 * The teammates who are down right now, one marker each: everyone down but the local player, and but
 * whoever the screen is watching through their eyes, whose own countdown is already on it.
 */
UCLASS(meta = (MVVMAllowedContextCreationType = "Resolver|Manual"))
class ROUND_SURVIVAL_API URSDownedTeammatesViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void SetTeammates(const TArray<TObjectPtr<URSDownedTeammateViewModel>>& InTeammates);

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "RS|Downed", meta = (AllowPrivateAccess = true))
	TArray<TObjectPtr<URSDownedTeammateViewModel>> Teammates;
};
