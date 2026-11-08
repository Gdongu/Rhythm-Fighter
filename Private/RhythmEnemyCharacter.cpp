#include "RhythmEnemyCharacter.h" 
#include "RhythmFighterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"  
#include "Animation/AnimInstance.h"             
#include "Components/SkeletalMeshComponent.h" 
#include "Kismet/GameplayStatics.h" 
#include "RhythmManager.h" 
#include "CombatStatsComponent.h" 
#include "Components/BoxComponent.h" 
#include "TimerManager.h" 
#include "Components/CapsuleComponent.h" 
#include "GameFramework/Character.h" 
#include "Kismet/KismetMathLibrary.h"
#include "Camera/PlayerCameraManager.h" 
#include "GameFramework/Actor.h"
#include "RhythmFighterGameMode.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "EquipmentStateSubsystem.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/SceneComponent.h"

ARhythmEnemyCharacter::ARhythmEnemyCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    auto* Move = GetCharacterMovement();
    Move->bOrientRotationToMovement = false;
    Move->SetPlaneConstraintEnabled(true);
    Move->SetPlaneConstraintAxisSetting(EPlaneConstraintAxisSetting::Y); // Y 고정(XZ만 이동)
    Move->bConstrainToPlane = true;

    Stats = CreateDefaultSubobject<UCombatStatsComponent>(TEXT("Stats"));
    GetCapsuleComponent()->SetGenerateOverlapEvents(true);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
    // 히트박스
    Hitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("Hitbox"));
    Hitbox->SetupAttachment(GetMesh());
    Hitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Hitbox->SetCollisionObjectType(ECC_WorldDynamic);
    Hitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
    Hitbox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // 캡슐과 겹침
    Hitbox->SetGenerateOverlapEvents(true);
    Hitbox->SetBoxExtent(FVector(30, 30, 30));
}

void ARhythmEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (Stats)
    {
        Stats->OnHealthChanged.AddDynamic(this, &ARhythmEnemyCharacter::HandleHealthChanged);
        Stats->OnDeath.AddDynamic(this, &ARhythmEnemyCharacter::OnSelfDied);
    }

    if (ACharacter* PC = UGameplayStatics::GetPlayerCharacter(this, 0))
    {
        TargetPlayer = PC;
    }

    CachedManager = Cast<ARhythmManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), ARhythmManager::StaticClass())
    );

    LockedY = GetActorLocation().Y;

    auto* Move = GetCharacterMovement();
    // 고정 Y 오프셋을 현재 위치로 맞추고 평면에 스냅
    Move->SetPlaneConstraintOrigin(FVector(0.f, LockedY, 0.f));
    Move->SnapUpdatedComponentToPlane();

    Hitbox->OnComponentBeginOverlap.AddDynamic(this, &ARhythmEnemyCharacter::OnHitboxOverlap);

    // 리젠 타이머 시작
    if (AttackRegenInterval > 0.f)
    {
        GetWorldTimerManager().SetTimer(
            AttackRegenHandle, this, &ARhythmEnemyCharacter::RegenAttackGauge,
            AttackRegenInterval, true, AttackRegenInterval);
    }

    // 죽으면 리젠 중단 (선택)
    if (Stats)
    {
        Stats->OnDeath.AddDynamic(this, &ARhythmEnemyCharacter::EndAttack); // 이미 있는 함수를 쓰거나…
        // 또는 람다로 클리어:
        // Stats->OnDeath.AddDynamic(this, &ARhythmEnemyCharacter::OnEnemyDeathStopRegen);
    }

    int32 EnemyLevel = 1;

    if (UGameInstance* GI = GetGameInstance())
    {
        if (UEquipmentStateSubsystem* EquipSys = GI->GetSubsystem<UEquipmentStateSubsystem>())
        {
            EnemyLevel = FMath::Max(1, EquipSys->PendingEnemyLevel);
        }
    }

    if (Stats)
    {
        // 1) HP 스케일링
        const float BonusHP = HPPerLevel * (EnemyLevel - 1);
        Stats->ModifyMaxHP(BonusHP);

        // 2) 공격력 스케일링
        const float LevelAttackBonus = AttackPerLevel * (EnemyLevel - 1);
        Stats->SetAttackBonus(LevelAttackBonus);

        UE_LOG(LogTemp, Log, TEXT("[Enemy] Level=%d  MaxHP=%.1f  AttackBonus=%.1f"),
            EnemyLevel, Stats->MaxHP, Stats->AttackBonus);
    }
}

