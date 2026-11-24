# Always-On Pointer Input Badge

**Date:** 2026-11-24  
**Status:** Approved for implementation  
**Revision base:** `2724a5794f711ebd6503191f4ad0537278c77cef`

## Goal

Replace the transient macOS input-source indicator with a persistent original
companion attached to the native mouse pointer. Cat is the default companion;
Dog and Turtle are alternatives. Every companion carries a tiny `A` or `한`
inside its torso or shell for the exact current macOS input-source ID while
Deskflow believes the pointer is local.

"Caret" means the mouse pointer location here. Decorating an arbitrary
application's text caret is intentionally out of scope.

## Decision

Keep the native cursor completely untouched and place the existing
non-activating `NSPanel` near it:

- The panel never becomes key or main.
- The panel ignores mouse events and has no animation or shadow.
- The panel remains excluded from screen capture.
- No `NSCursor`, cursor image, cursor hide/show, or cursor warp API is used.
- The native arrow, I-beam, resize, and other cursor shape remains the
  operating system's responsibility.
- The companion has no containing tile, badge, border, icon frame, or
  background. The complete free-standing silhouette stays inside 30 x 28 pt
  with a 6 pt pointer gap and 6 pt screen margin.
- Cat, Dog, and Turtle are unified filled silhouette paths, not illustrated
  faces. Cat is a recognizable side profile with a distinct head, two pointed
  ears, elongated torso, paws, and a naturally curved rear tail. Dog has
  floppy ears and a tail; Turtle has a shell and tiny head/tail. All are
  original in-code drawings with no stock asset or external dependency.
- The 8 pt semibold `A`/`한` is a negative/highlight glyph inside the torso or
  shell. There are no eye dots, pupils, mouth, whiskers, face marks, or
  external corner language badge.
- Bodies, ears, shell, tail, and paws use the configured opacity directly
  (default 65%, clamped to 60-100%). The `A`/`한` glyph may be at most
  10 percentage points above body opacity. There is no solid sticker, tile,
  large shadow, or layout overlay. Explicit light and dark tones remain
  available.
- English uses a cool tint and Korean uses a warm tint. Source changes morph the
  tint, a subtle <=10% body/ear pose, and the label over 160-220 ms.

This is preferable to replacing the native cursor image because cursor image
replacement loses application-specific cursor shapes and would require fragile
global cursor manipulation. It is also preferable to changing a text caret
because macOS applications do not expose a safe cross-application caret
overlay API.

## Visibility Contract

The feature remains off by default.

| Condition | Result |
| --- | --- |
| Feature off | Hide, stop the pointer timer, and release the TIS monitor |
| Enabled, exact ABC or Korean 2-Set source, pointer local | Show |
| Core disconnected or stopped | Assume local and continue showing |
| Core reports pointer off-computer | Hide until local state returns |
| Source is unknown or unsupported | Hide and clear stale language |
| Pointer has no valid containing screen | Hide until the next valid pointer frame |
| Panel presentation fails | Hide; retry only through the normal pointer frame |
| Active Space changes | Snap the follower and present again on the next frame |

No click, key event, recent activity, dwell, focus state, frontmost PID,
Accessibility trust, editable-field classification, or stale-activity check is
required for visibility.

## Architecture and Data Flow

1. The existing core subscription remains because it carries local/off-computer
   routing state. Its wire protocol is unchanged.
2. `MacInputIndicatorController` owns one TIS source monitor, local and global
   `NSEvent` mouse-move/drag monitors, and a precise render timer whose duty
   follows the companion state: 75 ms for idle and 8 ms for walk/run or source
   transition. The controller still has no pointer-movement IPC.
3. Enabling the feature reads a full `OSXInputSourceSnapshot`, including the
   exact identifier. A known source is presented immediately even if the core
   is disconnected.
4. A TIS notification reads a new snapshot. Only an identifier/source change
   reconfigures the panel label and style. An identical notification performs
   no panel presentation or redraw.
5. Local and global pointer events immediately mark one coalesced target
   update. The render timer reads `NSEvent.mouseLocation` locally and resolves a
   target panel frame from the screen containing the cursor. A bounded
   no-overshoot follower advances partway toward that target and never lags
   more than 8 pt. The timer stops as soon as position, tail pose, and source
   transition are settled.
6. `MacInputIndicatorPanel` separates `show(options)` from
   `move(position)`. `show` changes source/style and snaps the follower;
   `move` changes frame only and presents only when the panel is hidden or
   Space presentation is invalidated.
7. If local or global monitor setup is incomplete, a 16 ms/60 Hz fallback timer
   remains active while the companion is logically visible. It is silent and
   reads no additional sensitive data. Local and global duplicate events are
   coalesced; coordinates are never sent over IPC or logged.
8. Explicit local/off-computer updates and core-state invalidation drive
   visibility. Activity arrival and activity invalidation do not affect it.
9. An `NSWorkspaceActiveSpaceDidChangeNotification` marks panel presentation
   needed and resets the follower. On the next pointer frame, the panel calls
   `orderFrontRegardless` even if `NSPanel.isVisible` is already true, then
   snaps to the current target. It keeps the existing
   `CanJoinAllSpaces | FullScreenAuxiliary | Transient` collection behavior.
   It does not raise the level or use a private API.

## Motion

Companion pose uses a deterministic four-state machine:

