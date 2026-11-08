#pragma once

#include "Engine/DataAsset.h"
#include "ItemData.generated.h"

UENUM(BlueprintType)
enum class EItemType : uint8
{
    Equipment,
    Consumable
};

UENUM(BlueprintType)
enum class EEquipSlot : uint8
{
    None      UMETA(DisplayName = "None"),
    Weapon    UMETA(DisplayName = "Weapon"),
    Armor     UMETA(DisplayName = "Armor"),
    Accessory UMETA(DisplayName = "Accessory"),
};

USTRUCT(BlueprintType)
struct FStatModifiers
{
    GENERATED_BODY()

    // 합연산(+) 계열
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxHP_Add = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Attack_Add = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Defense_Add = 0.f;

    // 곱연산(*) 계열
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage_Mult = 1.f;           // 주는 피해 배수
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float GaugeGain_Mult = 1.f;       // 노트 히트 게이지 증가 배수
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SuperGain_Mult = 1.f;       // 필살 게이지 증가 배수
};

USTRUCT(BlueprintType)
struct FStatScoreWeights
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxHP_Add_W = 0.15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Attack_Add_W = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Defense_Add_W = 0.5f;

    // 배수 계열은 "증가율-1"을 점수로 환산
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage_Mult_W = 40.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float GaugeGain_Mult_W = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SuperGain_Mult_W = 10.f;
};

UCLASS(BlueprintType)
class RHYTHMFIGHTER_API UItemData : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) UTexture2D* Icon = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly) EItemType ItemType = EItemType::Equipment;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "ItemType==EItemType::Equipment"))
    EEquipSlot EquipSlot = EEquipSlot::Weapon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FStatModifiers Modifiers;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine)) FText Description;
};