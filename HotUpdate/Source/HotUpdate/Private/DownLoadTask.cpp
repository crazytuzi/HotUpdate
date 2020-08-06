#include "DownLoadTask.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "FileDownLog.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"

FString FDownloadTask::TempFileExtension = TEXT(".tmp");

FDownloadTask::FDownloadTask(const FString& URL, const FString& SaveRoot, const FString& FileName,
                             const int32 FileSize) : TempFileHandle(nullptr), Request(nullptr)
{
#if PLATFORM_ANDROID
	if (!FPlatformFileManager::Get().GetPlatformFile().GetLowerLevel()->DirectoryExists(*SaveRoot))
#elif PLATFORM_WINDOWS
    if (!FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*SaveRoot))
#endif
    {
        if (!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*SaveRoot))
        {
            UE_LOG(LogHotUpdate, Warning, TEXT("Cannot create directory : %s"), *SaveRoot);
        }
    }

    Root = SaveRoot;

    TaskInfo.URL = URL;

    TaskInfo.FileName = FileName;

    TaskInfo.FileSize = FileSize;

    bIsDownLoading = false;
}

bool FDownloadTask::IsFileExist() const
{
    return IFileManager::Get().FileExists(*FPaths::Combine(Root, TaskInfo.FileName));
}

void FDownloadTask::Start()
{
    if (TaskInfo.FileName.IsEmpty())
    {
        TaskInfo.FileName = FPaths::GetCleanFilename(TaskInfo.URL);
    }

    if (TaskInfo.URL.IsEmpty() || TaskInfo.FileName.IsEmpty())
    {
        OnTaskEvent.Execute(EDownloadTaskEvent::ERROR, TaskInfo);

        return;
    }

    if (bIsDownLoading)
    {
        return;
    }

    bIsDownLoading = true;

    ReqGetHead();
}

void FDownloadTask::Stop()
{
    if (TempFileHandle != nullptr)
    {
        delete TempFileHandle;

        TempFileHandle = nullptr;
    }

    if (Request.IsValid())
    {
        Request->OnProcessRequestComplete().Unbind();

        Request->OnRequestProgress().Unbind();

        Request = nullptr;
    }

    OnTaskEvent.Unbind();
}

FTaskInfo FDownloadTask::GetTaskInfo() const
{
    return TaskInfo;
}

void FDownloadTask::ReqGetHead()
{
    const auto& EncodedURL = GetEncodedURL();

    if (EncodedURL.IsEmpty())
    {
        UE_LOG(LogHotUpdate, Warning, TEXT("Error URL"))

        return;
    }

    Request = FHttpModule::Get().CreateRequest();

    Request->SetVerb("HEAD");

    Request->SetURL(EncodedURL);

    Request->OnProcessRequestComplete().BindRaw(this, &FDownloadTask::RetGetHead);

    Request->ProcessRequest();

    OnTaskEvent.Execute(EDownloadTaskEvent::REQ_HEAD, TaskInfo);
}

void FDownloadTask::RetGetHead(FHttpRequestPtr, const FHttpResponsePtr Response, const bool bConnectedSuccessfully)
{
    if (!Response.IsValid() || !bConnectedSuccessfully)
    {
        UE_LOG(LogHotUpdate, Warning, TEXT("RetGetHead Response error"));

        OnTaskEvent.Execute(EDownloadTaskEvent::ERROR, TaskInfo);

        return;
    }

    const auto ResponseCode = Response->GetResponseCode();

    if (ResponseCode >= 400 || ResponseCode < 200)
    {
        UE_LOG(LogHotUpdate, Warning, TEXT("Http Response code error : %d"), ResponseCode);

        if (TempFileHandle != nullptr)
        {
            delete TempFileHandle;

            TempFileHandle = nullptr;
        }

        OnTaskEvent.Execute(EDownloadTaskEvent::ERROR, TaskInfo);

        return;
    }

    OnTaskEvent.Execute(EDownloadTaskEvent::RET_HEAD, TaskInfo);

    TaskInfo.CurrentSize = 0;

    TaskInfo.TotalSize = Response->GetContentLength();

    TempFileName = GetFilePath() + TempFileExtension;

    const auto& SavePath = FPaths::GetPath(TempFileName);

    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*SavePath);

    if (!IFileManager::Get().DirectoryExists(*SavePath))
    {
        UE_LOG(LogHotUpdate, Warning, TEXT("%s, create temp directory error"), *SavePath);

        OnTaskEvent.Execute(EDownloadTaskEvent::ERROR, TaskInfo);

        return;
    }

    auto& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    TempFileHandle = PlatformFile.OpenWrite(*TempFileName, true);

    if (TempFileHandle == nullptr)
    {
        UE_LOG(LogHotUpdate, Warning, TEXT("%s, create temp file error"), *TempFileName);

        OnTaskEvent.Execute(EDownloadTaskEvent::ERROR, TaskInfo);

        return;
    }

    UE_LOG(LogHotUpdate, Log, TEXT("Create temp file success! Start downloading: %s"), *TempFileName);

    ReqGetChunk();
}

