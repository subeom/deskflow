# 디스플레이 구성별 배치 기억: 사용자 테스트 스크립트

이 기능은 실험 기능이며 기본값은 꺼짐
서버 Mac의 디스플레이 구성이 바뀌면 그때의 다른 PC 배치를 기억해 두었다가 자동으로 되돌리는 기능
이 문서는 설치된 앱에서 직접 확인하는 순서

## 0. 먼저 알아둘 것

- 이 기능이 들어간 빌드 필요: 브랜치 `feat/layout-profiles` 빌드 (기존 1.27.0.125 에는 없음)
- 서버 모드의 Mac 에서만 동작 (클라이언트 모드에서는 아무 일도 하지 않음)
- 프로토콜 변경 없음: 클라이언트 PC 와 Mac mini 는 새 빌드 불필요
- 외부 모니터 분리 때 코어가 한 번 재시작됨: 클라이언트 연결이 몇 초 끊겼다가 다시 붙음
- 로그는 메인 창 아래의 로그 창에서 확인 (접혀 있으면 펼치기)
- 로그 줄은 모두 `layout memory:` 로 시작: 로그 창 검색에서 이 문구로 거를 것
- 기억 파일 위치: `~/Library/Deskflow/layout-profiles.json` (설정 파일과 같은 폴더, 이 기능이 켜졌을 때만 생김)

## 1. 켜기

- Deskflow 를 서버 모드로 둔 상태에서 `Configure Server` 열기
- `Advanced` 탭에서 `Experimental: multi-monitor aware cursor mapping` 체크 후 `Save`
- 다시 열어 `Computers` 탭에서 `Use the arrangement` 눌러 캔버스(배치도) 사용
- 캔버스 위쪽의 `Remember the arrangement for each set of displays` 체크
- 외부 모니터가 연결된 지금 상태에서 PC 배치를 평소대로 맞춘 뒤 `Save`

기대 결과:
- 로그: `layout memory: on, 1 remembered layout, displays=XXXXXXXX (2 displays: external 3440x1440@100% at 0,0 (main) [....:....] + built-in ... )`
- 로그에 `restart=` 줄 없음, 코어 재시작 없음, 알림 없음
- `Remembered layouts...` 를 열면 현재 상황 1개 (이름 뒤에 `(current)`), 아래쪽에 `Current displays:` 줄 (8자리 id 와 모니터 요약)

## 2. 외부 모니터 분리

- 외부 모니터의 케이블을 뽑기 (노트북 화면만 남김)
- 약 2초 기다리기

기대 결과:
- 로그 1: `layout memory: displays changed old=XXXXXXXX (2 displays) new=YYYYYYYY (1 display: built-in ...)`
- 로그 2 (처음 겪는 구성이므로 기억된 배치 없음): `layout memory: no remembered layout for new=YYYYYYYY; kept the arrangement as "built-in ..." (re-attached: ...; left alone: ...) restart=yes`
- 메인 창 위쪽(또는 서버 설정 창의 배치도 위쪽)에 한 줄 알림과 `Undo`, `Dismiss` 버튼
- 창이 숨겨져 있으면 알림 센터 배너
- 알림 문구 예: `No layout was remembered for these displays. The arrangement was kept as "built-in 1512x982". Put against the server again: ... The core was restarted.`
- 서버 설정의 배치도에서 PC 들이 노트북 화면의 같은 방향 가장자리에 붙어 있음
- 클라이언트가 몇 초 뒤 다시 연결되고 포인터가 PC 로 넘어감

## 3. Undo

- 알림의 `Undo` 누르기 (또는 서버 설정 창을 열어 배치도 위쪽 알림의 `Undo`)

기대 결과:
- 서버 설정 창 밖에서 누르면 로그: `layout memory: undo: back to the arrangement before old=XXXXXXXX new=YYYYYYYY restart=...`, 알림이 `The change was undone: ...` 로 바뀜
- 서버 설정 창 안에서 누르면 창의 배치도만 이전 배치로 돌아가고 `Save` 가 켜짐 (저장하면 이 배치가 현재 구성의 배치로 기억됨)
- Undo 는 한 번만 가능

## 4. 외부 모니터 다시 연결

- 외부 모니터를 다시 연결
- 약 2초 기다리기

기대 결과:
- 로그: `layout memory: displays changed ...` 다음에 `layout memory: restored "external 3440x1440 + built-in ..." (exact) old=YYYYYYYY new=XXXXXXXX restart=yes`
- 알림: `Layout restored: "..." (the same displays as before). The core was restarted.`
- PC 배치가 1번에서 맞춘 처음 모양으로 복귀
- 직전 재시작으로부터 10초가 안 지났으면 `restart=later`: 10초 뒤 로그에 `restarted the core after the minimum time since the last restart`

