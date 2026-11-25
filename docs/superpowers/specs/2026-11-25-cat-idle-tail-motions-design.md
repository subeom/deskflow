# Five Idle Cat Tail Motions

**Date:** 2026-11-25
**Status:** Approved for immediate implementation
**Revision base:** `389bc3924bb0c03657ce5d44876af454f9f26e05`

## Goal

Give the pointer Cat exactly five visibly different, restrained tail motions
while it is idle. A motion is finite, begins and ends at the approved rest tail,
and is followed by a different motion. Pointer-driven Walk and Run animation
must never receive one of these idle motions.

The feature adds no setting and uses no cursor, Accessibility, input-method, or
live-configuration API.

## Existing Constraints

Preserve all current behavior outside the Cat idle tail:

- Walk and Run gait, phase periods, and 8 ms render cadence.
- The 180 ms movement-to-idle settle delay.
- The 280 ms facing turn, persistent direction, and 0.80 midpoint scale.
- The 33 x 31 pt panel, one 1.10 display transform, and 7 pt geometry.
- The approved Cat artwork, continuous near contour, no-eye/no-pad art, label,
  source palette, transition, and glow.
- The 8.0 x 7.0 pt inner language target and zero glyph spill.
- The Cat's mirrored source artwork and unmirrored language glyph.
- Dog and Turtle rendering and timing.

## Decision

Separate the new timing/state model from AppKit painting. Keep the model in the
pure C++ input-indicator geometry layer and pass only a completed frame into
the panel painter:

```cpp
enum class InputIndicatorCatIdleTailMotion
{
  RelaxedSway,
  TipFlick,
  CurlUnfurl,
  LazySweep,
  QuestionLift
};

struct InputIndicatorCatIdleTailFrame
{
  InputIndicatorCatIdleTailMotion motion =
      InputIndicatorCatIdleTailMotion::RelaxedSway;
  double progress = 0;
  bool active = false;
};
```

The model owns the seeded selection state, finite duration, and current
normalized progress. AppKit owns only the path paint operation.

## Exactly Five Motions

Each duration includes eased lead-in and return-to-rest. Motion offsets are
bounded in logical Cat coordinates to 0.90 pt horizontal and 1.35 pt vertical.
The dynamic area is distal to the approximately 12.4 pt tail root; the root and
proximal body contour stay fixed.

| ID | Duration | Visible behavior |
| --- | ---: | --- |
| `RelaxedSway` | 2400 ms | Low S-shaped flex, with the proximal and distal portions moving in opposite phases. |
| `TipFlick` | 1800 ms | One short distal tip flick and return, concentrated in the middle of the duration. |
| `CurlUnfurl` | 2800 ms | Slow distal curl toward the torso followed by an equally eased unfurl. |
| `LazySweep` | 3200 ms | Broad low-amplitude whole-tail sweep in one direction and back. |
| `QuestionLift` | 2400 ms | Distal tip lifts into a question-mark hook, holds briefly, then settles. |

All five formulas use smooth finite envelopes. Progress zero and one produce
the exact original tail point. A saturated or invalid progress remains finite
and cannot create a new pose outside the bounds above.

The runtime Cat idle artwork starts from `IdleNeutral`. The existing
`IdleRise`/`IdleFall` contour data remains unchanged as approved artwork and a
regression-test pose source; a second 1.2-second phase oscillation must not be
combined with the five idle motions.

## Selection and Timing

The scheduler starts with all five IDs in a consumed-ID mask. It uses a
32-bit seeded xorshift sequence to choose among the unconsumed candidates.
After an ID completes, it is removed; when the mask empties, all five are
available for the next bag.

If multiple candidates exist, the previously played ID is excluded. Therefore:

- every five-item bag contains each ID exactly once;
- consecutive frames across a bag boundary cannot repeat the same ID;
- the same seed and timestamps yield the same IDs and progress;
- a different seed can produce a different order.

The panel supplies one local seed when constructed. The pure advance function
accepts explicit monotonic milliseconds and the pointer's existing motion
state, so tests do not depend on wall time.

