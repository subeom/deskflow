# Always-On Pointer Input Badge

**Date:** 2026-11-24  
**Status:** Approved for implementation  
**Base:** `8bd772e8f7f007dce727226a28aa5d40cc249fa3`

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

No click, key event, recent activity, dwell, focus state, frontmost PID,
Accessibility trust, editable-field classification, or stale-activity check is
required for visibility.

## Architecture and Data Flow

1. The existing core subscription remains because it carries local/off-computer
   routing state. Its wire protocol is unchanged.
2. `MacInputIndicatorController` owns one TIS source monitor and one
   approximately 60 Hz pointer timer.
3. Enabling the feature reads a full `OSXInputSourceSnapshot`, including the
   exact identifier. A known source is presented immediately even if the core
   is disconnected.
4. A TIS notification reads a new snapshot. Only an identifier/source change
   reconfigures the panel label and style. An identical notification performs
   no panel presentation or redraw.
5. Every pointer timer tick reads `NSEvent.mouseLocation` locally, resolves
   valid geometry, and updates only the panel frame. It does not send pointer
   movement over IPC.
6. `MacInputIndicatorPanel` separates `show(options)` from
   `move(position)`. `show` changes source/style and presents the panel;
   `move` changes frame only and presents only after recovering from a hidden
   invalid-geometry state.
7. Explicit local/off-computer updates and core-state invalidation drive
   visibility. Activity arrival and activity invalidation do not affect it.

## Retained and Disconnected Work

Legacy activity IPC types, `OSXInputIndicatorRelay`,
`MacAccessibilityProbe`, `MacInputFocusMonitor`, and the old transient policy
remain buildable for compatibility and test coverage. They are disconnected
from the persistent controller. The macOS event path no longer creates or
notifies the activity relay, avoiding click/key routing work.

## Settings

- Keep `gui/inputIndicatorEnabled`, off by default.
- Keep position, opacity, and tone controls.
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
off-computer states. While visible, work is one local geometry calculation and
one frame update per tick.

## Test Contract

Focused tests cover:

- enabled known source shows without core connectivity or Accessibility trust;
- off-computer hides and local/disconnected transitions show;
- unknown source hides and cannot leave stale `A` or `한`;
- repeated pointer ticks move only, without source redraw;
- duplicate exact TIS snapshot notifications do not reconfigure the panel;
- ABC and Korean source changes update the label;
- feature off hides and stops all indicator monitors/timers;
- invalid geometry and panel failure hide;
- click/key/activity input is not required and cannot invalidate visibility;
- the panel presentation interface has no native cursor mutation command;
- removal of the display-time control while preserving its migration key.

The implementation must pass the required focused suites and the full Release
Qt 6.12 warnings-as-errors CTest gate, excluding only
`OSXClipboardTests` and `OSXKeyStateTests`.