bool ARhythmEnemyCharacter::IsInAttackRange() const
{
    if (!Target) return false;
    const float DX = FMath::Abs(Target->GetActorLocation().X - GetActorLocation().X);
    return DX <= AttackRange;
}

bool ARhythmEnemyCharacter::TryAttack()
{
    if (!Stats || bBusy || bAttackOnCooldown || bIsDead)
        return false;

    const float DistToPlayer = GetDistToPlayer();
    if (DistToPlayer > AttackRange)
        return false;

    // 콤보 전체에 필요한 최소 공격 게이지
    const int32 UseComboCount = FMath::Max(1, ComboCount);
    const float RequiredGauge = AttackCost * UseComboCount;

    // 예: AttackCost=0.25, ComboCount=4 → RequiredGauge=1.0
    if (Stats->AttackGauge < RequiredGauge)
        return false;

    if (AttackMontages.Num() == 0)
        return false;

    bBusy = true;
    bComboAttacking = true;
    RemainingComboHits = UseComboCount;

    // 첫 타 시작
    PlayNextComboHit();

    return true;
}

void ARhythmEnemyCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (CooldownTimer > 0.f) CooldownTimer -= DeltaSeconds;

    // 라운드 시작 전에는 정지
    if (CachedManager && !CachedManager->IsFightStarted())
        return;

    if (!Target || bBusy) return;

    const FVector My = GetActorLocation();
    const FVector Tg = Target->GetActorLocation();

    const float DX = Tg.X - My.X;
    const float Dist = FMath::Abs(DX);
    const float Sign = (DX >= 0.f) ? 1.f : -1.f; // 플레이어가 오른쪽이면 +1, 왼쪽이면 -1

    // 좌/우만 바라보기
    FRotator R = GetActorRotation();
    R.Yaw = (Sign >= 0.f) ? 0.f : 180.f;   // 메시 방향에 맞게 필요시 반대로
    SetActorRotation(R);

    // === 1) 필살기 우선 판단 ===
    const bool bCanUseFinisher =
        Stats && !bBusy && !bFinisherLocked &&
        CachedManager && CachedManager->IsFightStarted() &&
        Stats->SuperGauge >= FinisherCost &&
        FinisherMontage;

    if (bCanUseFinisher)
    {
        // 소비 먼저
        Stats->ConsumeSuperGauge(FinisherCost);

        StartEnemyCinematicZoom(60.f);

        // 연출/몽타주
        if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
        {
            bBusy = true;
            Anim->Montage_Play(FinisherMontage, 1.0f);

            FOnMontageEnded End;
            End.BindLambda([this](UAnimMontage*, bool)
                {
                    bBusy = false;
                    EndEnemyCinematicZoom();
                    // 필살기 쿨 잠깐 잠금(과다 사용 방지)
                    bFinisherLocked = true;
                    GetWorldTimerManager().SetTimer(
                        FinisherLockHandle, this, &ARhythmEnemyCharacter::UnlockFinisher, 2.0f, false);
                });
            Anim->Montage_SetEndDelegate(End, FinisherMontage);
        }

        return; // 필살기 중엔 이동/평타 안함
    }

    // === 2) 일반 공격 ===
    if (Dist <= AttackRange)
    {
        TryAttack();
        return;
    }

    // === 거리 기반 이동 결정을 스칼라 하나로 만든다 ===
    // Dist > Preferred → + (가까이 가기), Dist < Preferred → - (멀어지기)
    float desire = (Dist - PreferredDistance) / PreferredDistance; // 대략 -1~+∞
    desire = FMath::Clamp(desire, -1.f, 1.f);

    // 기본 전진/후퇴 성분 (멀면 +, 너무 가까우면 -)
    float inputToward = AI_ApproachWeight * desire;   // -1..+1

    // 좌우 흔들림(사인) 추가: 너무 과하면 역주행하니 StrafeWeight를 작게 유지
    const float T = GetWorld()->GetTimeSeconds();
    float strafe = AI_StrafeWeight * FMath::Sin(T * 2.f * PI * StrafeFrequency);

    // 최종 입력: 플레이어 방향 기준으로 투영
    float inputX = Sign * (inputToward + strafe);

    // 과도한 반대 주행 막기
    inputX = FMath::Clamp(inputX, -1.f, 1.f);

    // 너무 가까울 때는 강제 후퇴 우선권
    if (Dist < RetreatThreshold)
    {
        inputX = -Sign * 1.0f; // 플레이어 반대 방향으로 빠르게
    }

    // 실제 이동: X축(사이드스크롤)으로만
    AddMovementInput(FVector::ForwardVector, inputX);
    GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
}

