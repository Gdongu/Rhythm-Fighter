// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RhythmFighterGameMode.generated.h"

class UUserWidget;

UCLASS(minimalapi)
class ARhythmFighterGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
    // 승패 결과를 그려줄 위젯 (BP: WBP_BattleResult)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle")
    TSubclassOf<UUserWidget> ResultWidgetClass;

    // 전투 후 돌아갈 오픈월드 맵 이름 (예: "Overworld_Main")
    UPROPERTY(EditDefaultsOnly, Category = "Battle")
    FName OverworldLevelName = "Overworld_Main";

    // 전투 종료 연출 대기 시간
    UPROPERTY(EditDefaultsOnly, Category = "Battle")
    float ReturnDelay = 3.0f;

    // 캐릭터/적이 죽을 때 캐릭터들이 호출
    UFUNCTION() void OnAnyActorDied(AActor* DeadActor);

    // 적 생존 수 조회/갱신용
    FORCEINLINE int32 GetAliveEnemies() const { return AliveEnemies; }
    void RegisterDeath(AActor* DeadActor);

    UFUNCTION(BlueprintCallable, Category = "Battle")
    void EndBattle(bool bPlayerWon);

protected:
    virtual void BeginPlay() override;

private:
    bool bBattleEnded = false;
    int32 AliveEnemies = 0;
    FTimerHandle ReturnHandle;

    void ShowResult(bool bPlayerWon);

    void FreezeCombatWorld();

    UFUNCTION()
    void ReturnToOverworld();
};
