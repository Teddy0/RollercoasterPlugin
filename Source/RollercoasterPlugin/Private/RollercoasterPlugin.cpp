// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "RollercoasterPluginPrivatePCH.h"

#include "RollercoasterPlugin.generated.inl"


class FRollercoasterPlugin : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() OVERRIDE;
	virtual void ShutdownModule() OVERRIDE;
};

IMPLEMENT_MODULE(FRollercoasterPlugin, RollercoasterPlugin)



void FRollercoasterPlugin::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
}


void FRollercoasterPlugin::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}



