// Round Survival

#include "Camera/RSLocalPlayer.h"

#include "Camera/RSPlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"

bool URSLocalPlayer::CalcSceneViewInitOptions(FSceneViewInitOptions& OutInitOptions, FViewport* Viewport,
	FViewElementDrawer* ViewDrawer, int32 StereoViewIndex)
{
	if (!Super::CalcSceneViewInitOptions(OutInitOptions, Viewport, ViewDrawer, StereoViewIndex))
	{
		return false;
	}

	const ARSPlayerCameraManager* Camera = PlayerController ? Cast<ARSPlayerCameraManager>(PlayerController->PlayerCameraManager) : nullptr;
	if (Camera && Camera->IsViewingTargetFromOutside())
	{
		// The controller owns nothing that is drawn, so to the renderer every player is someone else.
		OutInitOptions.ViewActor = PlayerController;
	}

	return true;
}
