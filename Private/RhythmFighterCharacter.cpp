// Copyright Epic Games, Inc. All Rights Reserved.

#include "RhythmFighterCharacter.h"
#include "RhythmEnemyCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h" 
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "RhythmUI.h"
#include "CombatStatsComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "RhythmFighter.h"
#include "NPCInteractable.h"
#include "WorldItemPickup.h"
#include "InventoryComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "RhythmFighterGameMode.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SkeletalMeshComponent.h"
#include "EquipmentStateSubsystem.h"

ARhythmFighterCharacter::ARhythmFighterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	if (!CameraBoom)
	{
		CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
		CameraBoom->SetupAttachment(RootComponent);
		CameraBoom->TargetArmLength = 300.f;
		CameraBoom->bUsePawnControlRotation = true;
	}
	if (!FollowCamera)
	{
		FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
		FollowCamera->SetupAttachment(CameraBoom);
		FollowCamera->bUsePawnControlRotation = false;
	}

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	GetCapsuleComponent()->SetGenerateOverlapEvents(true);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 200.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	Stats = CreateDefaultSubobject<UCombatStatsComponent>(TEXT("Stats"));

	// 히트박스
	Hitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("Hitbox"));
	Hitbox->SetupAttachment(GetMesh());
	Hitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Hitbox->SetCollisionObjectType(ECC_WorldDynamic);
	Hitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
	Hitbox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // 캡슐과 겹침
	Hitbox->SetGenerateOverlapEvents(true);
	Hitbox->SetBoxExtent(FVector(30, 30, 30));

	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
}

void ARhythmFighterCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UEquipmentStateSubsystem* EquipSys = GI->GetSubsystem<UEquipmentStateSubsystem>())
		{
			// 1) 인벤토리 / 장비 / 스탯 복원
			EquipSys->ApplyToCharacter(this);

			// 2) 이 레벨에서 마지막으로 저장한 위치가 있으면 이동
			const FString LevelNameStr = UGameplayStatics::GetCurrentLevelName(this, true);
			FTransform SavedTransform;
			if (EquipSys->GetLastTransformForLevel(FName(*LevelNameStr), SavedTransform))
			{
				SetActorTransform(SavedTransform, false, nullptr, ETeleportType::TeleportPhysics);

				if (UCharacterMovementComponent* Move = GetCharacterMovement())
				{
					Move->StopMovementImmediately();
				}
			}
		}
		InitAfterImagePool();
	}

	if (Stats)
	{
		Stats->OnHealthChanged.AddDynamic(this, &ARhythmFighterCharacter::HandleHealthChanged);

		Stats->OnDeath.AddDynamic(this, &ARhythmFighterCharacter::OnSelfDied);
	}

	SetCharacterMode(DefaultMode);

	if (Hitbox)
	{
		Hitbox->OnComponentBeginOverlap.AddDynamic(this, &ARhythmFighterCharacter::OnHitboxOverlap);
	}

	if (UEquipmentStateSubsystem* Sub = GetGameInstance()->GetSubsystem<UEquipmentStateSubsystem>())
	{
		const FStatModifiers M = Sub->GetAggregated();
		if (UCombatStatsComponent* CS = FindComponentByClass<UCombatStatsComponent>())
		{
			CS->SetExternalModifiers(M); // 1차
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, [this, M]()
				{
					if (UCombatStatsComponent* CS2 = FindComponentByClass<UCombatStatsComponent>())
						CS2->SetExternalModifiers(M);     // 다음 프레임 2차
				}, 0.f, false);
		}
	}
}

void ARhythmFighterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		     // 탐험 전용(모드에서 필터)
		if (MoveAction)      EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ARhythmFighterCharacter::Move);
		if (LookAction)      EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ARhythmFighterCharacter::Look);
		if (MouseLookAction) EIC->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ARhythmFighterCharacter::Look);
		if (IA_Inventory) { EIC->BindAction(IA_Inventory, ETriggerEvent::Started, this, &ARhythmFighterCharacter::OnInventoryPressed); }
		
	       // 전투 전용(모드에서 필터)
		if (MoveRightAction)
		{
			EIC->BindAction(MoveRightAction, ETriggerEvent::Triggered, this, &ARhythmFighterCharacter::MoveRight);
			EIC->BindAction(MoveRightAction, ETriggerEvent::Completed, this, &ARhythmFighterCharacter::MoveRight);
		}
		
       // 리듬/공격/필살기/상호작용
		EIC->BindAction(IA_NoteUp, ETriggerEvent::Started, this, &ARhythmFighterCharacter::OnHitNoteUp);
		EIC->BindAction(IA_NoteDown, ETriggerEvent::Started, this, &ARhythmFighterCharacter::OnHitNoteDown);
		EIC->BindAction(IA_NoteLeft, ETriggerEvent::Started, this, &ARhythmFighterCharacter::OnHitNoteLeft);
		EIC->BindAction(IA_NoteRight, ETriggerEvent::Started, this, &ARhythmFighterCharacter::OnHitNoteRight);

		if (AttackAction)        EIC->BindAction(AttackAction, ETriggerEvent::Triggered, this, &ARhythmFighterCharacter::OnAttack);
		if (FinisherStartAction) EIC->BindAction(FinisherStartAction, ETriggerEvent::Triggered, this, &ARhythmFighterCharacter::OnFinisherStart);
		if (KeyAAction)          EIC->BindAction(KeyAAction, ETriggerEvent::Started, this, &ARhythmFighterCharacter::OnKeyA);
		if (KeySAction)          EIC->BindAction(KeySAction, ETriggerEvent::Started, this, &ARhythmFighterCharacter::OnKeyS);
		if (KeyDAction)          EIC->BindAction(KeyDAction, ETriggerEvent::Started, this, &ARhythmFighterCharacter::OnKeyD);
		if (KeyFAction)          EIC->BindAction(KeyFAction, ETriggerEvent::Started, this, &ARhythmFighterCharacter::OnKeyF);
		if (IA_Interact)         EIC->BindAction(IA_Interact, ETriggerEvent::Started, this, &ARhythmFighterCharacter::OnInteractPressed);
	}
	else
	{
		UE_LOG(LogRhythmFighter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ARhythmFighterCharacter::Move(const FInputActionValue& Value)
{
	if (bInHitReact || bDeathHandled) return;

	if (CurrentMode != ECharMode::Exploration) return; // 전투 모드에선 무시
	// input is a Vector2D
	const FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void ARhythmFighterCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void ARhythmFighterCharacter::Look(const FInputActionValue& Value)
{
	if (CurrentMode != ECharMode::Exploration) return; // 전투 모드에선 무시
	// input is a Vector2D
	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}



void ARhythmFighterCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ARhythmFighterCharacter::PlayRandomFightAnimation()
{
	if (IsAttacking) return; // 이미 공격 중이면 무시

	IsAttacking = true;

	if (RandomAnimations.Num() == 0) return;

	// 랜덤 인덱스 뽑기
	int32 Index = FMath::RandRange(0, RandomAnimations.Num() - 1);
	UAnimMontage* MontageToPlay = RandomAnimations[Index];

	if (MontageToPlay)
	{
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance->IsAnyMontagePlaying())
		{
			UE_LOG(LogTemp, Warning, TEXT("Some montage is already playing, skip."));
			return;
		}
		if (AnimInstance && !AnimInstance->Montage_IsPlaying(MontageToPlay))
		{
			FOnMontageEnded EndDelegate;
			EndDelegate.BindLambda([this](UAnimMontage*, bool)
				{
					IsAttacking = false;
				});

			AnimInstance->Montage_Play(MontageToPlay);
			AnimInstance->Montage_SetEndDelegate(EndDelegate, MontageToPlay);
		}
	}
}

void ARhythmFighterCharacter::MoveRight(const FInputActionValue& Value)
{
	if (bDeathHandled) return;
	if (CurrentMode != ECharMode::Combat) return;

	const float AxisValue = Value.Get<float>();

	// 1) 축 값으로 상승엣지 감지
	int8 Sign = 0;
	if (AxisValue > 0.7f) Sign = +1;
	else if (AxisValue < -0.7f) Sign = -1;

	if (Sign != 0 && PrevMoveSign == 0)
	{
		RegisterMoveTap(Sign);  // 피격 중에도 탭 기록/대쉬 가능
	}
	PrevMoveSign = Sign;

	// 2) 대쉬 중엔 일반 이동 무시
	if (bIsDashing) return;

	// 3) 피격 중/공격 중엔 "일반 이동"은 막되, 대쉬는 위에서 이미 발동 가능
	if (bInHitReact) return;
	if (IsAttacking) return;

	// 4) 기존 이동
	if (FMath::Abs(AxisValue) > 0.1f)
	{
		AddMovementInput(FVector(1.f, 0.f, 0.f), AxisValue);
		SetActorRotation(AxisValue > 0.f ? FRotator(0, 0, 0) : FRotator(0, 180, 0));
	}
}

void ARhythmFighterCharacter::EndAttack()
{
	IsAttacking = false;
}

void ARhythmFighterCharacter::OnAttack(const FInputActionValue& /*Value*/)
{
	if (bInHitReact || bDeathHandled) return;
	if (IsAttacking || !RhythmUIRef) return;
	if (RhythmUIRef->TryConsumeGauge(RhythmUIRef->AttackCost))
	{
		PlayRandomFightAnimation();
		// 공격 1회당 필살기 게이지 충전
		RhythmUIRef->AddSuperGauge(RhythmUIRef->SuperGainPerAttack);
	}
}

void ARhythmFighterCharacter::OnFinisherStart(const FInputActionValue&)
{
	UE_LOG(LogTemp, Warning, TEXT("[FINISHER] Shift pressed"));

	if (!RhythmUIRef) { UE_LOG(LogTemp, Error, TEXT("[FINISHER] RhythmUIRef NULL")); return; }
	if (RhythmUIRef->bFinisherActive) { UE_LOG(LogTemp, Warning, TEXT("[FINISHER] Already active")); return; }

	// 게이지 차면 시도
	if (RhythmUIRef->TryStartFinisher())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (ULocalPlayer* LP = PC->GetLocalPlayer())
			{
				if (UEnhancedInputLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
				{
					Subsys->ClearAllMappings();
					if (FinisherIMC) Subsys->AddMappingContext(FinisherIMC, 0);
					UE_LOG(LogTemp, Log, TEXT("[FINISHER] Switched to FinisherIMC"));
				}
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[FINISHER] TryStartFinisher failed"));
	}
}

// 커맨드 키 입력 전달
void ARhythmFighterCharacter::OnKeyA(const FInputActionValue&) { if (RhythmUIRef) RhythmUIRef->OnFinisherInput(TEXT("A")); }
void ARhythmFighterCharacter::OnKeyS(const FInputActionValue&) { if (RhythmUIRef) RhythmUIRef->OnFinisherInput(TEXT("S")); }
void ARhythmFighterCharacter::OnKeyD(const FInputActionValue&) { if (RhythmUIRef) RhythmUIRef->OnFinisherInput(TEXT("D")); }
void ARhythmFighterCharacter::OnKeyF(const FInputActionValue&) { if (RhythmUIRef) RhythmUIRef->OnFinisherInput(TEXT("F")); }

// === 성공 트리거: UI가 성공하는 순간을 캐릭터가 알아야 함 ===
// 간단한 폴링(매 틱 확인) 또는 Delegate를 추가할 수 있음.
// 여기서는 매 틱에서 UI 상태를 보고 처리한다:

void ARhythmFighterCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const bool bActiveNow = (RhythmUIRef && RhythmUIRef->bFinisherActive);

	// Finisher 종료 순간 (true → false) 에만 IMC 복원 + 결과 처리
	if (bPrevFinisherActive && !bActiveNow)
	{
		// IMC 복원
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (ULocalPlayer* LP = PC->GetLocalPlayer())
			{
				if (UEnhancedInputLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
				{
					Subsys->ClearAllMappings();
					if (NormalIMC) Subsys->AddMappingContext(NormalIMC, 0);
					UE_LOG(LogTemp, Log, TEXT("[FINISHER] Restored NormalIMC"));
				}
			}
		}

		// 성공이면 연출/몽타주
		const bool bSuccess = RhythmUIRef ? RhythmUIRef->ConsumeLastFinisherSuccess() : false;
		if (bSuccess)
		{
			StartCinematicZoom();
			if (FinisherMontage && GetMesh() && GetMesh()->GetAnimInstance())
			{
				GetMesh()->GetAnimInstance()->Montage_Play(FinisherMontage);
			}
			GetWorldTimerManager().SetTimer(ZoomHandle, this, &ARhythmFighterCharacter::EndCinematicZoom, 0.8f, false);
		}
	}
	UpdateAfterImages(DeltaSeconds);
	// 마지막에 현재 상태 저장
	bPrevFinisherActive = bActiveNow;
}

void ARhythmFighterCharacter::StartCinematicZoom()
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (APlayerCameraManager* PCM = PC->PlayerCameraManager)
		{
			SavedFOV = PCM->GetFOVAngle();                      // 저장
			PCM->SetFOV(FMath::Clamp(SavedFOV * 0.8f, 30.f, 120.f)); // 살짝 줌인
		}
	}
}

