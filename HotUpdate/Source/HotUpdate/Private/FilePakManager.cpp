#include "FilePakManager.h"
#include "FileDownloadManager.h"
#include "IPlatformFilePak.h"
#include "FileDownLog.h"
#include "Launch/Resources/Version.h"
#include "ShaderCodeLibrary.h"

DEFINE_LOG_CATEGORY(LogHotUpdate);

FFilePakManager::FFilePakManager() : PakPlatformFile(nullptr)
{
}

void FFilePakManager::StartUp()
{
    if (PakPlatformFile == nullptr)
    {
        PakPlatformFile = new FPakPlatformFile();
    }

    PakPlatformFile->SetLowerLevel(&(FPlatformFileManager::Get().GetPlatformFile()));

    PakPlatformFile->InitializeNewAsyncIO();

    FPlatformFileManager::Get().SetPlatformFile(*PakPlatformFile);

    for (auto i = 0; i < PakFiles.Num(); ++i)
    {
        const auto& PakName = PakFiles[i].PakName;

        if (FPaths::GetExtension(PakName) != TEXT("pak"))
        {
            continue;
        }

        auto bRet = IsPakValid(PakFiles[i]);

        if (!bRet)
        {
            UE_LOG(LogHotUpdate, Warning, TEXT("Failed to verify pak before mount: %s"), *PakName);

            FailedPakList.Add(PakFiles[i]);

            PakFiles.RemoveAt(i--);

            continue;
        }

        const auto& PakPath = FPaths::Combine(FFileDownloadManager::GetPakSaveRoot(), PakName);

        FPakFile PakFile(PakPlatformFile, *PakPath, false);

        const auto& MountPoint = PakFile.GetMountPoint();

        UE_LOG(LogHotUpdate, Display, TEXT("pak {%s} MountPoint at {%s}"), *PakPath, *MountPoint);

        bRet = PakPlatformFile->Mount(*PakPath, 0);

        if (!bRet)
        {
            FailedPakList.Add(PakFiles[i]);

            UE_LOG(LogHotUpdate, Error, TEXT("Failed to mount pak: %s"), *PakPath);
        }
        else
        {
            UE_LOG(LogHotUpdate, Display, TEXT("Success to mount pak: %s"), *PakPath);

            UpdateMountProgress(i);
        }
    }

#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 25
#if PLATFORM_IOS || PLATFORM_MAC
    FShaderCodeLibrary::OpenLibrary("Global", FPaths::Combine(FPaths::ProjectContentDir(), "Metal"));

    FShaderCodeLibrary::OpenLibrary(FApp::GetProjectName(), FPaths::Combine(FPaths::ProjectContentDir(), "Metal"));
#else
    FShaderCodeLibrary::OpenLibrary("Global", FPaths::ProjectContentDir());

    FShaderCodeLibrary::OpenLibrary(FApp::GetProjectName(), FPaths::ProjectContentDir());
#endif
#endif
}

void FFilePakManager::ShutDown()
{
    PakFiles.Empty();

    FailedPakList.Empty();

    OnMountUpdated.Unbind();

    if (PakPlatformFile != nullptr)
    {
        if (PakPlatformFile->GetLowerLevel() != nullptr)
        {
            FPlatformFileManager::Get().RemovePlatformFile(PakPlatformFile);
        }

        delete PakPlatformFile;

        PakPlatformFile = nullptr;
    }
}

bool FFilePakManager::IsSuccessful() const
{
    return FailedPakList.Num() <= 0;
}

void FFilePakManager::UpdateMountProgress(const int CurrentIndex)
{
    if (!PakFiles.IsValidIndex(CurrentIndex))
    {
        return;
    }

    const auto& PakName = PakFiles[CurrentIndex].PakName;

    const auto& Progress = static_cast<float>(CurrentIndex + 1) / static_cast<float>(PakFiles.Num());

    OnMountUpdated.Execute(PakName, Progress);
}

bool FFilePakManager::IsPakValid(const FPakFileProperty& PakInfo)
{
    const auto PakPath = FPaths::Combine(FFileDownloadManager::GetPakSaveRoot(), PakInfo.PakName);

    if (!IFileManager::Get().FileExists(*PakPath))
    {
        return false;
    }

    if (IFileManager::Get().FileSize(*PakPath) != PakInfo.PakSize)
    {
        return false;
    }

    const auto& Hash = FMD5Hash::HashFile(*PakPath);

    if (!Hash.IsValid())
    {
        return false;
    }

    return (BytesToHex(Hash.GetBytes(), Hash.GetSize()) == PakInfo.MD5);
}

void FFilePakManager::AddPakFile(FPakFileProperty&& PakFileProperty)
{
    PakFiles.Add(MoveTemp(PakFileProperty));
}
