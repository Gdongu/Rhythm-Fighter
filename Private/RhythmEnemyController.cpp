#include "RhythmEnemyController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "RhythmEnemyCharacter.h"
#include "Kismet/GameplayStatics.h" 
#include "GameFramework/Character.h"

ARhythmEnemyController::ARhythmEnemyController()
{
    Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
    SightConfig->SightRadius = 3000.f;
    SightConfig->LoseSightRadius = 3500.f;
    SightConfig->PeripheralVisionAngleDegrees = 90.f;
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

    Perception->ConfigureSense(*SightConfig);
    Perception->SetDominantSense(SightConfig->GetSenseImplementation());
    Perception->OnPerceptionUpdated.AddDynamic(this, &ARhythmEnemyController::OnPerceptionUpdated);
}

void ARhythmEnemyController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    // 시작 시 플레이어를 기본 타겟으로 삼고 싶다면:
    if (ARhythmEnemyCharacter* Enemy = Cast<ARhythmEnemyCharacter>(InPawn))
    {
        // 월드의 0번 플레이어 찾기
        if (ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0))
        {
            Enemy->SetTarget(Player);
        }
    }
}

void ARhythmEnemyController::OnPerceptionUpdated(const TArray<AActor*>& UpdatedActors)
{
    // 시야로 플레이어를 잡아 타겟 업데이트 (원하면 유지)
    APawn* P = GetPawn();
    if (!P) return;

    if (ARhythmEnemyCharacter* Enemy = Cast<ARhythmEnemyCharacter>(P))
    {
        // 가장 가까운 플레이어 비슷한 개체 찾기(단순화)
        AActor* Best = nullptr;
        float BestDistSq = TNumericLimits<float>::Max();

        for (AActor* A : UpdatedActors)
        {
            if (!A) continue;
            const float D = FVector::DistSquared(A->GetActorLocation(), P->GetActorLocation());
            if (D < BestDistSq)
            {
                BestDistSq = D;
                Best = A;
            }
        }
        if (Best) Enemy->SetTarget(Best);
    }
}
