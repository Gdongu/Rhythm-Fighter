#include "RhythmUI.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "TimerManager.h"               
#include "Engine/Texture2D.h"           
#include "Engine/World.h"
#include "CombatStatsComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"

namespace
{
	// 파일 내부 헬퍼: UWidget → UCanvasPanelSlot 캐스팅
	static UCanvasPanelSlot* AsCanvasSlot(UWidget* W)
	{
		return (W && W->Slot) ? Cast<UCanvasPanelSlot>(W->Slot) : nullptr;
	}
}

URhythmUI::URhythmUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void URhythmUI::NativeConstruct()
{
	Super::NativeConstruct();

	// 1) 위젯이 클릭을 잡아먹지 않게 (보이되 클릭 통과)
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	UpdatePlayerBars();

	// 3) 시작 시 숨겨야 하는 UI
	if (FinisherText)      FinisherText->SetVisibility(ESlateVisibility::Collapsed);
	if (FinisherTimerText) FinisherTimerText->SetVisibility(ESlateVisibility::Collapsed);
	if (HitFlash)          HitFlash->SetRenderOpacity(0.f);
	if (JudgeText)         JudgeText->SetRenderOpacity(0.f);

	// 4) 카운트다운 텍스트는 화면 정중앙 고정 + 처음엔 안 보이게
	if (CountdownText)
	{
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(CountdownText->Slot))
		{
			S->SetAnchors(FAnchors(0.5f, 0.5f));
			S->SetAlignment(FVector2D(0.5f, 0.5f));
			S->SetPosition(FVector2D(0.f, 0.f));
			S->SetAutoSize(true); // Size to Content
		}
		CountdownText->SetJustification(ETextJustify::Center);
		CountdownText->SetRenderOpacity(0.f);
	}

	// 5) 레이아웃이 잡힌 다음 프레임에 위치 계산
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			// 오른쪽 끝에서 조금 안쪽에서 스폰
			const FVector2D ViewSize = UWidgetLayoutLibrary::GetViewportSize(this);
			const float RightMargin = -1000.f;
			NoteStartX = ViewSize.X - RightMargin;

			// 판정선/플래시/판정텍스트를 트랙 위치에 정렬
			if (HitLine)
			{
				if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(HitLine->Slot))
				{
					S->SetAlignment(FVector2D(0.5f, 0.5f));
					S->SetPosition(FVector2D(HitLineX, NoteY));
				}
			}
			if (JudgeText)
			{
				if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(JudgeText->Slot))
				{
					S->SetAlignment(FVector2D(0.5f, 0.5f));
					S->SetPosition(FVector2D(HitLineX, NoteY));
				}
			}
			if (HitFlash)
			{
				if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(HitFlash->Slot))
				{
					S->SetAlignment(FVector2D(0.5f, 0.5f));
					S->SetPosition(FVector2D(HitLineX, NoteY));
				}
			}
		}));
}

void URhythmUI::SpawnNote()
{
	if (!NoteCanvas) return;

	// 1) 랜덤 방향 뽑기
	const int32 Rand = FMath::RandRange(0, 3);
	ERhythmInputDir Dir = ERhythmInputDir::Up;
	switch (Rand)
	{
	case 0: Dir = ERhythmInputDir::Up;    break;
	case 1: Dir = ERhythmInputDir::Down;  break;
	case 2: Dir = ERhythmInputDir::Left;  break;
	case 3: Dir = ERhythmInputDir::Right; break;
	}

	// 2) 방향에 맞는 텍스처 선택
	UTexture2D* Tex = nullptr;
	switch (Dir)
	{
	case ERhythmInputDir::Up:    Tex = ArrowUpTexture;    break;
	case ERhythmInputDir::Down:  Tex = ArrowDownTexture;  break;
	case ERhythmInputDir::Left:  Tex = ArrowLeftTexture;  break;
	case ERhythmInputDir::Right: Tex = ArrowRightTexture; break;
	}

	// 3) UImage 생성
	UImage* NoteImage = NewObject<UImage>(NoteCanvas);
	if (Tex)
	{
		NoteImage->SetBrushFromTexture(Tex, true);
	}
	NoteCanvas->AddChild(NoteImage);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(NoteImage->Slot))
	{
		CanvasSlot->SetAutoSize(false);
		CanvasSlot->SetSize(NoteSize);
		CanvasSlot->SetPosition(FVector2D(NoteStartX, NoteY));
	}

	ActiveNotes.Add(NoteImage);
	ActiveNoteDirs.Add(Dir); // 방향도 같이 저장
}

