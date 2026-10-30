// Round Survival

#pragma once

#include "Engine/TimerHandle.h"
#include "GameFramework/Actor.h"

#include "RSPowerUpPickup.generated.h"

class UMaterialInterface;
class URotatingMovementComponent;
class URSPowerUpDefinition;
class USphereComponent;
class UStaticMeshComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FRSPowerUpCollected, const URSPowerUpDefinition* /*PowerUp*/, AActor* /*Collector*/);

/**
 * A power-up lying on the floor, waiting for a player to walk over it.
 *
 * Spawned by the power-up director with its definition and a lifetime. A standing player touching
 * it picks it up for the whole team, on the server, and it is gone. Left alone, it goes after
 * `Lifetime`, blinking for the last `BlinkSeconds` so players can see it is about to.
 *
 * What it is and when it goes replicate; the blink and the spin are drawn locally from those and
 * decide nothing. Nothing on it ticks for gameplay: the end is a server timer and the blink a local
 * one. The spin is the engine's rotating movement, presentation only.
 */
UCLASS(Blueprintable)
class ROUND_SURVIVAL_API ARSPowerUpPickup : public AActor
{
	GENERATED_BODY()

public:
	ARSPowerUpPickup();

	/** Server, between spawning and finishing the spawn. */
	void Initialise(const URSPowerUpDefinition* InPowerUp, float InLifetime, float InBlinkSeconds);

	UFUNCTION(BlueprintPure, Category = "RS|Power-Up")
	const URSPowerUpDefinition* GetPowerUp() const { return PowerUp; }

	/** Server. Someone picked it up, just before it goes. */
	FRSPowerUpCollected OnCollected;

protected:
	//~AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End of AActor interface

	/** Only pawns overlap it. Its radius is how close a player has to walk. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Power-Up")
	TObjectPtr<USphereComponent> Trigger;

	/** The definition's mesh, tinted by its colour. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Power-Up")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Power-Up")
	TObjectPtr<URotatingMovementComponent> Spin;

	/**
	 * What the mesh is drawn with, tinted by the power-up's colour through its `Color` parameter. The
	 * engine's basic shapes come with a grid material that has no colour to set.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Power-Up")
	TObjectPtr<UMaterialInterface> TintMaterial;

	/** Seconds between the blinks of the last stretch. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Power-Up", meta = (ForceUnits = s, ClampMin = 0.05))
	float BlinkInterval = 0.2f;

private:
	UFUNCTION()
	void HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnRep_PowerUp();

	/** Every machine: the definition's mesh and colour, and the blink scheduled against the end. */
	void RefreshPresentation();

	void StartBlinking();
	void ToggleBlink();

	/**
	 * What it is. The director sets it on a drop; one placed in a level for testing has it set by
	 * hand, and having no lifetime it stays until someone takes it.
	 */
	UPROPERTY(EditInstanceOnly, ReplicatedUsing = OnRep_PowerUp, Category = "RS|Power-Up", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const URSPowerUpDefinition> PowerUp;

	/** Server world time it goes at, so a client that joins late blinks at the right moment. */
	UPROPERTY(Replicated)
	double ExpireServerTime = 0.0;

	UPROPERTY(Replicated)
	float BlinkSeconds = 0.0f;

	float Lifetime = 0.0f;
	bool bCollected = false;

	FTimerHandle ExpireTimerHandle;
	FTimerHandle BlinkTimerHandle;
};
