#include "AnimNotifyState_Hitbox.h"
#include "GameFramework/Actor.h"
#include "RhythmFighterCharacter.h"
#include "RhythmEnemyCharacter.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotifyState_Hitbox::NotifyBegin(
    USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
    float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
    if (!MeshComp) return;

    AActor* Owner = MeshComp->GetOwner();

    if (auto* C = Cast<ARhythmFighterCharacter>(Owner))
    {
        // 플레이어는 기존 그대로 써도 되고, 원하면 플레이어도 Ex버전 만들어서 호출해도 됨
        C->StartHitbox(AttachSocket, BoxExtent, Damage);
        C->StartAttackFX();
        return;
    }

    if (auto* E = Cast<ARhythmEnemyCharacter>(Owner))
    {
        E->StartHitboxEx(AttachSocket, BoxExtent, Damage, LocalOffset, LocalRotation);
        E->StartAttackFX();
        return;
    }
}

void UAnimNotifyState_Hitbox::NotifyTick(
    USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
    float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
    if (!bDoManualHitCheck || !MeshComp) return;

    AActor* Owner = MeshComp->GetOwner();
    if (auto* E = Cast<ARhythmEnemyCharacter>(Owner))
    {
        // ★ 매 프레임 Overlap 강제 처리
        E->ProcessHitbox();
    }
}

void UAnimNotifyState_Hitbox::NotifyEnd(
    USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyEnd(MeshComp, Animation, EventReference);
    if (!MeshComp) return;

    AActor* Owner = MeshComp->GetOwner();

    if (auto* C = Cast<ARhythmFighterCharacter>(Owner))
    {
        C->EndHitbox();
        C->StopAttackFX();
        return;
    }
    if (auto* E = Cast<ARhythmEnemyCharacter>(Owner))
    {
        E->EndHitbox();
        E->StopAttackFX();
        return;
    }
}