#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Quartz/AudioMixerClockHandle.h"
#include "RhythmManager.generated.h"

class UAudioComponent;
class USoundBase;
class USoundWave;
class URhythmUI;

UCLASS()
class RHYTHMFIGHTER_API ARhythmManager : public AActor
{
    GENERATED_BODY()

public:
    ARhythmManager();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /** 재생할 사운드(메타사운드 소스 or 사운드웨이브, 둘 다 가능) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Audio")
    USoundBase* MusicToPlay = nullptr;

    /** CuePoint 스케줄링에 사용할 SoundWave (CuePoint 포함 WAV) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Audio")
    USoundWave* MusicWithCuePoints = nullptr;

    /** UI 위젯 클래스 (URhythmUI를 부모로 한 Widget BP) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|UI")
    TSubclassOf<URhythmUI> RhythmUIClass;

    UPROPERTY(BlueprintReadOnly, Category = "Rhythm")
    bool bFightStarted = false;

    UFUNCTION(BlueprintPure, Category = "Rhythm")
    bool IsFightStarted() const { return bFightStarted; }

    FTimerHandle RemainingTimerHandle;
    float MusicStartTime = -1.f;
    UFUNCTION()
    void UpdateRemainingTimeUI();

    // 카운트다운 상태
    FTimerHandle CountdownTimerHandle;
    int32 CountdownStep = 3;

    // 진행 함수
    UFUNCTION()
    void RunStartCountdown();

    UFUNCTION()
    void TickCountdown();

    // 적 현재 타깃(단일 보스/1:1 기준)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm|Enemy")
    AActor* EnemyActorOverride = nullptr; // 에디터에서 지정해도 됨

protected:
    /** 오디오 재생용 컴포넌트 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rhythm|Audio")
    UAudioComponent* AudioComp;

    /** 생성된 UI 인스턴스 */
    UPROPERTY()
    URhythmUI* RhythmUI = nullptr;

    UPROPERTY()
    TArray<float> CueTimesSec;

    int32 NextCueIndex = 0;         // 다음으로 스폰할 큐 인덱스
    bool bCueProcessing = false;    // 처리 중인지

    void BuildCuePointTimeArray();  // SoundWave -> CueTimesSec 채우기
    void StartCueProcessing();      // 인덱스/플래그 초기화
    void ProcessCuePoints();        // 현재 음악시간 기준으로 SpawnNote() 호출

    float GetMusicTimeSeconds() const; // AudioComp 재생시간 기준

    // 현재 음악 재생시간(초) - OnAudioPlaybackPercent로 갱신
    float CurrentMusicTimeSec = 0.f;

    // 재생시간 유효해졌는지(첫 콜백 수신 여부)
    bool bMusicTimeValid = false;

    UFUNCTION()
    void OnAudioPlaybackPercent(const USoundWave* PlayingSoundWave, const float PlaybackPercent);
};
