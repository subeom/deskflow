# Cat Follow Speed Setting

**Date:** 2026-11-26

**Status:** Approved for immediate implementation

**Base:** `aeb981b6286975273fe55c980ed78c6baa8bae55` (`v2.0.11`)

## Goal

Let a user adjust how quickly the Cat companion follows the local pointer.
The setting is live, bounded, Cat-only in its UI effect, and preserves the
current Cat position when it changes. Dog and Turtle keep their existing
follower behavior.

## User-visible behavior

Add the setting key `gui/inputIndicatorSpeedPercent` with:

- default `100`;
- safe range `50..200`;
- integer `QSpinBox` in
  **Settings > Keyboard & mouse > Input source indicator**;
- label `Cat follow speed`;
- object name `sbInputIndicatorSpeed`;
- `%` suffix;
- live setting effect.

The control is enabled only when both conditions are true:

1. the input source indicator is enabled;
2. the selected companion is `cat`.

It remains disabled for Dog and Turtle. The setting does not create a new page
or dialog.

## Speed model

The pure Cat pursuit step accepts one validated multiplier. It scales only the
two motion caps:

| Setting | Walk cap | Run cap | 8 ms Walk step | 8 ms Run step |
| ---: | ---: | ---: | ---: | ---: |
| 50% | 60 pt/s | 360 pt/s | 0.48 pt | 2.88 pt |
| 100% | 120 pt/s | 720 pt/s | 0.96 pt | 5.76 pt |
| 200% | 240 pt/s | 1440 pt/s | 1.92 pt | 11.52 pt |

The multiplier is valid only when finite and in `0.5..2.0`. Any invalid
multiplier fails the pure pursuit call closed by returning `std::nullopt`.
Valid initial placement and Reduced Motion behavior still place the Cat
directly and therefore ignore the speed cap after the multiplier has been
validated.

The setting does not change:

- 48 pt Run distance threshold;
- 260 pt/s target-speed Run threshold;
- 12 pt Run hysteresis;
- 8 ms simulation cap;
- 180 ms settle interval;
- 10 rad/s heading limit;
- monotonic approach, no overshoot, or no-teleport behavior;
- 49 x 49 canvas, 33 x 31 anchor, and 7 pt cursor gap;
- source-change, Space-change, startle, tail, glow, facing, and upright glyph
  behavior;
- initial `show` placement semantics.

## Configuration boundary

The controller reads and clamps the integer setting. Values below 50 clamp to
50 and values above 200 clamp to 200. A stored value that cannot be converted
to an integer uses 100. The UI `QSpinBox` independently constrains user entry
to 50..200. The panel clamps any incoming option or setter value to the same
range.

`MacInputIndicatorPanelOptions` includes the speed percent. Initial panel
presentation carries the validated value into `MacInputIndicatorPanel`.

Live `settingsChanged` handling is intentionally different from the other
indicator settings:

- `Settings::Gui::InputIndicatorSpeedPercent` updates the controller member;
- the controller calls a dedicated panel speed setter;
- it does not hide the panel, reset `m_panelConfigured`, or call `show`;
- the setter changes only the multiplier used by future pursuit steps.

This direct path preserves the initialized pursuit state and visible position.
The next frame uses the new cap, and no frame snaps or reinitializes the Cat.
If the panel is not yet visible, the updated controller value is carried by
the next `show`.

## Architecture decision

Three approaches were considered:

1. **Re-show the panel on every speed change.** Rejected because `show` also
   participates in source crossfade/startle state and presentation work even
   though speed is motion configuration.
2. **Store speed in `InputIndicatorCatPursuitState`.** Rejected because the
   state should contain motion integration data, not a separately persistent
   user setting.
3. **Panel option plus direct live setter, with a pure multiplier argument.**
   Selected. It keeps configuration at the controller/panel boundary, pure
   motion math accepts a bounded primitive, and live updates cannot reset the
   initialized position.

## Test contract

### Settings

Tests prove:

- `Settings::defaultValue` returns 100 and the key is valid;
- `QSpinBox` value, minimum 50, maximum 200, integer step, and `%` suffix;
- the control follows indicator enablement and Cat/Dog/Turtle selection;
- safe clamping and invalid stored-value fallback do not change another
  indicator setting;
- `SettingEffects` reports `Live`.

### Pure motion

Tests prove exact 8 ms step caps for 50, 100, and 200 percent in Walk and Run,
monotonic/no-overshoot paths, and unchanged Run thresholds. NaN, infinite,
zero, below-minimum, and above-maximum factors return `std::nullopt`.
Reduced Motion still reaches the target directly.

### Controller and panel

Controller tests prove the current setting reaches initial options, a live
change reaches a dedicated setter, and no extra `show`/hide occurs. A native
panel test proves a live speed setter leaves the visible origin unchanged;
the next move uses the new bound without reinitializing the Cat.

Representative native 700 pt target jumps are exercised at 50, 100, and 200
percent. Tests account only for AppKit whole-point quantization and assert
per-step movement is bounded by 2.88, 5.76, or 11.52 pt respectively. They
also assert multiple intermediate positions and the existing 49 x 49 canvas,
glyph, and outside-pixel invariants.

### Regression gate

Existing idle-tail, source-startle, fur/glow, Reduced Motion, source/Space
non-snap, pointer/caret parity, settings, translation, and controller tests
remain green. The focused Release gate runs settings/controller/geometry
targets. The complete safe Release gate runs 157 tests: all registered tests
except only `OSXClipboardTests` and `OSXKeyStateTests`, with
`InputIndicatorGeometryTests` isolated for WindowServer.

## Scope

In scope: one live setting, one macOS settings control, controller/panel
propagation, bounded pure pursuit speed, focused tests, Korean text for the new
label, docs, and local commits.

Out of scope: other companions, movement thresholds, cursor/caret APIs,
reduced-motion policy, canvas/anchor changes, panel resizing, source-startle
state, translation changes for other languages, release, install, push, and
pull request.
