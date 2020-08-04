#include "FileDownloadManager.h"
#include "FileDownLog.h"
#include "HotUpdateSettings.h"

void FFileDownloadManager::StartUp()
{
    StartTime = FDateTime::Now();

    ClearTempPak();

    UE_LOG(LogHotUpdate, Display, TEXT("Begin Download : %s"), *StartTime.ToString());

    if (Tasks.Num() > 0)
    {
        TotalDownloadSize = 0;

        CurrentDownloadSize = 0;

        for (const auto& Task : Tasks)
        {
            TotalDownloadSize += Task.Value->GetTaskInfo().FileSize;
        }

        OnDownloadEvent.ExecuteIfBound(EDownloadState::BEGIN_DOWNLOAD, FTaskInfo());

        for (const auto& Task : Tasks)
        {
            Task.Value->Start();
        }
    }
    else
    {
        OnDownloadEvent.ExecuteIfBound(EDownloadState::END_DOWNLOAD, FTaskInfo());
    }
}

void FFileDownloadManager::ShutDown()
{
    for (const auto& Task : Tasks)
    {
        Task.Value->Stop();
    }

    Tasks.Empty();

    FailedTasks.Empty();

    ClearTempPak();
}

FString FFileDownloadManager::GetTempPakSaveRoot()
{
    const auto HotUpdateSettings = GetMutableDefault<UHotUpdateSettings>();

#if PLATFORM_DESKTOP &&  WITH_EDITOR
    return FPaths::Combine(FPaths::ProjectSavedDir(),
                           HotUpdateSettings != nullptr ? HotUpdateSettings->TempPakSaveRoot : "");
#else
	return FPaths::Combine(FPaths::RootDir(), HotUpdateSettings != nullptr ? HotUpdateSettings->TempPakSaveRoot : "");
#endif
}

FString FFileDownloadManager::GetPakSaveRoot()
{
    const auto HotUpdateSettings = GetMutableDefault<UHotUpdateSettings>();

#if PLATFORM_DESKTOP &&  WITH_EDITOR
    return FPaths::Combine(FPaths::ProjectSavedDir(),
                           HotUpdateSettings != nullptr ? HotUpdateSettings->PakSaveRoot : "");
#else
	return FPaths::Combine(FPaths::RootDir(), HotUpdateSettings != nullptr ? HotUpdateSettings->PakSaveRoot : "");
#endif
}

bool FFileDownloadManager::IsSuccessful() const
{
    return Tasks.Num() <= 0 && FailedTasks.Num() <= 0;
}

void FFileDownloadManager::OnTaskFinish(const FTaskInfo& Info, const bool bIsSuccess)
{
    CurrentDownloadSize += Info.DownloadSize;

    TSharedPtr<FDownloadTask> OutRemovedValue;

    if (!Tasks.RemoveAndCopyValue(Info.GUID, OutRemovedValue))
    {
        UE_LOG(LogHotUpdate, Warning, TEXT("Failed to remove:%s"), *Info.FileName);
        return;
    }

    if (bIsSuccess)
    {
        const auto& TempFile = FPaths::Combine(GetTempPakSaveRoot(), Info.FileName);

        if (!IFileManager::Get().FileExists(*TempFile))
        {
            UE_LOG(LogHotUpdate, Error, TEXT("File doesn't exist after download success : %s"), *TempFile);
        }
        else
        {
            const auto& File = FPaths::Combine(GetPakSaveRoot(), Info.FileName);

            if (!IFileManager::Get().Move(*File, *TempFile, true, false, false, true))
            {
                UE_LOG(LogHotUpdate, Error, TEXT("Failed to move file from %s to %s"), *TempFile, *File);
            }
        }
    }
    else
    {
        FailedTasks.Add(OutRemovedValue);
    }

    for (const auto& Task : Tasks)
    {
        if (!Task.Value->IsDownloading())
        {
            Task.Value->Start();
        }
    }
}

