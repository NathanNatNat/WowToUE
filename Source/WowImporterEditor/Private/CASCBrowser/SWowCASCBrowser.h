#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SListView.h"
#include "CASCBrowser/FCASCSession.h"
#include "WowCASCInterface.h"
#include "WowM2Loader.h"

class SWowModelPreview;

class SWowCASCBrowser : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWowCASCBrowser) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// Connection
	FReply OnBrowseClicked();
	FReply OnConnectClicked();
	FReply OnLoadBuildClicked();
	void HandleCASCInitialized();
	void HandleBuildLoaded();
	void HandleCASCError(const FString& Error);

	// Tabs
	enum class EBrowserTab : uint8
	{
		Models, Textures, Characters, Items, ItemSets, Decor,
		Creatures, Audio, Maps, Zones, Text, Fonts, Data, Raw, Install, Settings
	};

	struct FTabDef
	{
		FText Label;
		EBrowserTab Tab;
		FString ExtFilter;
		TArray<FString> QuickFilters;
	};

	void SwitchTab(EBrowserTab Tab);
	const FTabDef* GetActiveTabDef() const;

	// Settings
	void BuildSettingsPanel();
	void LoadSettings();
	void SaveSettings();
	TSharedPtr<SVerticalBox> SettingsPanel;

	// File list
	void RefreshFileList();
	void OnSearchTextChanged(const FText& NewText);
	void OnSearchTextCommitted(const FText& NewText, ETextCommit::Type CommitType);
	void SortFileList();
	void OnFileSelectionChanged(TSharedPtr<FWowFileEntry> Item, ESelectInfo::Type SelectInfo);
	void OnFileDoubleClicked(TSharedPtr<FWowFileEntry> Item);
	TSharedRef<ITableRow> GenerateFileRow(TSharedPtr<FWowFileEntry> Item, const TSharedRef<STableViewBase>& OwnerTable);

	// Preview
	void PreviewBLP(uint32 FileDataID, const FString& FileName);
	void PreviewM2(uint32 FileDataID, const FString& FileName);
	void ClearPreview();
	FReply OnExportClicked();

	// Geoset/Skin panel
	void BuildGeosetPanel();
	void ClearGeosetPanel();
	void OnGeosetToggled(int32 Index, ECheckBoxState NewState);
	void OnSkinSelected(TSharedPtr<FWowCreatureDisplay> Item, ESelectInfo::Type SelectInfo);

	enum class ESortMode : uint8 { ByName, ByID };

	// Widgets
	TSharedPtr<FCASCSession> Session;
	TSharedPtr<SEditableTextBox> PathInput;
	TSharedPtr<SVerticalBox> BuildListBox;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<SEditableTextBox> SearchInput;
	TSharedPtr<SListView<TSharedPtr<FWowFileEntry>>> FileListView;
	TSharedPtr<STextBlock> PreviewInfoText;
	TSharedPtr<SVerticalBox> PreviewArea;
	TSharedPtr<SImage> PreviewImage;
	TSharedPtr<SWowModelPreview> ModelPreview;
	FSlateBrush PreviewBrush;
	UTexture2D* PreviewTexture = nullptr;
	TSharedPtr<STextBlock> ListStatusText;
	TSharedPtr<SHorizontalBox> QuickFilterBar;
	TSharedPtr<SVerticalBox> GeosetPanel;
	TSharedPtr<SVerticalBox> GeosetCheckboxList;
	TSharedPtr<SListView<TSharedPtr<FWowCreatureDisplay>>> SkinListView;
	TArray<TSharedPtr<FWowCreatureDisplay>> CreatureDisplays;

	// State
	TArray<FTabDef> TabDefs;
	TArray<TSharedPtr<FWowFileEntry>> FileEntries;
	TSharedPtr<FWowFileEntry> SelectedFile;

	// Settings values
	FString SettingCDNRegion = TEXT("eu");
	FString SettingLocale = TEXT("enGB");
	FString SettingListfileURL = TEXT("https://github.com/wowdev/wow-listfile/releases/latest/download/community-listfile.csv");
	FString SettingListfileFallbackURL;
	int32 SettingListfileRefreshDays = 3;
	FString SettingTactKeysURL = TEXT("https://raw.githubusercontent.com/wowdev/TACTKeys/master/WoW.txt");
	FString SettingTactKeysFallbackURL;
	FString SettingCDNFallbackHosts = TEXT("cdn.blizzard.com");
	FString SettingDBDURL = TEXT("https://raw.githubusercontent.com/wowdev/WoWDBDefs/refs/heads/master/definitions/%s.dbd");
	FString SettingDBDFallbackURL;
	FString SettingDBDFilenameURL = TEXT("https://raw.githubusercontent.com/wowdev/WoWDBDefs/refs/heads/master/manifest.json");
	FString SettingDBDFilenameFallbackURL;
	int32 SettingCacheExpiryDays = 7;
	bool SettingEnableM2Skins = true;
	bool SettingEnableUnknownFiles = true;
	bool SettingOverwriteFiles = true;
	bool SettingExportSharedTextures = true;
	bool SettingExportSharedChildren = true;
	bool SettingExportCollision = false;
	bool SettingExportUV2 = false;
	bool SettingExportNamedFiles = true;
	bool SettingShowUnknownItems = false;

	FString CurrentSearch;
	FString CurrentExtFilter = TEXT("m2");
	ESortMode CurrentSortMode = ESortMode::ByID;
	EBrowserTab ActiveTab = EBrowserTab::Models;
	int32 SelectedBuildIndex = 0;
	bool bBuildLoaded = false;
	bool bBuildLoading = false;
};
