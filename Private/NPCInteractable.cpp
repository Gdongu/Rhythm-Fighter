#include "NPCInteractable.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "RhythmFighterCharacter.h"
#include "EquipmentStateSubsystem.h"
#include "DialogueWidget.h"
#include "NPCLevelWidget.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/LevelStreamingDynamic.h"

ANPCInteractable::ANPCInteractable()
{
    PrimaryActorTick.bCanEverTick = false;

    Proximity = CreateDefaultSubobject<USphereComponent>(TEXT("Proximity"));
    SetRootComponent(Proximity);
    Proximity->InitSphereRadius(180.f);
    Proximity->SetCollisionObjectType(ECC_WorldDynamic);
    Proximity->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Proximity->SetCollisionResponseToAllChannels(ECR_Ignore);
    Proximity->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    PromptWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PromptWidget"));
    PromptWidget->SetupAttachment(RootComponent);
    PromptWidget->SetWidgetSpace(EWidgetSpace::Screen);
    PromptWidget->SetDrawSize(FVector2D(220.f, 60.f));
    PromptWidget->SetVisibility(false);
}

void ANPCInteractable::BeginPlay()
{
    Super::BeginPlay();

    if (PromptWidget)
    {
        if (UUserWidget* W = PromptWidget->GetUserWidgetObject())
        {
            if (UNPCLevelWidget* LevelWidget = Cast<UNPCLevelWidget>(W))
            {
                LevelWidget->SetLevel(NPCLevel);
            }
        }
    }

    Proximity->OnComponentBeginOverlap.AddDynamic(this, &ANPCInteractable::OnProximityBegin);
    Proximity->OnComponentEndOverlap.AddDynamic(this, &ANPCInteractable::OnProximityEnd);
}

void ANPCInteractable::OnProximityBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor) return;

    if (OtherActor->ActorHasTag(PlayerTag))
    {
        bPlayerInRange = true;
        PromptWidget->SetVisibility(true);

        if (ARhythmFighterCharacter* PC = Cast<ARhythmFighterCharacter>(OtherActor))
        {
            PC->SetCurrentInteractable(this);
        }
    }

    if (ARhythmFighterCharacter* PC = Cast<ARhythmFighterCharacter>(OtherActor))
    {
        bPlayerInRange = true;
        PromptWidget->SetVisibility(true);
        PC->SetCurrentInteractable(this);
    }
}

void ANPCInteractable::OnProximityEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!OtherActor) return;

    if (OtherActor->ActorHasTag(PlayerTag))
    {
        bPlayerInRange = false;
        PromptWidget->SetVisibility(false);

        if (ARhythmFighterCharacter* PC = Cast<ARhythmFighterCharacter>(OtherActor))
        {
            if (PC->CurrentInteractable == this)
            {
                PC->SetCurrentInteractable(nullptr);
            }
        }
    }

    if (ARhythmFighterCharacter* PC = Cast<ARhythmFighterCharacter>(OtherActor))
    {
        bPlayerInRange = false;
        PromptWidget->SetVisibility(false);
        if (PC->CurrentInteractable == this)
        {
            PC->SetCurrentInteractable(nullptr);
        }
    }
}

void ANPCInteractable::Interact(ACharacter* Interactor)
{
    if (!bPlayerInRange || !Interactor) return;

    // 이미 대화 중이면 또 E 눌러도 무시
    if (ActiveDialogue)
        return;

    // 1) 대화 위젯이 세팅돼 있으면 → 먼저 대화부터 시작
    if (DialogueWidgetClass)
    {
        if (APlayerController* PC = Cast<APlayerController>(Interactor->GetController()))
        {
            UDialogueWidget* W = CreateWidget<UDialogueWidget>(PC, DialogueWidgetClass);
            if (!W) return;

            // NPC에 세팅해둔 대사 배열 전달
            W->Lines = DialogueLines;

            // (필요하면 위젯의 LevelToOpen은 안 써도 됨)
            // W->LevelToOpen = ...; // 지금은 생략

            // 대화 끝났을 때 이 NPC의 콜백 호출
            W->OnFinished.AddDynamic(this, &ANPCInteractable::OnDialogueFinished);

            W->AddToViewport();
            ActiveDialogue = W;
            CachedInteractor = Interactor;

            // 키/마우스 포커스는 위젯이 알아서 받도록 (NativeConstruct에서 SetKeyboardFocus 함)
        }

        // 대화가 있는 경우에는 여기서 바로 리턴 → 아직 맵 이동 X
        return;
    }

    // 2) 대화 위젯이 없으면 → 예전처럼 바로 맵 이동
    if (ARhythmFighterCharacter* RF = Cast<ARhythmFighterCharacter>(Interactor))
    {
        if (UGameInstance* GI = GetGameInstance())
        {
            if (UEquipmentStateSubsystem* EquipSys = GI->GetSubsystem<UEquipmentStateSubsystem>())
            {
                const FString LevelNameStr = UGameplayStatics::GetCurrentLevelName(this, true);
                EquipSys->SetLastTransformForLevel(FName(*LevelNameStr), RF->GetActorTransform());
                EquipSys->CaptureFromCharacter(RF);
            }
        }
    }

    TravelToTargetLevel(Interactor);
}

