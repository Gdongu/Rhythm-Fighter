#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_Hitbox.generated.h"

UCLASS(meta = (DisplayName = "Hitbox Window"))
class RHYTHMFIGHTER_API UAnimNotifyState_Hitbox : public UAnimNotifyState
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Hit")
    FName   AttachSocket = TEXT("hand_r");

    UPROPERTY(EditAnywhere, Category = "Hit")
    FVector BoxExtent = FVector(40.f, 25.f, 25.f);

    UPROPERTY(EditAnywhere, Category = "Hit")
    float   Damage = 10.f;

    UPROPERTY(EditAnywhere, Category = "Hit")
    FVector LocalOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category = "Hit")
    FRotator LocalRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, Category = "Hit")
    bool bDoManualHitCheck = true;

    virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

    virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

    virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;
};
