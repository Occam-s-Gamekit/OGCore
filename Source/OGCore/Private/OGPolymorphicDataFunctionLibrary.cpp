/// Copyright Occam's Gamekit contributors 2025


#include "OGPolymorphicDataFunctionLibrary.h"
#include "Blueprint/BlueprintExceptionInfo.h"
#include "Iris/ReplicationState/ReplicationStateUtil.h"

#define LOCTEXT_NAMESPACE "FOGCoreModule"

static FTextFormat DataBankTypeError = LOCTEXT("DataBankTypeError", "DataBank of type {0} may only hold types derived from {1}");

void UOGPolymorphicDataFunctionLibrary::AddUniqueGeneric(FOGPolymorphicDataBankBase& DataBank, const UScriptStruct* StructType, const FStructProperty* Prop, const void* InData)
{
	const uint16 Key = DataBank.GetKey(StructType);
	FOGPolymorphicStructBase& DataRef = DataBank.AddUnique_Internal(Key, StructType);
	void* RawDataPtr = &DataRef;
	Prop->CopyCompleteValueFromScriptVM(RawDataPtr, InData);
	DataBank.MarkDirty(DataRef);
}

DEFINE_FUNCTION(UOGPolymorphicDataFunctionLibrary::execAddUnique)
{
	P_GET_STRUCT_REF(FOGPolymorphicDataBankBase, DataBank);
	
	Stack.MostRecentProperty = nullptr;
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);
	const FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* PropertyContainerAddress = Stack.MostRecentPropertyContainer;
	
	P_FINISH;
	//TODO: Could make this error happen at compile time if this function were implemented as a custom node
	if (!StructProperty)
	{
		const FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::FatalError,
			FText::FromString(TEXT("DataBank AddUnique requires a struct derived from Inner type of Data Bank"))
		);
		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
		return;
	}

	const UScriptStruct* ScriptStruct = StructProperty->Struct;
	if (!ScriptStruct || !ScriptStruct->IsChildOf(DataBank.GetInnerStruct()))
	{
		const FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::FatalError,
			FText::Format(DataBankTypeError, FText::FromString(DataBank.StaticStruct()->GetStructCPPName()),
				FText::FromString(DataBank.GetInnerStruct()->GetStructCPPName()))
		);
		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
		return;
	}
	
	// Get the memory address of the struct
	const void* PropertyData = StructProperty->ContainerPtrToValuePtr<FOGPolymorphicStructBase>(PropertyContainerAddress);
	
	P_NATIVE_BEGIN;
	AddUniqueGeneric(DataBank, ScriptStruct, StructProperty, PropertyData);
	P_NATIVE_END;
}

void UOGPolymorphicDataFunctionLibrary::SetGeneric(FOGPolymorphicDataBankBase& DataBank, const UScriptStruct* StructType, const FStructProperty* Prop, const void* InData)
{
	const uint16 Key = DataBank.GetKey(StructType);
	FOGPolymorphicStructBase* RawDataPtr = DataBank.Get_Internal(Key);
	if (!RawDataPtr)
	{
		RawDataPtr = &DataBank.AddUnique_Internal(Key, StructType);
	}
	Prop->CopyCompleteValueFromScriptVM(RawDataPtr, InData);
	DataBank.MarkDirty(*RawDataPtr);
}

DEFINE_FUNCTION(UOGPolymorphicDataFunctionLibrary::execSet)
{
	P_GET_STRUCT_REF(FOGPolymorphicDataBankBase, DataBank);
	
	Stack.MostRecentProperty = nullptr;
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);
	const FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* PropertyContainerAddress = Stack.MostRecentPropertyContainer;
	
	P_FINISH;
	//TODO: Could make this error happen at compile time if this function were implemented as a custom node
	if (!StructProperty)
	{
		const FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::FatalError,
			FText::FromString(TEXT("DataBank AddUnique requires a struct derived from Inner type of Data Bank"))
		);
		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
		return;
	}

	const UScriptStruct* ScriptStruct = StructProperty->Struct;
	if (!ScriptStruct || !ScriptStruct->IsChildOf(DataBank.GetInnerStruct()))
	{
		const FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::FatalError,
			FText::Format(DataBankTypeError, FText::FromString(DataBank.StaticStruct()->GetStructCPPName()),
				FText::FromString(DataBank.GetInnerStruct()->GetStructCPPName()))
		);
		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
		return;
	}
	
	// Get the memory address of the struct
	const void* PropertyData = StructProperty->ContainerPtrToValuePtr<FOGPolymorphicStructBase>(PropertyContainerAddress);
	
	P_NATIVE_BEGIN;
	SetGeneric(DataBank, ScriptStruct, StructProperty, PropertyData);
	P_NATIVE_END;
}

