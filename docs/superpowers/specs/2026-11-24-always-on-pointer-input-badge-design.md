# Always-On Pointer Input Badge

**Date:** 2026-11-24  
**Status:** Approved for implementation  
**Revision base:** `58069ddaac60648528df19c40de92b93e425d398`

## Goal

Replace the transient macOS input-source indicator with a persistent `A` or `한`
badge attached to the native mouse pointer. The badge represents the exact
current macOS input-source ID while Deskflow believes the pointer is local.

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
- The badge is a 24 x 20 pt soft pill with a 7 pt radius, 11 pt semibold
  label, 6 pt pointer gap, and 6 pt screen margin.
- The badge has no shadow and does not add a layout overlay. Its automatic tint
  is lower emphasis than an accent fill; explicit light and dark tones remain
  available.

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
2. `MacInputIndicatorController` owns one TIS source monitor and one
   approximately 60 Hz pointer timer. The controller still has no pointer
   movement IPC.
3. Enabling the feature reads a full `OSXInputSourceSnapshot`, including the
   exact identifier. A known source is presented immediately even if the core
   is disconnected.
4. A TIS notification reads a new snapshot. Only an identifier/source change
   reconfigures the panel label and style. An identical notification performs
   no panel presentation or redraw.
5. Every pointer timer tick reads `NSEvent.mouseLocation` locally and resolves
   a target panel frame from the screen containing the cursor. A bounded
   no-overshoot follower advances partway toward that target, normally catching
   up in two or three frames. Its distance from the target never exceeds
   18 pt, so the lag reads as a tiny trailing attachment instead of jitter.
6. `MacInputIndicatorPanel` separates `show(options)` from
   `move(position)`. `show` changes source/style and snaps the follower;
   `move` changes frame only and presents only when the panel is hidden or
   Space presentation is invalidated.
7. Explicit local/off-computer updates and core-state invalidation drive
   visibility. Activity arrival and activity invalidation do not affect it.
8. An `NSWorkspaceActiveSpaceDidChangeNotification` marks panel presentation
   needed and resets the follower. On the next pointer frame, the panel calls
   `orderFrontRegardless` even if `NSPanel.isVisible` is already true, then
   snaps to the current target. It keeps the existing
   `CanJoinAllSpaces | FullScreenAuxiliary | Transient` collection behavior.
   It does not raise the level or use a private API.

## Motion

The follower uses deterministic local easing, not AppKit animation:

- calculate the current cursor-relative target frame;
- move toward it with a fixed 0.5 catch-up factor;
- clamp any remaining target distance to 18 pt;
- never move past the target;
- snap when a Space changes or a known source is presented after a hide/change.

The panel has no pulse, blink, zoom, spring-back, slide-in, or occluding
animation. At screen edges, the geometry helper first clamps the exact target
to a valid screen, then the follower approaches that valid target.

## Retained and Disconnected Work

Legacy activity IPC types, `OSXInputIndicatorRelay`,
`MacAccessibilityProbe`, `MacInputFocusMonitor`, and the old transient policy
remain buildable for compatibility and test coverage. They are disconnected
from the persistent controller. The macOS event path no longer creates or
notifies the activity relay, avoiding click/key routing work.

## Settings

- Keep `gui/inputIndicatorEnabled`, off by default.
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
- the global mouse pointer position for a local frame update;
- the existing core local/off-computer boolean.

It writes no input and changes no native cursor state. While logically hidden,
the pointer timer is stopped for feature-off, unknown-source, and
off-computer states. While visible, work is one local geometry calculation, one bounded follower
calculation, and one frame update per tick.

## Test Contract

Focused tests cover:

- enabled known source shows without core connectivity or Accessibility trust;
- off-computer hides and local/disconnected transitions show;
- unknown source hides and cannot leave stale `A` or `한`;
- repeated pointer ticks move only, without source redraw;
- compact panel metrics and low-emphasis automatic opacity;
- bounded follower convergence without overshoot or more than 18 pt lag;
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