## 5. 다시 분리 (이번에는 기억된 배치)

- 외부 모니터를 다시 분리

기대 결과:
- 로그: `restored "built-in ..." (exact) ...` (`no remembered layout` 줄이 나오지 않음)
- 2번에서 정리된 노트북 단독 배치가 그대로 복원

## 6. 잠자기와 깨우기, 덮개 닫기

- 외부 모니터 연결 상태에서 Mac 을 잠자기 후 깨우기 (디스플레이가 여러 번 재구성됨)
- 덮개를 닫았다가 열기 (외부 모니터만 사용하는 클램셸 포함)

기대 결과:
- 구성이 그대로면 `displays changed` 줄이 없고 재시작도 없음 (변화 없는 이벤트는 무시됨, GUI 디버그 로그를 켜면 `signature unchanged` 줄 확인 가능)
- 구성이 실제로 바뀌면 변화 한 번에 `displays changed` 한 줄 (깨우는 중 여러 번 재구성되어도 한 줄)
- 깨어난 직후 짧게 화면 목록이 비어 있어도 1초 간격으로 다시 읽음 (5번 실패하면 경고 한 줄을 남기고 배치 유지)
- 불필요한 재시작 없음: 같은 구성으로 돌아오면 `restart=no`

## 7. 비슷한 모니터

- 같은 해상도와 배율의 다른 외부 모니터 연결 (가능한 경우)

기대 결과:
- 로그: `restored "..." (similar, score 0.875) ...`
- 알림: `Layout restored: "..." (similar displays, 88% alike).`
- 별도 기억이 생기지 않음: 그 구성에서 배치를 고쳐 `Save` 하면 그때 새 기억이 생김

## 8. 직접 고치고 저장

- 현재 구성에서 서버 설정을 열어 PC 위치를 조금 옮기고 `Save`
- 다른 구성으로 바꿨다가 돌아오기

기대 결과:
- 로그: `layout memory: remembered the arrangement for XXXXXXXX as "..." (updated)`
- 돌아왔을 때 옮긴 위치가 복원됨

## 9. 목록 관리

- `Remembered layouts...` 열기
- 한 행 선택 후 `Rename...` 으로 이름 바꾸기, `Forget` 으로 삭제
- `Save current arrangement as...` 로 지금 배치도를 이름을 붙여 기억

기대 결과:
- 목록은 읽기 전용: 이름, 마지막 사용 시각, 모니터 요약
- 이름 변경, 삭제, 이름 저장은 `Save` 없이 바로 적용되고 파일에도 바로 반영
- 사용자가 이름을 붙인 항목은 개수가 많아져도 마지막에 지워짐 (최대 32개)

## 10. 끄기

- `Remember the arrangement for each set of displays` 체크 해제 후 `Save`

기대 결과:
- 이후 모니터를 빼고 꽂아도 `layout memory:` 로그 줄이 없고 알림도 없음
- 코어 재시작과 배치 변경 없음: 기능을 켜기 전과 같은 동작
- `layout-profiles.json` 은 남아 있음 (필요 없으면 지워도 됨, 앱이 만든 파일일 뿐 설정 파일이 아님)

## 11. 문제가 생기면 보낼 것

- 로그 창에서 `layout memory:` 가 들어간 줄 전부 (시각 포함)
- `Remembered layouts...` 의 `Current displays:` 줄
- 문제가 생긴 순서 (무엇을 뽑고 꽂았는지, 잠자기 여부, 몇 시쯤인지)
- 앱 버전 (`About` 화면)
- 필요하면 `~/Library/Deskflow/layout-profiles.json` (컴퓨터 이름이 들어 있으므로 보내기 전에 내용 확인)
- 모니터 시리얼 번호와 UUID 는 로그에 남지 않음 (8자리 id 와 제조사:모델 코드만 표시)

## 12. 되돌리기 (원상복구)

- 체크 해제 후 `Save`: 기능 정지
- 배치가 마음에 들지 않으면 서버 설정에서 PC 를 직접 끌어서 다시 맞추고 `Save`
- 이 기능이 바꾸는 설정은 PC 의 배치 값(`computer_<이름>/arrangementX`, `arrangementY`, `arrangementZoom`)뿐
- 기억 파일 삭제는 앱을 종료한 상태에서 `layout-profiles.json` 을 지우기

## 확인되지 않은 것 (실제 Mac 에서 처음 확인하는 항목)

- 실제 모니터 분리, 연결, 잠자기, 덮개 닫기에서의 동작 (코드는 가짜 디스플레이와 가짜 시계로만 검증)
- Windows 서버와 Linux 서버 (공용 코드는 컴파일되지만 실행하지 않음)
- 메인 창에 알림 줄이 나타날 때 창 높이 조정의 느낌
