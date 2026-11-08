#include "WorldItemPickup.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "InventoryComponent.h"
#include "GameFramework/Character.h"
#include "ItemData.h"

AWorldItemPickup::AWorldItemPickup()
{
    Proximity = CreateDefaultSubobject<USphereComponent>(TEXT("Proximity"));
    SetRootComponent(Proximity);
    Proximity->InitSphereRadius(120.f);
    Proximity->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Proximity->SetCollisionResponseToAllChannels(ECR_Ignore);
    Proximity->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(RootComponent);

    PromptWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("Prompt"));
    PromptWidget->SetupAttachment(RootComponent);
    PromptWidget->SetDrawAtDesiredSize(true);
    PromptWidget->SetTwoSided(true);
    PromptWidget->SetPivot(FVector2D(0.5f, 0.5f));
    PromptWidget->SetRelativeLocation(FVector(0, 0, 80.f)); // 아이템 위로 띄우기
    // 시작 시 숨김
    PromptWidget->SetVisibility(false);
    // (보수적으로) 게임 숨김도 꺼주기
    PromptWidget->SetHiddenInGame(true);
}

void AWorldItemPickup::BeginPlay()
{
    Super::BeginPlay();

    if (ClosedMesh)
    {
        Mesh->SetStaticMesh(ClosedMesh);
    }

    Proximity->OnComponentBeginOverlap.AddDynamic(this, &AWorldItemPickup::OnBegin);
    Proximity->OnComponentEndOverlap.AddDynamic(this, &AWorldItemPickup::OnEnd);
}

void AWorldItemPickup::OnBegin(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (Other && Other->ActorHasTag(PlayerTag)) {
        PromptWidget->SetHiddenInGame(false);
        PromptWidget->SetVisibility(true);
    }
}

void AWorldItemPickup::OnEnd(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32)
{
    if (Other && Other->ActorHasTag(PlayerTag)) {
        PromptWidget->SetVisibility(false);
        PromptWidget->SetHiddenInGame(true);
    }
}

void AWorldItemPickup::Pickup(ACharacter* Interactor)
{
    if (bOpened)
    {
        return;
    }

    // 1) 인벤토리에 아이템 추가
    if (Interactor && !Item.IsNull())
    {
        if (UInventoryComponent* Inv =
            Interactor->FindComponentByClass<UInventoryComponent>())
        {
            // 소프트 레퍼런스를 실제 UItemData* 로 로드
            UItemData* Resolved = Item.LoadSynchronous();
            if (Resolved)
            {
                Inv->AddItem(Resolved);
            }
        }
    }

    // 2) 시각 연출 + 상태 변경
    bOpened = true;

    // 2-1) 반짝 이펙트
    if (OpenEffect)
    {
        UGameplayStatics::SpawnEmitterAtLocation(
            GetWorld(),
            OpenEffect,
            Mesh->GetComponentLocation(),
            Mesh->GetComponentRotation()
        );
    }

    // 2-2) 메쉬를 열린 상자로 교체
    if (OpenedMesh)
    {
        Mesh->SetStaticMesh(OpenedMesh);
    }

    // 2-3) 더 이상 상호작용 안 되게 충돌/프롬프트 끄기
    if (Proximity)
    {
        Proximity->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Proximity->SetGenerateOverlapEvents(false);
    }

    if (PromptWidget)
    {
        PromptWidget->SetHiddenInGame(true);
        PromptWidget->SetVisibility(false);
    }
}