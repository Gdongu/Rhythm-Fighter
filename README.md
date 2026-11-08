# 리듬 파이터 · Rhythm Fighter

음악에 맞춰 방향 노트를 입력하면 내 공격 게이지가 차고 상대의 게이지는 줄어듭니다. 모은 게이지를 공격에 쓸지, 피격에서 빠져나오는 대시에 남겨 둘지 선택하는 **PC 리듬 격투 액션 게임**입니다.

[프로젝트 홈페이지](https://kdw1102.imweb.me/RhythmFighter) · [플레이 영상: 세 번의 대전](https://www.youtube.com/watch?v=dd0-fkghjx8) · [게임 빌드 다운로드](https://naver.me/FJOmAmw0)

[![리듬 파이터 대표 화면 · 클릭하면 플레이 영상으로 이동합니다](https://cdn-optimized.imweb.me/v2/brand/S20260802eaf9fc95f5402/763421d5-5f3d-41aa-b8f1-b8adbb203bd4)](https://www.youtube.com/watch?v=dd0-fkghjx8)

## 프로젝트 개요

| 항목 | 내용 |
| --- | --- |
| 개발 | 김동우 · 개인 프로젝트 |
| 기간 | 2025.09 ~ 2025.12 |
| 환경 | PC · Unreal Engine 5.6 · C++ / Blueprint |
| 담당 | 음악 큐 포인트 처리, 노트 판정과 전투 UI, 플레이어 공격·회피·필살기, 적 AI, 인벤토리·장비, 전투 맵 전환 |

## 핵심 구현

### 1. 음악에서 노트를 생성하고 입력을 판정하는 과정

카운트다운이 끝나면 음악 재생과 판정 입력을 시작합니다. `ARhythmManager`는 SoundWave 큐 포인트의 프레임 위치를 샘플레이트로 나눠 초 단위로 변환하고 시간순으로 정렬합니다. 오디오 재생 시간이 다음 큐에 도달하면 노트를 생성하며, 재생률 콜백이 아직 유효하지 않을 때는 음악 시작 이후의 월드 경과 시간을 사용합니다.

노트 **생성**은 음악 시간을 기준으로 하지만, **입력 판정**은 화면의 판정선을 기준으로 합니다. `URhythmUI::HitNote`는 입력과 방향이 같은 노트 중 판정선에 가장 가까운 것을 골라 X축 거리로 Perfect·Good·Miss를 구분합니다. 성공하면 판정 등급에 따라 플레이어와 적의 공격 게이지를 서로 반대 방향으로 조정합니다.

[큐 포인트 시간 변환](https://github.com/Gdongu/Rhythm-Fighter/blob/06fb0fcbd9e96b96d215886ba08788726ff4569c/Private/RhythmManager.cpp#L75-L113) · [재생 시간 추적과 노트 생성](https://github.com/Gdongu/Rhythm-Fighter/blob/06fb0fcbd9e96b96d215886ba08788726ff4569c/Private/RhythmManager.cpp#L245-L303) · [방향·거리 판정과 게이지 반영](https://github.com/Gdongu/Rhythm-Fighter/blob/06fb0fcbd9e96b96d215886ba08788726ff4569c/Private/RhythmUI.cpp#L139-L212)

### 2. 같은 게이지를 사용하는 공격과 회피

일반 공격은 공격 게이지가 충분할 때 몽타주 재생을 시도하고 필살기 게이지를 충전합니다. 회피는 같은 방향을 짧은 시간 안에 두 번 입력하면 발동합니다. 대시 비용을 지불할 수 있다면 피격 반응 중에도 이동 제한을 풀고 빠져나갈 수 있습니다. 플레이어와 적의 타격 판정은 공격 애니메이션의 Notify State가 시작되고 끝나는 구간에 맞춰 켜고 끕니다.

[일반 공격 비용과 필살기 충전](https://github.com/Gdongu/Rhythm-Fighter/blob/06fb0fcbd9e96b96d215886ba08788726ff4569c/Private/RhythmFighterCharacter.cpp#L311-L321) · [더블 탭과 피격 취소 대시](https://github.com/Gdongu/Rhythm-Fighter/blob/06fb0fcbd9e96b96d215886ba08788726ff4569c/Private/RhythmFighterCharacter.cpp#L970-L1072) · [애니메이션과 히트박스 연결](https://github.com/Gdongu/Rhythm-Fighter/blob/06fb0fcbd9e96b96d215886ba08788726ff4569c/Private/AnimNotifyState_Hitbox.cpp#L7-L64)

### 3. 제한 시간 안에 완성하는 필살기 커맨드

필살기 게이지가 충분하면 비용을 차감하고 전용 입력 매핑으로 전환합니다. 키 목록에서 4~5개의 커맨드를 무작위로 뽑아 표시하며, C++의 기본 대체 키는 **A·S·D·F**입니다. 입력 순서가 틀리거나 시간이 끝나면 실패합니다. 커맨드 종료 후에는 일반 입력 매핑을 복원하고, 성공했을 때만 카메라 줌과 필살기 몽타주를 실행합니다.

[커맨드 생성·입력 검증](https://github.com/Gdongu/Rhythm-Fighter/blob/06fb0fcbd9e96b96d215886ba08788726ff4569c/Private/RhythmUI.cpp#L330-L436) · [입력 매핑 전환과 성공 연출](https://github.com/Gdongu/Rhythm-Fighter/blob/06fb0fcbd9e96b96d215886ba08788726ff4569c/Private/RhythmFighterCharacter.cpp#L323-L401)

### 4. 장비 효과를 전투 맵까지 유지하는 인벤토리

아이템 DataTable을 사용한 프로젝트이며, 공개 C++에서 장착·복원 경로가 직접 참조하는 타입은 [UItemData](Public/ItemData.h)입니다. 이 Data Asset에 장착 슬롯과 체력·공격·방어 보정치, 피해·게이지 획득 배수를 정의합니다. 인벤토리 컴포넌트는 장착한 아이템의 보정치를 합산해 전투 스탯에 적용합니다.

레벨 이동 전에는 GameInstance 서브시스템에 아이템 에셋 경로, 수량, 장착 슬롯을 기록합니다. 새 캐릭터에 복원할 때는 아이템을 다시 추가하고 해당 슬롯에 장착한 뒤 스탯을 재계산합니다.

[장착·해제와 스탯 집계](https://github.com/Gdongu/Rhythm-Fighter/blob/06fb0fcbd9e96b96d215886ba08788726ff4569c/Private/InventoryComponent.cpp#L94-L218) · [맵 전환 전 장비 기록·복원](https://github.com/Gdongu/Rhythm-Fighter/blob/06fb0fcbd9e96b96d215886ba08788726ff4569c/Private/EquipmentStateSubsystem.cpp#L44-L125)

## 주요 클래스

| 클래스 | 코드에서 확인할 역할 |
| --- | --- |
| [RhythmManager](Private/RhythmManager.cpp) | 카운트다운, 오디오 재생 시간 추적, 큐 포인트 순회 |
| [RhythmUI](Private/RhythmUI.cpp) | 노트 이동·판정, 게이지 표시, 필살기 커맨드 진행 |
| [RhythmFighterCharacter](Private/RhythmFighterCharacter.cpp) | 탐험·전투 입력 전환, 공격·피격·대시, 필살기 연출 |
| [CombatStatsComponent](Private/CombatStatsComponent.cpp) | 체력과 게이지, 장비 보정치, 피해 계산 |
| [RhythmEnemyCharacter](Private/RhythmEnemyCharacter.cpp) | 적의 접근·이동, 공격 거리와 쿨타임, 연속 공격 |
| [RhythmFighterGameMode](Private/RhythmFighterGameMode.cpp) | 전투 종료 처리, 결과 UI 표시, 탐험 맵 복귀 |

## 회고

노트를 맞힌 결과를 공격 기회로 바꾸면서 리듬 입력과 격투를 하나의 플레이로 연결했습니다. 다만 노트를 읽는 동안에도 상대의 공격을 봐야 해서, 판정 결과와 필살기 사용 가능 여부를 한눈에 전달하는 데 아쉬움이 남았습니다. 다음에는 플레이 테스트에서 시선이 분산되는 순간을 확인하고, 판정음과 게이지 강조 효과를 조정해 상대를 보면서도 입력 결과를 알 수 있도록 개선하고 싶습니다.

## 실행 및 소스 범위

이 저장소의 `Public/`에는 C++ 헤더, `Private/`에는 구현 파일이 있습니다. 프로젝트 설정·블루프린트·음원·애니메이션 에셋은 포함되어 있지 않아 저장소만으로는 빌드할 수 없습니다. 게임 실행은 상단의 **게임 빌드 다운로드**, 플레이 흐름 확인은 **플레이 영상**을 이용해 주세요.
