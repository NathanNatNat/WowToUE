using UnrealBuildTool;
using System.IO;

public class WowLib : ModuleRules
{
	public WowLib(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.CPlusPlus;
		PCHUsage = ModuleRules.PCHUsageMode.NoPCHs;

		// WowLib is standalone C++ — no UE headers, no UE macros
		bAddDefaultIncludePaths = false;
		bEnableUndefinedIdentifierWarnings = false;

		// Allow C++20 features (std::format, std::span, etc.)
		CppStandard = CppStandardVersion.Cpp20;

		// Third-party code — suppress warnings as errors
		bUseUnity = false;
		bUseRTTI = true;

		UnsafeTypeCastWarningLevel = WarningLevel.Off;
		ShadowVariableWarningLevel = WarningLevel.Off;

		// Source lives directly under this module directory
		// UBT auto-discovers .cpp files in the module directory tree

		// Include paths for wow.export.cpp source structure
		PublicIncludePaths.AddRange(new string[] {
			Path.Combine(ModuleDirectory, "Public"),
			Path.Combine(ModuleDirectory, "src"),
		});

		// Bundled third-party headers
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "extern", "nlohmann"));
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "extern", "stb"));
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "extern", "cpp-httplib"));

		// UE-bundled zlib
		AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");

		// Bundled OpenSSL 3.x (UE's is too old for cpp-httplib)
		string OpenSSLDir = Path.Combine(ModuleDirectory, "extern", "openssl");
		PublicIncludePaths.Add(Path.Combine(OpenSSLDir, "include"));
		PublicAdditionalLibraries.Add(Path.Combine(OpenSSLDir, "lib", "libcrypto.lib"));
		PublicAdditionalLibraries.Add(Path.Combine(OpenSSLDir, "lib", "libssl.lib"));
		RuntimeDependencies.Add(Path.Combine(OpenSSLDir, "bin", "libcrypto-3-x64.dll"));
		RuntimeDependencies.Add(Path.Combine(OpenSSLDir, "bin", "libssl-3-x64.dll"));

		// Minimal UE dependency for platform defines
		PublicDependencyModuleNames.Add("Core");

		// Compile-time defines that wow.export.cpp expects from CMake
		// These must be Public because constants.h uses them in inline constexpr
		PublicDefinitions.Add("WOW_EXPORT_VERSION=\"0.1.0\"");
		PublicDefinitions.Add("WOW_EXPORT_SOURCE_DIR=\"\"");
		PublicDefinitions.Add("WOW_EXPORT_BUILD_TYPE=\"release\"");

		// httplib needs OpenSSL support for HTTPS
		PrivateDefinitions.Add("CPPHTTPLIB_OPENSSL_SUPPORT");

		// Disable WebP/miniaudio in buffer.cpp and blp.cpp (not needed for UE)
		PrivateDefinitions.Add("WOWLIB_NO_WEBP");
		PrivateDefinitions.Add("WOWLIB_NO_AUDIO");

		// Route wow.export.cpp logging to UE_LOG
		PrivateDefinitions.Add("WOWLIB_USE_UE_LOG=1");

		// Compiler settings
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDefinitions.Add("NOMINMAX");
			PublicDefinitions.Add("_CRT_SECURE_NO_WARNINGS");
		}
	}
}
