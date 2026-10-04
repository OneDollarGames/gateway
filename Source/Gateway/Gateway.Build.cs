using UnrealBuildTool;

public class Gateway : ModuleRules
{
	public Gateway(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore",
			"AudioMixer", "SignalProcessing",
			"Json", "JsonUtilities",
			"RenderCore", "RHI",
			"Slate", "SlateCore",
			"HeadMountedDisplay", "XRBase"
		});
	}
}
