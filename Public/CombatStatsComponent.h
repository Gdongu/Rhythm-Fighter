#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemData.h"
#include "CombatStatsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, NewHP, float, MaxHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class UCombatStatsComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCombatStatsComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HP")
    float MaxHP = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HP")
    float HP = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge")
    float AttackGauge = 0.f;     // 0..1

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge")
    float SuperGauge = 0.f;      // 0..1

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge")
    float AttackGainOnHit = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge")
    float SuperGainOnHit = 0.08f;

    UPROPERTY(BlueprintAssignable)
    FOnHealthChanged OnHealthChanged;

    UPROPERTY(BlueprintAssignable)
    FOnDeath OnDeath;

    UFUNCTION(BlueprintCallable)
    void InitStats(float InMaxHP);

    UFUNCTION(BlueprintCallable)
    void ApplyDamage(float Amount);

    UFUNCTION(BlueprintCallable)
    void AddAttackGauge(float Delta);

    UFUNCTION(BlueprintCallable)
    void AddSuperGauge(float Delta);

    UFUNCTION(BlueprintCallable)
    bool ConsumeAttackGauge(float Cost);

    UFUNCTION(BlueprintCallable)
    bool ConsumeSuperGauge(float Cost);

    UFUNCTION(BlueprintPure)
    bool IsAlive() const { return HP > 0.f; }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equip|Add")
    float AttackBonus = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equip|Add")
    float DefenseBonus = 0.f;

    // 곱연산(*) 계열 (1이 기본값)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equip|Mult")
    float DamageMultiplier = 1.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equip|Mult")
    float GaugeGainMultiplier = 1.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equip|Mult")
    float SuperGainMultiplier = 1.f;

    // MaxHP 보정은 Add형으로 처리하고 현재 HP 비율 유지하도록
    UFUNCTION(BlueprintCallable, Category = "Equip|Apply")
    void ModifyMaxHP(float DeltaMaxHP);

    UFUNCTION(BlueprintCallable, Category = "Equip|Apply")
    void SetAttackBonus(float InBonus) { AttackBonus = InBonus; }

    UFUNCTION(BlueprintCallable, Category = "Equip|Apply")
    void SetDefenseBonus(float InBonus) { DefenseBonus = InBonus; }

    UFUNCTION(BlueprintCallable, Category = "Equip|Apply")
    void SetDamageMultiplier(float InMult) { DamageMultiplier = FMath::Max(0.f, InMult); }

    UFUNCTION(BlueprintCallable, Category = "Equip|Apply")
    void SetGaugeGainMultiplier(float InMult) { GaugeGainMultiplier = FMath::Max(0.f, InMult); }

    UFUNCTION(BlueprintCallable, Category = "Equip|Apply")
    void SetSuperGainMultiplier(float InMult) { SuperGainMultiplier = FMath::Max(0.f, InMult); }

    // 외부(장비 등) 보정치
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
    FStatModifiers ExternalMods;

public:
    UFUNCTION(BlueprintCallable) void SetExternalModifiers(const FStatModifiers& Mods)
    {
        ExternalMods = Mods; RecalculateAll();
    }
    UFUNCTION(BlueprintCallable) void RecalculateAll();

    // --- 보정된 데미지 계산 ---
// 가하는 쪽: (기본공격력 + 공격보너스) * 피해배수
    UFUNCTION(BlueprintPure, Category = "Damage")
    float GetOutgoingDamage(float BaseDamage) const;

    // 받는 쪽: (들어온 데미지) * 방어감쇠(원하는 규칙대로)
    UFUNCTION(BlueprintPure, Category = "Damage")
    float GetIncomingAfterDefense(float InDamage) const;

private:
    // -------- Base(기본값) 보관: RecalculateAll에서 기준으로 사용 --------
    UPROPERTY(EditAnywhere, Category = "Base")
    float BaseMaxHP = 100.f;

    UPROPERTY(EditAnywhere, Category = "Base")
    float BaseAttack = 0.f;

    UPROPERTY(EditAnywhere, Category = "Base")
    float BaseDefense = 0.f;

    UPROPERTY(EditAnywhere, Category = "Base")
    float BaseDamageMult = 1.f;

    UPROPERTY(EditAnywhere, Category = "Base")
    float BaseGaugeGainMult = 1.f;

    UPROPERTY(EditAnywhere, Category = "Base")
    float BaseSuperGainMult = 1.f;
};
