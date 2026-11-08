#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Animation/AnimMontage.h"        
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Sound/SoundBase.h"
#include "RhythmFighterCharacter.generated.h"

class URhythmUI;
class UCombatStatsComponent;
class UBoxComponent;
class UInputAction;
class ANPCInteractable;
class UInventoryComponent;
class UUserWidget;
class UParticleSystem;
class UParticleSystemComponent;
class UPoseableMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

USTRUCT()
struct FAfterImageSlot
{
	GENERATED_BODY()

	UPROPERTY() UPoseableMeshComponent* Mesh = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* MID = nullptr;

	float Age = 0.f;
	bool bActive = false;
};

UENUM(BlueprintType)
enum class ECharMode : uint8 { Exploration, Combat };

UCLASS(abstract)
class RHYTHMFIGHTER_API ARhythmFighterCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UInventoryComponent* Inventory;
	UPROPERTY(EditDefaultsOnly, Category = "Input") UInputAction* IA_Inventory; // I 키
	UPROPERTY(EditDefaultsOnly, Category = "UI") TSubclassOf<UUserWidget> InventoryWidgetClass;

	UUserWidget* InventoryWidget = nullptr;
	bool bInventoryOpen = false;

	void OnInventoryPressed();
	void ShowInventory(bool bShow);

	void OnHitNoteUp(const FInputActionValue& Value);
	void OnHitNoteDown(const FInputActionValue& Value);
	void OnHitNoteLeft(const FInputActionValue& Value);
	void OnHitNoteRight(const FInputActionValue& Value);

	/** Input Actions (Enhanced Input) */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MouseLookAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* AttackAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MoveRightAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* HitNoteAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* IA_NoteUp;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* IA_NoteDown;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* IA_NoteLeft;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* IA_NoteRight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* FinisherStartAction; // Shift

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* KeyAAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* KeySAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* KeyDAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* KeyFAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputMappingContext* NormalIMC;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputMappingContext* FinisherIMC;

	UPROPERTY(EditAnywhere, Category = "Input") UInputMappingContext* IMC_Exploration = nullptr;

	// 필살기 연출
	UPROPERTY(EditAnywhere, Category = "Animations") UAnimMontage* FinisherMontage = nullptr;

	// 카메라 연출(간단 줌)
	float SavedFOV = 90.f;               // FOV 원복용 저장값
	void StartCinematicZoom();
	void EndCinematicZoom();
	FTimerHandle ZoomHandle;

	// 콜백
	void OnFinisherStart(const FInputActionValue& Value);
	void OnKeyA(const FInputActionValue& Value);
	void OnKeyS(const FInputActionValue& Value);
	void OnKeyD(const FInputActionValue& Value);
	void OnKeyF(const FInputActionValue& Value);

	bool bFinisherCinematic = false; // 상태

	void OnAttack(const FInputActionValue& Value);

	virtual void BeginPlay() override;

	// 좌우 이동(Enhanced Input)
	void MoveRight(const FInputActionValue& Value);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UCombatStatsComponent* Stats = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	class UBoxComponent* Hitbox = nullptr;

	// 현재 공격창 활성/피격 중복 방지
	bool bHitboxActive = false;
	TSet<TWeakObjectPtr<AActor>> AlreadyHit;

	UFUNCTION()
	void OnHitboxOverlap(UPrimitiveComponent* Overlapped, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep,
		const FHitResult& Sweep);

	float CurrentHitDamage = 10.f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	FName TeamTag = "Player";

	UFUNCTION(BlueprintPure)
	bool IsEnemy(const AActor* Other) const
	{
		if (!Other) return false;
		const bool bMeHasTeam = ActorHasTag("Player") || ActorHasTag("Enemy");
		const bool bOtherHasTeam = Other->ActorHasTag("Player") || Other->ActorHasTag("Enemy");
		if (!bMeHasTeam || !bOtherHasTeam) return true; // 태그 없으면 일단 적 처리
		const bool bMeIsPlayer = ActorHasTag("Player");
		return (bMeIsPlayer && Other->ActorHasTag("Enemy")) ||
			(!bMeIsPlayer && Other->ActorHasTag("Player"));
	}

public:
	ARhythmFighterCharacter();

	// 모드
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mode")
	ECharMode DefaultMode = ECharMode::Exploration;

	UFUNCTION(BlueprintCallable, Category = "Mode")
	void SetCharacterMode(ECharMode NewMode);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mode|Combat")
	bool bUseCameraActorForCombat = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Mode|Combat")
	AActor* CombatCameraActor = nullptr;

	UFUNCTION(BlueprintPure, Category = "Mode")
	ECharMode GetCharacterMode() const { return CurrentMode; }

	UFUNCTION(BlueprintCallable, Category = "Combat|Hitbox")
	void StartHitbox(FName AttachSocket, FVector BoxExtent, float Damage);

	UFUNCTION(BlueprintCallable, Category = "Combat|Hitbox")
	void EndHitbox();

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_Interact;

	// 현재 상호작용 가능한 NPC
	UPROPERTY(VisibleInstanceOnly, Category = "Interact")
	ANPCInteractable* CurrentInteractable = nullptr;

	// NPC가 범위 진입/이탈 시 세팅할 수 있도록 간단한 Setter
	void SetCurrentInteractable(ANPCInteractable* NewTarget) { CurrentInteractable = NewTarget; }

	UFUNCTION() void OnSelfDied();

protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION()
	void PlayRandomFightAnimation();

	UFUNCTION()
	void EndAttack();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
	TArray<UAnimMontage*> RandomAnimations;

	void OnInteractPressed();

