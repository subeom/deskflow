# macOS 한/A 입력 상태와 로컬 교정 설계

- **상태:** Track A 구현 완료. Track B 설계만 확정.
- **구현 기준:** `fork/master` `7f4bebb9613c70abe55ff4fecdb311bbe7a5e054` (`v2.0.6`)
- **구현 HEAD:** `5ce8bd895971bd1bce9b1d38e03acb1debd1193c`
- **구현 커밋:** `c023adcf8`, `7a5235929`, `8336e99a6`, `649695ad2`, `deb3745dd`, `5ce8bd895`
- **브랜치:** `subeomlee-microsoft-korean-input-cursor-indicator`

## 구현 상태

| 영역 | 구현 | 현재 동작 | 주요 안전 경계 |
|---|---|---|---|
| Track A: 입력 소스 표시기 | 구현 완료 | macOS Apple ABC에서 `A`, Korean 2-set에서 `한`, 기본 OFF | unknown AX/source, secure field, remote/off-computer, permission 부족 시 표시 숨김 |
| Track A: 보안 IPC | 구현 완료 | GUI PID, UID, 256-bit capability, per-client subscription, expiry 검증 | generic IPC 실패 시 indicator subscription 해제, capability 로그 금지 |
| Track A: UI | 구현 완료 | native non-activating `NSPanel`, cursor 인접, no motion, 위치·시간·투명도·색상 설정 | text value, clipboard, synthetic key 사용 없음 |
| Track B: 자동 교정 | 미구현 | preview, mutation, source switch, Undo 미활성 | AX replacement, native Undo, stale-token, Electron compatibility 증명 전 mutation 금지 |
| Track B: LLM | 미사용 | 외부 AI, 네트워크, 원격 사전 미사용 | 후보 생성과 ranking은 결정론적 local logic·offline data 범위 |

## 1. 권장 아키텍처와 선택 이유

### Track A: 구현 선택

1. `OSXComputer`의 기존 CGEventTap 하나에서 click 또는 keyDown의 발생 사실만 전달.
2. Deskflow core는 Unicode, key string, field value, AX value 전송 안 함.
3. Deskflow GUI가 별도 serial probe로 AX role, focused state, editable capability, secure/protected role 확인.
4. GUI가 TIS exact input source ID를 `ABC` 또는 `Korean 2-set`으로 분류.
5. GUI가 borderless transparent `NSPanel`에 `A` 또는 `한` 표시.
6. pointer 위치는 현재 `NSEvent.mouseLocation`, 화면 좌표는 logical point와 display rect로 clamp.
7. source/focus/IPC generation 불일치, stale event, unknown state 시 즉시 숨김.

선택 이유:

1. 추가 전역 event tap 없음.
2. AX/TIS/UI 호출이 tap callback 밖에서 실행.
3. GUI가 overlay와 Settings를 소유.
4. Deskflow core가 local/remote pointer state를 가장 정확한 지점에서 제공.
5. IPC payload에 입력 내용·keycode 없음.
6. 보안 IPC는 exact GUI PID, same UID, 256-bit capability, expiry, per-client subscription 적용.

### Track B: 권장 방향

1. Swift native helper의 deterministic keyboard-layout transducer.
2. finalized whole-token preview-only prototype.
3. bundled exact-key signature dictionary로 고확률 후보만 제공.
4. `RecommendOnly` default.
5. app/version/role별 exact replacement, one-Undo, source rollback 검증 후에만 mutation adapter 연결.
6. Apple Korean IME 조합을 재구성하거나 우회하지 못하면 preview-only 유지.

Track B 후보 비교:

| 후보 | 장점 | 한계 | 결정 |
|---|---|---|---|
| Hammerspoon | 빠른 observation prototype | IMK/Undo/AX/Electron atomicity 부재 | 탐색 전용 |
| Swift native app + CGEventTap/AX | deterministic transducer와 local UI에 적합 | mutation, stale input, Input Monitoring risk | preview-only 권장 |
| InputMethodKit input method | native text client와 marked/committed range 접근 | Apple Korean IME delegation 미증명, 조합 중복 risk | 현재 배제 |
| 오픈소스 fork | 기존 UI/event code 재사용 가능 | license, Undo, stale, Electron compatibility audit 필요 | gate 통과 전 미선택 |

