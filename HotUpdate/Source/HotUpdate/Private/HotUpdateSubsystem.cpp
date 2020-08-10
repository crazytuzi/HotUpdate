// Fill out your copyright notice in the Description page of Project Settings.


#include "HotUpdateSubsystem.h"
#include "Serialization/JsonSerializer.h"
#include "FileDownloadManager.h"
#include "FileDownLog.h"
#include "IPlatformFilePak.h"
#include "HttpModule.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/NetworkVersion.h"
#include "Interfaces/IHttpResponse.h"
#include "HotUpdateSettings.h"
#include "Policies/CondensedJsonPrintPolicy.h"

void UHotUpdateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    if (CanSkipUpdate())
    {
        return;
    }

    bIsUpdating = true;

    DownloadManager = MakeShareable(new FFileDownloadManager());

    if (!DownloadManager.IsValid())
    {
        return;
    }

    DownloadManager->OnDownloadEvent.BindUObject(this, &UHotUpdateSubsystem::OnDownloadEvent);

    PakManager = MakeShareable(new FFilePakManager());

    if (PakManager.IsValid())
    {
        PakManager->OnMountUpdated.BindUObject(this, &UHotUpdateSubsystem::OnMountProcess);
    }

    OnHotUpdateStateEvent.BindUObject(this, &UHotUpdateSubsystem::OnHotUpdateState);
}

void UHotUpdateSubsystem::Deinitialize()
{
    Super::Deinitialize();
}

void UHotUpdateSubsystem::StartUp()
{
    if (CanSkipUpdate())
    {
        OnSkipUpdate();

        return;
    }

    ReqGetVersion();
}

void UHotUpdateSubsystem::ShutDown()
{
    if (Request.IsValid())
    {
        Request->OnProcessRequestComplete().Unbind();
    }

    if (DownloadManager.IsValid())
    {
        DownloadManager->ShutDown();

        DownloadManager = nullptr;
    }

    if (PakManager.IsValid())
    {
        PakManager->ShutDown();

        PakManager = nullptr;
    }

    bIsUpdating = false;
}

bool UHotUpdateSubsystem::CanSkipUpdate()
{
#if WITH_EDITOR
    return false;
#else
    return false;
#endif
}

void UHotUpdateSubsystem::ForceSkipUpdate() const
{
    if (CanSkipUpdate())
    {
        return;
    }

    if (IsFinished())
    {
        return;
    }

    OnSkipUpdate();
}

void UHotUpdateSubsystem::OnSkipUpdate() const
{
    OnHotUpdateStateEvent.Execute(EHotUpdateState::END_HOTUPDATE, TEXT("OnSkipUpdate"));
}

FString UHotUpdateSubsystem::GetPlatform()
{
#if PLATFORM_DESKTOP &&  WITH_EDITOR
    return TEXT("editor");
#elif PLATFORM_WINDOWS
    return TEXT("win");
#elif PLATFORM_ANDROID
    return TEXT("android");
#elif PLATFORM_IOS
    return TEXT("ios");
#elif PLATFORM_MAC
    return TEXT("mac");
#elif PLATFORM_LINUX
    return TEXT("linux");
#endif
}

void UHotUpdateSubsystem::OnMountProcess(const FString& PakName, const float Progress) const
{
    OnMountUpdate.Broadcast(PakName, Progress);
}

void UHotUpdateSubsystem::OnHotUpdateState(const EHotUpdateState State, const FString& Message)
{
    if (State != EHotUpdateState::ERROR)
    {
        UE_LOG(LogHotUpdate, Log, TEXT("OnHotUpdateState %s"), *Message);
    }
    else
    {
        UE_LOG(LogHotUpdate, Error, TEXT("OnHotUpdateState %s"), *Message);
    }

    switch (State)
    {
    case EHotUpdateState::END_GETVERSION:
        {
            if (DownloadManager.IsValid())
            {
                DownloadManager->StartUp();
            }
        }
        break;
    case EHotUpdateState::END_DOWNLOAD:
        {
            OnUpdateDownloadProgress();

            OnHotUpdateStateEvent.Execute(EHotUpdateState::BEGIN_MOUNT, TEXT("BeginMount"));
        }
        break;
    case EHotUpdateState::BEGIN_MOUNT:
        {
            if (PakManager.IsValid())
            {
                PakManager->StartUp();

                if (PakManager->IsSuccessful())
                {
                    OnHotUpdateStateEvent.Execute(EHotUpdateState::END_MOUNT, TEXT("EndMount"));
                }
                else
                {
                    OnHotUpdateStateEvent.Execute(EHotUpdateState::ERROR, TEXT("Mount failed"));
                }
            }
        }
        break;
    case EHotUpdateState::END_MOUNT:
        {
            OnHotUpdateStateEvent.Execute(EHotUpdateState::END_HOTUPDATE, TEXT("FinishUpdate"));
        }
        break;
    case EHotUpdateState::END_HOTUPDATE:
        {
            if (IsSuccessful())
            {
                ShutDown();

                OnHotUpdateFinished.Broadcast();
            }
            else
            {
                OnHotUpdateStateEvent.Execute(EHotUpdateState::ERROR, TEXT("Is not successful"));
            }
        }
        break;
    default: break;
    }
}

