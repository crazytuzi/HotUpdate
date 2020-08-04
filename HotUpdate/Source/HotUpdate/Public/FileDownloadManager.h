#pragma once
#include "CoreMinimal.h"
#include "TaskInfo.h"
#include "DownLoadTask.h"
#include "FileDownType.h"

DECLARE_DELEGATE_TwoParams(FOnDownloadEvent, EDownloadState, const FTaskInfo&);

class HOTUPDATE_API FFileDownloadManager
{
public:
    void StartUp();

    void ShutDown();

    void OnTaskFinish(const FTaskInfo& Info, bool bIsSuccess);

    void OnAllTaskFinish() const;

    void AddTask(const FString& URL, const FString& Name, const int32 Size);

    void OnTaskEvent(EDownloadTaskEvent InEvent, const FTaskInfo& InInfo);

    bool IsSuccessful() const;

    FDownloadProgress GetDownloadProgress();

    static FString GetTempPakSaveRoot();

    static FString GetPakSaveRoot();

    FOnDownloadEvent OnDownloadEvent;

private:
    TMap<FGuid, TSharedPtr<FDownloadTask>> Tasks;

    TArray<TSharedPtr<FDownloadTask>> FailedTasks;

    FDateTime StartTime;

    uint64 CurrentDownloadSize = 0;

    uint64 TotalDownloadSize = 0;

    float LastUpdateTime = 0.f;

    uint32 LastDownloadedSize = 0;

    static void ClearTempPak();
};
