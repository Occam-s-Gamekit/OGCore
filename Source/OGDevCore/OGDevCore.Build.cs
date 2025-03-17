using UnrealBuildTool;

public class OGDevCore : ModuleRules
{
	public OGDevCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"OGAsync"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"FunctionalTesting",
				"UnrealEd",
				"NetCore"
			}
		);
	}
}