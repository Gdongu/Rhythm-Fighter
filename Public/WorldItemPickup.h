// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "WorldItemPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class UItemData;

UCLASS()
class RHYTHMFIGHTER_API AWorldItemPickup : public AActor
{
    GENERATED_BODY()
public:
    AWorldItemPickup();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere) USphereComponent* Proximity;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Mesh;
    UPROPERTY(VisibleAnywhere) UWidgetComponent* PromptWidget;

    UPROPERTY(EditAnywhere, Category = "Item") TSoftObjectPtr<UItemData> Item; // 에셋 할당
    UPROPERTY(EditAnywhere, Category = "Interact") FName PlayerTag = "Player";

    // 처음에 쓸 닫힌 상자 메쉬
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chest")
    UStaticMesh* ClosedMesh;

    // 상호작용 후에 바꿀 열린 상자 메쉬
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chest")
    UStaticMesh* OpenedMesh;

    // 반짝이는 이펙트 (ParticleSystem 버전)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chest")
    UParticleSystem* OpenEffect;

    // 이미 열렸는지 여부
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chest")
    bool bOpened = false;

    UFUNCTION()
    void OnBegin(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void OnEnd(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex);

    // 실제 줍는 동작(플레이어가 E 눌렀을 때 호출)
public:
    UFUNCTION(BlueprintCallable) void Pickup(class ACharacter* Interactor);
};