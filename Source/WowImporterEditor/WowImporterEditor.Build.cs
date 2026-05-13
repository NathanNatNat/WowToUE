using UnrealBuildTool;
using System.IO;

public class WowImporterEditor : ModuleRules
{
	public WowImporterEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"WowImporterRuntime",
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
