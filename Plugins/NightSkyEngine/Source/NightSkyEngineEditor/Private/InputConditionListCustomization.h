#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

/**
 * Details layout for FInputConditionList (each entry of a state's Input Condition Lists):
 * the usual fields, plus "Load from Input Library..." and "Save to Input Library..." in the row's
 * right-click menu and behind a small library button on the row.
 */
class FInputConditionListCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow,
	                             IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder,
	                               IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	/** Where the library lives and where Save starts. */
	static const TCHAR* LibraryPath;

private:
	void LoadFromLibrary();
	void SaveToLibrary();
	TSharedRef<SWidget> MakeLibraryMenu();
	FText GetNotation() const;

	TSharedPtr<IPropertyHandle> Handle;
};
