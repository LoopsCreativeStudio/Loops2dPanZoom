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
			"InputCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"UnrealEd",
			"EditorSubsystem",
			"LevelEditor",
			"ToolMenus",
			"Projects",
			"Sequencer",
			"ControlRig",
			"ControlRigEditor"
		});
	}
}
