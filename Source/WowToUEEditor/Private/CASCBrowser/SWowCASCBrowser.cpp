#include "CASCBrowser/SWowCASCBrowser.h"
#include "WowM2Loader.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SComboBox.h"
#include "CASCBrowser/SWowModelPreview.h"
#include "DesktopPlatformModule.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/Texture2D.h"
#include "Widgets/Images/SImage.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/TextureFactory.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PackageTools.h"

#define LOCTEXT_NAMESPACE "SWowCASCBrowser"

static const FString WowToUEIni = GEditorPerProjectIni;
static const TCHAR* ConfigSection = TEXT("WowToUE");

void SWowCASCBrowser::Construct(const FArguments& InArgs)
{
	// ── Tab definitions matching wow.export.cpp ──
	TabDefs = {
		{ LOCTEXT("TabModels",     "Models"),      EBrowserTab::Models,     TEXT("m2"),  { TEXT("m2"), TEXT("wmo"), TEXT("m3") } },
		{ LOCTEXT("TabTextures",   "Textures"),    EBrowserTab::Textures,   TEXT("blp"), { TEXT("blp") } },
		{ LOCTEXT("TabCharacters", "Characters"),  EBrowserTab::Characters, TEXT("m2"),  { TEXT("m2") } },
		{ LOCTEXT("TabItems",      "Items"),       EBrowserTab::Items,      TEXT("m2"),  { TEXT("m2") } },
		{ LOCTEXT("TabItemSets",   "Item Sets"),   EBrowserTab::ItemSets,   TEXT("m2"),  { TEXT("m2") } },
		{ LOCTEXT("TabDecor",      "Decor"),       EBrowserTab::Decor,      TEXT("m2"),  { TEXT("m2"), TEXT("wmo") } },
		{ LOCTEXT("TabCreatures",  "Creatures"),   EBrowserTab::Creatures,  TEXT("m2"),  { TEXT("m2") } },
		{ LOCTEXT("TabAudio",      "Audio"),       EBrowserTab::Audio,      TEXT("ogg"), { TEXT("ogg"), TEXT("mp3") } },
		{ LOCTEXT("TabMaps",       "Maps"),        EBrowserTab::Maps,       TEXT("wdt"), { TEXT("wdt"), TEXT("adt") } },
		{ LOCTEXT("TabZones",      "Zones"),       EBrowserTab::Zones,      TEXT(""),    {} },
		{ LOCTEXT("TabText",       "Text"),        EBrowserTab::Text,       TEXT(""),    { TEXT("lua"), TEXT("xml"), TEXT("txt"), TEXT("toc") } },
		{ LOCTEXT("TabFonts",      "Fonts"),       EBrowserTab::Fonts,      TEXT("ttf"), { TEXT("ttf") } },
		{ LOCTEXT("TabData",       "Data"),        EBrowserTab::Data,       TEXT("db2"), { TEXT("db2") } },
		{ LOCTEXT("TabRaw",        "Raw Files"),   EBrowserTab::Raw,        TEXT(""),    {} },
		{ LOCTEXT("TabInstall",    "Install"),     EBrowserTab::Install,    TEXT(""),    {} },
		{ LOCTEXT("TabSettings",   "Settings"),    EBrowserTab::Settings,   TEXT(""),    {} },
	};

	Session = MakeShared<FCASCSession>();
	Session->OnInitialized.BindSP(this, &SWowCASCBrowser::HandleCASCInitialized);
	Session->OnBuildLoaded.BindSP(this, &SWowCASCBrowser::HandleBuildLoaded);
	Session->OnError.BindSP(this, &SWowCASCBrowser::HandleCASCError);

	// Tab button builder
	auto MakeTab = [this](const FTabDef& Def)
	{
		return SNew(SButton)
			.Text(Def.Label)
			.ButtonColorAndOpacity_Lambda([this, Tab = Def.Tab]()
			{
				return (Tab == ActiveTab)
					? FSlateColor(FLinearColor(0.15f, 0.35f, 0.65f))
					: FSlateColor::UseForeground();
			})
			.OnClicked_Lambda([this, Tab = Def.Tab]() { SwitchTab(Tab); return FReply::Handled(); });
	};

	// Build tab bar
	TSharedRef<SHorizontalBox> TabBar = SNew(SHorizontalBox);
	for (const auto& Def : TabDefs)
	{
		TabBar->AddSlot()
		.AutoWidth()
		.Padding(0, 0, 1, 0)
		[
			MakeTab(Def)
		];
	}

	ChildSlot
	[
		SNew(SVerticalBox)

		// ── Connection bar ──
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8, 6)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0, 0, 4, 0)
			[
				SAssignNew(PathInput, SEditableTextBox)
				.HintText(LOCTEXT("PathHint", "Path to WoW installation"))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("Browse", "Browse..."))
				.OnClicked(this, &SWowCASCBrowser::OnBrowseClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Connect", "Connect"))
				.OnClicked(this, &SWowCASCBrowser::OnConnectClicked)
			]
		]

		// ── Status / build selector ──
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8, 2)
		[
			SAssignNew(StatusText, STextBlock)
			.Text(LOCTEXT("StatusReady", "Enter your WoW installation path and click Connect."))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8, 2)
		[
			SAssignNew(BuildListBox, SVerticalBox)
		]

		// ── Tab bar ──
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8, 4, 8, 0)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return bBuildLoaded ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SScrollBox)
				.Orientation(Orient_Horizontal)
				+ SScrollBox::Slot()
				[
					TabBar
				]
			]
		]

		// ── Settings panel (shown when Settings tab active) ──
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(8, 4, 8, 8)
		[
			SAssignNew(SettingsPanel, SVerticalBox)
			.Visibility(EVisibility::Collapsed)
		]

		// ── Main content area: list (left) + preview (right) ──
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(8, 4, 8, 8)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return (bBuildLoaded && ActiveTab != EBrowserTab::Settings) ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)

				// ════ LEFT COLUMN: file list + status + filter ════
				+ SSplitter::Slot()
				.Value(0.6f)
				[
					SNew(SVerticalBox)

					// File list (fills most of left column)
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(0, 0, 4, 0)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						[
							SAssignNew(FileListView, SListView<TSharedPtr<FWowFileEntry>>)
							.ListItemsSource(&FileEntries)
							.OnGenerateRow(this, &SWowCASCBrowser::GenerateFileRow)
							.OnSelectionChanged(this, &SWowCASCBrowser::OnFileSelectionChanged)
							.OnMouseButtonDoubleClick(this, &SWowCASCBrowser::OnFileDoubleClicked)
							.SelectionMode(ESelectionMode::Single)
						]
					]

					// Status bar: file count + quick filter buttons
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 4, 4, 0)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0, 0, 8, 0)
						[
							SAssignNew(ListStatusText, STextBlock)
							.Text(LOCTEXT("ListEmpty", "0 files"))
						]

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SAssignNew(QuickFilterBar, SHorizontalBox)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(4, 0, 0, 0)
						[
							SNew(SButton)
							.Text(LOCTEXT("SortAZ", "A-Z"))
							.OnClicked_Lambda([this]() { CurrentSortMode = ESortMode::ByName; SortFileList(); return FReply::Handled(); })
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2, 0, 0, 0)
						[
							SNew(SButton)
							.Text(LOCTEXT("SortID", "ID"))
							.OnClicked_Lambda([this]() { CurrentSortMode = ESortMode::ByID; SortFileList(); return FReply::Handled(); })
						]
					]

					// Filter bar: search input
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 4, 4, 0)
					[
						SAssignNew(SearchInput, SEditableTextBox)
						.HintText(LOCTEXT("SearchHint", "Search..."))
						.OnTextChanged(this, &SWowCASCBrowser::OnSearchTextChanged)
						.OnTextCommitted(this, &SWowCASCBrowser::OnSearchTextCommitted)
					]
				]

				// ════ RIGHT COLUMN: preview + controls ════
				+ SSplitter::Slot()
				.Value(0.4f)
				[
					SNew(SVerticalBox)

					// Preview area (fills most of right column)
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(4, 0, 0, 0)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.Padding(12)
						[
							SAssignNew(PreviewArea, SVerticalBox)

							// 3D model viewport + geoset/skin panel
							+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							[
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								[
									SAssignNew(ModelPreview, SWowModelPreview)
									.Visibility(EVisibility::Collapsed)
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew(SBox)
									.WidthOverride(200.f)
									[
										SAssignNew(GeosetPanel, SVerticalBox)
										.Visibility(EVisibility::Collapsed)
									]
								]
							]

							// Texture image preview
							+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SAssignNew(PreviewImage, SImage)
								.Image(&PreviewBrush)
								.Visibility(EVisibility::Collapsed)
							]

							// Info text (shown when no preview active, or below preview)
							+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Center)
							.Padding(0, 4, 0, 0)
							[
								SAssignNew(PreviewInfoText, STextBlock)
								.Text(LOCTEXT("PreviewNone", "Select a file to preview"))
								.Justification(ETextJustify::Center)
							]
						]
					]

					// Preview controls bar (export button, etc.)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(4, 4, 0, 0)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNullWidget::NullWidget
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.Text(LOCTEXT("Export", "Import to UE"))
							.IsEnabled_Lambda([this]() { return SelectedFile.IsValid(); })
							.OnClicked(this, &SWowCASCBrowser::OnExportClicked)
						]
					]
				]
			]
		]
	];

	// Load settings and build settings panel
	LoadSettings();
	BuildSettingsPanel();

	// Restore saved path
	FString SavedPath;
	if (GConfig->GetString(ConfigSection, TEXT("InstallPath"), SavedPath, WowToUEIni) && !SavedPath.IsEmpty())
	{
		PathInput->SetText(FText::FromString(SavedPath));
	}
}