void ARhythmEnemyCharacter::EndAttack()
{
    EndComboAttack();
}

void ARhythmEnemyCharacter::StartHitbox(FName AttachSocket, FVector BoxExtent, float Damage)
{
    StartHitboxEx(AttachSocket, BoxExtent, Damage, FVector::ZeroVector, FRotator::ZeroRotator);
}

void ARhythmEnemyCharacter::StartHitboxEx(FName AttachSocket, FVector BoxExtent, float Damage, FVector LocalOffset, FRotator LocalRotation)
{
    CurrentHitDamage = Damage;
    AlreadyHit.Reset();

    if (!Hitbox || !GetMesh()) return;

    if (!GetMesh()->DoesSocketExist(AttachSocket))
    {
        UE_LOG(LogTemp, Error, TEXT("[Enemy] Socket NOT found: %s"), *AttachSocket.ToString());
        return;
    }

    Hitbox->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocket);
    Hitbox->SetBoxExtent(BoxExtent, true);

    // 공격마다 조절 가능한 오프셋/회전
    Hitbox->SetRelativeLocation(LocalOffset);
    Hitbox->SetRelativeRotation(LocalRotation);

    Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Hitbox->SetGenerateOverlapEvents(true);

    // 켜는 순간 이미 겹친 상태도 잡기
    Hitbox->UpdateOverlaps();

    bHitboxActive = true;

    // 시작 프레임에도 한 번 강제 체크
    ProcessHitbox();
}

void ARhythmEnemyCharacter::ProcessHitbox()
{
    if (!bHitboxActive || !Hitbox) return;

    TArray<AActor*> Overlaps;
    Hitbox->GetOverlappingActors(Overlaps);

    for (AActor* Other : Overlaps)
    {
        TryApplyHit(Other);
    }
}

void ARhythmEnemyCharacter::EndHitbox()
{
    Hitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 끄기
    bHitboxActive = false;
    AlreadyHit.Reset();
}

void ARhythmEnemyCharacter::OnHitboxOverlap(
    UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (!bHitboxActive) return;
    TryApplyHit(Other);
}

void ARhythmEnemyCharacter::TryApplyHit(AActor* Other)
{
    if (!bHitboxActive || !Other || Other == this) return;
    if (AlreadyHit.Contains(Other)) return;
    if (!IsEnemy(Other)) return;

    UCombatStatsComponent* VictimStats = Other->FindComponentByClass<UCombatStatsComponent>();
    UCombatStatsComponent* AttackerStats = Stats;

    if (VictimStats && AttackerStats)
    {
        const float BaseDamage = CurrentHitDamage;
        const float Outgoing = AttackerStats->GetOutgoingDamage(BaseDamage);
        const float Final = VictimStats->GetIncomingAfterDefense(Outgoing);

        if (auto* P = Cast<ARhythmFighterCharacter>(Other))
        {
            if (P->IsInvulnerable()) return; // 대쉬 무적이면 HP도 안 깎음
        }

        VictimStats->ApplyDamage(Final);

        if (auto* P = Cast<ARhythmFighterCharacter>(Other)) P->ReceiveHit(Final, this);
        else if (auto* E = Cast<ARhythmEnemyCharacter>(Other)) E->ReceiveHit(Final, this);

        AttackerStats->AddSuperGauge(AttackerStats->SuperGainOnHit);

        AlreadyHit.Add(Other);
    }
}

