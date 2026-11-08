#include "CombatStatsComponent.h"

UCombatStatsComponent::UCombatStatsComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    // Base 값 초기화(에디터에서 수정해도 됨)
    BaseMaxHP = MaxHP;     // 시작값을 기준으로
    BaseAttack = 0.f;
    BaseDefense = 0.f;
    BaseDamageMult = 1.f;
    BaseGaugeGainMult = 1.f;
    BaseSuperGainMult = 1.f;

    HP = MaxHP;
}

void UCombatStatsComponent::InitStats(float InMaxHP)
{
    MaxHP = InMaxHP;
    HP = MaxHP;
    OnHealthChanged.Broadcast(HP, MaxHP);
}

void UCombatStatsComponent::ApplyDamage(float Amount)
{
    if (Amount <= 0.f || HP <= 0.f) return;
    HP = FMath::Clamp(HP - Amount, 0.f, MaxHP);
    OnHealthChanged.Broadcast(HP, MaxHP);
    if (HP <= 0.f) OnDeath.Broadcast();
}

void UCombatStatsComponent::AddAttackGauge(float Delta)
{
    AttackGauge = FMath::Clamp(AttackGauge + Delta, 0.f, 1.f);
}
void UCombatStatsComponent::AddSuperGauge(float Delta)
{
    SuperGauge = FMath::Clamp(SuperGauge + Delta, 0.f, 1.f);
}
bool UCombatStatsComponent::ConsumeAttackGauge(float Cost)
{
    if (AttackGauge >= Cost) { AttackGauge -= Cost; return true; }
    return false;
}
bool UCombatStatsComponent::ConsumeSuperGauge(float Cost)
{
    if (SuperGauge >= Cost) { SuperGauge -= Cost; return true; }
    return false;
}

void UCombatStatsComponent::ModifyMaxHP(float DeltaMaxHP)
{
    const float OldMax = MaxHP;
    const float OldHP = HP;

    MaxHP = FMath::Max(1.f, MaxHP + DeltaMaxHP); // 최소 1
    // 현재 체력 비율 유지
    const float Ratio = (OldMax > 0.f) ? (OldHP / OldMax) : 1.f;
    HP = FMath::Clamp(MaxHP * Ratio, 0.f, MaxHP);

    OnHealthChanged.Broadcast(HP, MaxHP);
}

void UCombatStatsComponent::RecalculateAll()
{
    // 1) MaxHP: Base + Add
    const float NewMaxHP = FMath::Max(1.f, BaseMaxHP + ExternalMods.MaxHP_Add);
    MaxHP = NewMaxHP;

    // 2) Add 계열
    AttackBonus = BaseAttack + ExternalMods.Attack_Add;
    DefenseBonus = BaseDefense + ExternalMods.Defense_Add;

    // 3) Mult 계열
    DamageMultiplier = BaseDamageMult * ExternalMods.Damage_Mult;
    GaugeGainMultiplier = BaseGaugeGainMult * ExternalMods.GaugeGain_Mult;
    SuperGainMultiplier = BaseSuperGainMult * ExternalMods.SuperGain_Mult;

    // 4) HP 범위 보정 & 브로드캐스트
    HP = FMath::Clamp(HP, 0.f, MaxHP);
    OnHealthChanged.Broadcast(HP, MaxHP);
}

float UCombatStatsComponent::GetOutgoingDamage(float BaseDamage) const
{
    const float Add = AttackBonus;          // 외부에서 장비로 들어온 +값
    const float Mult = DamageMultiplier;     // 외부에서 장비로 들어온 ×값(기본 1)
    return FMath::Max(0.f, (BaseDamage + Add) * Mult);
}

float UCombatStatsComponent::GetIncomingAfterDefense(float InDamage) const
{
    // 방어 규칙은 프로젝트에 맞게. 예: 선형감쇠 or 간단한 나눗셈
    //  선형 감쇠: InDamage - DefenseBonus(최소 0)
    // return FMath::Max(0.f, InDamage - DefenseBonus);

    // 배수 감쇠: InDamage / (1 + DefenseBonus/100)
    const float denom = 1.f + FMath::Max(0.f, DefenseBonus) / 100.f;
    return FMath::Max(0.f, InDamage / denom);
}