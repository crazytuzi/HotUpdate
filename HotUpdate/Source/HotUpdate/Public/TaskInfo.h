#pragma once
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TaskInfo.generated.h"

USTRUCT(BlueprintType)
struct FTaskInfo
{
    GENERATED_BODY()

    FTaskInfo() : FileSize(0), CurrentSize(0), DownloadSize(0), TotalSize(0), GUID(FGuid::NewGuid())
    {
    }

    FString FileName;

    FString URL;

    uint32 FileSize;

    int32 CurrentSize;

    int32 DownloadSize;

    int32 TotalSize;

    FGuid GUID;
};
