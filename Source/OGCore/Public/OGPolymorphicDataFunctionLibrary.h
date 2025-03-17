/// Copyright Occam's Gamekit contributors 2025

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OGPolymorphicDataBank.h"
#include "OGPolymorphicDataFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class OGCORE_API UOGPolymorphicDataFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, CustomThunk, Category="DataBank", meta=(CustomStructureParam="NewStruct", DefaultToSelf="DataBankOwner", AdvancedDisplay="DataBankOwner"))
	static void AddUnique(UPARAM(ref) FOGPolymorphicDataBankBase& DataBank, const int& NewStruct) {check(0);}
	DECLARE_FUNCTION(execAddUnique);
	static void AddUniqueGeneric(FOGPolymorphicDataBankBase& DataBank, const UScriptStruct* StructType, const FStructProperty* Prop, const void* InData);

	UFUNCTION(BlueprintCallable, CustomThunk, Category="DataBank", meta=(CustomStructureParam="InStruct"))
	static void Set(UPARAM(ref) FOGPolymorphicDataBankBase& DataBank, const int& InStruct) {check(0);}
	DECLARE_FUNCTION(execSet);
	static void SetGeneric(FOGPolymorphicDataBankBase& DataBank, const UScriptStruct* StructType, const FStructProperty* Prop, const void* InData);
	
	UFUNCTION(BlueprintCallable, CustomThunk, Category="DataBank", meta=(CustomStructureParam="OutStruct"))
	static void Get(UPARAM(ref) FOGPolymorphicDataBankBase& DataBank, int& OutStruct) {check(0);}
	DECLARE_FUNCTION(execGet);
	static void GetGeneric(const FOGPolymorphicDataBankBase& DataBank, const UScriptStruct* StructType, const FStructProperty* Prop, void* OutData);

	UFUNCTION(BlueprintCallable, CustomThunk, Category="DataBank", meta=(CustomStructureParam="OutStruct"))
	static void Find(UPARAM(ref) FOGPolymorphicDataBankBase& DataBank, bool& bOutFound, int& OutStruct) {check(0);}
	DECLARE_FUNCTION(execFind);
	static void FindGeneric(const FOGPolymorphicDataBankBase& DataBank, bool& OutFound, const UScriptStruct* StructType, const FStructProperty* Prop, void* OutData);

	UFUNCTION(BlueprintCallable, Category="DataBank")
	static void Empty(UPARAM(ref) FOGPolymorphicDataBankBase& DataBank);
};
