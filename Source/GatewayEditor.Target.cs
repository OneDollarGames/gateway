using UnrealBuildTool;
using System.Collections.Generic;

public class GatewayEditorTarget : TargetRules
{
	public GatewayEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		ExtraModuleNames.Add("Gateway");
	}
}