void ARhythmFighterCharacter::EndCinematicZoom()
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (APlayerCameraManager* PCM = PC->PlayerCameraManager)
		{
			PCM->SetFOV(SavedFOV); // 되돌리기
		}
	}
}

void ARhythmFighterCharacter::StartHitbox(FName AttachSocket, FVector BoxExtent, float Damage)
{
	CurrentHitDamage = Damage;
	AlreadyHit.Reset();

	if (!Hitbox || !GetMesh()) return;
	Hitbox->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocket);
	Hitbox->SetRelativeLocation(FVector(40.f, 0.f, 0.f)); // 손 앞으로
	Hitbox->SetBoxExtent((BoxExtent.GetMax() < 1) ? FVector(60) : BoxExtent, true);
	Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	bHitboxActive = true;
}

void ARhythmFighterCharacter::EndHitbox()
{
	if (Hitbox) Hitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	bHitboxActive = false;
	AlreadyHit.Reset();
}

void ARhythmFighterCharacter::OnHitboxOverlap(
	UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp,
	int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
	if (!Other || Other == this) return;

	UCombatStatsComponent* AttackerStats = Stats; // this에 붙은 컴포넌트
	UCombatStatsComponent* VictimStats = Other->FindComponentByClass<UCombatStatsComponent>();
	if (!AttackerStats || !VictimStats) return;

	const float BaseDamage = 10.f; // 몽타주/무기/스킬 별 기본값
	const float Outgoing = AttackerStats->GetOutgoingDamage(BaseDamage);
	const float Final = VictimStats->GetIncomingAfterDefense(Outgoing);

	VictimStats->ApplyDamage(Final); // 내부에서 HP 감소 + 브로드캐스트

	if (auto* C = Cast<ARhythmFighterCharacter>(Other)) C->ReceiveHit(Final, this);
	else if (auto* E = Cast<ARhythmEnemyCharacter>(Other)) E->ReceiveHit(Final, this);
}

