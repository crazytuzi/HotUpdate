// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FileDownType.h"
#include "FilePakManager.h"
#include "TaskInfo.h"
#include "Engine/EngineTypes.h"
#include "FileDownloadManager.h"
#include "Interfaces/IHttpRequest.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HotUpdateSubsystem.generated.h"

DECLARE_DELEGATE_TwoParams(FOnHotUpdatState, EHotUpdateState, const FString&);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDownloadUpdate, const FDownloadProgress&, Desc);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHotUpdateFinished);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMountUpdate, FString, PakName, float, Progress);

/**
 * 
 */
UCLASS(Config = "HotUpdate")
class HOTUPDATE_API UHotUpdateSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable)
    void StartUp();

    void ShutDown();

    UFUNCTION(BlueprintPure, Category = "HotUpdateSubsystem")
    bool IsFinished() const { return !bIsUpdating; }

    UFUNCTION(BlueprintCallable)
    static bool CanSkipUpdate();

    UFUNCTION(BlueprintCallable)
    void ForceSkipUpdate() const;

public:
    FOnHotUpdatState OnHotUpdateStateEvent;

    UPROPERTY(BlueprintAssignable)
    FOnDownloadUpdate OnDownloadUpdate;

    UPROPERTY(BlueprintAssignable)
    FOnHotUpdateFinished OnHotUpdateFinished;

    UPROPERTY(BlueprintAssignable)
    FOnMountUpdate OnMountUpdate;

protected:
    bool IsSuccessful() const;

private:
    void ReqGetVersion();

    void OnReqGetVersionTimeOut();

    void RetGetVersion(FHttpRequestPtr, const FHttpResponsePtr Response, const bool bConnectedSuccessfully);

    void OnDownloadEvent(const EDownloadState Event, const FTaskInfo& TaskInfo) const;

    void OnUpdateDownloadProgress() const;

    FString GetHotUpdateServerUrl() const;

    void OnSkipUpdate() const;

    UFUNCTION()
    void OnMountProcess(const FString& PakName, float Progress) const;

    UFUNCTION()
    void OnHotUpdateState(EHotUpdateState State, const FString& Message);

    static FString GetPlatform();

public:
    TSharedPtr<FFileDownloadManager> DownloadManager;

    TSharedPtr<FFilePakManager> PakManager;

    TSharedPtr<IHttpRequest> Request;

private:
    FTimerHandle TimeOutHandle;

    uint32 CurrentTimeRetry = 0;

    bool bIsUpdating = false;
};