Transition rules:

1. `Idle`: select or advance one finite motion.
2. `Walk` or `Run`: return a static rest frame immediately and stop the idle
   clock. Preserve the bag/last-ID state.
3. Re-entering `Idle`: start the next non-repeating ID at progress zero.
4. Reduced Motion: return a static rest frame without reading the random seed,
   selecting an ID, or advancing progress. Re-enabling animation resumes by
   selecting from the preserved state.

Idle keeps the existing 75 ms low-duty cadence. The panel still reports no
idle timer when Reduced Motion is enabled.

## Tail Deformation and Facing

The pure painter input warps near-contour points according to their distance
from the tail root:

1. Anchor the region at `x = 12.4`.
2. Ease influence from zero at the root to full at the distal tail.
3. Apply the selected ID's X/Y envelope to the weighted point.
4. Leave far-limb paths, torso points, language target, paws, and head
   unchanged.

The warp is authored in right-facing source coordinates. Existing AppKit
mirroring and the 0.80-1.00 facing turn apply after it, so a left-facing frame
is exactly the mirrored right-facing frame. The label remains outside that
silhouette mirror. No path parser, SVG asset, external file, or runtime asset
lookup is added.

The current `inputIndicatorCatTailPose` function remains the walking/running
continuity reference. It is not repurposed into an AppKit drawing primitive.

## Data Flow

1. `MacInputIndicatorPanel` computes the existing movement state.
2. The pure scheduler advances with that state, explicit monotonic time, and
   Reduced Motion.
3. The result is copied to `DeskflowInputIndicatorView`.
4. Cat painting selects `IdleNeutral` for Idle and warps only the near tail
   region from the pure frame.
5. Existing facing transforms, 1.10 display transform, halo, torso fill,
   language clipping, and source transition render in their current order.
6. Walk and Run bypass the tail warp and use the existing keyframes unchanged.

## Alternatives Considered

1. **Five authored Cat path sets.** This is simple at paint time but adds a
   large geometry surface and creates discrete jumps unless many intermediate
   paths are stored. Rejected.
2. **One faster global Cat phase with five path choices.** The existing idle
   oscillation would continue to move the tail independently of the selected
   pattern and would make clipping/timing less predictable. Rejected.
3. **Seeded state plus tail-only deformation.** This keeps the artwork and
   AppKit layer small, allows pure deterministic tests, and preserves the
   continuous contour. Selected.

## Test Contract

The pure QtTest coverage proves:

- exactly the five named IDs occur in the first bag;
- consecutive scheduler outputs never repeat an ID;
- the five offset curves differ at representative progress values;
- Walk and Run return inactive/rest frames;
- progress, offsets, and durations stay finite and bounded;
- mirrored left-facing points exactly equal mirrored right-facing points;
- the same seed and timestamps produce identical state and frames;
- different seeds can change the order;
- Reduced Motion leaves selection state untouched and produces exact rest
  coordinates.

Native paint coverage renders every approved Cat artwork pose. For each idle
artwork frame it covers all five motions at representative middle samples, in
both facing directions and for both language sources. It keeps the existing
CoreText ink check and also uses a padded bitmap to prove the scaled silhouette
does not escape the 33 x 31 pt panel. `outsidePixels == 0` must hold for every
case.

Focused indicator tests use the named targets and a scratch
`HOME`/`XDG`/`TMPDIR`. The final safe gate is a clean Release Qt 6.12 build
with warnings as errors, named registered targets, and CTest excluding only
`OSXClipboardTests` and `OSXKeyStateTests`. No live core, installer, release,
push, tag, install, or PR operation is part of this change.

## Scope

In scope: pure scheduling/deformation, Cat panel integration, native paint
coverage, the design document, and a tiny optional local preview.

Out of scope: new motions for Dog/Turtle, new Cat facial art, settings, network
data, pointer APIs, private accessibility/input APIs, release work, and
behavior changes to the language label or source halo.
