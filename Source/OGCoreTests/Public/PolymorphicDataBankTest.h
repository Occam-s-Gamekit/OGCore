﻿/// Copyright Occam's Gamekit contributors 2025

#pragma once

#include "OGPolymorphicDataBank.h"
#include "PolymorphicDataBankTest.generated.h"

USTRUCT()
struct FOGTestPolymorphicData_Base : public FOGPolymorphicStructBase
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct FOGTestPolymorphicData_Int : public FOGTestPolymorphicData_Base
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	int TestInt = 0;
};

USTRUCT(BlueprintType)
struct FOGTestPolymorphicData_String : public FOGTestPolymorphicData_Base
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FString TestString = TEXT("");
};

USTRUCT(BlueprintType)
struct FOGTestPolymorphicData_Actor : public FOGTestPolymorphicData_Base
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<AActor> TestActor = nullptr;
};

USTRUCT(BlueprintType)
struct FOGTestPolymorphicData_Object : public FOGTestPolymorphicData_Base
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UObject> TestObject = nullptr;
};

USTRUCT(BlueprintType)
struct FOGTestDataBank : public FOGPolymorphicDataBankBase
{
	GENERATED_BODY()

	virtual UScriptStruct* GetInnerStruct() const override {return FOGTestPolymorphicData_Base::StaticStruct();}
};

template<>
struct TStructOpsTypeTraits<FOGTestDataBank> : public TStructOpsTypeTraitsBase2<FOGTestDataBank>
{
	enum
	{
		WithAddStructReferencedObjects = true,
		WithNetSerializer = true,
	};
};

USTRUCT(BlueprintType)
struct FOGTestDataBank_Delta : public FOGPolymorphicDataBankBase
{
	GENERATED_BODY()

	virtual UScriptStruct* GetInnerStruct() const override {return FOGTestPolymorphicData_Base::StaticStruct();}
};

template<>
struct TStructOpsTypeTraits<FOGTestDataBank_Delta> : public TStructOpsTypeTraitsBase2<FOGTestDataBank_Delta>
{
	enum
	{
		WithAddStructReferencedObjects = true,
		WithNetDeltaSerializer = true,
	};
};

