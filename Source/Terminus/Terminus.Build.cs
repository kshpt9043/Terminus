// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Terminus : ModuleRules
{
	public Terminus(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core",
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"EnhancedInput", 
			"Slate",
			"SlateCore",
			"UMG",
			"OnlineSubsystem",
			"OnlineSubsystemUtils"
		});
	}
}