## 2. Hammerspoon의 정확한 한계

1. InputMethodKit client session이나 `NSTextInputClient` composition contract를 소유하지 않음.
2. Electron/Chromium AX value와 실제 rendered/composed text 일치 보장 없음.
3. Backspace와 replacement을 synthetic key event 여러 개로 표현할 때 partial token, flicker, 다중 Undo group 가능.
4. clipboard replacement는 rich text, paste handler, password field, clipboard history에 영향 가능.
5. async timer와 physical key event 사이 stale race 방지 한계.
6. AX `kAXValueAttribute` settable 여부만으로 one-Undo 보장 불가.
7. macOS secure input 구간에서 event tap event 누락 가능.
8. physical event와 synthetic feedback을 완벽히 구분하기 어려움.
9. Lua timer/eventtap 수명과 exception handling의 deterministic test 어려움.
10. Accessibility와 Input Monitoring 권한, update 수명, code signing 부담.

허용 범위:

1. keycode/TIS 관찰.
2. one-time AX role probe.
3. transducer candidate 확인.
4. mock UI.

금지 범위:

1. production text mutation.
2. clipboard, Backspace, Delete injection.
3. persistent Undo/source switch 계약.
4. GitHub Copilot 자동 교정.

## 3. 결정론적 자동 판단 알고리즘

Track B 첫 후보 알고리즘:

1. word boundary에서만 finalized token 하나 관찰.
2. source별 physical key signature와 TIS state가 같은 generation에 속하는지 확인.
3. paste, cut, drag/drop, click selection, arrow, Backspace, Delete, composition, focus 이동이 하나라도 있으면 generation 폐기.
4. source가 ABC인 token은 Korean 2-set signature로, Korean 2-set인 token은 ABC signature로 변환.
5. reverse signature의 bundled exact dictionary entry가 있을 때만 candidate 제공.
6. `dkssudgktpdy`에 대한 `안녕하세요`, `ㅗ디ㅣㅐ`에 대한 `hello`는 runtime jamo 계산 없이 사전 검증된 entry에서 제공.
7. Unicode normalization을 composition engine으로 사용하지 않음.
8. dictionary miss, low frequency gap, ambiguous direction, proper noun 가능성은 no-op 또는 preview-only.
9. focus element, exact range, exact original token, source ID, generation, timestamp가 mutation 직전에 모두 동일할 때만 진행.
10. exact proof가 하나라도 없으면 text 변경 안 함.

## 4. AI 및 local LLM 필요

첫 구현과 Track B preview에 LLM 불필요.

1. 두 예시는 keyboard signature와 local dictionary로 결정.
2. LLM은 AX stale state, physical event provenance, source switch, Undo transaction 증명 불가.
3. network 없는 privacy 요구와 불일치.
4. probabilistic output은 repeatability와 오교정 gate에 부적합.
5. local model이 필요할 경우에도 판단 근거의 ranker만 가능. mutation authority와 security proof 대체 불가.

## 5. GitHub Copilot, Electron, Chromium 전략

현재 검증:

1. Track A는 exact AX role/subrole, focused, enabled, value settable을 조회.
2. AX value, selected text, document, caret, clipboard 조회 없음.
3. Electron AX focus notification이 없으면 click 또는 keyDown trigger를 fallback으로 사용.
4. system-wide focused application AX 조회 실패 시 NSWorkspace frontmost PID로 재조회.
5. unknown/secure/unsupported role은 hide.
6. AX worker의 generation과 frontmost PID가 completion 시점과 다르면 hide.
7. core IPC는 no text/no keycode 정책.
8. 숨김 원인은 reason code만 Debug log에 기록, 5초당 최대 1회.

