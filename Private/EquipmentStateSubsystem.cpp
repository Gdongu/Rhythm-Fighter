#include "EquipmentStateSubsystem.h"
#include "RhythmFighterCharacter.h"
#include "InventoryComponent.h"
#include "CombatStatsComponent.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"

void UEquipmentStateSubsystem::PullFromInventory(UInventoryComponent* Inv)
{
    if (!Inv) return;
    // 깊은 복사(애셋/객체 레퍼런스 유지)
    Equipped = Inv->GetEquippedMap();
    Aggregated = Inv->GetAggregatedModifiers();
}

void UEquipmentStateSubsystem::ApplyTo(UCombatStatsComponent* Stats) const
{
    if (!Stats) return;

    // MaxHP는 증분(+)로, 배수는 1이 기본값
    if (Aggregated.MaxHP_Add != 0.f)
        Stats->ModifyMaxHP(Aggregated.MaxHP_Add);

    Stats->SetAttackBonus(Aggregated.Attack_Add);
    Stats->SetDefenseBonus(Aggregated.Defense_Add);
    Stats->SetDamageMultiplier(Aggregated.Damage_Mult);
    Stats->SetGaugeGainMultiplier(Aggregated.GaugeGain_Mult);
    Stats->SetSuperGainMultiplier(Aggregated.SuperGain_Mult);
}

void UEquipmentStateSubsystem::SetLastTransformForLevel(FName LevelName, const FTransform& T)
{
    LastTransformByLevel.Add(LevelName, T);
}
bool UEquipmentStateSubsystem::GetLastTransformForLevel(FName LevelName, FTransform& OutT) const
{
    if (const FTransform* Found = LastTransformByLevel.Find(LevelName))
    {
        OutT = *Found; return true;
    }
    return false;
}

void UEquipmentStateSubsystem::CaptureFromCharacter(ARhythmFighterCharacter* C)
{
    if (!C) return;

    // 1) 인벤토리 스냅샷
    InventorySnapshot.Reset();

    if (UInventoryComponent* Inv = C->FindComponentByClass<UInventoryComponent>())
    {
        // 소유 아이템
        for (const auto& It : Inv->GetAllItems())
        {
            if (!It.ItemData) continue;
            FStoredItemRef Ref;
            Ref.ItemPath = FSoftObjectPath(It.ItemData->GetPathName());
            Ref.Count = It.Count;
            Ref.bEquipped = false;
            Ref.Slot = EEquipSlot::None;
            InventorySnapshot.Add(Ref);
        }

        // 장착 맵
        for (const auto& P : Inv->GetEquippedMap())
        {
            if (!P.Value) continue;

            const FSoftObjectPath Path(P.Value->GetPathName());
            FStoredItemRef* Found = InventorySnapshot.FindByPredicate(
                [&](const FStoredItemRef& X) { return X.ItemPath == Path; });

            if (!Found)
            {
                FStoredItemRef Ref;
                Ref.ItemPath = Path;
                Ref.Count = 1;
                Ref.bEquipped = true;
                Ref.Slot = P.Key;
                InventorySnapshot.Add(Ref);
            }
            else
            {
                Found->bEquipped = true;
                Found->Slot = P.Key;
            }
        }

        // 필요하다면, 현재 집계값도 서브시스템에 저장
        Aggregated = Inv->GetAggregatedModifiers();
        Equipped = Inv->GetEquippedMap();
    }
}

void UEquipmentStateSubsystem::ApplyToCharacter(ARhythmFighterCharacter* C)
{
    if (!C) return;

    if (UInventoryComponent* Inv = C->FindComponentByClass<UInventoryComponent>())
    {
        Inv->ClearAll();

        FStreamableManager& SM = UAssetManager::GetStreamableManager();

        for (const FStoredItemRef& Ref : InventorySnapshot)
        {
            if (!Ref.ItemPath.IsValid()) continue;

            UObject* Obj = SM.LoadSynchronous(Ref.ItemPath, true);
            UItemData* Data = Cast<UItemData>(Obj);
            if (!Data) continue;

            Inv->AddItem(Data, Ref.Count);

            if (Ref.bEquipped && Ref.Slot != EEquipSlot::None)
            {
                Inv->Equip(Ref.Slot, Data);
            }
        }

        // ★ 이 두 줄은 그대로 유지 (InventoryComponent 멤버)
        Inv->RebuildAggregation();
        Inv->ApplyToCombatStats(true);
    }

    // ★ 여기의 ApplyToCombatStats(/*bLog=*/true); 는 삭제
}