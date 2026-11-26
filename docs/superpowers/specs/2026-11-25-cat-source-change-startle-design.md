# Cat Source-Change Startle Reaction

**Date:** 2026-11-25
**Status:** Approved for immediate implementation
**Revision base:** `95c7da5999ac15d3dcd5b1a2098bc3ea7a110925`

## Goal

Replace the Cat's source-change glow with one continuous startle reaction
while preserving the existing `A`/`한` label crossfade:

1. 0-70 ms: slight backward recoil and 6-8% horizontal compression.
2. 70-160 ms: dorsal back fur raises smoothly by at most 1.8 logical pt.
3. 160-300 ms: the complete contour eases to the exact approved baseline.

The reaction is Cat-only. Dog and Turtle keep their existing source behavior.
There is no Cat ambient halo, wave, detached spike, full-body jump, panel
resize, eye, pad, or setting.

## Existing Contracts

Preserve:

- exact known `A` to `한` and `한` to `A` source changes as the only trigger;
- no trigger for initial baseline, duplicate snapshot, or Unknown source;
- the existing 200 ms language label/body-color transition;
- a fixed, upright language glyph and readable 8.0 x 7.0 pt inner target;
- the 33 x 31 pt panel, 7 pt geometry, and one 1.10 display transform;
- the continuous near/far Cat contours and approved Walk/Run keyframes;
- persistent facing and the existing 280 ms turn;
- the five idle-tail patterns and their no-immediate-repeat scheduler;
- Reduced Motion: label/source crossfade only, with no startle or halo.

## Decision

Replace the source-change halo state in the panel with a pure
`InputIndicatorCatSourceStartle` state machine in
`InputIndicatorGeometry.*`. Pure code owns triggering, coalescing, progress, and
the completed contour frame:

```cpp
struct InputIndicatorCatStartleFrame
{
  bool active = false;
  double progress = 0;
  double recoilOffsetX = 0;
  double horizontalScale = 1;
  double dorsalLift = 0;
};
```

The scheduler returns one completed frame. AppKit only maps that frame to
existing Bézier points before mirror/facing/display transforms.

## Trigger and Coalescing

The existing Cat `sourceChanged` branch in `MacInputIndicatorPanel::show`
continues to detect the event. It calls the pure startle trigger with the
previous and next source plus monotonic milliseconds.

Rules:

- both sources must be exactly `LatinABC` or `Korean2Set`, and different;
- the first valid event starts immediately;
- a later event during an active reaction replaces the `latestSource`;
- no more than one replacement start occurs per 50 ms;
- if an event arrives inside that 50 ms window, retain one pending latest
  source and start it at the throttled boundary;
- only one startle state/frame exists, so rapid flips never stack;
- Reduced Motion rejects the event, clears active/pending reaction state, and
  performs no start;
- Unknown, identical, initial, hide, and companion-change paths do not start a
  reaction.

The pure state stores the current start, last start, latest source, active
flag, and one pending flag. Tests must prove the state has no queue or
afterimage array.

## 300 ms Pose

Let `age` be clamped to `[0, 300]`:

| Interval | Recoil | Compression | Dorsal lift |
| --- | --- | --- | --- |
| 0-70 ms | smooth 0 to -0.45 pt | smooth 1.00 to 0.92 | 0 |
| 70-160 ms | smooth -0.45 to 0 | smooth 0.92 to 1.00 | smooth 0 to 1.8 pt |
| 160-300 ms | 0 | 1.00 | smooth 1.8 to 0 pt |

Progress zero and 300 return:

```cpp
recoilOffsetX == 0
horizontalScale == 1
dorsalLift == 0
active == false at 300 ms
```

Invalid timestamps yield finite rest-safe output rather than extending the
reaction.

## Continuous Contour Deformation

The startle point helper accepts right-facing source coordinates:

1. Scale every Cat near/far silhouette point horizontally around
   `x = 15` by `horizontalScale`.
2. Add `recoilOffsetX` before existing mirroring/facing transforms. A left
   facing Cat therefore recoils backward in screen space.
3. On the near contour only, add a smooth vertical dorsal weight concentrated
   over the approved back curve. The weight fades to zero at the tail/root and
   head/ear boundary.
4. Apply torso, idle-tail, and startle point functions in one continuous path.

The far leg/head contours receive recoil/compression but no detached fur
primitive. The language target and glyph drawing receive no startle point
warp. Label clipping still uses the transformed near contour and the existing
upright label target.

The dorsal weight is attached to existing Bézier endpoints/controls. It is not
a triangle, spike overlay, or separate silhouette union.

## Glow Removal and Idle Tail Handoff

Delete Cat halo frame creation, storage, ambient glow drawing, pulse
properties, and pulse timer conditions. Keep the source body-color/glyph
transition unchanged.

Dog and Turtle rendering continues to use the existing `previousSource` to
`source` transition without any startle state.

During an active or pending Cat startle, the Cat idle-tail frame is forced to
inactive rest. The idle scheduler state and pattern start are preserved:

- record the start of the pause;
- do not consume another idle pattern;
- at startle end, shift the idle scheduler's start time by the paused duration;
- resume the same progress if the Cat is still Idle;
- if movement occurred, normal Walk/Run gating takes precedence.

The panel requests 8 ms frames while the 200 ms crossfade or 300 ms reaction
is active, then returns to the existing 75 ms settled Idle cadence. Reduced
Motion stops the reaction/timer and keeps only the existing source transition.

## Alternatives Considered

1. **Keep glow and add spikes.** Reintroduces the effect the user asked to
   replace and adds detached art/afterimages. Rejected.
2. **Animate the whole panel or Cat rigidly.** Moves the label and pointer
   relationship and risks clipping/full-body jumps. Rejected.
3. **Continuous contour recoil/fur deformation.** Preserves one silhouette,
   keeps the label fixed, and is directly testable from pure points. Selected.

## Test Contract

Pure tests cover:

- initial, duplicate, and Unknown triggers remain inactive;
- Latin/Korean transitions start a 300 ms finite frame;
- 70/160/300 ms compression, recoil, and dorsal bounds;
- exact start/end neutral point output and finite invalid time behavior;
- rapid flips keep only latest source with one start per 50 ms;
- Reduced Motion clears/ignores reaction state;
- Cat/idle-tail pause duration restores the same scheduler progress.

Panel/native tests cover:

- exact known source change triggers once;
- duplicate and Unknown changes do not trigger;
- Cat has no ambient or pulse halo property/path;
- Dog/Turtle crossfade behavior remains unchanged;
- rapid source changes do not stack;
- real `DeskflowInputIndicatorView` invalidates and repaints changed fur pixels;
- source label pixels remain stable/upright and inside the transformed contour;
- 1x/2x, both facings, and both sources remain inside 33 x 31;
- reaction endpoint exactly matches the baseline raster;
- existing 264 Cat paint/clipping cases continue with `outsidePixels == 0`;
- the previous tail-pixel perceptibility thresholds continue to pass.

## Scope

In scope: pure startle state/pose, Cat source-change trigger reuse, halo
removal, continuous Cat contour deformation, idle-tail handoff, native pixels,
docs, and focused/full safe tests.

Out of scope: Dog/Turtle reaction animation, sound, cursor movement, source
settings, release/install, live configuration/process changes, and any panel
size change.