void ARhythmFighterCharacter::OnInteractPressed()
{
	if (CurrentInteractable)
	{
		CurrentInteractable->Interact(this);
	}

	const float Radius = 150.f;
	TArray<AActor*> Hits;

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

	TArray<AActor*> Ignore; Ignore.Add(this);

	UKismetSystemLibrary::SphereOverlapActors(
		this, GetActorLocation(), Radius, ObjectTypes, AWorldItemPickup::StaticClass(), Ignore, Hits
	);

	AWorldItemPickup* Closest = nullptr;
	float BestDist = FLT_MAX;

	for (AActor* A : Hits)
	{
		if (AWorldItemPickup* P = Cast<AWorldItemPickup>(A))
		{
			const float D = FVector::DistSquared(P->GetActorLocation(), GetActorLocation());
			if (D < BestDist) { BestDist = D; Closest = P; }
		}
	}
	if (Closest) { Closest->Pickup(this); }
}

void ARhythmFighterCharacter::SetCharacterMode(ECharMode NewMode)
{
	CurrentMode = NewMode;

	// 이동/회전/평면제약
	if (NewMode == ECharMode::Exploration) ApplyExplorationMode();
	else                                   ApplyCombatMode();

	// 입력 맵핑 교체
	ApplyIMC(NewMode);

	// 카메라 뷰타겟/붐 회전
	ApplyViewTarget(NewMode);
}

void ARhythmFighterCharacter::ApplyExplorationMode()
{
	// 3인칭 일반 이동
	bUseControllerRotationYaw = false;
	auto* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;

	// 평면 제약 해제
	Move->bConstrainToPlane = false;
	Move->SetPlaneConstraintEnabled(false);

	// 카메라 붐 리셋
	if (CameraBoom)
	{
		CameraBoom->bUsePawnControlRotation = true;   // 컨트롤러 회전 사용
		CameraBoom->SetRelativeRotation(FRotator::ZeroRotator);
		CameraBoom->TargetArmLength = 300.f;          // 적당한 거리
		CameraBoom->SocketOffset = FVector(0.f, 50.f, 60.f);
	}

	if (FollowCamera)
		FollowCamera->bUsePawnControlRotation = false;
}

void ARhythmFighterCharacter::ApplyCombatMode()
{
	// 사이드 스크롤(좌우만)
	bUseControllerRotationYaw = true;
	auto* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = false;

	// Y축 고정 (X축만 이동)
	Move->bConstrainToPlane = true;
	Move->SetPlaneConstraintEnabled(true);
	Move->SetPlaneConstraintNormal(FVector(0, 1, 0)); // Y 고정 (월드 기준)

	// 카메라: 고정 시점(붐을 옆으로 돌리거나, 전용 카메라 액터 사용)
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->SetRelativeRotation(FRotator(0.f, -90.f, 0.f)); // 옆에서 보게
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 120.f);        // 필요시 머리 높이 보정
}

