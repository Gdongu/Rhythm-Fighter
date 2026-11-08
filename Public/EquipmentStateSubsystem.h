// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ItemData.h"
#include "EquipmentStateSubsystem.generated.h"

class UInventoryComponent;
class UCombatStatsComponent;
class ARhythmFighterCharacter;

USTRUCT(BlueprintType)
struct FStoredItemRef
{
    GENERATED_BODY()

    UPROPERTY() FSoftObjectPath ItemPath;   // UItemData 자산 경로
    UPROPERTY() int32 Count = 1;
    UPROPERTY() bool bEquipped = false;
    UPROPERTY() EEquipSlot Slot = EEquipSlot::None;
};

UCLASS()
class RHYTHMFIGHTER_API UEquipmentStateSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ---- 위치/맵 ----
    UPROPERTY() TMap<FName, FTransform> LastTransformByLevel;

    UFUNCTION(BlueprintCallable) void SetLastTransformForLevel(FName LevelName, const FTransform& T);
    UFUNCTION(BlueprintCallable) bool GetLastTransformForLevel(FName LevelName, FTransform& OutT) const;

    // ---- 인벤토리 스냅샷 ----
    UPROPERTY() TArray<FStoredItemRef> InventorySnapshot;

    UFUNCTION(BlueprintCallable) void CaptureFromCharacter(class ARhythmFighterCharacter* C);
    UFUNCTION(BlueprintCallable) void ApplyToCharacter(class ARhythmFighterCharacter* C);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TMap<EEquipSlot, UItemData*> Equipped;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FStatModifiers Aggregated;

    UFUNCTION(BlueprintCallable) void PullFromInventory(UInventoryComponent* Inv);
    UFUNCTION(BlueprintCallable) void ApplyTo(UCombatStatsComponent* Stats) const;

    // C++ 전용(참조 반환)
    FORCEINLINE const TMap<EEquipSlot, UItemData*>& GetEquippedRef() const { return Equipped; }

    // BP에서 필요할 때 복사로 꺼내쓰기
    UFUNCTION(BlueprintPure) void GetEquippedCopy(UPARAM(ref) TMap<EEquipSlot, UItemData*>& OutEquipped) const
    {
        OutEquipped = Equipped;
    }

    UFUNCTION(BlueprintPure) FStatModifiers GetAggregated() const { return Aggregated; }

    UPROPERTY(BlueprintReadWrite, Category = "Battle")
    int32 PendingEnemyLevel = 1;

};