// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "HotUpdateSettings.generated.h"

/**
 * 
 */
UCLASS(Config = Game, Defaultconfig)
class HOTUPDATE_API UHotUpdateSettings : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere)
    FString HotUpdateServerUrl = "http://127.0.0.1";

    UPROPERTY(Config, EditAnywhere)
    FString TempPakSaveRoot = "Paks/Temp";

    UPROPERTY(Config, EditAnywhere)
    FString PakSaveRoot = "Paks";

    UPROPERTY(Config, EditAnywhere)
    float TimeOutDelay = 10.f;

    UPROPERTY(Config, EditAnywhere)
    uint32 MaxRetryTime = 3;
};