// ── Tab switching ──

void SWowCASCBrowser::SwitchTab(EBrowserTab Tab)
{
	ActiveTab = Tab;

	const FTabDef* Def = GetActiveTabDef();
	if (Def)
	{
		CurrentExtFilter = Def->ExtFilter;

		// Rebuild quick filter buttons
		QuickFilterBar->ClearChildren();
		for (const FString& Ext : Def->QuickFilters)
		{
			QuickFilterBar->AddSlot()
			.AutoWidth()
			.Padding(0, 0, 2, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(FString::Printf(TEXT(".%s"), *Ext)))
				.OnClicked_Lambda([this, Ext]()
				{
					CurrentExtFilter = Ext;
					RefreshFileList();
					return FReply::Handled();
				})
			];
		}
	}

	SelectedFile = nullptr;
	PreviewInfoText->SetText(LOCTEXT("PreviewNone", "Select a file to preview"));

	if (Tab == EBrowserTab::Settings)
	{
		if (SettingsPanel.IsValid())
			SettingsPanel->SetVisibility(EVisibility::Visible);
	}
	else
	{
		if (SettingsPanel.IsValid())
			SettingsPanel->SetVisibility(EVisibility::Collapsed);
		RefreshFileList();
	}
}

const SWowCASCBrowser::FTabDef* SWowCASCBrowser::GetActiveTabDef() const
{
	for (const auto& Def : TabDefs)
	{
		if (Def.Tab == ActiveTab) return &Def;
	}
	return nullptr;
}

// ── File list ──

TSharedRef<ITableRow> SWowCASCBrowser::GenerateFileRow(TSharedPtr<FWowFileEntry> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	FString DisplayText = FString::Printf(TEXT("%s [%u]"), *Item->FileName, Item->FileDataID);
	return SNew(STableRow<TSharedPtr<FWowFileEntry>>, OwnerTable)
		[
			SNew(STextBlock)
			.Text(FText::FromString(DisplayText))
			.Margin(FMargin(4, 2))
		];
}

