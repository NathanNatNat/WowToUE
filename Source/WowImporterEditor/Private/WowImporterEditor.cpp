#include "WowImporterEditor.h"
#include "CASCBrowser/SWowCASCBrowser.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FWowImporterEditorModule"

const FName FWowImporterEditorModule::CASCBrowserTabId(TEXT("WowCASCBrowser"));

void FWowImporterEditorModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		CASCBrowserTabId,
		FOnSpawnTab::CreateRaw(this, &FWowImporterEditorModule::SpawnCASCBrowserTab))
		.SetDisplayName(LOCTEXT("CASCBrowserTabTitle", "WoW CASC Browser"))
		.SetTooltipText(LOCTEXT("CASCBrowserTabTooltip", "Browse and import World of Warcraft assets"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		FToolMenuSection& Section = Menu->FindOrAddSection("WowImporter");
		Section.AddMenuEntry(
			"OpenCASCBrowser",
			LOCTEXT("MenuEntryLabel", "WoW CASC Browser"),
			LOCTEXT("MenuEntryTooltip", "Open the World of Warcraft CASC asset browser"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(FName("WowCASCBrowser"));
			}))
		);
	}));
}

void FWowImporterEditorModule::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(CASCBrowserTabId);
}

TSharedRef<SDockTab> FWowImporterEditorModule::SpawnCASCBrowserTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(NomadTab)
		[
			SNew(SWowCASCBrowser)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWowImporterEditorModule, WowImporterEditor)
