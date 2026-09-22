// Copyright 2026 Loops Creative Studio. All Rights Reserved.

using UnrealBuildTool;

public class Loops2DPanZoom : ModuleRules
{
	public Loops2DPanZoom(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"EditorSubsystem",
			"LevelEditor",
			"ToolMenus",
			"Projects",
			"ControlRig",
			"ControlRigEditor"
		});
	}
}
