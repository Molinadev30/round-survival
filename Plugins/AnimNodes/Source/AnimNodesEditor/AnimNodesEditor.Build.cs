using UnrealBuildTool;

public class AnimNodesEditor : ModuleRules
{
	public AnimNodesEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// AnimGraph: UAnimGraphNode_SkeletalControlBase, the editor half of every bone controller.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AnimGraph",
			"AnimGraphRuntime",
			"AnimNodes",
		});

		// UnrealEd: the compiler results log the node validates into.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"BlueprintGraph",
		});
	}
}
