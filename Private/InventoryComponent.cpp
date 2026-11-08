#include "InventoryComponent.h"
#include "GameFramework/Actor.h"
#include "CombatStatsComponent.h"
#include "EquipmentStateSubsystem.h"
#include "Engine/World.h"
#include <cfloat>

//////////////////////////////////////////////////////////////////////////
// 내부 유틸

static float SafeBoost(float Mult) { return (Mult - 1.0f); } // 1.00 기준 증가율

float UInventoryComponent::ScoreItem(const UItemData* Item, const FStatScoreWeights& W)
{
    if (!Item) return -FLT_MAX;
    const FStatModifiers& M = Item->Modifiers;

    float S = 0.f;
    S += M.MaxHP_Add * W.MaxHP_Add_W;
    S += M.Attack_Add * W.Attack_Add_W;
    S += M.Defense_Add * W.Defense_Add_W;

    S += SafeBoost(M.Damage_Mult) * W.Damage_Mult_W;
    S += SafeBoost(M.GaugeGain_Mult) * W.GaugeGain_Mult_W;
    S += SafeBoost(M.SuperGain_Mult) * W.SuperGain_Mult_W;

    return S;
}

//////////////////////////////////////////////////////////////////////////
// 기본 Add / Remove / Equip

bool UInventoryComponent::AddItem(UItemData* Item)
{
    if (!Item) return false;

    AddItem(Item, 1);         // 실제 로직은 Count 버전으로
    OnInventoryChanged.Broadcast();

    OnItemAcquired.Broadcast(Item);

    // 인벤 변경 → 업그레이드 재평가
    FStatScoreWeights W;
    ReevaluateUpgrades(W);

    return true;
}

bool UInventoryComponent::RemoveItem(UItemData* Item)
{
    if (!Item) return false;

    // 1) 개수 감소
    bool bRemovedCompletely = false;
    for (int32 i = 0; i < Items.Num(); ++i)
    {
        if (Items[i].ItemData == Item)
        {
            Items[i].Count--;
            if (Items[i].Count <= 0)
            {
                Items.RemoveAt(i);
                bRemovedCompletely = true;
            }
            break;
        }
    }

    if (!bRemovedCompletely && Items.Num() == 0)
        return false;

    // 2) 장착 중인 아이템이면 해제
    for (auto It = Equipped.CreateIterator(); It; ++It)
    {
        if (It.Value() == Item)
        {
            ApplyToCombatStats(false);
            It.RemoveCurrent();
            RebuildAggregation();
            ApplyToCombatStats(true);
            OnEquipmentChanged.Broadcast();
            break;
        }
    }

    OnInventoryChanged.Broadcast();

    FStatScoreWeights W;
    ReevaluateUpgrades(W);

    return true;
}

bool UInventoryComponent::EquipItem(UItemData* Item)
{
    if (!Item || Item->ItemType != EItemType::Equipment) return false;

    // 인벤에 없는 아이템이면 실패
    bool bOwned = false;
    for (const FOwnedItem& Own : Items)
    {
        if (Own.ItemData == Item)
        {
            bOwned = true;
            break;
        }
    }
    if (!bOwned) return false;

    UItemData*& SlotRef = Equipped.FindOrAdd(Item->EquipSlot);
    if (SlotRef == Item) return false;            // 이미 장착

    ApplyToCombatStats(false);                    // 이전 장비 효과 제거
    SlotRef = Item;

    RebuildAggregation();
    ApplyToCombatStats(true);

    OnEquipmentChanged.Broadcast();

    // 저장용 서브시스템에 동기화
    if (UWorld* World = GetWorld())
    {
        if (UEquipmentStateSubsystem* Sub = World->GetGameInstance()->GetSubsystem<UEquipmentStateSubsystem>())
        {
            Sub->PullFromInventory(this);
        }
    }

    FStatScoreWeights W;
    ReevaluateUpgrades(W);

    return true;
}

bool UInventoryComponent::Unequip(EEquipSlot Slot)
{
    UItemData* const* P = Equipped.Find(Slot);
    if (!P) return false;

    ApplyToCombatStats(false);
    Equipped.Remove(Slot);

    RebuildAggregation();
    ApplyToCombatStats(true);

    OnEquipmentChanged.Broadcast();

    if (UWorld* World = GetWorld())
    {
        if (UEquipmentStateSubsystem* Sub = World->GetGameInstance()->GetSubsystem<UEquipmentStateSubsystem>())
        {
            Sub->PullFromInventory(this);
        }
    }

    FStatScoreWeights W;
    ReevaluateUpgrades(W);

    return true;
}