void SWowCASCBrowser::OnFileSelectionChanged(TSharedPtr<FWowFileEntry> Item, ESelectInfo::Type SelectInfo)
{
	SelectedFile = Item;

	if (!Item.IsValid())
	{
		ClearPreview();
		return;
	}

	FString Ext;
	Item->FileName.Split(TEXT("."), nullptr, &Ext, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

	if (Ext.Equals(TEXT("blp"), ESearchCase::IgnoreCase))
	{
		PreviewBLP(Item->FileDataID, Item->FileName);
	}
	else if (Ext.Equals(TEXT("m2"), ESearchCase::IgnoreCase))
	{
		PreviewM2(Item->FileDataID, Item->FileName);
	}
	else if (Ext.Equals(TEXT("m3"), ESearchCase::IgnoreCase))
	{
		PreviewM3(Item->FileDataID, Item->FileName);
	}
	else
	{
		ClearPreview();
		FString Info = FString::Printf(TEXT("%s\n\nFile Data ID: %u\nType: .%s"),
			*Item->FileName, Item->FileDataID, *Ext);
		PreviewInfoText->SetText(FText::FromString(Info));
	}
}

void SWowCASCBrowser::OnFileDoubleClicked(TSharedPtr<FWowFileEntry> Item)
{
	if (Item.IsValid())
	{
		SelectedFile = Item;
		OnExportClicked();
	}
}

void SWowCASCBrowser::ClearPreview()
{
	PreviewImage->SetVisibility(EVisibility::Collapsed);
	ModelPreview->SetVisibility(EVisibility::Collapsed);
	ModelPreview->ClearModel();
	ClearGeosetPanel();
	PreviewInfoText->SetVisibility(EVisibility::HitTestInvisible);
	PreviewInfoText->SetText(LOCTEXT("PreviewNone", "Select a file to preview"));
	PreviewTexture = nullptr;
}

void SWowCASCBrowser::ClearGeosetPanel()
{
	if (GeosetPanel.IsValid())
	{
		GeosetPanel->ClearChildren();
		GeosetPanel->SetVisibility(EVisibility::Collapsed);
	}
	CreatureDisplays.Empty();
	SkinListView.Reset();
}

void SWowCASCBrowser::BuildGeosetPanel()
{
	if (!GeosetPanel.IsValid())
		return;

	GeosetPanel->ClearChildren();

	int32 NumSubs = ModelPreview->GetNumSubMeshes();
	if (NumSubs == 0)
		return;

	GeosetPanel->ClearChildren();

	// Geosets header
	GeosetPanel->AddSlot()
	.AutoHeight()
	.Padding(4, 4)
	[
		SNew(STextBlock)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		.Text(LOCTEXT("GeosetsHeader", "Geosets"))
	];

	// Geoset checkboxes in scrollable box — 50% of panel height
	TSharedPtr<SScrollBox> GeosetScroll;
	GeosetPanel->AddSlot()
	.FillHeight(1.0f)
	.Padding(4, 0)
	[
		SAssignNew(GeosetScroll, SScrollBox)
		.ScrollBarAlwaysVisible(true)
	];

	for (int32 i = 0; i < NumSubs; ++i)
	{
		if (!ModelPreview->HasTextureUnit(i))
			continue;

		FString Label = ModelPreview->GetGeosetLabel(i);

		GeosetScroll->AddSlot()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([this, i]()
			{
				return ModelPreview->IsGeosetVisible(i) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([this, i](ECheckBoxState State)
			{
				OnGeosetToggled(i, State);
			})
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			]
		];
	}

	// Enable All / Disable All
	GeosetPanel->AddSlot()
	.AutoHeight()
	.Padding(4, 2)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.OnClicked_Lambda([this]()
			{
				ModelPreview->SetAllGeosetsVisible(true);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EnableAll", "Enable All"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.6f, 1.0f)))
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4, 0, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Separator", "/"))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4, 0, 0, 0)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.OnClicked_Lambda([this]()
			{
				ModelPreview->SetAllGeosetsVisible(false);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DisableAll", "Disable All"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.6f, 1.0f)))
			]
		]
	];

	// Skins section — always present, 50% of panel height
	GeosetPanel->AddSlot()
	.AutoHeight()
	.Padding(4, 8, 4, 4)
	[
		SNew(STextBlock)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		.Text(LOCTEXT("SkinsHeader", "Skins"))
	];

	GeosetPanel->AddSlot()
	.FillHeight(1.0f)
	.Padding(4, 0)
	[
		SAssignNew(SkinListView, SListView<TSharedPtr<FWowCreatureDisplay>>)
		.ListItemsSource(&CreatureDisplays)
		.OnGenerateRow_Lambda([](TSharedPtr<FWowCreatureDisplay> Item, const TSharedRef<STableViewBase>& Owner)
		{
			return SNew(STableRow<TSharedPtr<FWowCreatureDisplay>>, Owner)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Label))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				.ToolTipText(FText::FromString(Item->Label))
			];
		})
		.OnSelectionChanged(this, &SWowCASCBrowser::OnSkinSelected)
	];

	// Animation section
	AnimationList.Empty();
	if (ModelPreview.IsValid())
	{
		// "No Animation" entry
		auto NoAnim = MakeShared<FWowAnimationInfo>();
		NoAnim->AnimIndex = -1;
		NoAnim->Label = TEXT("No Animation");
		AnimationList.Add(NoAnim);

		for (const auto& Anim : CurrentModelData.Animations)
			AnimationList.Add(MakeShared<FWowAnimationInfo>(Anim));

		SelectedAnimation = AnimationList[0];
	}

	if (AnimationList.Num() > 1)
	{
		GeosetPanel->AddSlot()
		.AutoHeight()
		.Padding(4, 8, 4, 4)
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			.Text(LOCTEXT("AnimHeader", "Animation"))
		];

		// Animation dropdown
		GeosetPanel->AddSlot()
		.AutoHeight()
		.Padding(4, 0)
		[
			SNew(SComboBox<TSharedPtr<FWowAnimationInfo>>)
			.OptionsSource(&AnimationList)
			.OnSelectionChanged_Lambda([this](TSharedPtr<FWowAnimationInfo> Item, ESelectInfo::Type)
			{
				if (!Item.IsValid() || !ModelPreview.IsValid()) return;
				SelectedAnimation = Item;
				if (Item->AnimIndex < 0)
					ModelPreview->StopAnimation();
				else
					ModelPreview->PlayAnimation(Item->AnimIndex);
			})
			.OnGenerateWidget_Lambda([](TSharedPtr<FWowAnimationInfo> Item)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(Item->Label))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9));
			})
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					return SelectedAnimation.IsValid() ? FText::FromString(SelectedAnimation->Label) : FText::GetEmpty();
				})
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			]
		];

		// Play/Pause + Step buttons
		GeosetPanel->AddSlot()
		.AutoHeight()
		.Padding(4, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text_Lambda([this]()
				{
					return ModelPreview.IsValid() && ModelPreview->IsAnimationPaused()
						? LOCTEXT("Play", "Play") : LOCTEXT("Pause", "Pause");
				})
				.OnClicked_Lambda([this]()
				{
					if (ModelPreview.IsValid())
						ModelPreview->SetAnimationPaused(!ModelPreview->IsAnimationPaused());
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4, 0, 0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("StepBack", "<<"))
				.OnClicked_Lambda([this]()
				{
					if (ModelPreview.IsValid()) ModelPreview->StepAnimationFrame(-1);
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2, 0, 0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("StepFwd", ">>"))
				.OnClicked_Lambda([this]()
				{
					if (ModelPreview.IsValid()) ModelPreview->StepAnimationFrame(1);
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(8, 0, 0, 0)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					if (!ModelPreview.IsValid()) return FText::GetEmpty();
					int32 F = ModelPreview->GetAnimationFrame();
					int32 C = ModelPreview->GetAnimationFrameCount();
					return FText::FromString(FString::Printf(TEXT("%d / %d"), F, C));
				})
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			]
		];

		// Frame slider
		GeosetPanel->AddSlot()
		.AutoHeight()
		.Padding(4, 0)
		[
			SNew(SSlider)
			.Value_Lambda([this]()
			{
				if (!ModelPreview.IsValid()) return 0.f;
				int32 C = ModelPreview->GetAnimationFrameCount();
				return C > 1 ? static_cast<float>(ModelPreview->GetAnimationFrame()) / (C - 1) : 0.f;
			})
			.OnValueChanged_Lambda([this](float Value)
			{
				if (!ModelPreview.IsValid()) return;
				int32 C = ModelPreview->GetAnimationFrameCount();
				ModelPreview->SetAnimationFrame(FMath::RoundToInt32(Value * (C - 1)));
			})
		];
	}

	// Show Bones toggle
	GeosetPanel->AddSlot()
	.AutoHeight()
	.Padding(4, 8, 4, 4)
	[
		SNew(SCheckBox)
		.IsChecked_Lambda([this]() { return ModelPreview->bShowBones ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
		.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { ModelPreview->bShowBones = (State == ECheckBoxState::Checked); })
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ShowBones", "Show Bones"))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
		]
	];

	// Enable Skeleton toggle
	GeosetPanel->AddSlot()
	.AutoHeight()
	.Padding(4, 2, 4, 4)
	[
		SNew(SCheckBox)
		.IsChecked_Lambda([this]() { return ModelPreview->bSkeletonEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
		.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { ModelPreview->SetSkeletonEnabled(State == ECheckBoxState::Checked); })
		[
			SNew(STextBlock)
			.Text(LOCTEXT("EnableSkeleton", "Enable Skeleton"))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
		]
	];

	// Bone list (collapsible)
	int32 NumBones = ModelPreview->GetNumBones();
	if (NumBones > 0)
	{
		TSharedPtr<SVerticalBox> BoneListBox;
		GeosetPanel->AddSlot()
		.AutoHeight()
		.Padding(4, 8, 4, 0)
		[
			SNew(SExpandableArea)
			.AreaTitle(FText::Format(LOCTEXT("BoneListTitle", "Bones ({0})"), FText::AsNumber(NumBones)))
			.InitiallyCollapsed(true)
			.HeaderPadding(FMargin(2, 4))
			.BodyContent()
			[
				SNew(SBox)
				.MaxDesiredHeight(300.f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(BoneListBox, SVerticalBox)
					]
				]
			]
		];

		for (int32 i = 0; i < NumBones; ++i)
		{
			int32 Depth = ModelPreview->GetBoneDepth(i);
			FName BoneName = ModelPreview->GetBoneName(i);

			BoneListBox->AddSlot()
			.AutoHeight()
			.Padding(4 + Depth * 12, 1, 4, 1)
			[
				SNew(STextBlock)
				.Text(FText::FromName(BoneName))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f)))
			];
		}
	}

	GeosetPanel->SetVisibility(EVisibility::Visible);

	// Auto-select first creature skin (matches wow.export behavior)
	if (CreatureDisplays.Num() > 0 && SkinListView.IsValid())
	{
		SkinListView->SetSelection(CreatureDisplays[0]);
	}
}