void ARhythmEnemyCharacter::RegenAttackGauge()
{
    if (!Stats) return;
    if (!CachedManager || !CachedManager->IsFightStarted()) return;
    if (bComboAttacking) return;

    // 전투 중 강제 제한을 두고 싶으면 여기에서 조건 추가 가능(bBusy 등)
    Stats->AddAttackGauge(AttackRegenPerTick);
}

void ARhythmEnemyCharacter::StartEnemyCinematicZoom(float TargetFOV)
{
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (PC->PlayerCameraManager)
        {
            SavedFOV = PC->PlayerCameraManager->GetFOVAngle();
            PC->PlayerCameraManager->SetFOV(TargetFOV);
        }
    }
}
void ARhythmEnemyCharacter::EndEnemyCinematicZoom()
{
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (PC->PlayerCameraManager && SavedFOV > 0.f)
        {
            PC->PlayerCameraManager->SetFOV(SavedFOV);
        }
    }
}

float ARhythmEnemyCharacter::GetDistToPlayer() const
{
    ACharacter* P = TargetPlayer.Get();
    if (!P) P = UGameplayStatics::GetPlayerCharacter(const_cast<ARhythmEnemyCharacter*>(this), 0);
    if (!P) return TNumericLimits<float>::Max();

    const FVector My = GetCapsuleComponent()
        ? GetCapsuleComponent()->GetComponentLocation()
        : Super::GetActorLocation();

    const FVector Tg = P->GetCapsuleComponent()
        ? P->GetCapsuleComponent()->GetComponentLocation()
        : P->GetActorLocation();

    // 사이드 스크롤이면 축 하나만 쓰고 싶을 때:
    // return FMath::Abs(My.X - Tg.X);

    return FVector::Dist(My, Tg);
}

void ARhythmEnemyCharacter::StartAttackCooldown()
{
    bAttackOnCooldown = true;
    GetWorldTimerManager().SetTimer(
        AttackCooldownHandle,
        [this]() { bAttackOnCooldown = false; },
        AttackCooldown,
        false);
}