void UHotUpdateSubsystem::ReqGetVersion()
{
    OnHotUpdateStateEvent.Execute(EHotUpdateState::BEGIN_GETVERSION, TEXT("Begin to get version"));

    FString JsonStr;

    auto JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonStr);

    JsonWriter->WriteObjectStart();

    JsonWriter->WriteValue(TEXT("version"), FNetworkVersion::GetProjectVersion());

    JsonWriter->WriteValue(TEXT("platform"), GetPlatform());

    JsonWriter->WriteObjectEnd();

    JsonWriter->Close();

    const auto URL = GetHotUpdateServerUrl();

    if (!Request.IsValid())
    {
        Request = FHttpModule::Get().CreateRequest();
    }

    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));

    Request->SetURL(URL);

    Request->SetVerb(TEXT("POST"));

    Request->SetContentAsString(JsonStr);

    Request->OnProcessRequestComplete().BindUObject(this, &UHotUpdateSubsystem::RetGetVersion);

    Request->ProcessRequest();

    const auto HotUpdateSettings = GetMutableDefault<UHotUpdateSettings>();

    GetWorld()->GetTimerManager().SetTimer(TimeOutHandle, this, &UHotUpdateSubsystem::OnReqGetVersionTimeOut,
                                           HotUpdateSettings != nullptr ? HotUpdateSettings->TimeOutDelay : 10.f,
                                           false);
}

void UHotUpdateSubsystem::OnReqGetVersionTimeOut()
{
    if (Request.IsValid())
    {
        Request->OnProcessRequestComplete().Unbind();
    }

    CurrentTimeRetry++;

    const auto HotUpdateSettings = GetMutableDefault<UHotUpdateSettings>();

    const auto MaxRetryTime = HotUpdateSettings != nullptr ? HotUpdateSettings->MaxRetryTime : 3;

    if (CurrentTimeRetry <= MaxRetryTime)
    {
        ReqGetVersion();
    }
    else
    {
        CurrentTimeRetry = 0;

        GetWorld()->GetTimerManager().ClearTimer(TimeOutHandle);

        OnHotUpdateStateEvent.Execute(EHotUpdateState::ERROR, TEXT("Error: Failed to req version"));
    }
}

void UHotUpdateSubsystem::RetGetVersion(FHttpRequestPtr, const FHttpResponsePtr Response,
                                        const bool bConnectedSuccessfully)
{
    if (TimeOutHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(TimeOutHandle);
    }

    if (!bConnectedSuccessfully || !Response.IsValid())
    {
        OnHotUpdateStateEvent.Execute(EHotUpdateState::ERROR,
                                      TEXT("Error: Failed to get version"));
        return;
    }

    const auto ResponseCode = Response->GetResponseCode();

    if (ResponseCode >= 400 || ResponseCode < 200)
    {
        UE_LOG(LogHotUpdate, Warning, TEXT("Http Response code error : %d"), ResponseCode);

        OnHotUpdateStateEvent.Execute(EHotUpdateState::ERROR,
                                      TEXT("Error: Failed to get version"));

        return;
    }

    if (!DownloadManager.IsValid() || !PakManager.IsValid())
    {
        return;
    }

    const auto& URL = GetHotUpdateServerUrl() + "/" + FNetworkVersion::GetProjectVersion() + "/" + GetPlatform() + "/";

    TSharedPtr<FJsonObject> JsonObject;

    const auto& JsonReader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

    if (FJsonSerializer::Deserialize(JsonReader, JsonObject))
    {
        for (const auto& Value : JsonObject->Values)
        {
            const auto& Files = Value.Value->AsArray();

            for (const auto& File : Files)
            {
                const auto& FileObject = File->AsObject();

                const auto& FileName = FileObject->GetStringField("File");

                const auto& Hash = FileObject->GetStringField("HASH");

                const auto Size = FileObject->GetIntegerField("Size");

                FPakFileProperty PakFileProperty(FileName, Size, Hash);

                if (!FFilePakManager::IsPakValid(PakFileProperty))
                {
                    DownloadManager->AddTask(URL + FileName, FileName, Size);
                }

                PakManager->AddPakFile(MoveTemp(PakFileProperty));
            }
        }

        OnHotUpdateStateEvent.Execute(EHotUpdateState::END_GETVERSION, TEXT("End to get version"));
    }
    else
    {
        OnHotUpdateStateEvent.Execute(EHotUpdateState::ERROR,
                                      TEXT("Error: Failed to deserialize json"));
    }
}

bool UHotUpdateSubsystem::IsSuccessful() const
{
    if (!DownloadManager.IsValid() || !DownloadManager->IsSuccessful())
    {
        return false;
    }

    if (!PakManager.IsValid() || !PakManager->IsSuccessful())
    {
        return false;
    }

    return true;
}

void UHotUpdateSubsystem::OnDownloadEvent(const EDownloadState Event, const FTaskInfo&) const
{
    switch (Event)
    {
    case EDownloadState::UPDATE_DOWNLOAD:
        {
            OnUpdateDownloadProgress();
        }
        break;
    case EDownloadState::END_DOWNLOAD:
        {
            OnHotUpdateStateEvent.Execute(EHotUpdateState::END_DOWNLOAD, FString(TEXT("EndDownload")));
        }
        break;

    default:
        break;
    }
}

void UHotUpdateSubsystem::OnUpdateDownloadProgress() const
{
    if (DownloadManager.IsValid())
    {
        const auto& DownloadProgress = DownloadManager->GetDownloadProgress();

        OnDownloadUpdate.Broadcast(DownloadProgress);
    }
}

FString UHotUpdateSubsystem::GetHotUpdateServerUrl() const
{
    const auto HotUpdateSettings = GetMutableDefault<UHotUpdateSettings>();

    return HotUpdateSettings != nullptr ? HotUpdateSettings->HotUpdateServerUrl : "";
}