void SWowCASCBrowser::OnGeosetToggled(int32 Index, ECheckBoxState NewState)
{
	if (ModelPreview.IsValid())
	{
		ModelPreview->SetGeosetVisible(Index, NewState == ECheckBoxState::Checked);
	}
}

void SWowCASCBrowser::OnSkinSelected(TSharedPtr<FWowCreatureDisplay> Item, ESelectInfo::Type SelectInfo)
{
	if (Item.IsValid() && ModelPreview.IsValid())
	{
		ModelPreview->ApplyCreatureDisplay(*Item);
	}
}

void SWowCASCBrowser::PreviewBLP(uint32 FileDataID, const FString& FileName)
{
	FWowCASCInterface::FDecodedTexture Decoded;
	FString Error;

	if (!FWowCASCInterface::Get().DecodeBLP(FileDataID, Decoded, Error))
	{
		ClearPreview();
		PreviewInfoText->SetText(FText::FromString(FString::Printf(TEXT("Failed to decode: %s"), *Error)));
		return;
	}

	// Create transient UTexture2D for preview
	PreviewTexture = UTexture2D::CreateTransient(Decoded.Width, Decoded.Height, PF_R8G8B8A8);
	if (!PreviewTexture)
	{
		ClearPreview();
		PreviewInfoText->SetText(LOCTEXT("PreviewFailed", "Failed to create preview texture"));
		return;
	}

	PreviewTexture->AddToRoot();
	PreviewTexture->SRGB = true;
	PreviewTexture->Filter = TF_Default;

	// Copy RGBA data into texture
	FTexture2DMipMap& Mip = PreviewTexture->GetPlatformData()->Mips[0];
	void* MipData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(MipData, Decoded.RGBA.GetData(), Decoded.RGBA.Num());
	Mip.BulkData.Unlock();
	PreviewTexture->UpdateResource();

	// Set up brush for SImage
	PreviewBrush.SetResourceObject(PreviewTexture);
	PreviewBrush.ImageSize = FVector2D(Decoded.Width, Decoded.Height);

	PreviewImage->SetVisibility(EVisibility::Visible);

	FString Info = FString::Printf(TEXT("%s\n%ux%u"), *FileName, Decoded.Width, Decoded.Height);
	PreviewInfoText->SetVisibility(EVisibility::HitTestInvisible);
	PreviewInfoText->SetText(FText::FromString(Info));
}

