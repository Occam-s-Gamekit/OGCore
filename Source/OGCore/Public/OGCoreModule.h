// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

struct FOGPolymorphicStructCache;

class FOGCoreModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FOGPolymorphicStructCache* GetUniversalStructCache();

protected:

	static void OnAllModulesLoaded();
	
	FDelegateHandle ModulesLoadedHandle;
};