미확인 live 항목:

1. GitHub Copilot/Electron/Slack/Teams/Safari/Chrome의 실제 AX role matrix.
2. Electron contenteditable의 secure field 판별 consistency.
3. full screen, Space, multi-display에서 overlay layering.
4. Apple Korean 2-set 내부 Latin mode가 TIS ID와 일치하는지.
5. correction mutation의 AX range, one-Undo, composition behavior.

호환 전략:

1. live matrix에 등록된 app/version/role만 supported.
2. matrix 불일치 시 indicator hide.
3. correction은 default preview-only.
4. live fixture에서 flicker, partial token, stale range가 확인되면 해당 app mutation 비활성.

## 6. 입력 소스 전환, Undo, stale generation

### Track A

1. TIS exact ID만 source truth.
2. `com.apple.keylayout.ABC`: `A`.
3. `com.apple.inputmethod.Korean.2SetKorean`: `한`.
4. TIS 알림, focus 알림, click, keyDown을 generation으로 통합.
5. source switch 후 stale activity는 1,000 ms gate로 제거.
6. 표시기는 text mutation이 없으므로 native Undo 대상 없음.

### Track B

1. source switch 전 target source selectable 확인.
2. original source, exact field identity, UTF-16 range, token, caret, generation snapshot 보존.
3. global TIS가 focused app input session을 따라갔다는 public proof 없으면 mutation 중단.
4. source switch 후 field snapshot 재확인.
5. replace 실패 시 source rollback.
6. 한 번의 native `Cmd+Z`로 original token과 caret 복원 검증.
7. 다음 click/key/paste/backspace/delete/focus/source event 발생 시 Undo lease 폐기.
8. `Cmd+Z` event consume 금지. 앱의 native Undo가 처리한 뒤 exact state 확인.
9. source rollback 실패 시 자동 mutation 중단.

## 7. 오교정 방지 규칙

hard reject:

1. password, secure, OTP, passkey, token.
2. URL, email, file path, command line, terminal, code editor.
3. `.`, `@`, `/`, `\`, `:`, `#`, `=`, `_`, braces, brackets, digits, mixed script.
4. CamelCase, snake_case, kebab-case, all-caps.
5. person, product, organization, private identifier 가능성.
6. unknown field, app, role, source, event provenance.
7. paste, cut, drag/drop, selection, arrow, Backspace, Delete, mid-edit.
8. marked/composing text.
9. dictionary miss, frequency tie, low confidence.
10. source switch 또는 Undo preflight 실패.
11. AX text와 displayed text 불일치.

mutation 정책:

1. confidence가 높아도 high-risk field는 변경 안 함.
2. exact whole token 외 변경 안 함.
3. partial token, prefix, suffix substitution 안 함.
4. input tail에 새 문자가 들어오면 generation mismatch로 no-op.
5. direct AX edit, synthetic Backspace, clipboard replace default 금지.
6. proof 부족 시 suggestion 또는 no-op.

## 8. 최소 파일 구조

### Track A 구현 파일

Platform:

1. `src/lib/platform/OSXInputSource.h`
2. `src/lib/platform/OSXInputSource.mm`
3. `src/lib/platform/OSXInputIndicatorRelay.h`
4. `src/lib/platform/OSXInputIndicatorRelay.cpp`
5. `src/lib/platform/OSXComputer.h`
6. `src/lib/platform/OSXComputer.mm`

IPC:

1. `src/lib/deskflow/ipc/InputIndicatorIpc.h`
2. `src/lib/deskflow/ipc/InputIndicatorIpc.cpp`
3. `src/lib/deskflow/ipc/CoreIpc.h`
4. `src/lib/deskflow/ipc/CoreIpc.cpp`
5. `src/lib/deskflow/ipc/CoreIpcServer.h`
6. `src/lib/deskflow/ipc/CoreIpcServer.cpp`
7. `src/lib/deskflow/ipc/IpcServer.h`
8. `src/lib/deskflow/ipc/IpcServer.cpp`

