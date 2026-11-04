#include "NightSkyAssetDescribeLibrary.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(NightSkyAssetDescribeLibrary)

namespace NightSkyDescribe
{
	static FString ToString(const TSharedRef<FJsonObject>& Json)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Json, Writer);
		return Out;
	}

	static bool IsOwnedBy(const UObject* Object, const UObject* Root)
	{
		for (const UObject* Outer = Object ? Object->GetOuter() : nullptr; Outer; Outer = Outer->GetOuter())
		{
			if (Outer == Root) return true;
		}
		return false;
	}

	struct FDescriber
	{
		const UObject* Root = nullptr;
		int32 MaxDepth = 12;
		TSet<const UObject*> Seen;

		TSharedPtr<FJsonValue> DescribeValue(const FProperty* Prop, const void* ValuePtr, const UObject* Owner, int32 Depth)
		{
			if (const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop))
			{
				const UObject* Value = ObjProp->GetObjectPropertyValue(ValuePtr);
				if (Value && (IsOwnedBy(Value, Root) || Value->GetOuter() == Owner) && Depth < MaxDepth)
				{
					return MakeShared<FJsonValueObject>(Describe(Value, Depth + 1));
				}
				return MakeShared<FJsonValueString>(Value ? Value->GetPathName() : TEXT("None"));
			}
			if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
			{
				TArray<TSharedPtr<FJsonValue>> Items;
				FScriptArrayHelper Helper(ArrayProp, ValuePtr);
				for (int32 i = 0; i < Helper.Num(); i++)
				{
					Items.Add(DescribeValue(ArrayProp->Inner, Helper.GetRawPtr(i), Owner, Depth));
				}
				return MakeShared<FJsonValueArray>(Items);
			}
			if (const FStructProperty* StructProp = CastField<FStructProperty>(Prop))
			{
				const TSharedRef<FJsonObject> Fields = MakeShared<FJsonObject>();
				for (TFieldIterator<FProperty> It(StructProp->Struct); It; ++It)
				{
					Fields->SetField(It->GetName(), DescribeValue(*It, It->ContainerPtrToValuePtr<void>(ValuePtr), Owner, Depth));
				}
				return MakeShared<FJsonValueObject>(Fields);
			}
			if (const FTextProperty* TextProp = CastField<FTextProperty>(Prop))
			{
				return MakeShared<FJsonValueString>(TextProp->GetPropertyValue(ValuePtr).ToString());
			}
			FString Text;
			Prop->ExportTextItem_Direct(Text, ValuePtr, nullptr, const_cast<UObject*>(Owner), PPF_None);
			return MakeShared<FJsonValueString>(Text);
		}

		TSharedRef<FJsonObject> Describe(const UObject* Object, int32 Depth)
		{
			const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
			if (!Object) return Json;
			if (Seen.Contains(Object))
			{
				Json->SetStringField(TEXT("_ref"), Object->GetPathName());
				return Json;
			}
			Seen.Add(Object);
			Json->SetStringField(TEXT("_class"), Object->GetClass()->GetPathName());
			Json->SetStringField(TEXT("_name"), Object->GetName());
			Json->SetStringField(TEXT("_path"), Object->GetPathName());
			for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
			{
				const FProperty* Prop = *It;
				if (Prop->GetOwnerClass() == UObject::StaticClass()) continue;
				if (Prop->HasAnyPropertyFlags(CPF_Deprecated)) continue;
				Json->SetField(Prop->GetName(), DescribeValue(Prop, Prop->ContainerPtrToValuePtr<void>(Object), Object, Depth));
			}
			return Json;
		}
	};
}

FString UNightSkyAssetDescribeLibrary::DescribeObjectJson(UObject* Object, const int32 MaxDepth)
{
	NightSkyDescribe::FDescriber Describer;
	Describer.Root = Object;
	Describer.MaxDepth = MaxDepth;
	return NightSkyDescribe::ToString(Describer.Describe(Object, 0));
}

FString UNightSkyAssetDescribeLibrary::DescribeBlueprintGraphsJson(UBlueprint* Blueprint)
{
	const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!Blueprint) return NightSkyDescribe::ToString(Json);
	Json->SetStringField(TEXT("_path"), Blueprint->GetPathName());
	Json->SetStringField(TEXT("parent"), Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : TEXT("None"));

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	TArray<TSharedPtr<FJsonValue>> GraphValues;
	for (const UEdGraph* Graph : Graphs)
	{
		if (!Graph) continue;
		const TSharedRef<FJsonObject> G = MakeShared<FJsonObject>();
		G->SetStringField(TEXT("name"), Graph->GetName());
		TArray<TSharedPtr<FJsonValue>> Nodes;
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			const TSharedRef<FJsonObject> N = MakeShared<FJsonObject>();
			N->SetStringField(TEXT("id"), Node->GetName());
			N->SetStringField(TEXT("class"), Node->GetClass()->GetName());
			N->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
			if (!Node->NodeComment.IsEmpty()) N->SetStringField(TEXT("comment"), Node->NodeComment);
			TArray<TSharedPtr<FJsonValue>> Pins;
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->bHidden) continue;
				const TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
				P->SetStringField(TEXT("name"), Pin->PinName.ToString());
				P->SetStringField(TEXT("dir"), Pin->Direction == EGPD_Input ? TEXT("in") : TEXT("out"));
				P->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString() +
					(Pin->PinType.PinSubCategoryObject.IsValid() ? TEXT(":") + Pin->PinType.PinSubCategoryObject->GetName() : FString()));
				const FString Default = Pin->DefaultObject ? Pin->DefaultObject->GetPathName() :
					(!Pin->DefaultTextValue.IsEmpty() ? Pin->DefaultTextValue.ToString() : Pin->DefaultValue);
				if (!Default.IsEmpty()) P->SetStringField(TEXT("default"), Default);
				TArray<TSharedPtr<FJsonValue>> Links;
				for (const UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (Linked && Linked->GetOwningNode())
					{
						Links.Add(MakeShared<FJsonValueString>(Linked->GetOwningNode()->GetName() + TEXT(".") + Linked->PinName.ToString()));
					}
				}
				if (Links.Num()) P->SetArrayField(TEXT("links"), Links);
				Pins.Add(MakeShared<FJsonValueObject>(P));
			}
			N->SetArrayField(TEXT("pins"), Pins);
			Nodes.Add(MakeShared<FJsonValueObject>(N));
		}
		G->SetArrayField(TEXT("nodes"), Nodes);
		GraphValues.Add(MakeShared<FJsonValueObject>(G));
	}
	Json->SetArrayField(TEXT("graphs"), GraphValues);
	return NightSkyDescribe::ToString(Json);
}

FString UNightSkyAssetDescribeLibrary::DataTableToJson(UDataTable* Table)
{
	return Table ? Table->GetTableAsJSON() : FString();
}