void ARhythmFighterCharacter::ApplyIMC(ECharMode Mode)
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (auto* Subsys = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (IMC_Exploration) Subsys->RemoveMappingContext(IMC_Exploration);
				if (NormalIMC)      Subsys->RemoveMappingContext(NormalIMC);

				if (Mode == ECharMode::Exploration)
				{
					if (IMC_Exploration) Subsys->AddMappingContext(IMC_Exploration, 0);
				}
				else
				{
					if (NormalIMC) Subsys->AddMappingContext(NormalIMC, 0);
				}
			}
		}
	}
}

void ARhythmFighterCharacter::ApplyViewTarget(ECharMode Mode)
{
	/*if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (Mode == ECharMode::Combat && bUseCameraActorForCombat && CombatCameraActor)
		{
			//PC->SetViewTargetWithBlend(CombatCameraActor, 0.25f);
		}
		else
		{
			// TPS로 복귀: 내 카메라로 복원
			//PC->SetViewTargetWithBlend(this, 0.25f);
		}
	}*/
}

void ARhythmFighterCharacter::OnInventoryPressed()
{
	ShowInventory(!bInventoryOpen);
}

void ARhythmFighterCharacter::ShowInventory(bool bShow)
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (bShow)
		{
			if (!InventoryWidget && InventoryWidgetClass)
			{
				InventoryWidget = CreateWidget<UUserWidget>(PC, InventoryWidgetClass);
			}
			if (InventoryWidget && !InventoryWidget->IsInViewport())
				InventoryWidget->AddToViewport(20);

			FInputModeUIOnly Mode;
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(Mode);
			PC->bShowMouseCursor = true;
			bInventoryOpen = true;
		}
		else
		{
			if (InventoryWidget && InventoryWidget->IsInViewport())
				InventoryWidget->RemoveFromParent();

			FInputModeGameOnly Mode;
			PC->SetInputMode(Mode);
			PC->bShowMouseCursor = false;
			bInventoryOpen = false;
		}
	}
}

void ARhythmFighterCharacter::ReceiveHit(float /*Damage*/, AActor* /*From*/)
{
	if (bInvulnerable) return;
	if (bDeathHandled) return;
	if (!Stats || !Stats->IsAlive()) return;

	// 슈퍼아머면 끊기지 않음
	if (bSuperArmor) return;

	// 공격 중이면 끊기
	if (IsAttacking)
	{
		if (UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			// 재생 중인 공격 몽타주 중단
			Anim->Montage_Stop(0.1f);
		}
		EndHitbox();
		IsAttacking = false;
	}
	
	// 피격 리액트 진입
	if (HitReactMontage && GetMesh() && GetMesh()->GetAnimInstance())
	{
		UAnimInstance* Anim = GetMesh()->GetAnimInstance();

		// === 여기서부터 피격 상태 진입: 이동/입력 봉인 ===
		bInHitReact = true;

		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->SetIgnoreMoveInput(true);
			PC->SetIgnoreLookInput(true);
		}
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->StopMovementImmediately();
			Move->DisableMovement();
		}

		Anim->Montage_Play(HitReactMontage, 1.f);

		// 끝나면 자동 복구(단, 이미 죽었으면 유지)
		FOnMontageEnded End;
		End.BindLambda([this](UAnimMontage*, bool)
			{
				if (!bDeathHandled)
				{
					bInHitReact = false;

					if (APlayerController* PC = Cast<APlayerController>(GetController()))
					{
						PC->SetIgnoreMoveInput(false);
						PC->SetIgnoreLookInput(false);
					}
					if (UCharacterMovementComponent* Move = GetCharacterMovement())
					{
						Move->SetMovementMode(MOVE_Walking);
					}
				}
			});
		Anim->Montage_SetEndDelegate(End, HitReactMontage);
	}
	PlayHitFX(this);
}

void ARhythmFighterCharacter::HandleHealthChanged(float NewHP, float /*MaxHP*/)
{
	if (NewHP <= 0.f)
	{
		DieAndNotify();
	}
}

