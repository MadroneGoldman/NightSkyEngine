#include "InputConditionListCustomization.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "FileHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IContentBrowserSingleton.h"
#include "IDetailChildrenBuilder.h"
#include "Misc/MessageDialog.h"
#include "NightSkyEngine/Battle/Script/InputConditionListAsset.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "UObject/Package.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "InputConditionListCustomization"

const TCHAR* FInputConditionListCustomization::LibraryPath = TEXT("/NightSkyEngine/InputLibrary");

namespace InputLibrary
{
	/** The list behind the handle, when exactly one object is being edited. */
	const FInputConditionList* Read(const TSharedPtr<IPropertyHandle>& Handle)
	{
		if (!Handle.IsValid() || !Handle->IsValidHandle()) return nullptr;
		TArray<void*> Raw;
		Handle->AccessRawData(Raw);
		return Raw.Num() == 1 && Raw[0] ? static_cast<const FInputConditionList*>(Raw[0]) : nullptr;
	}

	void Apply(const TSharedPtr<IPropertyHandle>& Handle, const UInputConditionListAsset* Asset)
	{
		if (!Asset || !Handle.IsValid() || !Handle->IsValidHandle()) return;
		FString Text;
		FInputConditionList::StaticStruct()->ExportText(Text, &Asset->InputConditionList, nullptr, nullptr, PPF_None, nullptr);
		const FScopedTransaction Transaction(FText::Format(LOCTEXT("LoadTransaction", "Load Input Conditions ({0})"),
		                                                   FText::FromString(Asset->GetName())));
		// Through the handle, so undo, the Blueprint's dirty state and instance propagation all work as for a hand edit.
		Handle->SetValueFromFormattedString(Text);
	}

	void Load(const TSharedPtr<IPropertyHandle> Handle)
	{
		FContentBrowserModule& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		FAssetPickerConfig Config;
		Config.Filter.ClassPaths.Add(UInputConditionListAsset::StaticClass()->GetClassPathName());
		Config.Filter.bRecursiveClasses = true;
		Config.InitialAssetViewType = EAssetViewType::List;
		Config.SelectionMode = ESelectionMode::Single;
		Config.bAllowNullSelection = false;
		Config.bFocusSearchBoxWhenOpened = true;
		Config.bShowPathInColumnView = true;
		Config.OnAssetSelected = FOnAssetSelected::CreateLambda([Handle](const FAssetData& AssetData)
		{
			FSlateApplication::Get().DismissAllMenus();
			Apply(Handle, Cast<UInputConditionListAsset>(AssetData.GetAsset()));
		});
		Config.OnAssetEnterPressed = FOnAssetEnterPressed::CreateLambda([Handle](const TArray<FAssetData>& Selected)
		{
			FSlateApplication::Get().DismissAllMenus();
			if (Selected.Num()) Apply(Handle, Cast<UInputConditionListAsset>(Selected[0].GetAsset()));
		});

		const TSharedRef<SWidget> Picker = SNew(SBox)
			.WidthOverride(380.f)
			.HeightOverride(480.f)
			[
				ContentBrowser.Get().CreateAssetPicker(Config)
			];
		const TSharedPtr<SWindow> Parent = FSlateApplication::Get().GetActiveTopLevelWindow();
		if (Parent.IsValid())
		{
			FSlateApplication::Get().PushMenu(Parent.ToSharedRef(), FWidgetPath(), Picker,
			                                  FSlateApplication::Get().GetCursorPos(),
			                                  FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
		}
	}

	void Save(const TSharedPtr<IPropertyHandle> Handle)
	{
		const FInputConditionList* List = Read(Handle);
		if (!List)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SaveNeedsOne", "Select a single state to save its input conditions."));
			return;
		}
		const FString Notation = UInputConditionListAsset::DescribeInputConditionList(*List);
		// "236! + A" -> 236A, "[4]6A" -> Hold46A, "A" -> 5A; the library builder (witch_tools_battle.input_library) names
		// its assets the same way, plus suffixes for what sets variants apart.
		// No direction needed (the notation starts with a button): named like a standing normal, A -> 5A.
		FString DefaultName = !Notation.IsEmpty() && Notation[0] >= TEXT('A') && Notation[0] <= TEXT('H') ? TEXT("5") : TEXT("");
		for (const TCHAR C : Notation)
		{
			if (C == TEXT('[')) DefaultName += TEXT("Hold");
			else if (C == TEXT('~')) DefaultName += TEXT("Release");
			else if (FChar::IsAlnum(C)) DefaultName.AppendChar(C);
		}

		FContentBrowserModule& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		FSaveAssetDialogConfig Config;
		Config.DialogTitleOverride = LOCTEXT("SaveTitle", "Save to Input Library");
		Config.DefaultPath = FInputConditionListCustomization::LibraryPath;
		Config.DefaultAssetName = DefaultName.IsEmpty() ? FString(TEXT("NewInput")) : DefaultName;
		Config.AssetClassNames.Add(UInputConditionListAsset::StaticClass()->GetClassPathName());
		Config.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
		const FString ObjectPath = ContentBrowser.Get().CreateModalSaveAssetDialog(Config);
		if (ObjectPath.IsEmpty()) return;

		const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		UInputConditionListAsset* Asset = LoadObject<UInputConditionListAsset>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Asset)
		{
			const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
			if (Registry.GetAssetByObjectPath(FSoftObjectPath(ObjectPath)).IsValid())
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NotAnInputAsset", "Something else already has that name."));
				return;
			}
			UPackage* Package = CreatePackage(*PackageName);
			Asset = NewObject<UInputConditionListAsset>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Asset);
		}
		Asset->Modify();
		Asset->InputConditionList = *List;
		Asset->Notation = Notation;
		Asset->MarkPackageDirty();
		UEditorLoadingAndSavingUtils::SavePackages({Asset->GetPackage()}, false);
	}
}