void SWowCASCBrowser::PreviewM2(uint32 FileDataID, const FString& FileName)
{
	ClearPreview();
	PreviewInfoText->SetText(FText::FromString(FString::Printf(TEXT("Loading %s..."), *FileName)));

	Async(EAsyncExecution::Thread, [this, FileDataID, FileName]()
	{
		auto ModelDataPtr = MakeShared<FWowM2ModelData>();
		auto DisplaysPtr = MakeShared<TArray<FWowCreatureDisplay>>();
		auto LoadResult = MakeShared<FWowM2Loader::FM2LoadResult>();
		FString Error;

		bool bSuccess = FWowM2Loader::LoadM2(FileDataID, *ModelDataPtr, *LoadResult, Error);

		if (bSuccess)
		{
			FString CreatureError;
			FWowM2Loader::ResolveCreatureTextures(*ModelDataPtr, CreatureError);

			FString DisplayError;
			bool bDisplaySuccess = FWowM2Loader::GetCreatureDisplays(FileDataID, *DisplaysPtr, DisplayError);
			UE_LOG(LogTemp, Log, TEXT("GetCreatureDisplays(%u): success=%d, count=%d, error='%s'"),
				FileDataID, bDisplaySuccess, DisplaysPtr->Num(), *DisplayError);
		}

		AsyncTask(ENamedThreads::GameThread, [this, bSuccess, ModelDataPtr, DisplaysPtr, LoadResult, FileName, Error]()
		{
			if (!bSuccess)
			{
				PreviewInfoText->SetText(FText::FromString(FString::Printf(TEXT("Failed to load M2: %s"), *Error)));
				return;
			}

			PreviewImage->SetVisibility(EVisibility::Collapsed);
			ModelPreview->SetVisibility(EVisibility::Visible);
			CachedM2Buffer = LoadResult->M2Buffer;
			CachedM2Loader = LoadResult->Loader;
			CachedSkelBuffer = LoadResult->SkelBuffer;
			CachedSkelLoader = LoadResult->SkelLoader;
			CachedParentSkelBuffer = LoadResult->ParentSkelBuffer;
			CachedParentSkelLoader = LoadResult->ParentSkelLoader;
			CurrentModelData = *ModelDataPtr;
			ModelPreview->SetM2Model(*ModelDataPtr, LoadResult->Loader.Get(),
				LoadResult->SkelLoader.Get(), LoadResult->ParentSkelLoader.Get());

			CreatureDisplays.Empty();
			for (auto& D : *DisplaysPtr)
				CreatureDisplays.Add(MakeShared<FWowCreatureDisplay>(MoveTemp(D)));

			BuildGeosetPanel();

			FString Info = FString::Printf(TEXT("%s\n%d verts, %d tris, %d bones, %d anims"),
				*ModelDataPtr->Name, ModelDataPtr->VertexCount, ModelDataPtr->TriangleCount,
				ModelDataPtr->BoneCount, ModelDataPtr->AnimationCount);
			PreviewInfoText->SetText(FText::FromString(Info));
		});
	});
}

void SWowCASCBrowser::PreviewM3(uint32 FileDataID, const FString& FileName)
{
	ClearPreview();
	PreviewInfoText->SetText(FText::FromString(FString::Printf(TEXT("Loading %s..."), *FileName)));

	Async(EAsyncExecution::Thread, [this, FileDataID, FileName]()
	{
		auto ModelDataPtr = MakeShared<FWowM2ModelData>();
		FString Error;

		bool bSuccess = FWowM2Loader::LoadM3(FileDataID, *ModelDataPtr, Error);

		AsyncTask(ENamedThreads::GameThread, [this, bSuccess, ModelDataPtr, FileName, Error]()
		{
			if (!bSuccess)
			{
				PreviewInfoText->SetText(FText::FromString(FString::Printf(TEXT("Failed to load M3: %s"), *Error)));
				return;
			}

			PreviewImage->SetVisibility(EVisibility::Collapsed);
			ModelPreview->SetVisibility(EVisibility::Visible);
			CurrentModelData = *ModelDataPtr;
			ModelPreview->SetM2Model(*ModelDataPtr, nullptr);

			BuildGeosetPanel();

			FString Info = FString::Printf(TEXT("%s\n%d verts, %d tris"),
				*FileName, ModelDataPtr->VertexCount, ModelDataPtr->TriangleCount);
			PreviewInfoText->SetText(FText::FromString(Info));
		});
	});
}

FReply SWowCASCBrowser::OnExportClicked()
{
	if (!SelectedFile.IsValid()) return FReply::Handled();

	FString Ext;
	SelectedFile->FileName.Split(TEXT("."), nullptr, &Ext, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

	if (!Ext.Equals(TEXT("blp"), ESearchCase::IgnoreCase))
	{
		PreviewInfoText->SetText(LOCTEXT("ExportUnsupported", "Import not yet supported for this file type."));
		return FReply::Handled();
	}

	FWowCASCInterface::FDecodedTexture Decoded;
	FString Error;

	if (!FWowCASCInterface::Get().DecodeBLP(SelectedFile->FileDataID, Decoded, Error))
	{
		PreviewInfoText->SetText(FText::FromString(FString::Printf(TEXT("Decode failed: %s"), *Error)));
		return FReply::Handled();
	}

	// Mirror WoW listfile path as UE asset path (e.g. character/bloodelf/female/tex.blp -> /Game/WoW/character/bloodelf/female/tex)
	FString WowPath = SelectedFile->FileName;
	WowPath.ReplaceInline(TEXT("\\"), TEXT("/"));

	// Remove extension
	int32 DotIdx;
	if (WowPath.FindLastChar('.', DotIdx))
	{
		WowPath.LeftInline(DotIdx);
	}

	FString PackagePath = FString::Printf(TEXT("/Game/WoW/%s"), *WowPath);

	UPackage* Package = CreatePackage(*PackagePath);
	Package->FullyLoad();

	UTexture2D* NewTexture = NewObject<UTexture2D>(Package, *FPaths::GetBaseFilename(PackagePath), RF_Public | RF_Standalone);

	NewTexture->Source.Init(Decoded.Width, Decoded.Height, 1, 1, TSF_BGRA8);
	uint8* DestData = NewTexture->Source.LockMip(0);

	// RGBA -> BGRA swizzle for TSF_BGRA8
	const uint8* SrcData = Decoded.RGBA.GetData();
	int32 PixelCount = Decoded.Width * Decoded.Height;
	for (int32 i = 0; i < PixelCount; ++i)
	{
		DestData[i * 4 + 0] = SrcData[i * 4 + 2]; // B
		DestData[i * 4 + 1] = SrcData[i * 4 + 1]; // G
		DestData[i * 4 + 2] = SrcData[i * 4 + 0]; // R
		DestData[i * 4 + 3] = SrcData[i * 4 + 3]; // A
	}

	NewTexture->Source.UnlockMip(0);
	NewTexture->SRGB = true;
	NewTexture->CompressionSettings = TC_Default;
	NewTexture->PostEditChange();
	NewTexture->MarkPackageDirty();

	FAssetRegistryModule::AssetCreated(NewTexture);
	Package->MarkPackageDirty();

	PreviewInfoText->SetText(FText::FromString(FString::Printf(TEXT("Imported to %s"), *PackagePath)));

	return FReply::Handled();
}

// ── Connection ──

FReply SWowCASCBrowser::OnBrowseClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FString SelectedDir;
		const bool bOpened = DesktopPlatform->OpenDirectoryDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()),
			LOCTEXT("BrowseTitle", "Select WoW Installation Folder").ToString(),
			PathInput->GetText().ToString(),
			SelectedDir
		);

		if (bOpened)
		{
			PathInput->SetText(FText::FromString(SelectedDir));
		}
	}
	return FReply::Handled();
}

