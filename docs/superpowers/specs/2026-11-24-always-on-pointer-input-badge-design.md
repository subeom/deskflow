# Always-On Pointer Input Badge

**Date:** 2026-11-24  
**Status:** Approved for implementation  
**Revision base:** `b95cb353080bb7b1cd6e4fd40de1dc12146f741b`

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
  faces. Cat follows the approved external Cat pose sheet: three Idle contours
  (rise/neutral/fall), three Walk contours (reach/pass/push), and three Run
  contours (coil/air/land). The source sheet is design input only. Its
  quadratic/cubic geometry is converted at authoring time into compiled,
  normalized native Bézier constants; the product never loads SVG, parses path
  strings, or stores an external asset path. Dog and Turtle remain coherent
  original AppKit silhouettes.
- The semibold Cat `A` stays at 8 pt in Idle/Walk and 7.6 pt in Run. The
  denser Korean `한` uses 6.8 pt in Idle, 6.4 pt in Walk, and 6.1 pt in Run.
  Dog/Turtle retain their existing 8 pt label behavior. There are no eye dots,
  pupils, mouth, whiskers, face marks, or external language badge.
- Cat label layout treats typographic and glyph-ink rectangles as different
  metrics. It creates a `CTLine` and reads
  `CTLineGetBoundsWithOptions(..., kCTLineBoundsUseGlyphPathBounds)` for the
  actual ink origin and size; typographic bounds are only a baseline
  reference. The font is iteratively fitted so that actual ink is centered in
  an inner torso target, then CoreText draws at the baseline derived from that
  measured origin. There is no arbitrary Korean Y offset and no `NSString`
  typographic-origin inference. Invalid or oversized ink fails closed by
  scaling; the Cat silhouette plus label target clips as a final guard, so no
  glyph pixel can leave the filled torso interior.
- Walk and Run keep one continuous primary body path. A smooth torso-region
  deformation moves the near contour's Bézier points outward around the label
  anchor; no rounded belly union, overlay, pad, independent stroke, or second
  body primitive exists. The resulting torso is at least 8.0 pt thick at the
  label, with an 8.0 x 7.0 pt inner label target. Any path point moves at most
  1.5 pt beyond the approved outline and the overall silhouette width remains
  within 1.5 pt of the approved art. Head, ears, paws, and tail use their
  original keyframe points. Idle keeps its seated proportions unchanged.
- Bodies, ears, shell, tail, and paws use the configured opacity directly
  (default 65%, clamped to 60-100%). The body is a neutral translucent
  off-white close to the macOS cursor (`#F2F3F5`), with a subtle dark-neutral
  edge on light backgrounds and a strong silhouette/body boundary on dark
  backgrounds. The language color moves to the glyph: `A` uses muted steel/
  slate blue and `한` uses dusty mauve/rose. The glyph remains at most 10
  percentage points above body opacity and each Light/Dark tone variant
  maintains at least 4.5:1 contrast against the off-white body. There is no
  solid sticker, tile, large shadow, or layout overlay.
- The body remains fundamentally off-white: source tint contributes no more
  than 8% to the body blend. The same muted source palette powers a low-alpha
  ambient halo around the Cat. An exact ABC/Korean transition emits one soft
  280 ms halo pulse with a <=1.25x ease-out expansion, bloom then decay, and no
  hard zoom. Rapid starts are staggered to at least 50 ms and retain at most
  the three newest waves, so cool/warm afterimages merge like a restrained
  wave rather than strobe. The halo is drawn behind the body and never covers
  the glyph. Source label and glyph crossfade remains 160-220 ms.

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
moving feet. The selected frame is a compiled constant from the approved art:
Idle uses rise/neutral/fall, Walk uses reach/pass/push, and Run uses
coil/air/land. This preserves the source sheet's wider right-facing profile,
ear and muzzle proportions, continuous rear-tail contour, and near/far paw
placement. The normalized artwork footprint stays inside 30 x 28 pt. A pure
frame model also records the exact in-torso glyph center for every pose. The
authoring transform applies the same X scale as the source and a 10% optical Y
lift so the required 8 pt glyph remains fully inside the Run contours.

The Cat artwork is right-facing in source coordinates. When motion faces left,
only the path transforms horizontally; the glyph center mirrors with the
torso but the `A`/`한` itself remains upright. The follower remains capped at
8 pt and never delays, replaces, hides, or warps the native cursor. Source
transition uses an eased 200 ms glyph-color/label crossfade.

The approved art carries the Cat tail as part of the continuous near contour,
with a thick anchored proximal section and a tapered two-cubic-section tip.
Idle rise/neutral/fall paths preserve the soft curl; Run coil/air/land reduce
the curl and carry controlled lag through the tail contour. The profile faces
the latest sustained horizontal motion and keeps that direction through
stationary frames; only the silhouette mirrors, so `A`/`한` always remains
upright and readable. Dog uses its existing continuity model and Turtle keeps
a subtle head/tail settle. Reduced Motion keeps the neutral seated Cat and
fixed curled tail, with no gait phase.

`isOSXReduceMotion()` disables idle tail, walk/run pose, follower easing, and
source morphing: the stable companion moves directly to the target, uses the
neutral seated Cat, and starts no animation timer. Reduced Motion draws no
ambient halo, pulse, afterimage, or trail.

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
- the Cat's right-facing source outline, Idle/Walk/Run keyframe sequence, near
  and far limb layers, and normalized label center match the approved external
  pose sheet within explicit point tolerances;
- source/state Cat font preferences are exactly `A`: 8/8/7.6 pt and `한`:
  6.8/6.4/6.1 pt for Idle/Walk/Run, with the final CoreText glyph-path ink
  rectangle contained by baseline-derived placement;
- native offscreen coverage renders every Cat source, artwork frame, and
  direction and reports zero glyph pixels outside the filled torso silhouette;
- every Walk/Run outer torso contour is at least 8.0 pt thick around the label
  anchor, no torso-pad render primitive/model exists, and contour expansion
  changes no path point by more than 1.5 pt or the complete width by more than
  1.5 pt;
- the generated native path data contains only normalized compiled constants;
  no SVG loader, XML/path parser, external dependency, runtime file path, or
  network access exists;
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
- neutral off-white body/edge values with at most an 8% source blend, muted
  steel/slate blue for `A`, dusty mauve/rose for `한`, and at least 4.5:1
  glyph/body contrast in Automatic/Light/Dark tones;
- deterministic halo frame scale/alpha bounds, <=50 ms start cadence, at most
  three alternating source waves, smooth decay, and Reduced Motion bypass;
- every body part uses clamped configured body alpha and glyph alpha stays
  within 10 percentage points;
- companion setting persistence and Cat default;
- bounded follower convergence without overshoot or more than 8 pt lag;
- local/global event seams, fallback registration, and render-timer stop;
- 4-8 pt motion pose clamp and Reduced Motion direct-to-target behavior;
- deterministic Idle/Walk/Run thresholds and deterministic phase wrap;
- hidden and Reduced Motion states have no render/fallback timer;
- 200 ms source glyph-color/label transition, at most 8% body tint, and a
  capped soft halo pulse;
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