UItemData* UInventoryComponent::GetEquipped(EEquipSlot Slot) const
{
    if (UItemData* const* Found = Equipped.Find(Slot))
    {
        return *Found;
    }
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
// 합산 / CombatStats 반영

void UInventoryComponent::RebuildAggregation()
{
    Aggregated = FStatModifiers{};

    for (const auto& Pair : Equipped)
    {
        if (const UItemData* Item = Pair.Value)
        {
            Aggregated.MaxHP_Add += Item->Modifiers.MaxHP_Add;
            Aggregated.Attack_Add += Item->Modifiers.Attack_Add;
            Aggregated.Defense_Add += Item->Modifiers.Defense_Add;
            Aggregated.Damage_Mult *= Item->Modifiers.Damage_Mult;
            Aggregated.GaugeGain_Mult *= Item->Modifiers.GaugeGain_Mult;
            Aggregated.SuperGain_Mult *= Item->Modifiers.SuperGain_Mult;
        }
    }
}

void UInventoryComponent::ApplyToCombatStats(bool bApply)
{
    if (AActor* Owner = GetOwner())
    {
        if (UCombatStatsComponent* Stats = Owner->FindComponentByClass<UCombatStatsComponent>())
        {
            const float Sign = bApply ? 1.f : -1.f;

            if (Aggregated.MaxHP_Add != 0.f)
            {
                Stats->ModifyMaxHP(Sign * Aggregated.MaxHP_Add);
            }

            Stats->SetDamageMultiplier(bApply ? Aggregated.Damage_Mult : 1.f);
            Stats->SetGaugeGainMultiplier(bApply ? Aggregated.GaugeGain_Mult : 1.f);
            Stats->SetSuperGainMultiplier(bApply ? Aggregated.SuperGain_Mult : 1.f);

            Stats->SetAttackBonus(bApply ? Aggregated.Attack_Add : 0.f);
            Stats->SetDefenseBonus(bApply ? Aggregated.Defense_Add : 0.f);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
// 업그레이드 추천

void UInventoryComponent::ReevaluateUpgrades(const FStatScoreWeights& W)
{
    HasUpgradeBySlot.Reset();
    SuggestedBySlot.Reset();

    // 슬롯별 현재 점수 계산
    TMap<EEquipSlot, float> CurScoreBySlot;
    for (auto& KVP : Equipped)
    {
        CurScoreBySlot.Add(KVP.Key, ScoreItem(KVP.Value, W));
    }

    // 슬롯별 최고 후보 탐색
    TMap<EEquipSlot, float> BestDeltaBySlot;
    for (const FOwnedItem& Own : Items)
    {
        UItemData* It = Own.ItemData;
        if (!It) continue;

        const EEquipSlot Slot = It->EquipSlot;
        const float Cur = CurScoreBySlot.FindRef(Slot); // 없으면 0
        const float Cand = ScoreItem(It, W);
        const float Delta = Cand - Cur;

        float& BestDelta = BestDeltaBySlot.FindOrAdd(Slot, -FLT_MAX);
        if (Delta > 0.f && Delta > BestDelta)
        {
            BestDelta = Delta;
            HasUpgradeBySlot.Add(Slot, true);
            SuggestedBySlot.Add(Slot, It);
        }
    }

    // 어느 후보도 없었던 슬롯은 false로 명시
    for (int32 SlotIdx = 0; SlotIdx <= static_cast<int32>(EEquipSlot::Accessory); ++SlotIdx)
    {
        const EEquipSlot Slot = static_cast<EEquipSlot>(SlotIdx);
        HasUpgradeBySlot.FindOrAdd(Slot, false);
    }

    OnInventoryChanged.Broadcast();
}

bool UInventoryComponent::HasUpgrade(EEquipSlot Slot) const
{
    if (const bool* b = HasUpgradeBySlot.Find(Slot)) return *b;
    return false;
}

UItemData* UInventoryComponent::GetSuggested(EEquipSlot Slot) const
{
    if (const TObjectPtr<UItemData>* P = SuggestedBySlot.Find(Slot))
        return P->Get();
    return nullptr;
}

bool UInventoryComponent::IsSuggestedItem(const UItemData* Item) const
{
    if (!Item) return false;
    if (const TObjectPtr<UItemData>* P = SuggestedBySlot.Find(Item->EquipSlot))
        return P->Get() == Item;
    return false;
}

//////////////////////////////////////////////////////////////////////////
// 저장/복원용 로우레벨 API

void UInventoryComponent::ClearAll()
{
    Items.Reset();
    Equipped.Reset();
    Aggregated = FStatModifiers{};
}

void UInventoryComponent::AddItem(UItemData* Item, int32 Count)
{
    if (!Item || Count <= 0) return;

    if (FOwnedItem* Found = Items.FindByPredicate(
        [&](const FOwnedItem& X) { return X.ItemData == Item; }))
    {
        Found->Count += Count;
    }
    else
    {
        FOwnedItem New;
        New.ItemData = Item;
        New.Count = Count;
        Items.Add(New);
    }
}

bool UInventoryComponent::Equip(EEquipSlot Slot, UItemData* Item)
{
    if (!Item || Slot == EEquipSlot::None) return false;
    Equipped.FindOrAdd(Slot) = Item;
    return true;
}

void UInventoryComponent::GetItemDataList(TArray<UItemData*>& OutItems) const
{
    OutItems.Reset();

    for (const FOwnedItem& Owned : Items)
    {
        if (Owned.ItemData)
        {
            OutItems.Add(Owned.ItemData);  // UI는 UItemData만 쓰면 됨
        }
    }
}