void ANPCInteractable::TravelToTargetLevel(ACharacter* Interactor)
{
    if (TargetLevel.IsNull())
    {
        UE_LOG(LogTemp, Warning, TEXT("TargetLevel is null."));
        return;
    }

    // /Game/Maps/BattleMap1 형태의 패키지 경로 (".BattleMap1" 잘라낼 필요 X)
    const FString PackagePath = TargetLevel.ToSoftObjectPath().GetLongPackageName();

    switch (TravelMethod)
    {
    case ETravelMethod::OpenLevel:
    {
        if (TravelOptions.IsEmpty())
        {
            // 가장 안전한 경로: SoftObjectPtr로 직접 오픈
            UGameplayStatics::OpenLevelBySoftObjectPtr(this, TargetLevel);
        }
        else
        {
            // 옵션이 필요하면 URL로 ClientTravel
            if (APlayerController* PC = Interactor ? Cast<APlayerController>(Interactor->GetController()) : nullptr)
            {
                FString URL = PackagePath;                 // "/Game/Maps/BattleMap1"
                URL += TEXT("?") + TravelOptions;          // "/Game/Maps/BattleMap1?listen" 등
                PC->ClientTravel(URL, TRAVEL_Absolute);
            }
            else
            {
                UGameplayStatics::OpenLevelBySoftObjectPtr(this, TargetLevel);
            }
        }
        break;
    }

    case ETravelMethod::ClientTravel:
    {
        if (APlayerController* PC = Interactor ? Cast<APlayerController>(Interactor->GetController()) : nullptr)
        {
            FString URL = PackagePath;
            if (!TravelOptions.IsEmpty())
                URL += TEXT("?") + TravelOptions;
            PC->ClientTravel(URL, TRAVEL_Absolute);
        }
        else
        {
            // 컨트롤러가 없으면 안전하게 OpenLevelBySoftObjectPtr로
            UGameplayStatics::OpenLevelBySoftObjectPtr(this, TargetLevel);
        }
        break;
    }

    case ETravelMethod::StreamLevel:
    {
        // 같은 Persistent Level 내 스트리밍일 때만 의미 있음 (필요 시 구현)
        // ULevelStreamingDynamic* Streamed = ...
        break;
    }

    default:
    {
        UGameplayStatics::OpenLevelBySoftObjectPtr(this, TargetLevel);
        break;
    }
    }
}

void ANPCInteractable::OnDialogueFinished()
{
    // 위젯 쪽에서 RemoveFromParent() 는 이미 호출됨 (NextLine 마지막에서)
    ActiveDialogue = nullptr;

    ACharacter* Interactor = CachedInteractor;
    CachedInteractor = nullptr;

    if (!Interactor)
        return;

    // 여기서 '전투 전에 상태 저장' + 맵 이동 실행
    if (ARhythmFighterCharacter* RF = Cast<ARhythmFighterCharacter>(Interactor))
    {
        if (UGameInstance* GI = GetGameInstance())
        {
            if (UEquipmentStateSubsystem* EquipSys = GI->GetSubsystem<UEquipmentStateSubsystem>())
            {
                const FString LevelNameStr = UGameplayStatics::GetCurrentLevelName(this, true);
                EquipSys->SetLastTransformForLevel(FName(*LevelNameStr), RF->GetActorTransform());
                EquipSys->CaptureFromCharacter(RF);

                EquipSys->PendingEnemyLevel = NPCLevel;
            }
        }
    }

    TravelToTargetLevel(Interactor);
}