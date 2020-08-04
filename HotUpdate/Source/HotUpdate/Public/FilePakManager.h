#pragma once
#include "CoreMinimal.h"
#include "TaskInfo.h"
#include "FileDownType.h"
#include "IPlatformFilePak.h"

DECLARE_DELEGATE_TwoParams(FOnMountUpdated, const FString&, float);

class HOTUPDATE_API FFilePakManager
{
public:
    FFilePakManager();

    void StartUp();

    void ShutDown();

    bool IsSuccessful() const;

    static bool IsPakValid(const FPakFileProperty& PakInfo);

    void AddPakFile(FPakFileProperty&& PakFileProperty);

protected:
    TArray<FPakFileProperty> PakFiles;

    TArray<FPakFileProperty> FailedPakList;

    FPakPlatformFile* PakPlatformFile;

    void UpdateMountProgress(const int CurrentIndex);

public:
    FOnMountUpdated OnMountUpdated;
};
