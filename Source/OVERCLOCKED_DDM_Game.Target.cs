using UnrealBuildTool;
using System.Collections.Generic;

public class OVERCLOCKED_DDM_GameTarget : TargetRules
{
	public OVERCLOCKED_DDM_GameTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		ExtraModuleNames.Add("StateRunner_Arcade");
	}
}
