#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "RhythmEnemyController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;

UCLASS()
class ARhythmEnemyController : public AAIController
{
    GENERATED_BODY()
public:
    ARhythmEnemyController();

protected:
    virtual void OnPossess(APawn* InPawn) override;

private:
    UPROPERTY(VisibleAnywhere)
    UAIPerceptionComponent* Perception = nullptr;

    UPROPERTY()
    UAISenseConfig_Sight* SightConfig = nullptr;

    UFUNCTION()
    void OnPerceptionUpdated(const TArray<AActor*>& UpdatedActors);
};