protected:
	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

public:
	virtual void Tick(float DeltaSeconds) override;

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoLook(float Yaw, float Pitch);

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UPROPERTY(VisibleAnywhere)
	bool IsAlive = true;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool IsAttacking = false;

	UFUNCTION(BlueprintCallable)
	void SetRhythmUI(URhythmUI* InUI) { RhythmUIRef = InUI; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rhythm")
	URhythmUI* RhythmUIRef = nullptr;

	bool bPrevFinisherActive = false;

	// --- Hit React / SuperArmor ---
	UPROPERTY(EditAnywhere, Category = "Combat|HitReact")
	UAnimMontage* HitReactMontage = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|HitReact")
	bool bSuperArmor = false;                 // 노티파이로 on/off

	UFUNCTION(BlueprintCallable, Category = "Combat|HitReact")
	void SetSuperArmor(bool bEnable) { bSuperArmor = bEnable; }

	void ReceiveHit(float Damage, AActor* From);   // 데미지 들어오면 호출 (공격 끊기/피격 몽타주)

	// 사망 연출용
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Death")
	UAnimMontage* DeathMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Death")
	UAnimationAsset* DeathPoseAsset = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bInHitReact = false;

	UFUNCTION()
	void OnPlayerDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Death")
	bool bCorpseStateEntered = false;

	bool IsDead() const;

	UFUNCTION()
	void EnterCorpseState_Player();

	UFUNCTION()
	void HandleHealthChanged(float NewHP, float MaxHP);

	void DieAndNotify();

	UPROPERTY(EditAnywhere, Category = "FX|Attack")
	UParticleSystem* AttackFX = nullptr;

	UPROPERTY(EditAnywhere, Category = "FX|Attack")
	FName AttackFXSocket = "hand_r";

	UPROPERTY(EditAnywhere, Category = "FX|Hit")
	UParticleSystem* HitFX = nullptr;

	UPROPERTY(EditAnywhere, Category = "FX|Hit")
	FName HitFXSocket = "spine_02";

	UPROPERTY(EditAnywhere, Category = "SFX|Attack")
	USoundBase* AttackSwingSFX = nullptr;   // 휘두르는 소리

	UPROPERTY(EditAnywhere, Category = "SFX|Attack")
	USoundBase* AttackVoiceSFX = nullptr;   // "얍"

	UPROPERTY(EditAnywhere, Category = "SFX|Hit")
	USoundBase* HitImpactSFX = nullptr;     // 맞는 타격음

	UPROPERTY(EditAnywhere, Category = "SFX|Hit")
	USoundBase* HitVoiceSFX = nullptr;      // "윽"

	// --- Dash ---
	UPROPERTY(EditAnywhere, Category = "Dash")
	float DashCost = 0.1f;

	UPROPERTY(EditAnywhere, Category = "Dash")
	float DashSpeed = 1200.f;

	UPROPERTY(EditAnywhere, Category = "Dash")
	float DashDuration = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Dash")
	float DoubleTapWindow = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Dash")
	bool bDashInvincible = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dash")
	bool bIsDashing = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dash")
	bool bInvulnerable = false; // 실제 무적 상태(대쉬 중 true)

	UFUNCTION(BlueprintPure, Category = "Dash")
	bool IsInvulnerable() const { return bInvulnerable; }

	float LastTapLeftTime = -1000.f;
	float LastTapRightTime = -1000.f;
	int8 PrevMoveSign = 0;

	FTimerHandle DashTimerHandle;

	void RegisterMoveTap(int8 Sign);
	bool TryStartDash(int8 Sign);   // -1 left, +1 right
	void StartDash(int8 Sign);
	void EndDash();
	void CancelHitReactForDash();

	UPROPERTY(EditAnywhere, Category = "Dash|AfterImage")
	UMaterialInterface* AfterImageMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Dash|AfterImage")
	FLinearColor AfterImageTint = FLinearColor(0.7f, 0.9f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Dash|AfterImage")
	int32 AfterImagePoolSize = 8;

	UPROPERTY(EditAnywhere, Category = "Dash|AfterImage")
	float AfterImageInterval = 0.03f;

	UPROPERTY(EditAnywhere, Category = "Dash|AfterImage")
	float AfterImageLifeTime = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Dash|AfterImage")
	float AfterImageStartFade = 0.85f; // 처음 투명도(1에 가까울수록 진함)

	UPROPERTY(EditAnywhere, Category = "Dash|AfterImage")
	FName AfterImageFadeParam = TEXT("Fade");

	UPROPERTY(EditAnywhere, Category = "Dash|AfterImage")
	FName AfterImageTintParam = TEXT("Tint");

	// 내부
	UPROPERTY() TArray<FAfterImageSlot> AfterImagePool;
	FTimerHandle AfterImageTimerHandle;

	void InitAfterImagePool();
	void SpawnAfterImage();
	void UpdateAfterImages(float DeltaSeconds);

	// 공격 FX를 끄기 위해 핸들 저장용
	UPROPERTY()
	UParticleSystemComponent* ActiveAttackFX = nullptr;

	void StartAttackFX();
	void StopAttackFX();

	UFUNCTION(BlueprintCallable)
	void PlayHitFX(AActor* Victim);

	private:
		ECharMode CurrentMode = ECharMode::Exploration;

		void ApplyExplorationMode();
		void ApplyCombatMode();
		void ApplyIMC(ECharMode Mode);
		void ApplyViewTarget(ECharMode Mode);

		FTimerHandle DeathEndHandle;
		bool bDeathHandled = false;
		void NotifyBattleEndAsLoser();

};