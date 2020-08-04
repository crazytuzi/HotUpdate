#pragma once
#include "CoreMinimal.h"
#include "FileDownType.generated.h"

UENUM(BlueprintType)
enum class EDownloadTaskEvent : uint8
{
    REQ_HEAD,
    RET_HEAD,
    BEGIN_DOWNLOAD,
    UPDATE_DOWNLOAD,
    END_DOWNLOAD,
    ERROR
};

UENUM(BlueprintType)
enum class EDownloadState : uint8
{
    BEGIN_DOWNLOAD,
    UPDATE_DOWNLOAD,
    BEGIN_FILE_DOWNLOAD,
    END_FILE_DOWNLOAD,
    END_DOWNLOAD,
};

UENUM(BlueprintType)
enum class EHotUpdateState : uint8
{
    BEGIN_HOTUPDATE,
    BEGIN_GETVERSION,
    END_GETVERSION,
    BEGIN_DOWNLOAD,
    END_DOWNLOAD,
    BEGIN_MOUNT,
    END_MOUNT,
    END_HOTUPDATE,
    ERROR
};

struct FPakFileProperty
{
    FPakFileProperty(const FString& PakName, const int32 PakSize, const FString& MD5) : PakName(PakName),
        PakSize(PakSize), MD5(MD5)
    {
    }

    FString PakName;

    int32 PakSize;

    FString MD5;

    bool operator ==(const FPakFileProperty& Other) const
    {
        return PakName.Equals(Other.PakName) && PakSize == Other.PakSize && MD5.ToLower().Equals(Other.MD5.ToLower());
    }
};

USTRUCT(BlueprintType)
struct FDownloadProgress
{
    GENERATED_BODY()

    FDownloadProgress() = default;

    FDownloadProgress(const int32 CurrentDownloadSize, const int32 TotalDownloadSize, const FString& DownSpeed):
        CurrentDownloadSize(CurrentDownloadSize), TotalDownloadSize(TotalDownloadSize), DownloadSpeed(DownSpeed)
    {
    }

    UPROPERTY(BlueprintReadOnly, Category = "FDownloadProgress")
    int32 CurrentDownloadSize;

    UPROPERTY(BlueprintReadOnly, Category = "FDownloadProgress")
    int32 TotalDownloadSize;

    UPROPERTY(BlueprintReadOnly, Category = "FDownloadProgress")
    FString DownloadSpeed;

    static FString ConvertIntToSize(uint64 Size)
    {
        if (Size < 1024)
        {
            return FString::FormatAsNumber(Size).Append(TEXT("B"));
        }

        Size /= 1024;

        if (Size < 1024)
        {
            return FString::FormatAsNumber(Size).Append(TEXT("KB"));
        }

        Size /= 1024;

        return FString::FormatAsNumber(Size).Append(TEXT("MB"));
    }
};
