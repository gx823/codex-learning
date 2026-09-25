using UnrealBuildTool;

public class HarborCityEditor : ModuleRules
{
    public HarborCityEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "HarborCity", "UnrealEd",
            "AnimGraph", "AnimGraphRuntime", "BlueprintGraph", "AnimationWarpingRuntime",
            "AnimationWarpingEditor", "Json", "JsonUtilities",
            "VRM4U", "ClothingSystemEditor", "ClothingSystemEditorInterface",
            "ClothingSystemRuntimeCommon", "ClothingSystemRuntimeInterface", "ChaosCloth"
        });
    }
}
