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
			"HeadMountedDisplay", "XRBase",
			"ApplicationCore"
		});

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// FJavaWrapper (AndroidJNI.h) para la difusion que ignora el sensor de proximidad en modo audio libre
			PrivateDependencyModuleNames.Add("Launch");
		}
	}
}
