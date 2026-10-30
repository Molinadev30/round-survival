// Round Survival

#include "Performance/RSSignificanceManager.h"

#include "DrawDebugHelpers.h"
#include "Enemies/RSZombieCharacter.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

namespace RSConsoleVariables
{
	static bool bSignificanceEnabled = true;
	static FAutoConsoleVariableRef CVarSignificanceEnabled(
		TEXT("RS.Significance.Enabled"),
		bSignificanceEnabled,
		TEXT("Scales what each enemy updates by whether any player can see it. 0 updates every enemy in full, to compare."),
		ECVF_Default);

	static bool bSignificanceDraw = false;
	static FAutoConsoleVariableRef CVarSignificanceDraw(
		TEXT("RS.Significance.Draw"),
		bSignificanceDraw,
		TEXT("Labels each enemy with its significance tier on the server: green High, red Low."),
		ECVF_Default);
}

namespace RSSignificance
{
	static const FName EnemyTag(TEXT("RS.Enemy"));
}

URSSignificanceManager::URSSignificanceManager()
{
	// The server decides for everyone. A client's copies use the engine's rendered test, which is right there.
	bCreateOnClient = false;
}

URSSignificanceManager* URSSignificanceManager::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? Cast<URSSignificanceManager>(USignificanceManager::Get(World)) : nullptr;
}

bool URSSignificanceManager::IsEnabled()
{
	return RSConsoleVariables::bSignificanceEnabled;
}

ERSSignificanceTier URSSignificanceManager::ToTier(float Significance)
{
	return static_cast<ERSSignificanceTier>(FMath::Clamp(FMath::RoundToInt(Significance),
		static_cast<int32>(ERSSignificanceTier::Low), static_cast<int32>(ERSSignificanceTier::High)));
}

void URSSignificanceManager::RegisterEnemy(ARSZombieCharacter* Enemy)
{
	UWorld* World = GetWorld();
	if (!Enemy || !World || World->GetNetMode() == NM_Client || GetManagedObject(Enemy))
	{
		return;
	}

	RegisterObject(Enemy, RSSignificance::EnemyTag, &URSSignificanceManager::CalculateSignificance,
		EPostSignificanceType::Sequential, &URSSignificanceManager::ApplySignificance);

	// The timer starts with the first enemy rather than with the world: the manager is made before the world can
	// run timers, and a world with no enemies has nothing to tier.
	if (!UpdateTimerHandle.IsValid())
	{
		const float Interval = FMath::Max(GetDefault<URSSignificanceSettings>()->UpdateInterval, 0.01f);
		World->GetTimerManager().SetTimer(UpdateTimerHandle, this, &ThisClass::UpdateFromPlayers, Interval, /*bLoop=*/ true);
	}
}

void URSSignificanceManager::UnregisterEnemy(ARSZombieCharacter* Enemy)
{
	if (Enemy && GetManagedObject(Enemy))
	{
		UnregisterObject(Enemy);
	}
}

void URSSignificanceManager::UpdateFromPlayers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!IsEnabled())
	{
		if (bWasEnabled)
		{
			bWasEnabled = false;
			ApplyTierToAll(ERSSignificanceTier::High);
		}
		return;
	}
	bWasEnabled = true;

	// Every player's view, from the server's copy of each controller: a remote player's view point is its pawn's eye
	// and its replicated control rotation, a spectator's is whatever it is watching.
	TArray<FTransform> PlayerViews;
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (const APlayerController* PlayerController = Iterator->Get())
		{
			FVector Location;
			FRotator Rotation;
			PlayerController->GetPlayerViewPoint(Location, Rotation);
			PlayerViews.Add(FTransform(Rotation, Location));
		}
	}

	// The post-significance pass runs for every managed enemy, every update, and an enemy whose tier did not change
	// does nothing with it. So switching significance back on needs nothing of its own.
	Update(PlayerViews);

#if ENABLE_DRAW_DEBUG
	if (RSConsoleVariables::bSignificanceDraw)
	{
		for (const FManagedObjectInfo* Info : GetManagedObjects(RSSignificance::EnemyTag))
		{
			if (const ARSZombieCharacter* Enemy = Cast<ARSZombieCharacter>(Info->GetObject()))
			{
				const ERSSignificanceTier Tier = Enemy->GetSignificanceTier();
				const FColor Color = Tier == ERSSignificanceTier::High ? FColor::Green : FColor::Red;
				DrawDebugString(World, Enemy->GetActorLocation() + FVector(0.0, 0.0, 120.0), *UEnum::GetDisplayValueAsText(Tier).ToString(),
					nullptr, Color, GetDefault<URSSignificanceSettings>()->UpdateInterval, /*bDrawShadow=*/ true);
			}
		}
	}
#endif
}

float URSSignificanceManager::CalculateSignificance(USignificanceManager::FManagedObjectInfo* ObjectInfo, const FTransform& Viewpoint)
{
	const AActor* Enemy = Cast<AActor>(ObjectInfo->GetObject());
	if (!Enemy)
	{
		return static_cast<float>(ERSSignificanceTier::Low);
	}

	const URSSignificanceSettings* Settings = GetDefault<URSSignificanceSettings>();
	const FVector ToEnemy = Enemy->GetActorLocation() - Viewpoint.GetLocation();
	const double Distance = ToEnemy.Size();

	// Close enough to be in the player's face before the next update, whichever way the player is looking.
	if (Distance <= FMath::Max(Settings->NearRadius, 1.0f))
	{
		return static_cast<float>(ERSSignificanceTier::High);
	}

	const double CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(Settings->ViewHalfAngle));
	return static_cast<float>(FVector::DotProduct(ToEnemy / Distance, Viewpoint.GetRotation().GetForwardVector()) >= CosHalfAngle
		? ERSSignificanceTier::High
		: ERSSignificanceTier::Low);
}

void URSSignificanceManager::ApplySignificance(USignificanceManager::FManagedObjectInfo* ObjectInfo, float OldSignificance, float NewSignificance, bool bFinal)
{
	ARSZombieCharacter* Enemy = Cast<ARSZombieCharacter>(ObjectInfo->GetObject());
	if (!Enemy)
	{
		return;
	}

	// An enemy the manager lets go of — dying, pooled, destroyed — goes back to full updates, so whatever brings it
	// into the world next starts from the class's own settings.
	Enemy->SetSignificanceTier(bFinal ? ERSSignificanceTier::High : ToTier(NewSignificance));
}

void URSSignificanceManager::ApplyTierToAll(ERSSignificanceTier Tier)
{
	for (const FManagedObjectInfo* Info : GetManagedObjects(RSSignificance::EnemyTag))
	{
		if (ARSZombieCharacter* Enemy = Cast<ARSZombieCharacter>(Info->GetObject()))
		{
			Enemy->SetSignificanceTier(Tier);
		}
	}
}