FReply SWowCASCBrowser::OnConnectClicked()
{
	FString Path = PathInput->GetText().ToString();
	if (Path.IsEmpty())
	{
		StatusText->SetText(LOCTEXT("NoPath", "Please enter a WoW installation path."));
		return FReply::Handled();
	}

	GConfig->SetString(ConfigSection, TEXT("InstallPath"), *Path, WowToUEIni);
	GConfig->Flush(false, WowToUEIni);

	StatusText->SetText(LOCTEXT("Connecting", "Connecting to CASC archive..."));
	BuildListBox->ClearChildren();
	bBuildLoaded = false;
	FileEntries.Empty();

	// Apply settings before connecting
	FWowCASCInterface::FWowSettings WowSettings;
	WowSettings.CDNRegion = SettingCDNRegion;
	WowSettings.Locale = SettingLocale;
	WowSettings.ListfileURL = SettingListfileURL;
	WowSettings.ListfileFallbackURL = SettingListfileFallbackURL;
	WowSettings.ListfileRefreshDays = SettingListfileRefreshDays;
	WowSettings.TactKeysURL = SettingTactKeysURL;
	WowSettings.TactKeysFallbackURL = SettingTactKeysFallbackURL;
	WowSettings.CDNFallbackHosts = SettingCDNFallbackHosts;
	WowSettings.DBDURL = SettingDBDURL;
	WowSettings.DBDFallbackURL = SettingDBDFallbackURL;
	WowSettings.DBDFilenameURL = SettingDBDFilenameURL;
	WowSettings.DBDFilenameFallbackURL = SettingDBDFilenameFallbackURL;
	WowSettings.CacheExpiryDays = SettingCacheExpiryDays;
	FWowCASCInterface::Get().ApplySettings(WowSettings);

	Session->Initialize(Path);
	return FReply::Handled();
}

FReply SWowCASCBrowser::OnLoadBuildClicked()
{
	if (bBuildLoading)
		return FReply::Handled();

	bBuildLoading = true;

	StatusText->SetText(FText::Format(
		LOCTEXT("LoadingBuild", "Loading build {0}... (this may take a few seconds)"),
		FText::AsNumber(SelectedBuildIndex)));

	Session->LoadBuild(SelectedBuildIndex);
	return FReply::Handled();
}

void SWowCASCBrowser::HandleCASCInitialized()
{
	const auto& Builds = Session->GetBuilds();
	StatusText->SetText(FText::Format(
		LOCTEXT("FoundBuilds", "Found {0} build(s). Select one to load."),
		FText::AsNumber(Builds.Num())));

	BuildListBox->ClearChildren();
	for (const auto& Build : Builds)
	{
		int32 BuildIdx = Build.Index;
		BuildListBox->AddSlot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SButton)
			.Text(FText::FromString(Build.DisplayName))
			.OnClicked_Lambda([this, BuildIdx]() -> FReply
			{
				SelectedBuildIndex = BuildIdx;
				return OnLoadBuildClicked();
			})
		];
	}
}

void SWowCASCBrowser::HandleBuildLoaded()
{
	bBuildLoaded = true;
	bBuildLoading = false;
	BuildListBox->ClearChildren();

	StatusText->SetText(FText::Format(
		LOCTEXT("BuildLoaded", "Build loaded! {0} files available."),
		FText::AsNumber(Session->GetFileCount())));

	// Initialize first tab
	SwitchTab(ActiveTab);
}

void SWowCASCBrowser::HandleCASCError(const FString& Error)
{
	bBuildLoading = false;
	StatusText->SetText(FText::Format(
		LOCTEXT("Error", "Error: {0}"),
		FText::FromString(Error)));
}

// ── Search + refresh ──

void SWowCASCBrowser::OnSearchTextChanged(const FText& NewText)
{
	CurrentSearch = NewText.ToString();
	RefreshFileList();
}

void SWowCASCBrowser::OnSearchTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	CurrentSearch = NewText.ToString();
	RefreshFileList();
}

void SWowCASCBrowser::RefreshFileList()
{
	TArray<FWowFileEntry> RawFiles;
	FWowCASCInterface::Get().GetFileList(CurrentSearch, CurrentExtFilter, RawFiles, 10000);

	FileEntries.Reset();
	FileEntries.Reserve(RawFiles.Num());
	for (auto& Entry : RawFiles)
	{
		FileEntries.Add(MakeShared<FWowFileEntry>(MoveTemp(Entry)));
	}

	SortFileList();

	// Update status text
	FString CountStr = FString::Printf(TEXT("%d files"), FileEntries.Num());
	if (FileEntries.Num() >= 10000)
	{
		CountStr += TEXT(" (capped — refine your search)");
	}
	if (ListStatusText.IsValid())
	{
		ListStatusText->SetText(FText::FromString(CountStr));
	}
}

void SWowCASCBrowser::SortFileList()
{
	if (CurrentSortMode == ESortMode::ByName)
	{
		FileEntries.Sort([](const TSharedPtr<FWowFileEntry>& A, const TSharedPtr<FWowFileEntry>& B)
		{
			return A->FileName.Compare(B->FileName, ESearchCase::IgnoreCase) < 0;
		});
	}
	else
	{
		FileEntries.Sort([](const TSharedPtr<FWowFileEntry>& A, const TSharedPtr<FWowFileEntry>& B)
		{
			return A->FileDataID < B->FileDataID;
		});
	}

	if (FileListView.IsValid())
	{
		FileListView->RequestListRefresh();
	}
}

// ── Settings ──

