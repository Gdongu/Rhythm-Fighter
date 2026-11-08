#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Sound/SoundBase.h"
#include "RhythmEnemyCharacter.generated.h"

class UCombatStatsComponent;
class UBoxComponent;
class UAnimMontage;
class ARhythmManager;
class ACharacter;
class AActor;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAnimMontage;
class UParticleSystem;
class UParticleSystemComponent;

UCLASS()
class RHYTHMFIGHTER_API ARhythmEnemyCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    ARhythmEnemyCharacter();

    UPROPERTY(EditAnywhere, Category = "Enemy")
    float MoveSpeed = 450.f;

    UPROPERTY(EditAnywhere, Category = "Enemy")
    float AttackRange = 100.f;

    UPROPERTY(EditAnywhere, Category = "Enemy|Gauge")
    float AttackCost = 0.25f;

    UPROPERTY(EditAnywhere, Category = "Enemy")
    float AttackCooldown = 1.2f;

    FTimerHandle AttackCooldownHandle;
    bool bAttackOnCooldown = false;

    // 기본 공격 몽타주들 (랜덤 선택)
    UPROPERTY(EditAnywhere, Category = "Enemy|Attack")
    TArray<UAnimMontage*> AttackMontages;

    // 기본 공격 재생 속도 (1.0 = 원래, 1.5면 1.5배 빠르게)
    UPROPERTY(EditAnywhere, Category = "Enemy|Attack")
    float AttackPlayRate = 1.5f;

    // 한 번 TryAttack() 들어가면 몇 타까지 콤보할지
    UPROPERTY(EditAnywhere, Category = "Enemy|Attack")
    int32 ComboCount = 4;

    // 현재 타겟(플레이어)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
    AActor* Target = nullptr;

    // 입력 잠금(공격 중 등)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
    bool bBusy = false;

    // 콤보 공격 중인지 여부
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Attack")
    bool bComboAttacking = false;

    int32 RemainingComboHits = 0;   // 남은 타수
    void PlayNextComboHit();        // 내부에서 다음 타 플레이
    void EndComboAttack();          // 콤보 종료/쿨타임

    // 이동/접근 파라미터
    UPROPERTY(EditAnywhere, Category = "Enemy|Move")
    float ApproachWeight = 0.6f;        // 전진 성분 가중치(0~1), 커질수록 빠르게 접근

    UPROPERTY(EditAnywhere, Category = "Enemy|Move")
    float StrafeWeight = 2.5f;          // 좌우 흔들림 성분(0~1), 커질수록 지그재그가 커짐

    UPROPERTY(EditAnywhere, Category = "Enemy|Move")
    float StrafeFrequency = 1.5f;        // 흔들림 빈도(Hz)

    UPROPERTY(EditAnywhere, Category = "Enemy|Move")
    float KeepDistance = 80.f;          // 너무 붙었을 때 물러서지 않도록 유지거리(사거리보다 조금 작게)

    // 접근/유지/후퇴 파라미터
    UPROPERTY(EditAnywhere, Category = "Enemy|Spacing")
    float PreferredDistance = 360.f;        // 유지하고 싶은 중심 거리

    UPROPERTY(EditAnywhere, Category = "Enemy|Spacing")
    float RetreatThreshold = 70.f;        // 이보다 가까우면 뒤로 빠짐

    UPROPERTY(EditAnywhere, Category = "Enemy|Move")
    float AI_ApproachWeight = 0.9f;         // 멀 때 전진 기여(0~1)

    UPROPERTY(EditAnywhere, Category = "Enemy|Move")
    float AI_StrafeWeight = 0.20f;        // 좌우 흔들림 크기(0~1)

    UPROPERTY(EditAnywhere, Category = "Enemy|Move")
    float AI_StrafeFrequency = 2.5f;         // 흔들림 빈도(Hz)

    // 라운드 시작 체크용 (매니저 캐시)
    UPROPERTY()
    class ARhythmManager* CachedManager = nullptr;

    virtual void Tick(float DeltaSeconds) override;

    void SetTarget(AActor* InTarget) { Target = InTarget; }
    bool IsInAttackRange() const;
    bool TryAttack();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    UCombatStatsComponent* Stats = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
    float HPPerLevel = 20.f;     // 레벨당 HP 증가량

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
    float AttackPerLevel = 2.f;  // 레벨당 공격력 증가량

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    class UBoxComponent* Hitbox = nullptr;

    // 현재 공격창 활성/피격 중복 방지
    bool bHitboxActive = false;
    TSet<TWeakObjectPtr<AActor>> AlreadyHit;

    UFUNCTION()
    void OnHitboxOverlap(UPrimitiveComponent* Overlapped, AActor* Other,
        UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep,
        const FHitResult& Sweep);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void StartHitbox(FName AttachSocket, FVector BoxExtent, float Damage);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void EndHitbox();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void StartHitboxEx(FName AttachSocket, FVector BoxExtent, float Damage, FVector LocalOffset, FRotator LocalRotation);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ProcessHitbox();

    float CurrentHitDamage = 10.f;

    UPROPERTY(EditAnywhere, Category = "Combat")
    FName TeamTag = "Enemy";

    void StartAttackCooldown();

    bool IsEnemy(const AActor* Other) const
    {
        if (!Other) return false;
        // 팀 태그가 설정되어 있지 않다면 기본적으로 적으로 간주
        const bool bMeHasTeam = ActorHasTag("Player") || ActorHasTag("Enemy");
        const bool bOtherHasTeam = Other->ActorHasTag("Player") || Other->ActorHasTag("Enemy");
        if (!bMeHasTeam || !bOtherHasTeam) return true;

        const bool bMeIsPlayer = ActorHasTag("Player");
        const bool bOtherIsEnemy = Other->ActorHasTag("Enemy");
        return (bMeIsPlayer && bOtherIsEnemy) || (!bMeIsPlayer && Other->ActorHasTag("Player"));
    }

    UPROPERTY(EditAnywhere, Category = "Enemy|Finisher")
    UAnimMontage* FinisherMontage = nullptr;

    UPROPERTY(EditAnywhere, Category = "Enemy|Finisher")
    float FinisherCost = 1.0f;  // 슈퍼게이지 소모량(0~1)

    UPROPERTY(EditAnywhere, Category = "Enemy|Finisher")
    float FinisherMinDistance = 220.f; // 너무 가까우면 안 쓰게(선택)

    // 내부 쿨다운(연속 난사 방지)
    FTimerHandle FinisherLockHandle;
    bool bFinisherLocked = false;
    void UnlockFinisher() { bFinisherLocked = false; }

    // 카메라 줌
    float SavedFOV = 0.f;
    FTimerHandle ZoomHandle;
    void StartEnemyCinematicZoom(float TargetFOV = 60.f);
    void EndEnemyCinematicZoom();

    // 리젠 세팅
    UPROPERTY(EditAnywhere, Category = "Enemy|Gauge")
    float AttackRegenPerTick = 0.04f;   // 1초마다 4% 증가(원하면 조절)

    UPROPERTY(EditAnywhere, Category = "Enemy|Gauge")
    float AttackRegenInterval = 1.0f;   // 1초 간격

    FTimerHandle AttackRegenHandle;

    UFUNCTION()
    void RegenAttackGauge();

    // --- Hit React / SuperArmor ---
    UPROPERTY(EditAnywhere, Category = "Enemy|HitReact")
    UAnimMontage* HitReactMontage = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|HitReact")
    bool bSuperArmor = false;

    UFUNCTION(BlueprintCallable, Category = "Enemy|HitReact")
    void SetSuperArmor(bool bEnable) { bSuperArmor = bEnable; }

    void ReceiveHit(float Damage, AActor* From);
   
    UPROPERTY(EditDefaultsOnly, Category = "Enemy|Death")
    UAnimMontage* DeathMontage = nullptr;

    UFUNCTION()
    void HandleHealthChanged(float NewHP, float MaxHP);

    void DieAndNotify();

    UFUNCTION() void OnSelfDied();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|State")
    bool bIsDead = false;

    UPROPERTY(EditDefaultsOnly, Category = "Enemy|Death")
    bool bRagdollOnDeath = true;

    UFUNCTION()
    void OnDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    UFUNCTION()
    void EnterCorpseState(); // 몽타주 끝나면 시체 상태로 전환

    UPROPERTY(EditAnywhere, Category = "FX|Attack")
    UParticleSystem* AttackFX = nullptr;

    UPROPERTY(EditAnywhere, Category = "FX|Attack")
    FName AttackFXSocket = "hand_r";

    UPROPERTY(EditAnywhere, Category = "FX|Hit")
    UParticleSystem* HitFX = nullptr;

    UPROPERTY(EditAnywhere, Category = "FX|Hit")
    FName HitFXSocket = "spine_02";

    UPROPERTY(EditAnywhere, Category = "SFX|Attack")
    USoundBase* AttackSwingSFX = nullptr;   // 휘두르는 소리

    UPROPERTY(EditAnywhere, Category = "SFX|Attack")
    USoundBase* AttackVoiceSFX = nullptr;   // "얍"

    UPROPERTY(EditAnywhere, Category = "SFX|Hit")
    USoundBase* HitImpactSFX = nullptr;     // 맞는 타격음

    UPROPERTY(EditAnywhere, Category = "SFX|Hit")
    USoundBase* HitVoiceSFX = nullptr;      // "윽"

    // 공격 FX를 끄기 위해 핸들 저장용
    UPROPERTY()
    UParticleSystemComponent* ActiveAttackFX = nullptr;

    void StartAttackFX();
    void StopAttackFX();
    UFUNCTION(BlueprintCallable)
    void PlayHitFX(AActor* Victim);

protected:
    virtual void BeginPlay() override;
    /** 추적할 플레이어 (없으면 자동으로 0번 플레이어를 찾음) */
    UPROPERTY()
    TWeakObjectPtr<ACharacter> TargetPlayer;

    /** 현재 플레이어까지 거리(없으면 큰 값 반환) */
    UFUNCTION(BlueprintPure, Category = "Enemy|AI")
    float GetDistToPlayer() const;

private:
    FTimerHandle DeathEndHandle;
    bool bDeathHandled = false;
    void NotifyBattleEndAsWinner();
    float CooldownTimer = 0.f;
    void EndAttack(); // 몽타주 끝 콜백
    float LockedY = 0.f;

    void TryApplyHit(AActor* Other);
};
