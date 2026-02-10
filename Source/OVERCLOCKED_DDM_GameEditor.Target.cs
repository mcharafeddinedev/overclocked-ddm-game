using UnrealBuildTool;
using System.Collections.Generic;

public class OVERCLOCKED_DDM_GameEditorTarget : TargetRules
{
	public OVERCLOCKED_DDM_GameEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		ExtraModuleNames.Add("StateRunner_Arcade");
	}
}
