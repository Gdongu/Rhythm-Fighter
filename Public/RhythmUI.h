#pragma once

#include "CoreMinimal.h"
#include "Components/RichTextBlock.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "RhythmUI.generated.h"

UENUM(BlueprintType)
enum class ERhythmJudgement : uint8 { Miss, Good, Perfect };

UENUM(BlueprintType)
enum class ERhythmInputDir : uint8
{
	Up,
	Down,
	Left,
	Right
};

UCLASS()
class RHYTHMFIGHTER_API URhythmUI : public UUserWidget
{
	GENERATED_BODY()

public:
	URhythmUI(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Note")
	UTexture2D* ArrowUpTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Note")
	UTexture2D* ArrowDownTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Note")
	UTexture2D* ArrowLeftTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Note")
	UTexture2D* ArrowRightTexture = nullptr;

	// 입력 잠금
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Flow")
	bool bAcceptInput = false;

	// 카운트다운 텍스트
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* CountdownText;

	// UI에서 카운트다운 표시/숨김
	UFUNCTION(BlueprintCallable, Category = "Rhythm|Flow")
	void SetCountdownText(const FString& S);

	UFUNCTION(BlueprintCallable, Category = "Rhythm|Flow")
	void SetAcceptInput(bool b);

	// 게이지 수치
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Gauge")
	float Gauge = 0.f;                    // 0~1

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Gauge")
	float GaugeGainPerHit = 0.10f;        // Perfect 획득량

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Gauge")
	float GreatMultiplier = 0.5f;         // Great 배수

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Gauge")
	float AttackCost = 0.5f;              // 공격 1회 소모량

	// 적 공격 게이지 감소량(Perfect 기준)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Enemy")
	float EnemyGaugeDamagePerHit = 0.20f;

	UFUNCTION(BlueprintCallable, Category = "Rhythm")
	ERhythmJudgement HitNote(ERhythmInputDir InputDir);           // 판정하고 게이지 반영
	UFUNCTION(BlueprintCallable, Category = "Rhythm")
	bool TryConsumeGauge(float Cost);     // 게이지 소비 (성공 여부)
	UFUNCTION(BlueprintPure, Category = "Rhythm")
	float GetGauge() const { return Gauge; }

	/* 디자이너에서 끌어다 놓을 바인딩 위젯들 (W_RhythmUI에 실제 존재해야 함) */
	UPROPERTY(meta = (BindWidget))
	UCanvasPanel* NoteCanvas;

	UPROPERTY(meta = (BindWidgetOptional)) UProgressBar* PlayerAttackBar;

	UPROPERTY(meta = (BindWidgetOptional)) UProgressBar* PlayerSuperBar;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* HitFlash;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* HitLine;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* JudgeText;

	// 이 거리 안에서만 판정 시도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Judge")
	float ClickEnableWindow = 60.f;

	// 미스일 때 노트 제거 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Judge")
	bool bRemoveOnMiss = false;

	void ShowJudgement(ERhythmJudgement J);

	/** 활성 노트들(UI 이미지) */
	UPROPERTY()
	TArray<UImage*> ActiveNotes;

	// 각 노트의 방향(이미지와 인덱스 맞추기)
	UPROPERTY()
	TArray<ERhythmInputDir> ActiveNoteDirs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Note")
	FVector2D NoteSize = FVector2D(64.f, 64.f); // 이미지 크기

	/** px/sec 이동 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm")
	float NoteSpeed = 500.f;

	/** 노트 시작 X좌표(오른쪽에서 왼쪽으로 흐르게 예시값) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm")
	float NoteStartX = 1000.f;

	/** 판정선 X좌표 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm")
	float HitLineX = 250.f;

	/** 노트 Y좌표(트랙 높이) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm")
	float NoteY = 300.f;

	/** 화면 밖으로 나가면 제거할 X 임계값 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm")
	float DespawnX = -100.f;

	/** 판정 윈도우(px) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm")
	float PerfectWindow = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm")
	float GoodWindow = 60.f;

	/** 판정 시 게이지 증가량 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm")
	float GaugePerPerfect = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm")
	float GaugePerGood = 0.05f;

	// 판정 이미지 색
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|FX")
	FLinearColor PerfectColor = FLinearColor(1.f, 0.95f, 0.2f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|FX")
	FLinearColor GreatColor = FLinearColor(0.3f, 0.8f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|FX")
	FLinearColor MissColor = FLinearColor(1.f, 0.2f, 0.2f, 1.f);

	// 판정 연출 파라미터
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|FX")
	float FlashDuration = 0.35f;          // 총 지속

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|FX")
	float FlashScale = 1.6f;              // 최초 번쩍 스케일

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|FX")
	float TextRise = 40.f;                // 텍스트가 살짝 위로 떠오르는 거리

	/** 노트 스폰 (C++/BP 어디서든 호출 가능) */
	UFUNCTION(BlueprintCallable, Category = "Rhythm")
	void SpawnNote();

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* TimeText;

	UFUNCTION(BlueprintCallable, Category = "Rhythm")
	void UpdateRemainingTime(float Seconds);

	// 필살기 게이지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Super")
	float SuperGauge = 0.f;                          // 0~1

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Super")
	float SuperGainPerAttack = 0.25f;                // 공격 1회당 슈퍼 상승량

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Super")
	float SuperCost = 1.0f;                          // 필살기 필요량

	UPROPERTY(meta = (BindWidgetOptional)) UProgressBar* PlayerHPBar;

	// 필살기 커맨드 프롬프트
	UPROPERTY(meta = (BindWidgetOptional)) class URichTextBlock* FinisherText;
	UPROPERTY(meta = (BindWidgetOptional)) class UTextBlock* FinisherTimerText;

	UPROPERTY() TArray<FString> CommandFull;
	int32 ProgressIndex = 0;                 

	// 표시 업데이트 함수
	UFUNCTION() void UpdateFinisherDisplay();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Super")
	float FinisherTimeLimit = 3.0f;                  // 전체 제한 시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Super")
	TArray<FString> FinisherKeyPool{ TEXT("A"), TEXT("S"), TEXT("D"), TEXT("F") }; // 커맨드

	// 상태
	UPROPERTY(BlueprintReadOnly, Category = "Rhythm|Super")
	bool bFinisherActive = false;

	bool bLastFinisherSuccess = false;

	UFUNCTION(BlueprintCallable, Category = "Rhythm|Super")
	bool ConsumeLastFinisherSuccess()             // 캐릭터가 읽고 리셋
	{
		const bool b = bLastFinisherSuccess;
		bLastFinisherSuccess = false;
		return b;
	}

	UFUNCTION(BlueprintCallable, Category = "Rhythm|Super")
	void AddSuperGauge(float Delta);

	UFUNCTION(BlueprintCallable, Category = "Rhythm|Super")
	bool TryStartFinisher();                         // 게이지 차면 커맨드 생성/시작

	UFUNCTION(BlueprintCallable, Category = "Rhythm|Super")
	void OnFinisherInput(const FString& Key);        // 키 입력 처리

	UFUNCTION(BlueprintCallable, Category = "Rhythm|Super")
	void CancelFinisher(bool bConsumeGauge);         // 실패 처리

	// 내부
	FString CurrentCmd;      // 예: "A S D F"
	int32   CmdIndex = 0;    // 현재 몇 번째 글자
	float   FinisherRemain = 0.f; // 남은 시간

	// ---- (새로 추가) 적 바 바인딩 ----
	UPROPERTY(meta = (BindWidgetOptional))
	UProgressBar* EnemyHPBar;

	UPROPERTY(meta = (BindWidgetOptional))
	UProgressBar* EnemyAttackBar;

	UPROPERTY(meta = (BindWidgetOptional))
	UProgressBar* EnemySuperBar;

	UFUNCTION(BlueprintCallable, Category = "Rhythm|Player")
	void SetPlayerActor(AActor* InPlayer);

	// 적 캐릭터/스탯 캐시
	UFUNCTION(BlueprintCallable, Category = "Rhythm|Enemy")
	void SetEnemyActor(AActor* InEnemy);

	UFUNCTION() void OnPlayerHealthChanged(float NewHP, float MaxHP);
	UFUNCTION() void OnEnemyHealthChanged(float NewHP, float MaxHP);

protected:
	/** 위젯 생성 시 */
	virtual void NativeConstruct() override;

	/** 매 프레임 갱신 */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:

	/** 내부 헬퍼: 노트 제거 */
	void RemoveNoteAt(int32 Index);

	void AddGauge(float Delta);

	void AddEnemyGauge(float Delta);

	float LastHitTime = -100.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm|Judge")
	float HitDebounce = 0.06f; // 60ms

	// 플래시 상태
	bool  bFlashActive = false;
	float FlashTime = 0.f; // 0~FlashDuration
	ERhythmJudgement LastJudge = ERhythmJudgement::Miss;

	float LastFinisherInputAt = -100.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm|Super")
	float FinisherInputDebounce = 0.08f; // 80ms

	TWeakObjectPtr<AActor> PlayerActor;
	class UCombatStatsComponent* PlayerStats = nullptr;

	TWeakObjectPtr<AActor> EnemyActor;
	class UCombatStatsComponent* EnemyStats = nullptr;

	void UpdatePlayerBars();
	void UpdateEnemyBars();
};