void ARhythmFighterCharacter::DieAndNotify()
{
	if (bDeathHandled) return;
	bDeathHandled = true;

	// 더 이상 데미지/공격 X
	SetCanBeDamaged(false);
	EndHitbox();
	IsAttacking = false;
	bInHitReact = false;

	Tags.AddUnique("Dead");

	// 입력·이동 정지
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
		Move->Velocity = FVector::ZeroVector;
		Move->SetMovementMode(MOVE_None);
	}

	float PlayLen = 0.f;

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (UAnimInstance* Anim = MeshComp->GetAnimInstance())
		{
			// 공격/피격 몽타주 전부 끊고 Death만 재생
			Anim->StopAllMontages(0.0f);

			if (DeathMontage)
			{
				PlayLen = Anim->Montage_Play(DeathMontage, 1.f);

				FOnMontageEnded EndDelegate;
				EndDelegate.BindUObject(this, &ARhythmFighterCharacter::OnPlayerDeathMontageEnded);
				Anim->Montage_SetBlendingOutDelegate(EndDelegate, DeathMontage);

				if (PlayLen <= 0.01f)
				{
					UE_LOG(LogTemp, Warning, TEXT("[PlayerDeath] Montage_Play returned 0"));
					EnterCorpseState_Player();
				}
			}
			else
			{
				EnterCorpseState_Player();
			}
		}
		else
		{
			EnterCorpseState_Player();
		}
	}
	else
	{
		EnterCorpseState_Player();
	}

	// 몽타주 길이(최소 0.9초) 뒤 패배 처리
	const float Delay = FMath::Max(PlayLen, 0.9f);
	GetWorldTimerManager().SetTimer(
		DeathEndHandle, this, &ARhythmFighterCharacter::NotifyBattleEndAsLoser, Delay, false);
}

void ARhythmFighterCharacter::OnSelfDied()
{
	if (auto* GM = GetWorld()->GetAuthGameMode<ARhythmFighterGameMode>())
	{
		GM->OnAnyActorDied(this);
	}
	// 사망 연출(몽타주)은 기존 구현 유지 가능
}

void ARhythmFighterCharacter::NotifyBattleEndAsLoser()
{
	if (auto* GM = GetWorld()->GetAuthGameMode<ARhythmFighterGameMode>())
	{
		GM->EndBattle(false); // 여기서 결과 위젯 띄우고 이후 맵 이동
	}
}

void ARhythmFighterCharacter::OnPlayerDeathMontageEnded(UAnimMontage* /*Montage*/, bool /*bInterrupted*/)
{
	EnterCorpseState_Player();
}

void ARhythmFighterCharacter::EnterCorpseState_Player()
{
	if (bCorpseStateEntered)
	{
		// 이미 시체 상태라면 다시 애니메이션 재생 X
		return;
	}
	bCorpseStateEntered = true;

	bDeathHandled = true;
	Tags.AddUnique("Dead");
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
		Move->Velocity = FVector::ZeroVector;
		Move->SetMovementMode(MOVE_None);
	}
	// 플레이어는 죽은 포즈로 완전히 고정
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (UAnimInstance* Anim = MeshComp->GetAnimInstance())
		{
			// 혹시 남아 있는 몽타주가 있으면 모두 끊고
			Anim->StopAllMontages(0.0f);
		}

		if (DeathPoseAsset)
		{
			MeshComp->bPauseAnims = false; // 일단 풀고
			MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			MeshComp->SetAnimation(DeathPoseAsset);
			MeshComp->Play(false); // 루프 X, 한 포즈로 고정
		}
		else
		{
			// DeathPoseAsset이 없다면, 최소한 현재 포즈에서 멈춰두기
			MeshComp->bPauseAnims = true;
			MeshComp->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		}
	}
}

bool ARhythmFighterCharacter::IsDead() const
{
	return bDeathHandled || (Stats && !Stats->IsAlive());
}

void ARhythmFighterCharacter::StartAttackFX()
{
	// 이펙트 에셋이 없으면 아무 것도 하지 않음
	if (!AttackFX)
		return;

	// 이미 나가고 있으면 중복 생성 방지
	if (ActiveAttackFX && ActiveAttackFX->IsActive())
		return;

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
		return;

	// 팔(무기) 소켓에 붙여서 이펙트 생성
	ActiveAttackFX = UGameplayStatics::SpawnEmitterAttached(
		AttackFX,               // UNiagaraSystem* AttackFX;
		MeshComp,               // Parent
		AttackFXSocket,     // 소켓 이름
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget,
		true                    // bAutoDestroy
	);

	// 공격 휘두르는 효과음
	if (AttackSwingSFX)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			AttackSwingSFX,
			MeshComp->GetSocketLocation(HitFXSocket)
		);
	}

	// 공격할 때 내는 음성
	if (AttackVoiceSFX)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			AttackVoiceSFX,
			GetActorLocation()
		);
	}
}

