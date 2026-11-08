#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPCInteractable.generated.h"

class USphereComponent;
class UWidgetComponent;
class ACharacter;
class UDialogueWidget;
class ARhythmFighterCharacter;
class UEquipmentStateSubsystem;

UENUM(BlueprintType)
enum class ETravelMethod : uint8
{
    OpenLevel UMETA(DisplayName = "OpenLevel (권장)"),
    ClientTravel UMETA(DisplayName = "ClientTravel (옵션)"),
    StreamLevel UMETA(DisplayName = "Level Streaming (고급)")
};

UCLASS()
class RHYTHMFIGHTER_API ANPCInteractable : public AActor
{
    GENERATED_BODY()

public:
    ANPCInteractable();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    USphereComponent* Proximity;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    UWidgetComponent* PromptWidget;

    // 프로젝트 규칙: 플레이어 태그
    UPROPERTY(EditAnywhere, Category = "Interact|Rules")
    FName PlayerTag = "Player";

    UPROPERTY(EditInstanceOnly, Category = "Interact|Travel", meta = (AllowedClasses = "World"))
    TSoftObjectPtr<UWorld> TargetLevel;

    // 선택: 맵 전환 방식
    UPROPERTY(EditAnywhere, Category = "Interact|Travel")
    ETravelMethod TravelMethod = ETravelMethod::OpenLevel;

    // 선택: 전환 시 옵션
    UPROPERTY(EditAnywhere, Category = "Interact|Travel")
    FString TravelOptions;

    // 대화 설정
    UPROPERTY(EditAnywhere, Category = "Dialogue")
    TSubclassOf<UDialogueWidget> DialogueWidgetClass;

    // NPC마다 다른 대사 넣을 배열
    UPROPERTY(EditAnywhere, Category = "Dialogue")
    TArray<FText> DialogueLines;

    UFUNCTION()
    void OnProximityBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnProximityEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
    int32 NPCLevel = 1;

public:
    UFUNCTION(BlueprintCallable, Category = "Interact")
    void Interact(ACharacter* Interactor);

    bool IsPlayerInRange() const { return bPlayerInRange; }

private:
    bool bPlayerInRange = false;

    // 대화 진행 중에 필요한 상태
    UPROPERTY() UDialogueWidget* ActiveDialogue = nullptr;
    UPROPERTY() ACharacter* CachedInteractor = nullptr;

    UFUNCTION()
    void OnDialogueFinished();   // 대화 끝났을 때 콜백

    void TravelToTargetLevel(ACharacter* Interactor);
};
