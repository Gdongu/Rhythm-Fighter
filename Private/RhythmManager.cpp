#include "RhythmManager.h"
#include "Components/AudioComponent.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "Sound/SoundWave.h"
#include "Kismet/GameplayStatics.h"
#include "RhythmFighterCharacter.h"
#include "RhythmUI.h"
#include "RhythmEnemyCharacter.h"

ARhythmManager::ARhythmManager()
{
    PrimaryActorTick.bCanEverTick = true;

    AudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComp"));
    SetRootComponent(AudioComp);
}

void ARhythmManager::BeginPlay()
{
    Super::BeginPlay();

    // UI 생성
    if (RhythmUIClass)
    {
        RhythmUI = CreateWidget<URhythmUI>(GetWorld(), RhythmUIClass);
        if (RhythmUI) 
        { 
            RhythmUI->AddToViewport();
            RhythmUI->SetAcceptInput(false);

            if (ACharacter* P0 = UGameplayStatics::GetPlayerCharacter(this, 0))
            {
                if (auto* Fighter = Cast<ARhythmFighterCharacter>(P0))
                {
                    Fighter->SetRhythmUI(RhythmUI);
                    UE_LOG(LogTemp, Log, TEXT("[MANAGER] Injected RhythmUI to Character"));
                }
            }
        }
    }
    RunStartCountdown();

    // Player 전달
    if (RhythmUI)
    {
        AActor* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
        if (Player) RhythmUI->SetPlayerActor(Player);
    }

    // Enemy 전달
    if (RhythmUI)
    {
        AActor* Enemy = EnemyActorOverride;
        if (!Enemy)
        {
            TArray<AActor*> Found;
            UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARhythmEnemyCharacter::StaticClass(), Found);
            if (Found.Num() > 0) Enemy = Found[0];
        }
        if (Enemy) RhythmUI->SetEnemyActor(Enemy);
    }
}

void ARhythmManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bFightStarted) return;
    if (!bCueProcessing) return;

    ProcessCuePoints();
}

void ARhythmManager::BuildCuePointTimeArray()
{
    CueTimesSec.Reset();
    NextCueIndex = 0;

    if (!MusicWithCuePoints) return;

    const TArray<FSoundWaveCuePoint>& CuePoints = MusicWithCuePoints->GetCuePoints();
    if (CuePoints.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No CuePoints found on SoundWave: %s"), *MusicWithCuePoints->GetName());
        return;
    }

    const float SampleRate = MusicWithCuePoints->GetSampleRateForCurrentPlatform();
    if (SampleRate <= 0.f)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid SampleRate on SoundWave: %s"), *MusicWithCuePoints->GetName());
        return;
    }

    CueTimesSec.Reserve(CuePoints.Num());

    for (const FSoundWaveCuePoint& CP : CuePoints)
    {
        int32 Frame = 0;
#if (ENGINE_MAJOR_VERSION > 5) || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)
        Frame = CP.FramePosition;   // UE5.4+
#else
        Frame = CP.FrameOffset;     // UE5.3 이하
#endif
        const float TimeSec = FMath::Max(0.0f, static_cast<float>(Frame) / SampleRate);
        CueTimesSec.Add(TimeSec);
    }

    CueTimesSec.Sort();

    UE_LOG(LogTemp, Log, TEXT("Built CueTimesSec: %d cue points"), CueTimesSec.Num());
}

void ARhythmManager::UpdateRemainingTimeUI()
{
    if (!RhythmUI) return;

    // 1) 전체 길이(Duration) 구하기
    float Duration = 0.f;
    if (MusicWithCuePoints && MusicWithCuePoints->Duration > 0.f)
    {
        Duration = MusicWithCuePoints->Duration;
    }
    else if (MusicToPlay)
    {
        const float D = MusicToPlay->GetDuration();
        if (D > 0.f) Duration = D;
    }

    // 2) 경과시간은 오디오 재생 시간 기준으로
    const float Elapsed = GetMusicTimeSeconds();

    // 3) 남은 시간 계산
    float Remaining = 0.f;
    if (Duration > 0.f)
    {
        Remaining = FMath::Max(0.f, Duration - Elapsed);
    }

    RhythmUI->UpdateRemainingTime(Remaining);

    // 4) 종료 시 타이머 정지
    if (Duration > 0.f && Remaining <= 0.01f)
    {
        GetWorldTimerManager().ClearTimer(RemainingTimerHandle);
    }
}

void ARhythmManager::RunStartCountdown()
{
    CountdownStep = 3;
    if (RhythmUI) RhythmUI->SetCountdownText(TEXT("3"));

    // 1초 간격으로 진행
    GetWorldTimerManager().SetTimer(
        CountdownTimerHandle, this,
        &ARhythmManager::TickCountdown,
        1.0f, true);
}

