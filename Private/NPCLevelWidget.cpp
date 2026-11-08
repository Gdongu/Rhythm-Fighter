// Fill out your copyright notice in the Description page of Project Settings.


#include "NPCLevelWidget.h"
#include "Components/TextBlock.h"

void UNPCLevelWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UNPCLevelWidget::SetLevel(int32 InLevel)
{
    if (!Text_Level) return;

    FText LevelText = FText::Format(
        NSLOCTEXT("NPC", "LevelFormat", "Lv. {0}"),
        FText::AsNumber(InLevel)
    );

    Text_Level->SetText(LevelText);
}