ERhythmJudgement URhythmUI::HitNote(ERhythmInputDir InputDir)
{
	if (!bAcceptInput) return ERhythmJudgement::Miss;

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (Now - LastHitTime < HitDebounce)
		return ERhythmJudgement::Miss; // 바로 직전 입력은 무시

	LastHitTime = Now;

	if (!NoteCanvas || ActiveNotes.Num() == 0)
	{
		ShowJudgement(ERhythmJudgement::Miss);
		return ERhythmJudgement::Miss;
	}

	// 1) 입력된 방향과 같은 노트들 중, 판정선에 가장 가까운 것 찾기
	int32 BestIdx = INDEX_NONE;
	float BestDist = BIG_NUMBER;

	for (int32 i = 0; i < ActiveNotes.Num(); ++i)
	{
		if (!ActiveNoteDirs.IsValidIndex(i)) continue;
		if (ActiveNoteDirs[i] != InputDir) continue; // 방향 다르면 스킵

		if (auto* S = AsCanvasSlot(ActiveNotes[i]))
		{
			const float d = FMath::Abs(S->GetPosition().X - HitLineX);
			if (d < BestDist)
			{
				BestDist = d;
				BestIdx = i;
			}
		}
	}

	if (BestIdx == INDEX_NONE)
	{
		// 해당 방향 노트가 없음
		ShowJudgement(ERhythmJudgement::Miss);
		return ERhythmJudgement::Miss;
	}

	if (BestDist > ClickEnableWindow)
	{
		ShowJudgement(ERhythmJudgement::Miss);
		return ERhythmJudgement::Miss;
	}

	ERhythmJudgement R = ERhythmJudgement::Miss;

	if (BestDist <= PerfectWindow)
	{
		RemoveNoteAt(BestIdx);
		AddGauge(GaugeGainPerHit);
		AddEnemyGauge(-EnemyGaugeDamagePerHit);
		R = ERhythmJudgement::Perfect;
	}
	else if (BestDist <= GoodWindow)
	{
		RemoveNoteAt(BestIdx);
		AddGauge(GaugeGainPerHit * GreatMultiplier);
		AddEnemyGauge(-EnemyGaugeDamagePerHit * GreatMultiplier);
		R = ERhythmJudgement::Good;
	}
	else
	{
		if (bRemoveOnMiss) RemoveNoteAt(BestIdx);
		R = ERhythmJudgement::Miss;
	}

	ShowJudgement(R);
	return R;
}

bool URhythmUI::TryConsumeGauge(float Cost)
{
	if (!PlayerStats) return false;
	if (PlayerStats->AttackGauge >= Cost)
	{
		PlayerStats->AttackGauge = FMath::Clamp(PlayerStats->AttackGauge - Cost, 0.f, 1.f);
		UpdatePlayerBars();
		return true;
	}
	return false;
}

void URhythmUI::ShowJudgement(ERhythmJudgement J)
{
	LastJudge = J;
	bFlashActive = true;
	FlashTime = 0.f;

	// 색/문구 지정
	FLinearColor C = MissColor;
	FText T = FText::FromString(TEXT("MISS"));
	if (J == ERhythmJudgement::Perfect) { C = PerfectColor; T = FText::FromString(TEXT("PERFECT")); }
	else if (J == ERhythmJudgement::Good) { C = GreatColor; T = FText::FromString(TEXT("GREAT")); }

	if (HitFlash) {
		HitFlash->SetColorAndOpacity(C);
		HitFlash->SetRenderOpacity(1.f);
		HitFlash->SetRenderScale(FVector2D(FlashScale, FlashScale)); // 크게 번쩍
		if (auto* S = AsCanvasSlot(HitFlash)) {
			S->SetPosition(FVector2D(HitLineX, NoteY));
		}
	}
	if (JudgeText) {
		JudgeText->SetText(T);
		JudgeText->SetColorAndOpacity(C);
		JudgeText->SetRenderOpacity(1.f);
		if (auto* S = AsCanvasSlot(JudgeText)) {
			S->SetPosition(FVector2D(HitLineX, NoteY - 0.f)); // 시작 위치
		}
	}
}