void SWowCASCBrowser::LoadSettings()
{
	GConfig->GetString(ConfigSection, TEXT("CDNRegion"), SettingCDNRegion, WowToUEIni);
	GConfig->GetString(ConfigSection, TEXT("Locale"), SettingLocale, WowToUEIni);
	GConfig->GetString(ConfigSection, TEXT("ListfileURL"), SettingListfileURL, WowToUEIni);
	GConfig->GetString(ConfigSection, TEXT("ListfileFallbackURL"), SettingListfileFallbackURL, WowToUEIni);
	GConfig->GetInt(ConfigSection, TEXT("ListfileRefreshDays"), SettingListfileRefreshDays, WowToUEIni);
	GConfig->GetString(ConfigSection, TEXT("TactKeysURL"), SettingTactKeysURL, WowToUEIni);
	GConfig->GetString(ConfigSection, TEXT("TactKeysFallbackURL"), SettingTactKeysFallbackURL, WowToUEIni);
	GConfig->GetString(ConfigSection, TEXT("CDNFallbackHosts"), SettingCDNFallbackHosts, WowToUEIni);
	GConfig->GetString(ConfigSection, TEXT("DBDURL"), SettingDBDURL, WowToUEIni);
	GConfig->GetString(ConfigSection, TEXT("DBDFallbackURL"), SettingDBDFallbackURL, WowToUEIni);
	GConfig->GetString(ConfigSection, TEXT("DBDFilenameURL"), SettingDBDFilenameURL, WowToUEIni);
	GConfig->GetString(ConfigSection, TEXT("DBDFilenameFallbackURL"), SettingDBDFilenameFallbackURL, WowToUEIni);
	GConfig->GetInt(ConfigSection, TEXT("CacheExpiryDays"), SettingCacheExpiryDays, WowToUEIni);
	GConfig->GetBool(ConfigSection, TEXT("EnableM2Skins"), SettingEnableM2Skins, WowToUEIni);
	GConfig->GetBool(ConfigSection, TEXT("EnableUnknownFiles"), SettingEnableUnknownFiles, WowToUEIni);
	GConfig->GetBool(ConfigSection, TEXT("OverwriteFiles"), SettingOverwriteFiles, WowToUEIni);
	GConfig->GetBool(ConfigSection, TEXT("ExportSharedTextures"), SettingExportSharedTextures, WowToUEIni);
	GConfig->GetBool(ConfigSection, TEXT("ExportSharedChildren"), SettingExportSharedChildren, WowToUEIni);
	GConfig->GetBool(ConfigSection, TEXT("ExportCollision"), SettingExportCollision, WowToUEIni);
	GConfig->GetBool(ConfigSection, TEXT("ExportUV2"), SettingExportUV2, WowToUEIni);
	GConfig->GetBool(ConfigSection, TEXT("ExportNamedFiles"), SettingExportNamedFiles, WowToUEIni);
	GConfig->GetBool(ConfigSection, TEXT("ShowUnknownItems"), SettingShowUnknownItems, WowToUEIni);
}

void SWowCASCBrowser::SaveSettings()
{
	GConfig->SetString(ConfigSection, TEXT("CDNRegion"), *SettingCDNRegion, WowToUEIni);
	GConfig->SetString(ConfigSection, TEXT("Locale"), *SettingLocale, WowToUEIni);
	GConfig->SetString(ConfigSection, TEXT("ListfileURL"), *SettingListfileURL, WowToUEIni);
	GConfig->SetString(ConfigSection, TEXT("ListfileFallbackURL"), *SettingListfileFallbackURL, WowToUEIni);
	GConfig->SetInt(ConfigSection, TEXT("ListfileRefreshDays"), SettingListfileRefreshDays, WowToUEIni);
	GConfig->SetString(ConfigSection, TEXT("TactKeysURL"), *SettingTactKeysURL, WowToUEIni);
	GConfig->SetString(ConfigSection, TEXT("TactKeysFallbackURL"), *SettingTactKeysFallbackURL, WowToUEIni);
	GConfig->SetString(ConfigSection, TEXT("CDNFallbackHosts"), *SettingCDNFallbackHosts, WowToUEIni);
	GConfig->SetString(ConfigSection, TEXT("DBDURL"), *SettingDBDURL, WowToUEIni);
	GConfig->SetString(ConfigSection, TEXT("DBDFallbackURL"), *SettingDBDFallbackURL, WowToUEIni);
	GConfig->SetString(ConfigSection, TEXT("DBDFilenameURL"), *SettingDBDFilenameURL, WowToUEIni);
	GConfig->SetString(ConfigSection, TEXT("DBDFilenameFallbackURL"), *SettingDBDFilenameFallbackURL, WowToUEIni);
	GConfig->SetInt(ConfigSection, TEXT("CacheExpiryDays"), SettingCacheExpiryDays, WowToUEIni);
	GConfig->SetBool(ConfigSection, TEXT("EnableM2Skins"), SettingEnableM2Skins, WowToUEIni);
	GConfig->SetBool(ConfigSection, TEXT("EnableUnknownFiles"), SettingEnableUnknownFiles, WowToUEIni);
	GConfig->SetBool(ConfigSection, TEXT("OverwriteFiles"), SettingOverwriteFiles, WowToUEIni);
	GConfig->SetBool(ConfigSection, TEXT("ExportSharedTextures"), SettingExportSharedTextures, WowToUEIni);
	GConfig->SetBool(ConfigSection, TEXT("ExportSharedChildren"), SettingExportSharedChildren, WowToUEIni);
	GConfig->SetBool(ConfigSection, TEXT("ExportCollision"), SettingExportCollision, WowToUEIni);
	GConfig->SetBool(ConfigSection, TEXT("ExportUV2"), SettingExportUV2, WowToUEIni);
	GConfig->SetBool(ConfigSection, TEXT("ExportNamedFiles"), SettingExportNamedFiles, WowToUEIni);
	GConfig->SetBool(ConfigSection, TEXT("ShowUnknownItems"), SettingShowUnknownItems, WowToUEIni);
	GConfig->Flush(false, WowToUEIni);
}

