#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemData.h"
#include "InventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEquipmentChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemAcquired, UItemData*, Item);

// 개수까지 포함한 소지 아이템 정보
USTRUCT(BlueprintType)
struct FOwnedItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    UItemData* ItemData = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Count = 1;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RHYTHMFIGHTER_API UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /** --- 저장/복원용 읽기 전용 게터 --- */
    const TArray<FOwnedItem>& GetAllItems() const { return Items; }
    const TMap<EEquipSlot, UItemData*>& GetEquippedMap() const { return Equipped; }

    /** --- 저장/복원용 로우레벨 API --- */
    void ClearAll();                           // 전부 비우기
    void AddItem(UItemData* Item, int32 Count);
    bool Equip(EEquipSlot Slot, UItemData* Item);

    /** 합산 수치 다시 계산 + CombatStats에 반영 */
    void RebuildAggregation();
    void ApplyToCombatStats(bool bApply);

    UFUNCTION(BlueprintPure, Category = "Inventory")
    void GetItemDataList(TArray<UItemData*>& OutItems) const;

public:
    /** 인벤토리/장비 변경 델리게이트 (UI에서 바인딩) */
    UPROPERTY(BlueprintAssignable) FOnInventoryChanged OnInventoryChanged;
    UPROPERTY(BlueprintAssignable) FOnEquipmentChanged OnEquipmentChanged;

    UPROPERTY(BlueprintAssignable) FOnItemAcquired OnItemAcquired;
    /** 블루프린트에서 쓰기 쉬운 기본 기능들 */

    // 한 개 줍기
    UFUNCTION(BlueprintCallable)
    bool AddItem(UItemData* Item);

    // 한 개 버리기 / 소비
    UFUNCTION(BlueprintCallable)
    bool RemoveItem(UItemData* Item);

    // 아이템 장착 (Item 안에 EquipSlot 사용)
    UFUNCTION(BlueprintCallable)
    bool EquipItem(UItemData* Item);

    // 슬롯 기준 해제
    UFUNCTION(BlueprintCallable)
    bool Unequip(EEquipSlot Slot);

    // 슬롯에 현재 뭐가 장착되어 있는지
    UFUNCTION(BlueprintPure)
    UItemData* GetEquipped(EEquipSlot Slot) const;

    // 현재 합산된 스탯 보정
    UFUNCTION(BlueprintPure)
    FStatModifiers GetAggregatedModifiers() const { return Aggregated; }

    /** 더 좋은 장비 추천 기능 관련 */

    // 이 슬롯에 업그레이드 후보가 있는가?
    UFUNCTION(BlueprintPure)
    bool HasUpgrade(EEquipSlot Slot) const;

    // 이 슬롯에 추천하는 아이템
    UFUNCTION(BlueprintPure)
    UItemData* GetSuggested(EEquipSlot Slot) const;

    // 이 아이템이 “추천” 아이템인가?
    UFUNCTION(BlueprintPure)
    bool IsSuggestedItem(const UItemData* Item) const;

public:
    /** 현재 장비 전체 합산 보정값 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FStatModifiers Aggregated;

private:
    /** 소유 아이템 목록(개수 포함) */
    UPROPERTY()
    TArray<FOwnedItem> Items;

    /** 장착 슬롯 → 아이템 */
    UPROPERTY()
    TMap<EEquipSlot, UItemData*> Equipped;

    /** 업그레이드 캐시 */
    UPROPERTY()
    TMap<EEquipSlot, bool> HasUpgradeBySlot;

    UPROPERTY(Transient)
    TMap<EEquipSlot, TObjectPtr<UItemData>> SuggestedBySlot;

    /** 내부 유틸 함수들 */
    static float ScoreItem(const UItemData* Item, const FStatScoreWeights& W);
    void ReevaluateUpgrades(const FStatScoreWeights& W);
};