void URhythmUI::RemoveNoteAt(int32 Index)
{
	if (!ActiveNotes.IsValidIndex(Index)) return;

	if (UWidget* W = ActiveNotes[Index])
	{
		W->RemoveFromParent();
	}
	ActiveNotes.RemoveAt(Index);

	if (ActiveNoteDirs.IsValidIndex(Index))
	{
		ActiveNoteDirs.RemoveAt(Index);
	}
}

void URhythmUI::AddGauge(float Delta)
{
	if (PlayerStats)
	{
		PlayerStats->AttackGauge = FMath::Clamp(PlayerStats->AttackGauge + Delta, 0.f, 1.f);
	}
	UpdatePlayerBars(); // 막대 즉시 반영
}

void URhythmUI::AddEnemyGauge(float Delta)
{
	if (EnemyStats)
	{
		EnemyStats->AttackGauge = FMath::Clamp(EnemyStats->AttackGauge + Delta, 0.f, 1.f);
	}
	UpdateEnemyBars();
}

void URhythmUI::UpdateRemainingTime(float Seconds)
{
	if (!TimeText) return;

	// mm:ss 로 표기 (반올림)
	int32 Total = FMath::Max(0, FMath::RoundToInt(Seconds));
	int32 Min = Total / 60;
	int32 Sec = Total % 60;

	TimeText->SetText(FText::FromString(
		FString::Printf(TEXT("%02d:%02d"), Min, Sec)
	));
}

void URhythmUI::SetCountdownText(const FString& S)
{
	if (!CountdownText) return;
	if (S.IsEmpty())
	{
		CountdownText->SetRenderOpacity(0.f);
		return;
	}
	CountdownText->SetText(FText::FromString(S));
	CountdownText->SetRenderOpacity(1.f);
}

void URhythmUI::SetAcceptInput(bool b)
{
	bAcceptInput = b;
}

void URhythmUI::AddSuperGauge(float Delta)
{
	if (PlayerStats)
	{
		PlayerStats->SuperGauge = FMath::Clamp(PlayerStats->SuperGauge + Delta, 0.f, 1.f);
	}
	UpdatePlayerBars();
}

bool URhythmUI::TryStartFinisher()
{
	if (bFinisherActive || !FinisherText || !PlayerStats) return false;
	if (PlayerStats->SuperGauge < SuperCost) return false;

	// 1) 풀 정리: BP가 망가뜨려도 안전하게
	TArray<FString> Pool = FinisherKeyPool;
	// 공백/빈값 제거 + 대문자화
	for (int32 i = Pool.Num() - 1; i >= 0; --i)
	{
		Pool[i] = Pool[i].ToUpper().TrimStartAndEnd();
		if (Pool[i].IsEmpty())
		{
			Pool.RemoveAt(i);
		}
	}
	// 유효 키가 2개 미만이면 강제로 기본값 사용
	if (Pool.Num() < 2)
	{
		Pool = { TEXT("A"), TEXT("S"), TEXT("D"), TEXT("F") };
	}

	// 2) 확정 랜덤: 현재 시간으로 시드 고정해서 매번 다른 시퀀스
	const int32 Len = FMath::RandRange(4, 5);
	CommandFull.Reset(Len);
	FRandomStream R(FDateTime::Now().GetTicks() & 0xFFFFFFFF); // 간단 시드
	for (int32 i = 0; i < Len; ++i)
	{
		const int32 idx = R.RandRange(0, Pool.Num() - 1);
		CommandFull.Add(Pool[idx]);   // 예: "A","S","D","F"에서 랜덤
	}
	ProgressIndex = 0;

	// 3) 상태 세팅 & 표시
	bFinisherActive = true;
	bLastFinisherSuccess = false;
	FinisherRemain = FinisherTimeLimit;

	FinisherText->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (FinisherTimerText)
	{
		FinisherTimerText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), FinisherRemain)));
		FinisherTimerText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	// 디버그: 실제 생성된 커맨드 출력 (원하면 유지)
	UE_LOG(LogTemp, Log, TEXT("[FIN] Pool=%s"), *FString::Join(Pool, TEXT(",")));
	UE_LOG(LogTemp, Log, TEXT("[FIN] Command=%s"), *FString::Join(CommandFull, TEXT(" ")));

	UpdateFinisherDisplay();
	PlayerStats->SuperGauge = FMath::Clamp(PlayerStats->SuperGauge - SuperCost, 0.f, 1.f);
	UpdatePlayerBars();
	return true;
}