void ARhythmEnemyCharacter::ReceiveHit(float /*Damage*/, AActor* /*From*/)
{
    if (bSuperArmor || bDeathHandled) return;

    if (bBusy)
    {
        if (UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
        {
            Anim->Montage_Stop(0.1f);
        }
        EndHitbox();
        bBusy = false;
    }

    if (HitReactMontage && GetMesh() && GetMesh()->GetAnimInstance())
    {
        bBusy = true;
        UAnimInstance* Anim = GetMesh()->GetAnimInstance();
        Anim->Montage_Play(HitReactMontage, 1.f);

        FOnMontageEnded End;
        End.BindLambda([this](UAnimMontage*, bool) { bBusy = false; });
        Anim->Montage_SetEndDelegate(End, HitReactMontage);
    }
	PlayHitFX(this);
}

void ARhythmEnemyCharacter::HandleHealthChanged(float NewHP, float /*MaxHP*/)
{
    if (NewHP <= 0.f)
    {
        DieAndNotify();
    }
}

void ARhythmEnemyCharacter::DieAndNotify()
{
    if (bDeathHandled) return;
    bDeathHandled = true;
    bIsDead = true;                 // 공격/AI 전역 가드용

    SetCanBeDamaged(false);
    EndHitbox();
    bBusy = false;

    Tags.AddUnique("Dead");         // ★ FreezeCombatWorld에서 몽타주 예외

    // 이동/충돌/AI 완전 정지
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->StopMovementImmediately();
        Move->DisableMovement();
        Move->Velocity = FVector::ZeroVector;
        Move->SetMovementMode(MOVE_None);
    }
    if (UCapsuleComponent* Cap = GetCapsuleComponent())
    {
        Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Cap->SetGenerateOverlapEvents(false);
    }
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        if (UBrainComponent* Brain = AIC->GetBrainComponent())
            Brain->StopLogic(TEXT("Dead"));
        AIC->StopMovement();
        // AIC->UnPossess(); // 필요하면 해제
    }

    float PlayLen = 0.f;

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        MeshComp->SetSimulatePhysics(false);
        MeshComp->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
        MeshComp->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

        if (UAnimInstance* Anim = MeshComp->GetAnimInstance())
        {
            Anim->StopAllMontages(0.0f);

            if (DeathMontage)
            {
                PlayLen = Anim->Montage_Play(DeathMontage, 1.f);

                FOnMontageBlendingOutStarted BlendOut;
                BlendOut.BindUObject(this, &ThisClass::OnDeathMontageEnded);
                Anim->Montage_SetBlendingOutDelegate(BlendOut, DeathMontage);
            }
            else
            {
                EnterCorpseState();
            }
        }
        else
        {
            EnterCorpseState();
        }
    }
    else
    {
        EnterCorpseState();
    }

    // 공격 리젠 타이머 끄기
    GetWorldTimerManager().ClearTimer(AttackRegenHandle);

    // 마지막 적인지 체크 후 승리 신호 예약
    bool bPlayerWillWin = false;
    if (auto* GM = GetWorld()->GetAuthGameMode<ARhythmFighterGameMode>())
    {
        GM->RegisterDeath(this);
        bPlayerWillWin = (GM->GetAliveEnemies() == 0);
    }

    const float Delay = FMath::Max(PlayLen, 0.9f);
    GetWorldTimerManager().SetTimer(
        DeathEndHandle,
        FTimerDelegate::CreateWeakLambda(this, [this, bPlayerWillWin]()
            {
                if (auto* GM = GetWorld()->GetAuthGameMode<ARhythmFighterGameMode>())
                {
                    GM->EndBattle(/*bPlayerWon=*/bPlayerWillWin);
                }
            }),
        Delay, false);
}

void ARhythmEnemyCharacter::NotifyBattleEndAsWinner()
{
    if (ARhythmFighterGameMode* GM = Cast<ARhythmFighterGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        GM->EndBattle(true); // ← 플레이어 승리
    }
}

void ARhythmEnemyCharacter::OnSelfDied()
{
    if (auto* GM = GetWorld()->GetAuthGameMode<ARhythmFighterGameMode>())
    {
        GM->OnAnyActorDied(this);
    }
    // 사망 연출(몽타주)은 기존 구현 유지 가능
}

void ARhythmEnemyCharacter::OnDeathMontageEnded(UAnimMontage* /*Montage*/, bool /*bInterrupted*/)
{
    EnterCorpseState();
}

void ARhythmEnemyCharacter::EnterCorpseState()
{
    if (bRagdollOnDeath)
    {
        if (USkeletalMeshComponent* MeshComp = GetMesh())
        {
            MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
            MeshComp->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
            MeshComp->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
            MeshComp->SetSimulatePhysics(true);
        }
        return;
    }

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        if (UAnimInstance* Anim = MeshComp->GetAnimInstance())
        {
            // 진행 중인 몽타주 전부 끊고
            Anim->StopAllMontages(0.0f);
        }

        // 마지막 프레임을 그대로 유지
        MeshComp->bPauseAnims = true;

        // ★ 적도 마찬가지로 애님 틱을 꺼서 다시 일어나지 못하게 한다
        MeshComp->SetComponentTickEnabled(false);
    }
}

