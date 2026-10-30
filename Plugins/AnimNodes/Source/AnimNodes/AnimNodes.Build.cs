using UnrealBuildTool;

public class AnimNodes : ModuleRules
{
	public AnimNodes(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// AnimGraphRuntime: FAnimNode_SkeletalControlBase, the base of every bone controller.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AnimGraphRuntime",
		});
	}
}