void ARhythmManager::TickCountdown()
{
    if (!RhythmUI)
    {
        GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
        return;
    }

    CountdownStep--;

    if (CountdownStep == 2)
    {
        RhythmUI->SetCountdownText(TEXT("2"));
    }
    else if (CountdownStep == 1)
    {
        RhythmUI->SetCountdownText(TEXT("1"));
    }
    else if (CountdownStep == 0)
    {
        RhythmUI->SetCountdownText(TEXT("FIGHT!"));
    }
    else if (CountdownStep < 0)
    {
        // 카운트다운 종료
        GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
        RhythmUI->SetCountdownText(TEXT(""));  // 숨김
        RhythmUI->SetAcceptInput(true);        // 이제부터 판정 허용
        bFightStarted = true;

        // 여기서 음악/노트 시작
        if (AudioComp)
        {
            AudioComp->bAllowSpatialization = false;
            AudioComp->VolumeMultiplier = 1.0f;

            // 1) 퍼센트 콜백 바인딩(Play 전에!)
            AudioComp->OnAudioPlaybackPercent.RemoveAll(this);
            AudioComp->OnAudioPlaybackPercent.AddDynamic(this, &ARhythmManager::OnAudioPlaybackPercent);

            // 2) 추적값 초기화
            bMusicTimeValid = false;
            CurrentMusicTimeSec = 0.f;

            // 3) 어떤 사운드를 재생할지 결정
            USoundBase* SoundToPlay = MusicWithCuePoints ? Cast<USoundBase>(MusicWithCuePoints) : MusicToPlay;
            AudioComp->SetSound(SoundToPlay);

            // 4) 월드타임 기준도 같이 세팅(백업용)
            MusicStartTime = GetWorld()->GetTimeSeconds();

            // 5) 재생은 한 번만
            AudioComp->Play(0.f);
        }

        if (MusicWithCuePoints && RhythmUI)
        {
            BuildCuePointTimeArray();
            StartCueProcessing();
        }

        // 남은 시간 UI 타이머 시작 중이라면 여기서 시작/유지
        if (AudioComp && RhythmUI)
        {
            GetWorldTimerManager().SetTimer(
                RemainingTimerHandle, this,
                &ARhythmManager::UpdateRemainingTimeUI,
                0.1f, true);
        }
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
        {
            FInputModeGameOnly Mode;
            PC->SetInputMode(Mode);
            PC->bShowMouseCursor = false;
        }
        if (RhythmUI && RhythmUI->CountdownText)
        {
            RhythmUI->CountdownText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
   // RhythmUI->SetAcceptInput(true);
}

void ARhythmManager::StartCueProcessing()
{
    NextCueIndex = 0;
    bCueProcessing = (CueTimesSec.Num() > 0);
}

void ARhythmManager::ProcessCuePoints()
{
    if (!RhythmUI) return;
    if (CueTimesSec.Num() == 0) { bCueProcessing = false; return; }

    const float MusicTime = bMusicTimeValid
        ? CurrentMusicTimeSec
        : (GetWorld()->GetTimeSeconds() - MusicStartTime);

    int32 SpawnedThisFrame = 0;
    constexpr int32 MaxSpawnPerFrame = 32;
    const float Epsilon = 0.005f;

    while (NextCueIndex < CueTimesSec.Num() &&
        CueTimesSec[NextCueIndex] <= (MusicTime + Epsilon) &&
        SpawnedThisFrame < MaxSpawnPerFrame)
    {
        RhythmUI->SpawnNote();
        NextCueIndex++;
        SpawnedThisFrame++;
    }

    if (NextCueIndex >= CueTimesSec.Num())
    {
        bCueProcessing = false;
    }
}

float ARhythmManager::GetMusicTimeSeconds() const
{
    // 1) 가장 정확: 오디오 콜백 기반
    if (bMusicTimeValid)
    {
        return CurrentMusicTimeSec;
    }

    // 2) fallback: 월드타임 기반(콜백이 플랫폼에서 안 올 때 대비)
    if (MusicStartTime >= 0.f && GetWorld())
    {
        return GetWorld()->GetTimeSeconds() - MusicStartTime;
    }

    return 0.f;
}

void ARhythmManager::OnAudioPlaybackPercent(const USoundWave* PlayingSoundWave, const float PlaybackPercent)
{
    if (!PlayingSoundWave) return;

    const float Duration = PlayingSoundWave->GetDuration();
    CurrentMusicTimeSec = FMath::Clamp(PlaybackPercent, 0.f, 1.f) * Duration;
    bMusicTimeValid = true;
}