void FDownloadTask::ReqGetChunk()
{
    const auto& EncodedURL = GetEncodedURL();

    if (EncodedURL.IsEmpty())
    {
        UE_LOG(LogHotUpdate, Warning, TEXT("Error URL"))

        return;
    }

    const auto BeginPosition = TaskInfo.CurrentSize;

    auto EndPosition = BeginPosition + ChunkSize - 1;

    if (EndPosition >= TaskInfo.TotalSize)
    {
        EndPosition = TaskInfo.TotalSize - 1;
    }

    const auto& RangeStr = FString("bytes=") + FString::FromInt(BeginPosition) +
        FString(TEXT("-")) + FString::FromInt(EndPosition);

    Request = FHttpModule::Get().CreateRequest();

    Request->SetVerb("GET");

    Request->SetURL(EncodedURL);

    Request->AppendToHeader(FString("Range"), RangeStr);

    Request->OnProcessRequestComplete().BindRaw(this, &FDownloadTask::RetGetChunk);

    Request->OnRequestProgress().BindRaw(this, &FDownloadTask::GetChunkProgress);

    Request->ProcessRequest();

    OnTaskEvent.Execute(EDownloadTaskEvent::BEGIN_DOWNLOAD, TaskInfo);
}

FString FDownloadTask::GetFilePath() const
{
    return FPaths::Combine(Root, TaskInfo.FileName);
}

void FDownloadTask::RetGetChunk(FHttpRequestPtr, const FHttpResponsePtr Response, const bool bConnectedSuccessfully)
{
    if (!Response.IsValid() || !bConnectedSuccessfully)
    {
        UE_LOG(LogHotUpdate, Warning, TEXT("RetGetChunk Response error"));

        OnTaskEvent.Execute(EDownloadTaskEvent::ERROR, TaskInfo);

        return;
    }

    const auto ResponseCode = Response->GetResponseCode();

    if (ResponseCode >= 400 || ResponseCode < 200)
    {
        UE_LOG(LogHotUpdate, Warning, TEXT("%d, ResponseCode code error"), ResponseCode);

        if (TempFileHandle != nullptr)
        {
            delete TempFileHandle;
            TempFileHandle = nullptr;
        }

        OnTaskEvent.Execute(EDownloadTaskEvent::ERROR, TaskInfo);

        return;
    }

    const auto& Buffer = Response->GetContent();

    if (TempFileHandle != nullptr)
    {
        TempFileHandle->Seek(TaskInfo.CurrentSize);

        if (TempFileHandle->Write(Buffer.GetData(), Buffer.Num()))
        {
            OnWriteChunkEnd(Buffer.Num());
        }
        else
        {
            UE_LOG(LogHotUpdate, Warning, TEXT("write file error"));

            OnTaskEvent.Execute(EDownloadTaskEvent::ERROR, TaskInfo);
        }
    }
}

void FDownloadTask::GetChunkProgress(FHttpRequestPtr, int32, const int32 BytesReceived)
{
    const auto DownloadSize = TaskInfo.CurrentSize + BytesReceived;

    if (DownloadSize > TaskInfo.TotalSize)
    {
        UE_LOG(LogHotUpdate, Warning, TEXT("%s, DownloadSize is more than total size"), *GetFilePath());
        return;
    }

    TaskInfo.DownloadSize = DownloadSize;

    OnTaskEvent.Execute(EDownloadTaskEvent::UPDATE_DOWNLOAD, TaskInfo);
}

void FDownloadTask::OnTaskCompleted()
{
    if (TempFileHandle != nullptr)
    {
        delete TempFileHandle;

        TempFileHandle = nullptr;
    }

    bIsDownLoading = false;

    if (!IsFileExist())
    {
        if (IFileManager::Get().Move(*GetFilePath(), *TempFileName))
        {
            OnTaskEvent.Execute(EDownloadTaskEvent::END_DOWNLOAD, TaskInfo);
        }
        else
        {
            UE_LOG(LogHotUpdate, Warning, TEXT("%s, move error"), *GetFilePath());

            OnTaskEvent.Execute(EDownloadTaskEvent::ERROR, TaskInfo);
        }
    }
    else
    {
        TaskInfo.TotalSize = IFileManager::Get().FileSize(*GetFilePath());

        if (TaskInfo.CurrentSize != TaskInfo.TotalSize)
        {
            TaskInfo.CurrentSize = TaskInfo.TotalSize;
        }

        OnTaskEvent.Execute(EDownloadTaskEvent::END_DOWNLOAD, TaskInfo);
    }
}

void FDownloadTask::OnWriteChunkEnd(const int32 BufferSize)
{
    TaskInfo.CurrentSize = TaskInfo.CurrentSize + BufferSize;

    OnTaskEvent.Execute(EDownloadTaskEvent::UPDATE_DOWNLOAD, TaskInfo);

    if (TaskInfo.CurrentSize < TaskInfo.TotalSize)
    {
        ReqGetChunk();
    }
    else
    {
        OnTaskCompleted();
    }
}

FString FDownloadTask::GetEncodedURL() const
{
    const auto URLSplit = TaskInfo.URL.Find(FString("/"),
                                            ESearchCase::IgnoreCase, ESearchDir::FromStart, 14) + 1;

    if (URLSplit > 0)
    {
        auto SubURL = TaskInfo.URL.Mid(URLSplit);

        TArray<FString> URLUnit;

        SubURL.ParseIntoArray(URLUnit, TEXT("/"));

        SubURL.Empty();

        for (auto i = 0; i < URLUnit.Num(); ++i)
        {
            SubURL += FGenericPlatformHttp::UrlEncode(URLUnit[i]);

            if (i < URLUnit.Num() - 1)
            {
                SubURL += "/";
            }
        }

        return TaskInfo.URL.Left(URLSplit) + SubURL;
    }

    return FString();
}

FGuid FDownloadTask::GetGuid() const
{
    return TaskInfo.GUID;
}
