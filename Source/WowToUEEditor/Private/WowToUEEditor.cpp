#include "WowToUEEditor.h"
#include "CASCBrowser/SWowCASCBrowser.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FWowToUEEditorModule"

const FName FWowToUEEditorModule::CASCBrowserTabId(TEXT("WowCASCBrowser"));

void FWowToUEEditorModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		CASCBrowserTabId,
		FOnSpawnTab::CreateRaw(this, &FWowToUEEditorModule::SpawnCASCBrowserTab))
		.SetDisplayName(LOCTEXT("CASCBrowserTabTitle", "WowToUE"))
		.SetTooltipText(LOCTEXT("CASCBrowserTabTooltip", "Browse and preview World of Warcraft assets"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		FToolMenuSection& Section = Menu->FindOrAddSection("WowToUE");
		Section.AddMenuEntry(
			"OpenCASCBrowser",
			LOCTEXT("MenuEntryLabel", "WowToUE"),
			LOCTEXT("MenuEntryTooltip", "Browse and preview World of Warcraft assets"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(FName("WowCASCBrowser"));
			}))
		);
	}));
}

void FWowToUEEditorModule::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(CASCBrowserTabId);
}

TSharedRef<SDockTab> FWowToUEEditorModule::SpawnCASCBrowserTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(NomadTab)
		[
			SNew(SWowCASCBrowser)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWowToUEEditorModule, WowToUEEditor)
