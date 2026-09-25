// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OpenCircle : ModuleRules
{
	public OpenCircle(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"OpenCircle",
			"OpenCircle/Variant_Platforming",
			"OpenCircle/Variant_Platforming/Animation",
			"OpenCircle/Variant_Combat",
			"OpenCircle/Variant_Combat/AI",
			"OpenCircle/Variant_Combat/Animation",
			"OpenCircle/Variant_Combat/Gameplay",
			"OpenCircle/Variant_Combat/Interfaces",
			"OpenCircle/Variant_Combat/UI",
			"OpenCircle/Variant_SideScrolling",
			"OpenCircle/Variant_SideScrolling/AI",
			"OpenCircle/Variant_SideScrolling/Gameplay",
			"OpenCircle/Variant_SideScrolling/Interfaces",
			"OpenCircle/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
