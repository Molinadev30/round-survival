// Round Survival

#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "RSDebugSubsystem.generated.h"

class ARSZombieCharacter;
class URSRoundDirector;

/**
 * The console commands the game is tested with.
 *
 * `RS.SetRound`, `RS.AddScrap`, `RS.KillAll`, `RS.SpawnHorde`, `RS.ShowSpawners`, `RS.GiveWeapon`, `RS.SetWeaponSkin`, `RS.SpawnPowerUp`,
 * `RS.SupplyCacheMove`,
 * `RS.DownPlayer`, `RS.RevivePlayer`, `RS.BleedOut` and `RS.RestartMatch`. Every one of them is a
 * thin wrapper over the same entry points the game itself uses — the round director's `SkipToRound`,
 * its `SpawnHorde`, the damage pipeline, a revive's `Interact`, the bleed-out's own end, the game
 * mode's restart — rather than a private back door that
 * writes the state directly. A cheat that skips the code it is meant to be exercising tests
 * nothing, and the difficulty curve is impossible to judge without being able to jump to round
 * twenty on demand.
 *
 * `RS.RefillSprint` is the one exception, and on purpose: it fills the server's copy of a player's
 * sprint and nothing else, so the server and that player's client disagree. That disagreement is
 * the state the sprint's correction exists to repair, and the only way to put it there at will.
 *
 * Server only in effect: everything here goes through the round director or an ability system,
 * both of which refuse to act on a client. Compiled out of shipping builds entirely.
 */
UCLASS()
class ROUND_SURVIVAL_API URSDebugSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static URSDebugSubsystem* Get(const UObject* WorldContextObject);

	/** Jumps to a round's intermission through the director, clearing the board on the way. */
	void SetRound(int32 RoundNumber);

	/** Adds scrap to every local player. Writes the attribute directly — see the implementation. */
	void AddScrap(float Amount);

	/** Kills every living enemy through the damage pipeline. Returns how many were killed. */
	int32 KillAll();

	/** Releases enemies outside the quota, scaled to the current round. Returns how many landed. */
	int32 SpawnHorde(int32 Count);

	/**
	 * Gives every player the weapon item whose asset name contains Name, through the same
	 * GiveWeapon a Wall Locker uses. Returns how many players received it.
	 */
	int32 GiveWeapon(const FString& Name);

	/**
	 * Puts the weapon skin whose asset name contains Name on every player's drawn weapon, through
	 * the same override the Weapon Forge will use; an empty name takes the override off. Returns
	 * how many weapons changed.
	 */
	int32 SetWeaponSkin(const FString& Name);

	/**
	 * Drops the power-up whose asset name contains Name a few steps in front of the host, through
	 * the power-up director that drops them from kills. No name lists them. Returns whether one landed.
	 */
	bool SpawnPowerUp(const FString& Name);

	/**
	 * The next roll paid for at each Supply Cache draws nothing and moves it, as its own rule would
	 * sooner or later. Returns how many caches will.
	 */
	int32 ForceSupplyCacheMove();

	/**
	 * Takes the health of one player — by index in the game state's player list, 0 being the host —
	 * to zero through the damage pipeline, so they go down the way a hit would put them down.
	 * False if there is no such player or they are already down.
	 */
	bool DownPlayer(int32 PlayerIndex);

	/**
	 * Revives one downed player, by the same index, through the same Interact a teammate's completed
	 * hold calls — with nobody as the reviver. False if there is no such player or they are not down.
	 */
	bool RevivePlayer(int32 PlayerIndex);

	/**
	 * Ends one downed player's bleed-out now, by the same index, through the path the clock running
	 * out takes: they die, and their teammates pay for it. False if there is no such player or they
	 * are not down.
	 */
	bool BleedOut(int32 PlayerIndex);

	/**
	 * Fills one player's sprint on the server's copy of their pawn only, by the same index. For a
	 * remote player that leaves their client still spent, until the server corrects it. False if
	 * there is no such player or they have no pawn.
	 */
	bool RefillSprint(int32 PlayerIndex);

	/**
	 * Takes parts off the living enemy nearest the host, through the same call a hit's damage
	 * makes. A part named "All" means every part that can come off a living enemy; one that only
	 * the killing blow takes — the head — is refused, since no living enemy ever lacks one.
	 * Returns how many came off.
	 */
	int32 SeverNearestEnemy(const FString& PartName);

	/** Starts the match over from round one, as the host's restart does — whether or not it is lost. */
	void RestartMatch();

	/** Turns the spawn point read-out on or off. */
	void SetSpawnerDrawingEnabled(bool bEnabled);

	bool IsSpawnerDrawingEnabled() const { return bDrawSpawners; }

protected:
	//~USubsystem interface
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;
	//~End of USubsystem interface

	/** Draws one frame's worth of spawn point state. Driven by a timer, never by a tick. */
	void DrawSpawners();

	URSRoundDirector* GetRoundDirector() const;

private:
	bool bDrawSpawners = false;

	FTimerHandle DrawTimerHandle;
};