void ARhythmEnemyCharacter::StartAttackFX()
{
    // 이펙트 에셋이 없으면 아무 것도 하지 않음
    if (!AttackFX)
        return;

    // 이미 나가고 있으면 중복 생성 방지
    if (ActiveAttackFX && ActiveAttackFX->IsActive())
        return;

    USkeletalMeshComponent* MeshComp = GetMesh();
    if (!MeshComp)
        return;

    // 팔(무기) 소켓에 붙여서 이펙트 생성
    ActiveAttackFX = UGameplayStatics::SpawnEmitterAttached(
        AttackFX,               // UNiagaraSystem* AttackFX;
        MeshComp,               // Parent
        AttackFXSocket,     // 소켓 이름
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        EAttachLocation::SnapToTarget,
        true                    // bAutoDestroy
    );

    // 공격 휘두르는 효과음
    if (AttackSwingSFX)
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            AttackSwingSFX,
            MeshComp->GetSocketLocation(HitFXSocket)
        );
    }

    // 공격할 때 내는 음성
    if (AttackVoiceSFX)
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            AttackVoiceSFX,
            GetActorLocation()
        );
    }
}

void ARhythmEnemyCharacter::StopAttackFX()
{
    if (ActiveAttackFX)
    {
        ActiveAttackFX->Deactivate();
        ActiveAttackFX = nullptr;
    }
}

void ARhythmEnemyCharacter::PlayHitFX(AActor* Victim)
{
    if (!HitFX || !GetMesh())
        return;

    const FVector Loc = GetMesh()->GetSocketLocation(HitFXSocket);
    const FRotator Rot = GetActorRotation();

    // 피격 파티클
    UGameplayStatics::SpawnEmitterAtLocation(
        GetWorld(),
        HitFX,
        Loc,
        Rot
    );

    // 타격 사운드
    if (HitImpactSFX)
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            HitImpactSFX,
            GetActorLocation()
        );
    }

    // "윽!" 같은 피격 음성
    if (HitVoiceSFX)
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            HitVoiceSFX,
            GetActorLocation()
        );
    }
}

void ARhythmEnemyCharacter::PlayNextComboHit()
{
    if (!Stats || bIsDead)
    {
        EndComboAttack();
        return;
    }

    if (RemainingComboHits <= 0)
    {
        EndComboAttack();
        return;
    }

    // 한 타당 게이지 소모
    if (!Stats->ConsumeAttackGauge(AttackCost))
    {
        // 이론상 RequiredGauge 체크로 여기 안 들어오지만,
        // 혹시 모를 상황 대비해서 방어 코드
        EndComboAttack();
        return;
    }

    if (AttackMontages.Num() == 0 || !GetMesh())
    {
        EndComboAttack();
        return;
    }

    UAnimInstance* Anim = GetMesh()->GetAnimInstance();
    if (!Anim)
    {
        EndComboAttack();
        return;
    }

    // 랜덤 몽타주 선택
    const int32 Index = FMath::RandRange(0, AttackMontages.Num() - 1);
    UAnimMontage* MontageToPlay = AttackMontages[Index];
    if (!MontageToPlay)
    {
        EndComboAttack();
        return;
    }

    const float Rate = (AttackPlayRate > 0.f) ? AttackPlayRate : 1.f;
    Anim->Montage_Play(MontageToPlay, Rate);

    // 이 타가 끝나면 다음 타로 넘어가거나 종료
    FOnMontageEnded End;
    End.BindLambda([this](UAnimMontage* /*Montage*/, bool bInterrupted)
        {
            if (bInterrupted || bIsDead)
            {
                EndComboAttack();
                return;
            }

            RemainingComboHits--;

            if (RemainingComboHits > 0)
            {
                // 다음 타 재생
                PlayNextComboHit();
            }
            else
            {
                EndComboAttack();
            }
        });

    Anim->Montage_SetEndDelegate(End, MontageToPlay);
}

void ARhythmEnemyCharacter::EndComboAttack()
{
    bComboAttacking = false;
    bBusy = false;
    RemainingComboHits = 0;

    StartAttackCooldown();   // 콤보 전체가 끝났을 때 한 번만 쿨타임 시작
}