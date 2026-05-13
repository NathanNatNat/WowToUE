using UnrealBuildTool;
using System.IO;

public class WowToUEEditor : ModuleRules
{
	public WowToUEEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"WowToUERuntime",
				"WowLib",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"ToolMenus",
				"InputCore",
				"MeshDescription",
				"StaticMeshDescription",
				"SkeletalMeshDescription",
				"SkeletalMeshUtilitiesCommon",
				"AnimationCore",
				"HTTP",
				"WorkspaceMenuStructure",
				"DesktopPlatform",
				"AssetTools",
				"AssetRegistry",
				"AdvancedPreviewScene",
				"MeshConversion",
				"RenderCore",
				"RHI",
			}
		);

		string WowLibPath = Path.Combine(ModuleDirectory, "..", "ThirdParty", "WowLib");
		PublicIncludePaths.Add(Path.Combine(WowLibPath, "include"));
	}
}
