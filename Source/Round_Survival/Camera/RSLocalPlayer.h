// Round Survival

#pragma once

#include "Engine/LocalPlayer.h"

#include "RSLocalPlayer.generated.h"

/**
 * The local player, extended only to say whose eyes its view is from.
 *
 * The renderer draws owner-only primitives — the viewmodel — and hides owner-hidden ones — the body,
 * which for its owner only casts a shadow — by comparing each primitive's owner with the view's
 * ViewActor, and the engine fills that in with the view target. That is right whenever the camera
 * sits at the view target's eyes. A spectator orbiting a teammate in third person has the teammate
 * as view target but is not looking through their eyes, so the engine's answer would show them the
 * teammate's viewmodel and hide the body. Here the view is said to be the spectator's own instead,
 * and every primitive falls into place by the renderer's own rules: nothing is shown or hidden by
 * hand. ARSPlayerCameraManager knows which view it is.
 *
 * Set as the engine's LocalPlayerClassName in DefaultEngine.ini.
 */
UCLASS()
class ROUND_SURVIVAL_API URSLocalPlayer : public ULocalPlayer
{
	GENERATED_BODY()

public:
	//~ULocalPlayer interface
	virtual bool CalcSceneViewInitOptions(FSceneViewInitOptions& OutInitOptions, FViewport* Viewport,
		FViewElementDrawer* ViewDrawer = nullptr, int32 StereoViewIndex = INDEX_NONE) override;
	//~End of ULocalPlayer interface
};
