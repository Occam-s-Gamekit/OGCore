﻿/// Copyright Occam's Gamekit contributors 2025

#include "PolymorphicDataBankTest.h"

#include "Engine/StaticMeshActor.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPolymorphicDataBankTest, "OccamsGamekit.OGCore.OGPolymorphicDataBank.BasicUsage",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPolymorphicDataBankTest::RunTest(const FString& Parameters)
{
	//Test 1: Basic storage, retrieval, deletion
	{
		FOGTestDataBank_Delta DataBank;
		TestFalse(TEXT("DataBank does not contain struct initially"),DataBank.Contains<FOGTestPolymorphicData_Int>());

		DataBank.AddUnique<FOGTestPolymorphicData_Int>().TestInt = 123;

		TestTrue(TEXT("DataBank contains TestStructOne"), DataBank.Contains<FOGTestPolymorphicData_Int>());
		const FOGTestPolymorphicData_Int& OutStructOne = DataBank.GetConstChecked<FOGTestPolymorphicData_Int>();
		TestEqual(TEXT("value in TestStructOne is what we set it to"), OutStructOne.TestInt, 123);

		DataBank.Remove<FOGTestPolymorphicData_Int>();

		TestFalse(TEXT("DataBank does not contain TestStructOne"), DataBank.Contains<FOGTestPolymorphicData_Int>());
	}

	//Test 2: Set to copy existing structs into the data bank and override existing structs
	{
		FOGTestDataBank_Delta DataBank;
		FOGTestPolymorphicData_String TestStructTwo;
		TestStructTwo.TestString = TEXT("Initial Value");
		DataBank.SetByCopy(TestStructTwo);
		TestTrue(TEXT("DataBank should contain copied value"), DataBank.GetConstChecked<FOGTestPolymorphicData_String>().TestString.Equals(TEXT("Initial Value")));
		FOGTestPolymorphicData_String OverrideStruct;
		OverrideStruct.TestString = TEXT("Overridden Value");
		DataBank.SetByCopy(OverrideStruct);
		TestTrue(TEXT("DataBank should contain overridden value"), DataBank.GetConstChecked<FOGTestPolymorphicData_String>().TestString.Equals(TEXT("Overridden Value")));
	}
	
	// Make the test pass by returning true, or fail by returning false.
	return true;
}