GUI:

1. `src/lib/gui/input/InputIndicatorPolicy.h`
2. `src/lib/gui/input/InputIndicatorPolicy.cpp`
3. `src/lib/gui/input/InputIndicatorGeometry.h`
4. `src/lib/gui/input/InputIndicatorGeometry.cpp`
5. `src/lib/gui/input/MacAccessibilityProbe.h`
6. `src/lib/gui/input/MacAccessibilityProbe.mm`
7. `src/lib/gui/input/MacInputFocusMonitor.h`
8. `src/lib/gui/input/MacInputFocusMonitor.mm`
9. `src/lib/gui/input/MacInputIndicatorPanel.h`
10. `src/lib/gui/input/MacInputIndicatorPanel.mm`
11. `src/lib/gui/input/MacInputIndicatorController.h`
12. `src/lib/gui/input/MacInputIndicatorController.mm`
13. `src/lib/gui/core/CoreProcess.h`
14. `src/lib/gui/core/CoreProcess.cpp`
15. `src/lib/gui/ipc/CoreIpcClient.h`
16. `src/lib/gui/ipc/CoreIpcClient.cpp`
17. `src/lib/gui/ipc/IpcClient.h`
18. `src/lib/gui/ipc/IpcClient.cpp`
19. `src/lib/gui/MainWindow.h`
20. `src/lib/gui/MainWindow.cpp`

설정과 문서:

1. `src/lib/common/Settings.h`
2. `src/lib/common/Settings.cpp`
3. `src/lib/gui/settings/SettingEffects.cpp`
4. `src/lib/gui/settings/SettingsPages.cpp`
5. `translations/deskflow_ko.ts`
6. `docs/Configuration.md`

### Track B 제안 파일

```text
src/input-correction/
  core/
    LayoutMap.h
    LayoutTransducer.h
    LayoutTransducer.cpp
    TokenObservation.h
    TokenGeneration.h
    ConfidenceGate.h
    ConfidenceGate.cpp
    ProtectedFieldClassifier.h
  macos/
    MacEventObserver.h
    MacEventObserver.mm
    MacTextClientProbe.h
    MacTextClientProbe.mm
    MacInputSourceTransaction.h
    MacInputSourceTransaction.mm
    RecommendOnlyAdapter.mm
    AxReplacementAdapter.mm
  ui/
    CorrectionPreviewController.swift
    CorrectionPreviewWindow.swift
  data/
    bundled-word-frequency.bin
  tests/
    LayoutTransducerTests.cpp
    ConfidenceGateTests.cpp
    TokenGenerationTests.cpp
    ProtectedFieldClassifierTests.cpp
    CorrectionTransactionTests.cpp
```

`AxReplacementAdapter.mm`는 기본 build와 자동 교정에서 제외.

## 9. 단계별 실행 계획과 현재 상태

| 단계 | 계획 | 상태 |
|---|---|---|
| 0 | base SHA 확인, fork-only worktree | 완료 |
| 1 | fail-closed policy, geometry, strict IPC codec | 완료 |
| 2 | TIS exact ID, existing CGEventTap relay, stale/remote gate | 완료 |
| 3 | AX probe, focus observer, source monitor, NSPanel | 완료 |
| 4 | MainWindow/CoreProcess wiring, redesigned Settings, Korean UI | 완료 |
| 5 | per-GUI PID/UID/capability IPC auth, raw parser hardening | 완료 |
| 6 | named tests, full CTest gate, translation restore, live-state hash | 완료 |
| 7 | live GitHub Copilot, Slack/Teams, browser, native, multi-display matrix | 실기기 수동 확인 필요 |
| B0 | AX/Undo/source evidence spike | 미실행 |
| B1 | reverse signature transducer와 offline dictionary | 미구현 |
| B2 | preview-only detector | 미구현 |
| B3 | app/version별 mutation capability probe | 미구현 |
| B4 | one-Undo automatic correction | 미구현, 별도 승인 필요 |