void URhythmUI::OnFinisherInput(const FString& Key)
{
	if (!bFinisherActive || !FinisherText) return;

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (Now - LastFinisherInputAt < FinisherInputDebounce) return; // 짧은 중복 호출 무시
	LastFinisherInputAt = Now;

	if (!CommandFull.IsValidIndex(ProgressIndex))
	{
		CancelFinisher(false);
		return;
	}

	const FString Expect = CommandFull[ProgressIndex].ToUpper();
	const FString Input = Key.ToUpper();
	UE_LOG(LogTemp, Log, TEXT("[FIN] Input=%s Expect=%s (idx %d/%d)"), *Input, *Expect, ProgressIndex, CommandFull.Num());

	if (Input == Expect)
	{
		++ProgressIndex;          // 맞췄으니 한 칸 전진
		if (ProgressIndex >= CommandFull.Num())
		{
			// 모든 키 성공
			bFinisherActive = false;
			bLastFinisherSuccess = true;
			FinisherText->SetVisibility(ESlateVisibility::Collapsed);
			if (FinisherTimerText) FinisherTimerText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			UpdateFinisherDisplay(); // 남은 키 새로 그리기 (앞쪽은 초록색)
		}
	}
	else
	{
		// 오입력 → 실패
		CancelFinisher(false);
	}
}

void URhythmUI::CancelFinisher(bool /*bConsumeGauge*/)
{
	bFinisherActive = false;
	bLastFinisherSuccess = false;
	CommandFull.Reset();
	ProgressIndex = 0;
	if (FinisherText)      FinisherText->SetVisibility(ESlateVisibility::Collapsed);
	if (FinisherTimerText) FinisherTimerText->SetVisibility(ESlateVisibility::Collapsed);
}

// Tick에서 제한시간 감소
void URhythmUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdatePlayerBars();   // 매 프레임 게이지 갱신
	UpdateEnemyBars();
	// 1) 노트 이동/소멸
	for (int32 i = ActiveNotes.Num() - 1; i >= 0; --i)
	{
		if (UCanvasPanelSlot* CanvasSlot = AsCanvasSlot(ActiveNotes[i]))
		{
			FVector2D Pos = CanvasSlot->GetPosition();
			Pos.X -= NoteSpeed * InDeltaTime;          // 오른쪽 → 왼쪽
			CanvasSlot->SetPosition(Pos);

			if (Pos.X <= DespawnX)
			{
				RemoveNoteAt(i);
			}
		}
	}

	// 2) 판정 플래시/텍스트 연출 (페이드 + 위로 살짝 상승)
	if (bFlashActive)
	{
		FlashTime += InDeltaTime;
		const float t = FMath::Clamp(FlashTime / FlashDuration, 0.f, 1.f);
		const float fade = 1.f - t;                       // 1 → 0
		const float scale = FMath::Lerp(FlashScale, 1.f, t);

		if (HitFlash)
		{
			HitFlash->SetRenderOpacity(fade);
			HitFlash->SetRenderScale(FVector2D(scale, scale));
		}
		if (JudgeText)
		{
			JudgeText->SetRenderOpacity(fade);
			if (UCanvasPanelSlot* S = AsCanvasSlot(JudgeText))
			{
				const float rise = FMath::Lerp(0.f, TextRise, t);
				S->SetPosition(FVector2D(HitLineX, NoteY - rise));
			}
		}

		if (t >= 1.f)
		{
			bFlashActive = false;
			if (HitFlash)  HitFlash->SetRenderOpacity(0.f);
			if (JudgeText) JudgeText->SetRenderOpacity(0.f);
		}
	}

	// 3) 필살기 커맨드 제한시간
	if (bFinisherActive)
	{
		FinisherRemain -= InDeltaTime;

		if (FinisherTimerText)
		{
			FinisherTimerText->SetText(
				FText::FromString(FString::Printf(TEXT("%.1f"), FMath::Max(0.f, FinisherRemain)))
			);
		}

		if (FinisherRemain <= 0.f)
		{
			CancelFinisher(/*bConsumeGauge=*/false);  // 시작 시 이미 게이지 소모
		}
	}
	UpdatePlayerBars();
	UpdateEnemyBars();
}

