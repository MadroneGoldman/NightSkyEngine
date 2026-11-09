// Reusable input condition lists for states (Input Library).

#include "InputConditionListAsset.h"

#include "UObject/ObjectSaveContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InputConditionListAsset)

namespace
{
	FString DirectionDigit(const int32 Flag)
	{
		const bool bUp = Flag & INP_Up, bDown = Flag & INP_Down, bLeft = Flag & INP_Left, bRight = Flag & INP_Right;
		if (bDown && bLeft) return TEXT("1");
		if (bDown && bRight) return TEXT("3");
		if (bUp && bLeft) return TEXT("7");
		if (bUp && bRight) return TEXT("9");
		if (bDown) return TEXT("2");
		if (bUp) return TEXT("8");
		if (bLeft) return TEXT("4");
		if (bRight) return TEXT("6");
		if (Flag & INP_Neutral) return TEXT("5");
		return FString();
	}

	FString Buttons(const int32 Flag)
	{
		static const TCHAR* Letters[] = {TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D"), TEXT("E"), TEXT("F"), TEXT("G"), TEXT("H")};
		FString Out;
		for (int32 i = 0; i < 8; i++)
		{
			if (Flag & (INP_A << i)) Out += Letters[i];
		}
		return Out;
	}

	FString DescribeCondition(const FInputCondition& Condition)
	{
		FString Out;
		for (const FInputBitmask& Bit : Condition.Sequence)
		{
			FString Token = DirectionDigit(Bit.InputFlag) + Buttons(Bit.InputFlag);
			if (Token.IsEmpty()) Token = TEXT("*"); // no input set: matches anything
			if (Bit.Hold > 0) Token = FString::Printf(TEXT("[%s]"), *Token);
			Out += Token;
		}
		switch (Condition.Method)
		{
		case EInputMethod::Strict: Out += TEXT("!"); break;
		case EInputMethod::PressAndRelease: Out = FString::Printf(TEXT("%s~"), *Out); break;
		case EInputMethod::NegativeEdge: Out = FString::Printf(TEXT("~%s"), *Out); break;
		case EInputMethod::IsUp:
		case EInputMethod::IsUpStrict: Out = FString::Printf(TEXT("up(%s)"), *Out); break;
		default: break;
		}
		return Out;
	}
}

FString UInputConditionListAsset::DescribeInputConditionList(const FInputConditionList& List)
{
	TArray<FString> Parts;
	bool bAnyDirection = false;
	for (const FInputCondition& Condition : List.InputConditions)
	{
		Parts.Add(DescribeCondition(Condition));
		for (const FInputBitmask& Bit : Condition.Sequence)
		{
			bAnyDirection |= (Bit.InputFlag & (INP_Directions | INP_Neutral)) != 0;
		}
	}
	if (!Parts.Num()) return FString(TEXT("(none)"));
	FString Out = FString::Join(Parts, TEXT(" + "));
	// No direction needed anywhere: written like a standing normal (A -> 5A), the same as the library's asset names.
	if (!bAnyDirection && Out[0] >= TEXT('A') && Out[0] <= TEXT('H'))
	{
		Out = TEXT("5") + Out;
	}
	return Out;
}

bool UInputConditionListAsset::InputConditionListsMatch(const FInputConditionList& A, const FInputConditionList& B)
{
	return FInputConditionList::StaticStruct()->CompareScriptStruct(&A, &B, PPF_None);
}

void UInputConditionListAsset::PreSave(FObjectPreSaveContext SaveContext)
{
	Notation = DescribeInputConditionList(InputConditionList);
	Super::PreSave(SaveContext);
}

#if WITH_EDITOR
void UInputConditionListAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	Notation = DescribeInputConditionList(InputConditionList);
}
#endif