## 10. 검증 시나리오와 성공 기준

### 자동 검증 결과

1. Release, Qt 6.12.0, `CMAKE_COMPILE_WARNING_AS_ERROR=ON`.
2. `Deskflow`와 named executable target 158개 build.
3. 등록 test 159개.
4. 제외: `OSXClipboardTests`, `OSXKeyStateTests`.
5. 실행 test 157개: 157 pass, 0 fail, 0 skip.
6. CMake script test `GitVersionTests` 포함.
7. sandbox CTest, scratch HOME/XDG/TMPDIR, offscreen platform.
8. `~/Library/Deskflow` 파일 hash 4개 exact match.
9. pre-existing GUI PID `60570`, core PID `60599` 동일.
10. `*:24800` listener는 PID `60599` 하나로 유지.
11. non-Korean translation diff 0.

### 실기기 수동 성공 기준

| 시나리오 | 성공 기준 |
|---|---|
| GitHub Copilot Electron | click, focus, typing에서 `한/A` 1회 표시 후 종료. cursor, caret, text 변경 없음 |
| Slack/Teams | verified role이면 표시, unsupported role이면 hide |
| Safari/Chrome input | editable input/contenteditable만 표시, password hide |
| native/Qt | ABC/Korean source 전환과 focus 전환 정확 |
| source switch | 연속 전환 시 stale label 없음 |
| secure field | password/token/secure field에서 panel 없음 |
| remote/off-computer | 입력 이벤트 또는 panel 없음 |
| display | 1x/2x scaling, multi-display, negative coordinate, edge clamp 정상 |
| full screen/Space | 안전 표시 또는 hide, Deskflow window activation 없음 |
| timing | blink, pulse, slide, zoom, late token insertion 없음 |
| Accessibility failure | permission/role/source unknown에서 hide |

### Track B 자동 교정 성공 기준

1. `dkssudgktpdy` preview 후보 `안녕하세요`.
2. `ㅗ디ㅣㅐ` preview 후보 `hello`.
3. word boundary 외 no-op.
4. paste/backspace/mid-edit/new-tail no deletion.
5. protected field no-op.
6. single `Cmd+Z` exact original text, caret, source.
7. GitHub Copilot/Slack/Teams/browser에서 app/version allowlist 통과.
8. criteria 불통과 시 preview-only.

## 11. 개인정보와 권한

### Track A

1. IPC payload: sequence, timestamp, trigger kind, local state, capability만 사용.
2. keycode, Unicode, field value, selected text, document, app title, clipboard 전송 없음.
3. AX 조회: focused element, role, subrole, enabled, focused, value settable.
4. `kAXValueAttribute` 실제 값 읽기 없음.
5. source identifier exact allowlist만 사용.
6. screen-sharing capture 방지를 위해 `NSWindowSharingNone`.
7. network, telemetry, external AI, analytics 없음.
8. existing Accessibility permission 경로 재사용.
9. second global event tap 없음.
10. capability는 GUI child core 환경 전달, 로그 redaction, PID/UID 검증, per-client expiry 적용.
11. Debug hidden-reason log는 5초당 최대 1회, app identity, PID, title, text, value 미포함.

### Track B

1. token buffer memory 보관, focus/window/idle 후 즉시 삭제.
2. original/candidate/AX value crash log 금지.
3. local dictionary network download 금지.
4. personal learning default OFF.
5. preview document 저장 금지.
6. Swift CGEventTap prototype는 별도 Input Monitoring permission과 사용자 승인 필요.
7. InputMethodKit input method 설치·선택 자동화 금지.
8. Apple Korean IME delegation 증명 전 custom Korean composition 금지.

##  결론

Track A는 현재 구현과 자동 검증 범위에서 완료. Track B는 보수적 preview/spec 범위이며 자동 교정, source switch, Undo mutation, 사용자 입력기 교체는 미구현. 실기기 11개 항목의 manual matrix 확인 전 Electron/Chromium/IME compatibility를 확정된 것으로 표현하지 않음.