void SWowCASCBrowser::BuildSettingsPanel()
{
	SettingsPanel->ClearChildren();

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);

	auto Heading = [&Content](const FText& Text)
	{
		Content->AddSlot().AutoHeight().Padding(0, 14, 0, 2)
		[
			SNew(STextBlock).Text(Text).Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		];
	};

	auto Desc = [&Content](const FText& Text)
	{
		Content->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
		[
			SNew(STextBlock).Text(Text).AutoWrapText(true)
		];
	};

	auto Check = [&Content, this](bool* Value, const FText& Label)
	{
		Content->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([Value]() { return *Value ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([Value, this](ECheckBoxState S) { *Value = (S == ECheckBoxState::Checked); SaveSettings(); })
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(4, 0, 0, 0)
			[
				SNew(STextBlock).Text(Label)
			]
		];
	};

	auto TextInput = [&Content, this](FString* Value)
	{
		Content->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
		[
			SNew(SEditableTextBox)
			.Text_Lambda([Value]() { return FText::FromString(*Value); })
			.OnTextCommitted_Lambda([Value, this](const FText& T, ETextCommit::Type) { *Value = T.ToString(); SaveSettings(); })
		];
	};

	auto IntInput = [&Content, this](int32* Value, int32 Min = 0, int32 Max = 9999)
	{
		Content->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
		[
			SNew(SBox).WidthOverride(80)
			[
				SNew(SEditableTextBox)
				.Text_Lambda([Value]() { return FText::AsNumber(*Value); })
				.OnTextCommitted_Lambda([Value, Min, Max, this](const FText& T, ETextCommit::Type)
				{
					*Value = FMath::Clamp(FCString::Atoi(*T.ToString()), Min, Max);
					SaveSettings();
				})
			]
		];
	};

	// ── CDN Region ──
	Heading(LOCTEXT("H_CDN", "CDN Region"));
	Desc(LOCTEXT("D_CDN", "Region for downloading missing files from Blizzard's CDN."));
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		const TCHAR* Regions[] = { TEXT("eu"), TEXT("us"), TEXT("kr"), TEXT("tw"), TEXT("cn") };
		const TCHAR* Labels[] = { TEXT("EU"), TEXT("US"), TEXT("KR"), TEXT("TW"), TEXT("CN") };
		for (int32 i = 0; i < 5; ++i)
		{
			FString R = Regions[i];
			Row->AddSlot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SButton).Text(FText::FromString(Labels[i]))
				.ButtonColorAndOpacity_Lambda([this, R]() { return SettingCDNRegion == R ? FSlateColor(FLinearColor(0.15f, 0.35f, 0.65f)) : FSlateColor::UseForeground(); })
				.OnClicked_Lambda([this, R]() { SettingCDNRegion = R; SaveSettings(); return FReply::Handled(); })
			];
		}
		Content->AddSlot().AutoHeight().Padding(0, 0, 0, 4) [ Row ];
	}

	// ── CASC Locale ──
	Heading(LOCTEXT("H_Locale", "CASC Locale"));
	Desc(LOCTEXT("D_Locale", "Which locale to use for file reading. Should match your WoW client locale."));
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		const TCHAR* Locs[] = { TEXT("enUS"), TEXT("enGB"), TEXT("deDE"), TEXT("frFR"), TEXT("esES"), TEXT("ruRU"), TEXT("koKR"), TEXT("zhCN"), TEXT("zhTW"), TEXT("ptBR"), TEXT("itIT") };
		for (int32 i = 0; i < 11; ++i)
		{
			FString L = Locs[i];
			Row->AddSlot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SButton).Text(FText::FromString(L))
				.ButtonColorAndOpacity_Lambda([this, L]() { return SettingLocale == L ? FSlateColor(FLinearColor(0.15f, 0.35f, 0.65f)) : FSlateColor::UseForeground(); })
				.OnClicked_Lambda([this, L]() { SettingLocale = L; SaveSettings(); return FReply::Handled(); })
			];
		}
		Content->AddSlot().AutoHeight().Padding(0, 0, 0, 4) [ Row ];
	}

	// ── Import Options ──
	Heading(LOCTEXT("H_Import", "Import Options"));
	Check(&SettingEnableM2Skins, LOCTEXT("C_Skins", "Load Model Skins (parse creature/item skins from DB2)"));
	Check(&SettingEnableUnknownFiles, LOCTEXT("C_Unknown", "Find Unknown Files (scan DB2 for unlisted assets, uses more memory)"));
	Check(&SettingOverwriteFiles, LOCTEXT("C_Overwrite", "Always Overwrite Existing Files"));
	Check(&SettingExportSharedTextures, LOCTEXT("C_SharedTex", "Enable Shared Textures (avoid duplicating textures across models)"));
	Check(&SettingExportSharedChildren, LOCTEXT("C_SharedChild", "Enable Shared Children (avoid duplicating child meshes in WMO/ADT)"));
	Check(&SettingExportCollision, LOCTEXT("C_Collision", "Export Model Collision (import M2 collision as UE physics)"));
	Check(&SettingExportUV2, LOCTEXT("C_UV2", "Export Additional UV Layers (UV2/UV3 for lightmaps)"));
	Check(&SettingExportNamedFiles, LOCTEXT("C_Named", "Name Files by Listfile (use names instead of FileDataID numbers)"));
	Check(&SettingShowUnknownItems, LOCTEXT("C_UnknownItems", "Show Unknown Items (list unnamed items in Items tab)"));

	// ── Listfile Source ──
	Heading(LOCTEXT("H_Listfile", "Listfile Source"));
	Desc(LOCTEXT("D_Listfile", "Remote URL or local path for the CASC listfile."));
	Desc(LOCTEXT("D_LF_Primary", "Primary:"));
	TextInput(&SettingListfileURL);
	Desc(LOCTEXT("D_LF_Fallback", "Fallback:"));
	TextInput(&SettingListfileFallbackURL);

	Heading(LOCTEXT("H_LFRefresh", "Listfile Update Frequency (days)"));
	Desc(LOCTEXT("D_LFRefresh", "How often the listfile is re-downloaded. 0 = always re-download."));
	IntInput(&SettingListfileRefreshDays, 0, 999);

	// ── Encryption Keys ──
	Heading(LOCTEXT("H_Tact", "Encryption Keys"));
	Desc(LOCTEXT("D_Tact", "Remote URL used to update keys for encrypted files."));
	Desc(LOCTEXT("D_Tact_Primary", "Primary:"));
	TextInput(&SettingTactKeysURL);
	Desc(LOCTEXT("D_Tact_Fallback", "Fallback:"));
	TextInput(&SettingTactKeysFallbackURL);

	// ── CDN Fallback Hosts ──
	Heading(LOCTEXT("H_CDNFallback", "CDN Fallback Hosts"));
	Desc(LOCTEXT("D_CDNFallback", "Comma-separated list of additional CDN hostnames to try when official servers are slow."));
	TextInput(&SettingCDNFallbackHosts);

	// ── DBD Repository ──
	Heading(LOCTEXT("H_DBD", "Data Table Definitions (DBD)"));
	Desc(LOCTEXT("D_DBD", "Remote URL for DBD definitions used to parse DB2 database files."));
	Desc(LOCTEXT("D_DBD_Primary", "Primary:"));
	TextInput(&SettingDBDURL);
	Desc(LOCTEXT("D_DBD_Fallback", "Fallback:"));
	TextInput(&SettingDBDFallbackURL);

	Heading(LOCTEXT("H_DBDManifest", "DBD Manifest"));
	Desc(LOCTEXT("D_DBDManifest", "Remote URL for the DBD manifest file."));
	Desc(LOCTEXT("D_DBDM_Primary", "Primary:"));
	TextInput(&SettingDBDFilenameURL);
	Desc(LOCTEXT("D_DBDM_Fallback", "Fallback:"));
	TextInput(&SettingDBDFilenameFallbackURL);

	// ── Cache ──
	Heading(LOCTEXT("H_Cache", "Cache Expiry (days)"));
	Desc(LOCTEXT("D_Cache", "After how many days of inactivity cached data is deleted. 0 disables cleanup."));
	IntInput(&SettingCacheExpiryDays, 0, 9999);

	SettingsPanel->AddSlot().FillHeight(1.0f).Padding(8)
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			Content
		]
	];
}

#undef LOCTEXT_NAMESPACE