void UOGPolymorphicDataFunctionLibrary::GetGeneric(const FOGPolymorphicDataBankBase& DataBank, const UScriptStruct* StructType, const FStructProperty* Prop, void* OutData)
{
	const uint16 Key = DataBank.GetKey(StructType);
	const void* Existing = DataBank.GetConst_Internal(Key);
	if (!ensureAlwaysMsgf(Existing, TEXT("Tried getting a value from the data bank that is not present")))
	{
		return;
	}
	Prop->CopyCompleteValueToScriptVM(OutData, Existing);
}

DEFINE_FUNCTION(UOGPolymorphicDataFunctionLibrary::execGet)
{
	P_GET_STRUCT_REF(FOGPolymorphicDataBankBase, DataBank);
	
	Stack.MostRecentProperty = nullptr;
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);
	
	const FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* PropertyContainerAddress = Stack.MostRecentPropertyContainer;

	P_FINISH;

	//TODO: Could make this error happen at compile time if this function were implemented as a custom node
	if (!StructProperty)
	{
		const FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::FatalError,
			FText::FromString(TEXT("DataBank Get's wildcard pin must connect to a struct derived from FOGPolymorphicStructBase"))
		);
		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
		return;
	}

	const UScriptStruct* ScriptStruct = StructProperty->Struct;
	if (!ScriptStruct || !ScriptStruct->IsChildOf(FOGPolymorphicStructBase::StaticStruct()))
	{
		const FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::FatalError,
			FText::Format(DataBankTypeError, FText::FromString(DataBank.StaticStruct()->GetStructCPPName()),
				FText::FromString(DataBank.GetInnerStruct()->GetStructCPPName()))
		);
		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
		return;
	}

	// Get the memory address of the struct
	void* PropertyData = StructProperty->ContainerPtrToValuePtr<FOGPolymorphicStructBase>(PropertyContainerAddress);
	
	P_NATIVE_BEGIN;
	GetGeneric(DataBank, ScriptStruct, StructProperty, PropertyData);
	P_NATIVE_END;
}

void UOGPolymorphicDataFunctionLibrary::FindGeneric(const FOGPolymorphicDataBankBase& DataBank, bool& OutFound, const UScriptStruct* StructType, const FStructProperty* Prop,
	void* OutData)
{
	const uint16 Key = DataBank.GetKey(StructType);
	const void* Existing = DataBank.GetConst_Internal(Key);
	if (!Existing)
	{
		OutFound = false;
		return;
	}
	OutFound = true;
	Prop->CopyCompleteValueToScriptVM(OutData, Existing);
}

DEFINE_FUNCTION(UOGPolymorphicDataFunctionLibrary::execFind)
{
	P_GET_STRUCT_REF(FOGPolymorphicDataBankBase, DataBank);
	P_GET_UBOOL_REF(bOutFound);
	
	Stack.MostRecentProperty = nullptr;
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);
	
	const FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* PropertyContainerAddress = Stack.MostRecentPropertyContainer;

	P_FINISH;

	//TODO: Could make this error happen at compile time if this function were implemented as a custom node
	if (!StructProperty)
	{
		const FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::FatalError,
			FText::FromString(TEXT("DataBank Get's wildcard pin must connect to a struct derived from FOGPolymorphicStructBase"))
		);
		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
		return;
	}

	const UScriptStruct* ScriptStruct = StructProperty->Struct;
	if (!ScriptStruct || !ScriptStruct->IsChildOf(FOGPolymorphicStructBase::StaticStruct()))
	{
		const FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::FatalError,
			FText::Format(DataBankTypeError, FText::FromString(DataBank.StaticStruct()->GetStructCPPName()),
				FText::FromString(DataBank.GetInnerStruct()->GetStructCPPName()))
		);
		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
		return;
	}

	// Get the memory address of the struct
	void* PropertyData = StructProperty->ContainerPtrToValuePtr<FOGPolymorphicStructBase>(PropertyContainerAddress);
	
	P_NATIVE_BEGIN;
	FindGeneric(DataBank, bOutFound, ScriptStruct, StructProperty, PropertyData);
	P_NATIVE_END;
}

void UOGPolymorphicDataFunctionLibrary::Empty(FOGPolymorphicDataBankBase& DataBank)
{
	DataBank.Empty();
}

#undef LOCTEXT_NAMESPACE
