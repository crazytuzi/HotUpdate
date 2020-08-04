// Copyright Epic Games, Inc. All Rights Reserved.

#include "HotUpdate.h"
#include "Settings/Public/ISettingsModule.h"
#include "HotUpdateSettings.h"

#define LOCTEXT_NAMESPACE "FHotUpdateModule"

void FHotUpdateModule::StartupModule()
{
    // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
    RegisterSettings();
}

void FHotUpdateModule::ShutdownModule()
{
    // This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
    // we call this function before unloading the module.
    UnregisterSettings();
}

void FHotUpdateModule::RegisterSettings() const
{
    if (auto SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        SettingsModule->RegisterSettings("Project", "Plugins", "HotUpdate",
                                         LOCTEXT("HotUpdateSettingsName", "HotUpdate"),
                                         LOCTEXT("HotUpdateSettingsDescription", "Configure the HotUpdate plugin"),
                                         GetMutableDefault<UHotUpdateSettings>()
        );
    }
}

void FHotUpdateModule::UnregisterSettings() const
{
    if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        SettingsModule->UnregisterSettings("Project", "Plugins", "HotUpdate");
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FHotUpdateModule, HotUpdate)
