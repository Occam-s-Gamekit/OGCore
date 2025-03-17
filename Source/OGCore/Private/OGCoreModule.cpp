// Copyright Epic Games, Inc. All Rights Reserved.

#include "OGCoreModule.h"
#include "OGPolymorphicDataBank.h"

#define LOCTEXT_NAMESPACE "FOGUtilitiesModule"

static FOGPolymorphicStructCache UniversalStructCache;

void FOGCoreModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	// In your class constructor or initialization function:
	ModulesLoadedHandle = FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddStatic(&FOGCoreModule::OnAllModulesLoaded);
}

void FOGCoreModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FCoreDelegates::OnAllModuleLoadingPhasesComplete.Remove(ModulesLoadedHandle);
}

FOGPolymorphicStructCache* FOGCoreModule::GetUniversalStructCache()
{
	return &UniversalStructCache;
}

void FOGCoreModule::OnAllModulesLoaded()
{
	UniversalStructCache.InitTypeCache(FOGPolymorphicStructBase::StaticStruct());
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FOGCoreModule, OGCore)