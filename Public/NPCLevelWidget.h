// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NPCLevelWidget.generated.h"

class UTextBlock;

UCLASS()
class RHYTHMFIGHTER_API UNPCLevelWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
    UFUNCTION(BlueprintCallable, Category = "NPC")
    void SetLevel(int32 InLevel);

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* Text_Level;

};