void ARhythmFighterCharacter::StopAttackFX()
{
	if (ActiveAttackFX)
	{
		ActiveAttackFX->Deactivate();
		ActiveAttackFX = nullptr;
	}
}

void ARhythmFighterCharacter::PlayHitFX(AActor* Victim)
{
	if (!HitFX || !GetMesh())
		return;

	const FVector Loc = GetMesh()->GetSocketLocation(HitFXSocket);
	const FRotator Rot = GetActorRotation();

	// 피격 파티클
	UGameplayStatics::SpawnEmitterAtLocation(
		GetWorld(),
		HitFX,
		Loc,
		Rot
	);

	// 타격 사운드
	if (HitImpactSFX)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			HitImpactSFX,
			GetActorLocation()
		);
	}

	// "윽!" 같은 피격 음성
	if (HitVoiceSFX)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			HitVoiceSFX,
			GetActorLocation()
		);
	}
}

void ARhythmFighterCharacter::OnHitNoteUp(const FInputActionValue& /*Value*/)
{
	if (!RhythmUIRef) return;

	const ERhythmJudgement J = RhythmUIRef->HitNote(ERhythmInputDir::Up);
}

void ARhythmFighterCharacter::OnHitNoteDown(const FInputActionValue& /*Value*/)
{
	if (!RhythmUIRef) return;

	const ERhythmJudgement J = RhythmUIRef->HitNote(ERhythmInputDir::Down);
}

void ARhythmFighterCharacter::OnHitNoteLeft(const FInputActionValue& /*Value*/)
{
	if (!RhythmUIRef) return;

	const ERhythmJudgement J = RhythmUIRef->HitNote(ERhythmInputDir::Left);
}

void ARhythmFighterCharacter::OnHitNoteRight(const FInputActionValue& /*Value*/)
{
	if (!RhythmUIRef) return;

	const ERhythmJudgement J = RhythmUIRef->HitNote(ERhythmInputDir::Right);
}

void ARhythmFighterCharacter::RegisterMoveTap(int8 Sign)
{
	const float Now = GetWorld()->GetTimeSeconds();

	if (Sign < 0)
	{
		if (Now - LastTapLeftTime <= DoubleTapWindow)
		{
			TryStartDash(-1);
			LastTapLeftTime = -1000.f;
			return;
		}
		LastTapLeftTime = Now;
	}
	else
	{
		if (Now - LastTapRightTime <= DoubleTapWindow)
		{
			TryStartDash(+1);
			LastTapRightTime = -1000.f;
			return;
		}
		LastTapRightTime = Now;
	}
}

bool ARhythmFighterCharacter::TryStartDash(int8 Sign)
{
	if (bDeathHandled) return false;
	if (bIsDashing) return false;

	// 게이지 소모: 너는 공격도 RhythmUIRef->TryConsumeGauge를 쓰고 있음 :contentReference[oaicite:5]{index=5}
	if (!RhythmUIRef) return false;
	if (!RhythmUIRef->TryConsumeGauge(DashCost)) return false;

	// 피격 중에도 사용: 피격 봉인(입력/이동 Disable)을 풀고 대쉬로 캔슬
	CancelHitReactForDash();

	StartDash(Sign);
	return true;
}

void ARhythmFighterCharacter::CancelHitReactForDash()
{
	if (!bInHitReact) return;

	bInHitReact = false;

	// ReceiveHit에서 봉인한 입력/이동을 해제 :contentReference[oaicite:6]{index=6}
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
	}

	// 피격 몽타주 끊기
	if (HitReactMontage && GetMesh() && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Stop(0.05f, HitReactMontage);
	}
}

void ARhythmFighterCharacter::StartDash(int8 Sign)
{
	bIsDashing = true;

	if (bDashInvincible)
		bInvulnerable = true;

	// 방향 고정(사이드스크롤)
	SetActorRotation(Sign > 0 ? FRotator(0, 0, 0) : FRotator(0, 180, 0));

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->SetMovementMode(MOVE_Walking);
	}

	// 빠르게 이동
	const FVector DashVel = FVector(1.f, 0.f, 0.f) * (DashSpeed * (float)Sign);
	LaunchCharacter(DashVel, true, true);

	GetWorldTimerManager().ClearTimer(DashTimerHandle);
	GetWorldTimerManager().SetTimer(DashTimerHandle, this, &ARhythmFighterCharacter::EndDash, DashDuration, false);

	// 대쉬 시작하자마자 한 번 찍고
	SpawnAfterImage();

	// 이후 DashDuration 동안 일정 간격으로 계속 찍기
	GetWorldTimerManager().ClearTimer(AfterImageTimerHandle);
	GetWorldTimerManager().SetTimer(
		AfterImageTimerHandle,
		this,
		&ARhythmFighterCharacter::SpawnAfterImage,
		AfterImageInterval,
		true
	);
}

