// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Round_Survival : ModuleRules
{
	public Round_Survival(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",

			// Gameplay Ability System
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",

			// Modular pawn components
			"ModularGameplay",

			// Networking
			"NetCore",

			// Enemy AI: controller, path following and the StateTree the behaviour runs on.
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",

			// Window vaulting: the traversal montage is warped onto the window it is crossing.
			"MotionWarping",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"PhysicsCore",

			// Animation: CalculateDirection for the locomotion blend spaces.
			"AnimGraphRuntime",

			// Muzzle flashes, spawned by the fire cue.
			"Niagara",

			// HUD: CommonUI layers and activatable widgets, MVVM viewmodels bound in the designer.
			"UMG",
			"Slate",
			"SlateCore",
			"CommonUI",
			"CommonInput",
			"ModelViewViewModel",
			"FieldNotification",

			// Performance: the significance manager the enemies are scaled by, and its project settings.
			"SignificanceManager",
			"DeveloperSettings",
		});
	}
}