| State | Entry | Render duty | Visible behavior |
| --- | --- | --- | --- |
| Hidden | feature off, off-computer, or source unknown | no timer | panel hidden |
| Idle | no qualifying pointer movement for 180 ms | 75 ms / 13.3 Hz | slow tail sway, 1.2 s cycle; body fixed |
| Walk | cursor speed >= 30 pt/s and < 650 pt/s | 8 ms / 120 Hz | 4 Hz paw/leg phase, <=0.45 pt bob, small ear/tail response |
| Run | cursor speed >= 650 pt/s | 8 ms / 120 Hz | 7 Hz paw/leg phase, <=0.9 pt bob, 6-8% lean/stretch |

The follower uses deterministic local easing, not AppKit animation:

- calculate the current cursor-relative target frame;
- move toward it with a fixed 0.5 catch-up factor;
- clamp any remaining target distance to 8 pt;
- never move past the target;
- snap when a Space changes or a known source is presented after a hide/change.

The panel has no pulse, blink, zoom, spring-back, slide-in, or occluding
animation. At screen edges, the geometry helper first clamps the exact target
to a valid screen, then the follower approaches that valid target.

Pet motion derives only from successive local pointer positions. Idle has a
gentle tail sway with an otherwise steady silhouette. Walk begins with a soft
one- or two-point waddle, alternating paws, and a small head/body lean. Only
genuinely fast motion enters Run; even then body bob stays <=0.9 pt and
lean/stretch stays <=8%. The target never follows the pose. Source transition
uses an eased 200 ms tint/label crossfade.

The Cat tail is one natural two-segment cubic curve with an anchored root, a
continuous proximal/mid tangent, and a tip that lags the root by one phase
offset. Idle uses a gentle compound sine so the tip curls slowly rather than
snapping or rotating like a pendulum. Pointer velocity adds opposite-direction
inertia to the tip, but the complete bend is clamped to preserve the
silhouette. Cat tail thickness tapers toward the tip. Dog uses the same
continuity principle without the Cat curl; Turtle keeps a subtle head/tail
settle. The Cat profile faces the latest sustained horizontal motion and keeps
that direction through stationary frames; only the silhouette mirrors, so
`A`/`한` always remains upright and readable. Reduced Motion freezes a neutral
curled Cat tail.

`isOSXReduceMotion()` disables idle tail, walk/run pose, follower easing, and
source morphing: the stable companion moves directly to the target and starts
no animation timer.

## Retained and Disconnected Work

Legacy activity IPC types, `OSXInputIndicatorRelay`,
`MacAccessibilityProbe`, `MacInputFocusMonitor`, and the old transient policy
remain buildable for compatibility and test coverage. They are disconnected
from the persistent controller. The macOS event path no longer creates or
notifies the activity relay, avoiding click/key routing work.

## Settings

- Keep `gui/inputIndicatorEnabled`, off by default.
- Add `gui/inputIndicatorCompanion` with `cat` (default), `dog`, and `turtle`.
- Keep position, opacity, and tone controls.
- Change the default opacity from 85% to 65% while retaining the 60-100 range.
- Remove the visible `Display time` control because the badge is persistent.
- Preserve `gui/inputIndicatorDwellMs` as an unread migration key so old
  configurations continue to parse.
- Update the privacy note to describe continuous input-source and pointer
  position display without Accessibility, app focus, text, caret, keycode,
  clipboard, title, URL, selection, value, or screenshot access.

## Privacy and Performance

The feature reads only:

- the exact macOS TIS input-source identifier;
- local/global mouse-move or drag notifications and the global pointer
  position for a local frame update and velocity estimate;
- the existing core local/off-computer boolean.

It writes no input and changes no native cursor state. While logically hidden,
the pointer timer is stopped for feature-off, unknown-source, and
off-computer states. Reduce Motion and hidden states have no active timer. Idle
uses one low-duty 75 ms timer; walk/run and source morph use one 8 ms timer.
A 60 Hz timer exists only as a fallback for an incomplete monitor setup. No
pointer coordinate is persisted, transmitted, or logged.

## Test Contract

Focused tests cover:

- enabled known source shows without core connectivity or Accessibility trust;
- off-computer hides and local/disconnected transitions show;
- unknown source hides and cannot leave stale `A` or `한`;
- repeated pointer ticks move only, without source redraw;
- compact Cat/Dog/Turtle metrics and low-emphasis automatic opacity;
- a two-primitive silhouette/language render plan with no tile or eye
  primitives;
- distinct Cat head/torso/paw/tail regions in one side profile, with the
  language bounds inside the torso;
- persistent horizontal mirroring based on sustained motion, including
  stationary and non-finite-motion fallbacks;
- the language glyph bounds remain inside the body/shell;
- two-segment Cat tail continuity, phase lag, root anchor, and max-bend bounds;
- every body part uses clamped configured body alpha and glyph alpha stays
  within 10 percentage points;
- companion setting persistence and Cat default;
- bounded follower convergence without overshoot or more than 8 pt lag;
- local/global event seams, fallback registration, and render-timer stop;
- 4-8 pt motion pose clamp and Reduced Motion direct-to-target behavior;
- deterministic Idle/Walk/Run thresholds and deterministic phase wrap;
- hidden and Reduced Motion states have no render/fallback timer;
- 200 ms source color/shape/label transition;
- duplicate exact TIS snapshot notifications do not reconfigure the panel;
- ABC and Korean source changes update the label;
- feature off hides and stops all indicator monitors/timers;
- invalid geometry and panel failure hide;
- active Space invalidation presents again even when the panel reports visible;
- click/key/activity input is not required and cannot invalidate visibility;
- the panel presentation interface has no native cursor mutation command;
- removal of the display-time control while preserving its migration key.

The implementation must pass the required focused suites and the full Release
Qt 6.12 warnings-as-errors CTest gate, excluding only
`OSXClipboardTests` and `OSXKeyStateTests`.