void ARhythmFighterCharacter::EndDash()
{
	bIsDashing = false;
	bInvulnerable = false;
	GetWorldTimerManager().ClearTimer(AfterImageTimerHandle);
}

void ARhythmFighterCharacter::InitAfterImagePool()
{
	AfterImagePool.Reset();

	if (!AfterImageMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dash] AfterImageMaterial is null. AfterImage will not spawn."));
		return;
	}

	if (!GetMesh() || !GetMesh()->GetSkeletalMeshAsset())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dash] SkeletalMesh missing."));
		return;
	}

	AfterImagePool.SetNum(AfterImagePoolSize);

	for (int32 i = 0; i < AfterImagePoolSize; ++i)
	{
		UPoseableMeshComponent* PM = NewObject<UPoseableMeshComponent>(this);
		PM->RegisterComponent();

		PM->SetSkeletalMesh(GetMesh()->GetSkeletalMeshAsset());
		PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PM->SetGenerateOverlapEvents(false);
		PM->SetVisibility(false, true);
		PM->SetHiddenInGame(true);

		// 잔상 머티리얼 적용 + MID 만들기
		PM->SetMaterial(0, AfterImageMaterial);
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(AfterImageMaterial, this);
		PM->SetMaterial(0, MID);

		// 초기 파라미터
		MID->SetScalarParameterValue(AfterImageFadeParam, 0.f);
		MID->SetVectorParameterValue(AfterImageTintParam, AfterImageTint);

		AfterImagePool[i].Mesh = PM;
		AfterImagePool[i].MID = MID;
		AfterImagePool[i].Age = 0.f;
		AfterImagePool[i].bActive = false;
	}
}

void ARhythmFighterCharacter::SpawnAfterImage()
{
	if (!AfterImageMaterial) return;
	if (!GetMesh()) return;

	// 비활성 슬롯 찾기
	FAfterImageSlot* Slot = nullptr;
	for (FAfterImageSlot& S : AfterImagePool)
	{
		if (!S.bActive)
		{
			Slot = &S;
			break;
		}
	}

	// 전부 사용 중이면 가장 오래된 걸 재활용(간단하게 첫 번째 활성 슬롯 재사용)
	if (!Slot)
	{
		Slot = &AfterImagePool[0];
	}

	if (!Slot || !Slot->Mesh || !Slot->MID) return;

	UPoseableMeshComponent* PM = Slot->Mesh;

	// 현재 포즈 복사(고정)
	PM->SetSkeletalMesh(GetMesh()->GetSkeletalMeshAsset());
	PM->CopyPoseFromSkeletalComponent(GetMesh());

	// 위치/회전/스케일을 원본 메시 기준으로 세팅
	PM->SetWorldTransform(GetMesh()->GetComponentTransform());

	// 보이게 + 페이드 초기화
	Slot->Age = 0.f;
	Slot->bActive = true;

	PM->SetHiddenInGame(false);
	PM->SetVisibility(true, true);

	Slot->MID->SetVectorParameterValue(AfterImageTintParam, AfterImageTint);
	Slot->MID->SetScalarParameterValue(AfterImageFadeParam, AfterImageStartFade);
}

void ARhythmFighterCharacter::UpdateAfterImages(float DeltaSeconds)
{
	if (AfterImagePool.Num() == 0) return;

	for (FAfterImageSlot& S : AfterImagePool)
	{
		if (!S.bActive || !S.Mesh || !S.MID) continue;

		S.Age += DeltaSeconds;

		const float T = (AfterImageLifeTime > 0.f) ? (S.Age / AfterImageLifeTime) : 1.f;
		const float Fade = FMath::Clamp(AfterImageStartFade * (1.f - T), 0.f, 1.f);

		S.MID->SetScalarParameterValue(AfterImageFadeParam, Fade);

		if (S.Age >= AfterImageLifeTime)
		{
			S.bActive = false;
			S.Age = 0.f;

			S.Mesh->SetVisibility(false, true);
			S.Mesh->SetHiddenInGame(true);
		}
	}
}