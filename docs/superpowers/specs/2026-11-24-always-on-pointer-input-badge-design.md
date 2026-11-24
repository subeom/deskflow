# Always-On Pointer Input Badge

**Date:** 2026-11-24  
**Status:** Approved for implementation  
**Revision base:** `732b06ac4cab581b59a052cda47c55104d1407fa`

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
  faces. Cat has distinct seated, walking, and running side-profile geometry:
  Idle is seated with tucked paws, Walk is a level standing stride, and Run is
  low and elongated with forward/rearward paws and a forward lean. Dog has
  floppy ears and a tail; Turtle has a shell and tiny head/tail. All are
  original in-code drawings with no stock asset or external dependency.
- The 8 pt semibold `A`/`한` stays inside the torso or shell. There are no eye
  dots, pupils, mouth, whiskers, face marks, or external language badge.
- Bodies, ears, shell, tail, and paws use the configured opacity directly
  (default 65%, clamped to 60-100%). The body is a neutral translucent
  off-white close to the macOS cursor (`#F2F3F5`), with a subtle dark-neutral
  edge on light backgrounds and a strong silhouette/body boundary on dark
  backgrounds. The language color moves to the glyph: `A` uses dark cool blue
  and `한` uses deep coral/rose. The glyph remains at most 10 percentage
  points above body opacity and each Light/Dark tone variant maintains at
  least 4.5:1 contrast against the off-white body. There is no solid sticker,
  tile, large shadow, or layout overlay.
- The body never changes between blue and coral by input source. Source changes
  crossfade the `A`/`한` glyph and its source color over 160-220 ms; a subtle
  <=10% source transition still may affect body/ear pose.

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
| Idle | no qualifying pointer movement for 180 ms | 75 ms / 13.3 Hz | seated side profile, tucked paws, curled-tail sway, 1.2 s cycle |
| Walk | speed >= 30 pt/s or per-frame pointer distance >= 3 pt, without a Run condition | 8 ms / 120 Hz | standing level body, head forward, alternating paws, <=0.45 pt bob |
| Run | speed >= 650 pt/s, per-frame distance >= 48 pt, or pre-catch-up target lag >= 24 pt | 8 ms / 120 Hz | elongated low body, front/rear paws extended, <=0.9 pt leap bob, <=8%/8 degree lean, straighter tail |

The follower uses deterministic local easing, not AppKit animation:

- calculate the current cursor-relative target frame;
- move toward it with a fixed 0.5 catch-up factor;
- clamp any remaining target distance to 8 pt;
- never move past the target;
- snap when a Space changes or a known source is presented after a hide/change.

The panel has no pulse, blink, zoom, spring-back, slide-in, or occluding
animation. At screen edges, the geometry helper first clamps the exact target
to a valid screen, then the follower approaches that valid target.

Pet motion derives only from successive local pointer positions. The pure
state selector receives pointer speed, raw target movement, and the
pre-catch-up distance between the companion and target. That last value makes
the Cat run when the cursor escapes even if an instantaneous speed sample is
incomplete. Idle returns only after 180 ms without qualifying movement.

Cat pose geometry is state-specific rather than one seated silhouette with
moving feet. Idle is upright and seated with tucked paws. Walk lowers and
lengthens the torso into a standing stride with a forward head and alternating
front/rear paw travel. Run uses the longest, lowest torso, extended forward and
rear paws, bounded forward lean, a longer leap phase, and a straighter
controlled tail. The follower remains capped at 8 pt and never delays,
replaces, hides, or warps the native cursor. Source transition uses an eased
200 ms glyph-color/label crossfade.

The Cat tail is one natural two-segment cubic curve with an anchored root, a
continuous proximal/mid tangent, and a tip that lags the root by one phase
offset. Idle uses the fullest curl, Walk keeps a soft curl, and Run reduces
the vertical curl/phase amplitude while retaining controlled inertia. Pointer
velocity adds opposite-direction inertia to the tip, but the complete bend is
clamped to preserve the silhouette. Cat tail thickness tapers toward the tip.
Dog uses the same
continuity principle without the Cat curl; Turtle keeps a subtle head/tail
settle. The Cat profile faces the latest sustained horizontal motion and keeps
that direction through stationary frames; only the silhouette mirrors, so
`A`/`한` always remains upright and readable. Reduced Motion keeps the neutral
seated Cat and fixed curled tail, with no gait phase.

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
- distinct seated, walking, and running Cat torso/head/paw geometry, including
  bounded limb travel, tilt, and Reduced Motion fallback;
- Run selection from speed, raw pointer movement, or pre-catch-up cursor
  escape distance;
- persistent horizontal mirroring based on sustained motion, including
  stationary and non-finite-motion fallbacks;
- the language glyph bounds remain inside every body pose, and its render plan
  explicitly never mirrors with the silhouette;
- two-segment Cat tail continuity, phase lag, root anchor, and max-bend bounds;
- neutral off-white body/edge values independent of input source, dark cool
  blue for `A`, deep coral/rose for `한`, and at least 4.5:1 glyph/body
  contrast in Automatic/Light/Dark tones;
- every body part uses clamped configured body alpha and glyph alpha stays
  within 10 percentage points;
- companion setting persistence and Cat default;
- bounded follower convergence without overshoot or more than 8 pt lag;
- local/global event seams, fallback registration, and render-timer stop;
- 4-8 pt motion pose clamp and Reduced Motion direct-to-target behavior;
- deterministic Idle/Walk/Run thresholds and deterministic phase wrap;
- hidden and Reduced Motion states have no render/fallback timer;
- 200 ms source glyph-color/label transition without whole-body source tint;
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
