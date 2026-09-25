// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HarborCity : ModuleRules
{
	public HarborCity(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			"ChaosVehicles",
			"PhysicsCore",
			"Json",
			"JsonUtilities"
		});

		// Runtime calibration uses the existing platform mapper to identify the local mouse device.
		PrivateDependencyModuleNames.AddRange(new string[] { "ApplicationCore", "MovieSceneCapture", "RenderCore", "RHI" });
		PrivateDependencyModuleNames.AddRange(new string[] { "AnimGraphRuntime", "AnimationWarpingRuntime", "AudioMixer", "AnimationCore", "Chaos" });
		// Read-only VS2 diagnostics inspect the already-enabled VRM4U spring node.
		PrivateDependencyModuleNames.Add("VRM4U");
		PrivateDependencyModuleNames.AddRange(new string[] { "ClothingSystemRuntimeInterface", "ClothingSystemRuntimeCommon" });
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "AnimGraph", "AnimationWarpingEditor", "BlueprintGraph", "UnrealEd", "AssetRegistry", "AssetTools", "MeshDescription", "SkeletalMeshDescription", "StaticMeshDescription", "IKRig", "AnimationBlueprintLibrary", "PhysicsUtilities" });
		}

		PublicIncludePaths.AddRange(new string[] {
			"HarborCity",
			"HarborCity/Variant_Platforming",
			"HarborCity/Variant_Platforming/Animation",
			"HarborCity/Variant_Combat",
			"HarborCity/Variant_Combat/AI",
			"HarborCity/Variant_Combat/Animation",
			"HarborCity/Variant_Combat/Gameplay",
			"HarborCity/Variant_Combat/Interfaces",
			"HarborCity/Variant_Combat/UI",
			"HarborCity/Variant_SideScrolling",
			"HarborCity/Variant_SideScrolling/AI",
			"HarborCity/Variant_SideScrolling/Gameplay",
			"HarborCity/Variant_SideScrolling/Interfaces",
			"HarborCity/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
