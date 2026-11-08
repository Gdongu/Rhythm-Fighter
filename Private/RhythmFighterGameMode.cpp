// Copyright Epic Games, Inc. All Rights Reserved.

#include "RhythmFighterGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "RhythmFighterCharacter.h"
#include "RhythmEnemyCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Animation/AnimInstance.h"

void ARhythmFighterGameMode::BeginPlay()
{
    Super::BeginPlay();

    // 전투 시작 시 현재 적 수 집계
    TArray<AActor*> Enemies;
    UGameplayStatics::GetAllActorsOfClass(this, ARhythmEnemyCharacter::StaticClass(), Enemies);
    AliveEnemies = Enemies.Num();
}

void ARhythmFighterGameMode::OnAnyActorDied(AActor* DeadActor)
{
    RegisterDeath(DeadActor);
}

void ARhythmFighterGameMode::EndBattle(bool bPlayerWon)
{
    if (bBattleEnded) return;
    bBattleEnded = true;

    FreezeCombatWorld();
    
    ShowResult(bPlayerWon);

    GetWorldTimerManager().SetTimer(
        ReturnHandle,
        this,
        &ARhythmFighterGameMode::ReturnToOverworld,
        ReturnDelay,
        false
    );
}

void ARhythmFighterGameMode::RegisterDeath(AActor* DeadActor)
{
    if (!DeadActor) return;

    if (Cast<ARhythmFighterCharacter>(DeadActor) || DeadActor->ActorHasTag(TEXT("Player")))
    {
        // 플레이어 사망 플래그만 기록해두고, EndBattle은 캐릭터가 딜레이 후 호출
        return;
    }

    if (Cast<ARhythmEnemyCharacter>(DeadActor) || DeadActor->ActorHasTag(TEXT("Enemy")))
    {
        AliveEnemies = FMath::Max(0, AliveEnemies - 1);
        return;
    }
}

void ARhythmFighterGameMode::FreezeCombatWorld()
{
    // 플레이어 입력 정지
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->SetIgnoreMoveInput(true);
        PC->SetIgnoreLookInput(true);
        // PC->SetPause(true); // 쓰지 말 것 (타이머 멈춤)
    }

    // 모든 적/플레이어의 히트박스/AI/몽타주 정지
    TArray<AActor*> All;
    UGameplayStatics::GetAllActorsOfClass(this, ACharacter::StaticClass(), All);
    for (AActor* A : All)
    {
        // Overlap/피해 막기
        TInlineComponentArray<UPrimitiveComponent*> PrimArray;
        A->GetComponents(PrimArray);               // 템플릿으로 바로 Primitive만 수집
        for (UPrimitiveComponent* P : PrimArray)
        {
            P->SetGenerateOverlapEvents(false);
        }

        if (!A->ActorHasTag(TEXT("Dead")))
        {
            if (USkeletalMeshComponent* MeshComp = A->FindComponentByClass<USkeletalMeshComponent>())
            {
                if (UAnimInstance* Anim = MeshComp->GetAnimInstance())
                {
                    // 진행 중 몽타주만 부드럽게 정지
                    Anim->StopAllMontages(0.15f);
                }
            }
        }

        // AI 멈추기
        if (APawn* Pawn = Cast<APawn>(A))
        {
            if (AAIController* AIC = Cast<AAIController>(Pawn->GetController()))
            {
                if (UBrainComponent* Brain = AIC->GetBrainComponent())
                {
                    Brain->StopLogic(TEXT("BattleEnded"));
                }
                AIC->StopMovement();
            }
        }
    }
}

void ARhythmFighterGameMode::ShowResult(bool bPlayerWon)
{
    if (!ResultWidgetClass) return;

    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        UUserWidget* W = CreateWidget<UUserWidget>(PC, ResultWidgetClass);
        if (!W) return;

        // BP 위젯이라면 변수명 bVictory를 만들어두고, ExposeOnSpawn을 안 썼다면 아래처럼 주입:
        // (ExposeOnSpawn을 켰다면 CreateWidget 핀으로 직접 전달해도 됨)
        // KismetSystemLibrary 이용 (SetPropertyByName는 없음)
        UKismetSystemLibrary::SetBoolPropertyByName(W, TEXT("bVictory"), bPlayerWon);

        W->AddToViewport(100);

        PC->bShowMouseCursor = true;
        UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(PC, W, EMouseLockMode::DoNotLock, true);
    }
}

void ARhythmFighterGameMode::ReturnToOverworld()
{
    GetWorldTimerManager().ClearTimer(ReturnHandle);
    if (OverworldLevelName != NAME_None)
    {
        UGameplayStatics::OpenLevel(this, OverworldLevelName);
    }
}