void URhythmUI::UpdateFinisherDisplay()
{
	if (!FinisherText) return;

	// 예: "<Green>A</>  <White>S</>  <Gray>D</>  <Gray>F</>"
	FString Markup;
	for (int32 i = 0; i < CommandFull.Num(); ++i)
	{
		const FString& Key = CommandFull[i];
		const TCHAR* Tag =
			(i < ProgressIndex) ? TEXT("Green") :     // 이미 맞춘 키
			(i == ProgressIndex) ? TEXT("White") :     // 지금 눌러야 할 키
			TEXT("Gray");       // … 아직 남은 키

		Markup += FString::Printf(TEXT("<%s>%s</>"), Tag, *Key);
		if (i < CommandFull.Num() - 1) Markup += TEXT("  "); // 보기 좋게 공백
	}

	FinisherText->SetText(FText::FromString(Markup));
}

void URhythmUI::SetPlayerActor(AActor* InPlayer)
{
	PlayerActor = InPlayer;
	PlayerStats = nullptr;
	if (AActor* P = PlayerActor.Get())
	{
		PlayerStats = P->FindComponentByClass<UCombatStatsComponent>();
		if (PlayerStats)
		{
			// HP 바뀌면 즉시 반영 (있으면 바인딩, 없으면 무시)
			PlayerStats->OnHealthChanged.AddDynamic(this, &URhythmUI::OnPlayerHealthChanged);
		}
	}
	UpdatePlayerBars();
}

void URhythmUI::SetEnemyActor(AActor* InEnemy)
{
	EnemyActor = InEnemy;
	EnemyStats = nullptr;

	if (AActor* E = EnemyActor.Get())
	{
		EnemyStats = E->FindComponentByClass<UCombatStatsComponent>();
		if (EnemyStats)
		{
			EnemyStats->OnHealthChanged.AddDynamic(this, &URhythmUI::OnEnemyHealthChanged);
		}
	}
	UpdateEnemyBars();
}

void URhythmUI::UpdatePlayerBars()
{
	if (!PlayerStats)
	{
		if (AActor* P = PlayerActor.Get())
			PlayerStats = P->FindComponentByClass<UCombatStatsComponent>();
	}
	if (!PlayerStats) return;

	if (PlayerAttackBar)
		PlayerAttackBar->SetPercent(FMath::Clamp(PlayerStats->AttackGauge, 0.f, 1.f));
	if (PlayerSuperBar)
		PlayerSuperBar->SetPercent(FMath::Clamp(PlayerStats->SuperGauge, 0.f, 1.f));

	// HP는 델리게이트에서도 갱신되지만 틱에서도 안전망으로 갱신
	if (PlayerHPBar)
	{
		const float HpPct = (PlayerStats->MaxHP > 0.f) ? (PlayerStats->HP / PlayerStats->MaxHP) : 0.f;
		PlayerHPBar->SetPercent(FMath::Clamp(HpPct, 0.f, 1.f));
	}
}

void URhythmUI::UpdateEnemyBars()
{
	if (!EnemyStats)
	{
		// 포인터가 끊긴 경우 다시 시도
		if (AActor* E = EnemyActor.Get())
		{
			EnemyStats = E->FindComponentByClass<UCombatStatsComponent>();
		}
	}
	if (!EnemyStats) return;

	// HP: 0..1로 정규화
	const float HpPct = (EnemyStats->MaxHP > 0.f) ? (EnemyStats->HP / EnemyStats->MaxHP) : 0.f;
	if (EnemyHPBar)     EnemyHPBar->SetPercent(FMath::Clamp(HpPct, 0.f, 1.f));
	if (EnemyAttackBar) EnemyAttackBar->SetPercent(FMath::Clamp(EnemyStats->AttackGauge, 0.f, 1.f));
	if (EnemySuperBar)  EnemySuperBar->SetPercent(FMath::Clamp(EnemyStats->SuperGauge, 0.f, 1.f));
}

void URhythmUI::OnPlayerHealthChanged(float NewHP, float MaxHP)
{
	if (PlayerHPBar && MaxHP > 0.f)
	{
		PlayerHPBar->SetPercent(FMath::Clamp(NewHP / MaxHP, 0.f, 1.f));
	}
}

void URhythmUI::OnEnemyHealthChanged(float NewHP, float MaxHP)
{
	if (EnemyHPBar && MaxHP > 0.f)
	{
		EnemyHPBar->SetPercent(FMath::Clamp(NewHP / MaxHP, 0.f, 1.f));
	}
}