TSharedRef<IPropertyTypeCustomization> FInputConditionListCustomization::MakeInstance()
{
	return MakeShared<FInputConditionListCustomization>();
}

FText FInputConditionListCustomization::GetNotation() const
{
	const FInputConditionList* List = InputLibrary::Read(Handle);
	return List ? FText::FromString(UInputConditionListAsset::DescribeInputConditionList(*List)) : FText::GetEmpty();
}

void FInputConditionListCustomization::LoadFromLibrary()
{
	InputLibrary::Load(Handle);
}

void FInputConditionListCustomization::SaveToLibrary()
{
	InputLibrary::Save(Handle);
}

TSharedRef<SWidget> FInputConditionListCustomization::MakeLibraryMenu()
{
	FMenuBuilder Menu(true, nullptr);
	const TSharedPtr<IPropertyHandle> H = Handle;
	Menu.AddMenuEntry(LOCTEXT("Load", "Load from Input Library..."),
	                  LOCTEXT("LoadTip", "Replace this list with one from an Input Condition List asset"),
	                  FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderOpen"),
	                  FUIAction(FExecuteAction::CreateLambda([H]() { InputLibrary::Load(H); })));
	Menu.AddMenuEntry(LOCTEXT("Save", "Save to Input Library..."),
	                  LOCTEXT("SaveTip", "Save this list as an Input Condition List asset (new or existing)"),
	                  FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save"),
	                  FUIAction(FExecuteAction::CreateLambda([H]() { InputLibrary::Save(H); })));
	return Menu.MakeWidget();
}

void FInputConditionListCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow,
                                                       IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	Handle = PropertyHandle;
	const TSharedPtr<IPropertyHandle> H = Handle;

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(this, &FInputConditionListCustomization::GetNotation)
				.ToolTipText(LOCTEXT("NotationTip", "Numpad notation of this list. Right-click the row to load or save it."))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f)
			[
				SNew(SComboButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.HasDownArrow(false)
				.ToolTipText(LOCTEXT("LibraryTip", "Input Library: load or save this list"))
				.OnGetMenuContent(this, &FInputConditionListCustomization::MakeLibraryMenu)
				.ButtonContent()
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.FolderOpen"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				PropertyHandle->CreateDefaultPropertyButtonWidgets()
			]
		];

	HeaderRow.AddCustomContextMenuAction(
		FUIAction(FExecuteAction::CreateLambda([H]() { InputLibrary::Load(H); })),
		LOCTEXT("Load", "Load from Input Library..."),
		LOCTEXT("LoadTip", "Replace this list with one from an Input Condition List asset"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderOpen"));
	HeaderRow.AddCustomContextMenuAction(
		FUIAction(FExecuteAction::CreateLambda([H]() { InputLibrary::Save(H); })),
		LOCTEXT("Save", "Save to Input Library..."),
		LOCTEXT("SaveTip", "Save this list as an Input Condition List asset (new or existing)"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save"));
}

void FInputConditionListCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder,
                                                         IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	uint32 NumChildren = 0;
	PropertyHandle->GetNumChildren(NumChildren);
	for (uint32 i = 0; i < NumChildren; i++)
	{
		ChildBuilder.AddProperty(PropertyHandle->GetChildHandle(i).ToSharedRef());
	}
}

#undef LOCTEXT_NAMESPACE
