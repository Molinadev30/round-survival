// Round Survival

#pragma once

#include "Camera/PlayerCameraManager.h"

#include "RSPlayerCameraManager.generated.h"

/** How a dead player watches a teammate. */
UENUM(BlueprintType)
enum class ERSSpectatorView : uint8
{
	/** Through the teammate's eyes: their camera, their viewmodel, where they aim. */
	FirstPerson,

	/** Orbiting behind them, turned by the spectator's own look, with their whole body in view. */
	ThirdPerson
};

DECLARE_MULTICAST_DELEGATE(FRSOnViewChanged);

/**
 * The local player's camera, and the one place that knows whether it is looking through the eyes of
 * the actor it is following.
 *
 * Alive, the answer is always yes: the view target is the player's own pawn and its camera. Dead
 * and spectating, it depends on the spectator view: first person goes on looking through the
 * teammate's camera, third person orbits outside them, centred on where their body is drawn — the
 * network-smoothed mesh, not the capsule that jumps to each update.
 *
 * That answer decides what gets drawn, because the renderer decides what the owner sees —
 * the viewmodel, the body that only casts a shadow — by comparing against the view target, and the
 * view target is the teammate either way. URSLocalPlayer asks this camera before it builds a view,
 * and ARSPlayerCharacter::IsViewedInFirstPerson asks it before a shot's flash, sound and kick pick
 * a copy of the weapon to play on.
 *
 * The same answer, and who is being watched, is what the HUD shows "the player on screen" from —
 * whose health reddens the edges, whose name the spectator banner gives — so the camera says when
 * either changes, through OnViewChanged.
 *
 * Local only: the server decides who is watched, the spectator decides how.
 */
UCLASS()
class ROUND_SURVIVAL_API ARSPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	ARSPlayerCameraManager(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The owning controller started spectating. The view goes back to DefaultSpectatorView. */
	void BeginSpectating();

	/** The owning controller has a pawn again: back to its own camera. */
	void EndSpectating();

	/** Changes how a teammate is watched. Kept until the player stops spectating. */
	UFUNCTION(BlueprintCallable, Category = "RS|Spectating")
	void SetSpectatorView(ERSSpectatorView NewView);

	/** First person to third and back. Does nothing unless spectating. */
	UFUNCTION(BlueprintCallable, Category = "RS|Spectating")
	void ToggleSpectatorView();

	UFUNCTION(BlueprintPure, Category = "RS|Spectating")
	ERSSpectatorView GetSpectatorView() const { return SpectatorView; }

	UFUNCTION(BlueprintPure, Category = "RS|Spectating")
	bool IsSpectating() const { return bSpectating; }

	/**
	 * Whether this camera is following its view target from outside, rather than through its eyes.
	 * Only ever true while spectating in third person.
	 */
	bool IsViewingTargetFromOutside() const { return bSpectating && SpectatorView == ERSSpectatorView::ThirdPerson; }

	/**
	 * The player this camera is showing: its own player alive, the teammate being watched while
	 * spectating, and null while it shows nobody. The engine's own record of it, which follows the
	 * same player from pawn to pawn.
	 */
	APlayerState* GetViewedPlayerState() const { return ViewTarget.GetPlayerState(); }

	/** Whether the screen is what the viewed player sees: through their eyes, not orbiting them. */
	bool IsViewingThroughEyes() const { return GetViewedPlayerState() && !IsViewingTargetFromOutside(); }

	/**
	 * The view moved to another player, or switched between their eyes and outside them, or the
	 * owner started or stopped spectating. The HUD rebinds what it shows of "the player on screen"
	 * on it.
	 */
	FRSOnViewChanged OnViewChanged;

protected:
	//~APlayerCameraManager interface
	virtual void UpdateCamera(float DeltaTime) override;
	virtual void UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime) override;
	//~End of APlayerCameraManager interface

	/** How a player who has just died starts watching. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Spectating")
	ERSSpectatorView DefaultSpectatorView = ERSSpectatorView::FirstPerson;

	/** How far behind a teammate the third-person view sits. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Spectating", meta = (ForceUnits = cm, ClampMin = 0.0))
	float SpectatorCameraDistance = 256.0f;

	/** Half-size of the box swept out from the teammate, so the third-person view stops short of walls. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Spectating", meta = (ForceUnits = cm, ClampMin = 0.0))
	float SpectatorCameraProbeExtent = 12.0f;

private:
	/** Where the target is drawn rather than where it is simulated. See the definition. */
	static FVector GetSmoothedLocation(const AActor* Target);

	bool bSpectating = false;
	ERSSpectatorView SpectatorView = ERSSpectatorView::FirstPerson;

	/** The view as OnViewChanged last reported it. */
	TWeakObjectPtr<APlayerState> ReportedPlayerState;
	bool bReportedSpectating = false;
	ERSSpectatorView ReportedSpectatorView = ERSSpectatorView::FirstPerson;
};