void FFileDownloadManager::OnAllTaskFinish() const
{
    const auto& EndTime = FDateTime::Now();

    UE_LOG(LogHotUpdate, Display, TEXT("download finish use:%s"), *(EndTime - StartTime).ToString());

    OnDownloadEvent.ExecuteIfBound(EDownloadState::END_DOWNLOAD, FTaskInfo());
}

void FFileDownloadManager::AddTask(const FString& URL, const FString& Name, const int32 Size)
{
    auto Task = MakeShared<FDownloadTask>(URL, GetTempPakSaveRoot(), Name, Size);

    if (Tasks.Contains(Task->GetGuid()))
    {
        return;
    }

    Task->OnTaskEvent.BindRaw(this, &FFileDownloadManager::OnTaskEvent);

    Tasks.Add(Task->GetGuid(), Task);
}


void FFileDownloadManager::OnTaskEvent(const EDownloadTaskEvent InEvent, const FTaskInfo& InInfo)
{
    switch (InEvent)
    {
    case EDownloadTaskEvent::BEGIN_DOWNLOAD:
        {
            OnDownloadEvent.Execute(EDownloadState::BEGIN_FILE_DOWNLOAD, InInfo);
        }
        break;
    case EDownloadTaskEvent::UPDATE_DOWNLOAD:
        {
            const auto CurrentTime = GWorld->GetTimeSeconds();

            if (CurrentTime <= LastUpdateTime)
            {
                return;
            }

            LastUpdateTime = CurrentTime;

            OnDownloadEvent.Execute(EDownloadState::UPDATE_DOWNLOAD, InInfo);
        }
        break;
    case EDownloadTaskEvent::END_DOWNLOAD:
        {
            OnDownloadEvent.Execute(EDownloadState::END_FILE_DOWNLOAD, InInfo);

            UE_LOG(LogHotUpdate, Log, TEXT("%s download finish"), *InInfo.FileName);

            OnTaskFinish(InInfo, true);

            if (IsSuccessful())
            {
                UE_LOG(LogHotUpdate, Log, TEXT("All tasks download finish"));

                OnAllTaskFinish();
            }
        }
        break;
    case EDownloadTaskEvent::ERROR:
        {
            UE_LOG(LogHotUpdate, Log, TEXT("%s download failed"), *InInfo.URL);

            OnTaskFinish(InInfo, false);
        }
        break;
    default:
        break;
    }
}

void FFileDownloadManager::ClearTempPak()
{
    const auto& SearchPath = GetTempPakSaveRoot();

    TArray<FString> Files;

    auto& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    PlatformFile.FindFiles(Files, *SearchPath, TEXT(".pak"));

    PlatformFile.FindFiles(Files, *SearchPath, *FDownloadTask::TempFileExtension);

    for (const auto& File : Files)
    {
        if (PlatformFile.DeleteFile(*File))
        {
            UE_LOG(LogHotUpdate, Log, TEXT("Success to delete file: %s"), *File);
        }
        else
        {
            UE_LOG(LogHotUpdate, Warning, TEXT("Failed to delete file: %s"), *File);
        }
    }
}

FDownloadProgress FFileDownloadManager::GetDownloadProgress()
{
    const auto CurrentTime = GWorld->GetTimeSeconds();

    auto CurrentDownSize = 0;

    for (const auto& Task : Tasks)
    {
        CurrentDownSize += Task.Value->GetTaskInfo().DownloadSize;
    }

    FDownloadProgress DownloadProgress(CurrentDownSize, TotalDownloadSize,
                                       FDownloadProgress::ConvertIntToSize(
                                           (CurrentDownSize - LastDownloadedSize) /
                                           (CurrentTime - LastUpdateTime)).
                                       Append(TEXT("/s")));

    LastDownloadedSize = CurrentDownSize;

    LastUpdateTime = CurrentTime;

    return DownloadProgress;
}
