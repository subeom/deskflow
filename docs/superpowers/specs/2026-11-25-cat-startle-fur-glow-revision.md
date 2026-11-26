# Cat Startle Fur and Glow Revision

**Date:** 2026-11-25
**Status:** Approved for immediate implementation
**Revision base:** `650d2216593a55c5fd636ea8180230d13e85e171`

## Goal

Make the existing 300 ms Cat source-change startle more visible while keeping
the Cat readable at pointer scale:

- raise the continuous dorsal contour from 1.8 to 2.4 logical pt;
- add one soft, source-colored full-Cat silhouette glow behind the body;
- keep one coalesced reaction, not old ambient/stacked halo waves.

## Existing Constraints

Preserve:

- exact LatinABC/Korean2Set transitions as the only trigger;
- the 0-70 ms recoil/compression, 70-160 ms raise, 160-300 ms settle;
- 8% maximum body compression and -0.45 pt recoil;
- 200 ms `A`/`한` body/label color transition;
- 50 ms replacement/coalescing and one active startle state;
- five idle-tail patterns and paused/resumed scheduler progress;
- 120 pt/s Walk, 720 pt/s Run, no-teleport pursuit, and diagonal heading;
- one upright/unmirrored label and the existing CoreText ink clip;
- 33 x 31 visible art, 7 pt geometry, and 49 x 49 Cat canvas;
- no panel resize, detached spike, eye, pad, setting, caret read, or source API;
- Dog/Turtle behavior unchanged.

Reduced Motion performs the existing source crossfade only. It creates no fur
movement and no glow.

## Fur Revision

Change only the peak dorsal value:

| Time | Dorsal lift |
| --- | ---: |
| 0-70 ms | 0 |
| 70-160 ms | smooth 0 to 2.4 pt |
| 160-300 ms | smooth 2.4 to 0 pt |

The same continuous Bézier-point weight is retained. Tail, paws, ears, head,
and label controls remain at their prior startle behavior. There is no new
primitive or independent fur object.

## Glow Decision

Add one `InputIndicatorCatStartleFrame` glow envelope:

```cpp
double haloScale = 1;
double haloAlpha = 0;
```

The frame remains the complete one-reaction state; no pulse vector, queue,
ring, or afterimage list is introduced.

The halo uses the same 300 ms age as the Cat:

- at 0 ms: scale 1.00, alpha 0;
- rise smoothly with the reaction;
- peak scale 1.12;
- peak alpha 0.16;
- at 300 ms: scale 1.00, alpha 0;
- one finite smooth envelope, with the peak near the 160 ms fur peak.

Color follows `latestSource` after coalescing:

- `LatinABC`: existing muted steel/slate glyph color;
- `Korean2Set`: existing dusty rose/mauve glyph color.

The glow is not a persistent ambient tint and does not read an intermediate
crossfade color. The current body/label transition remains 200 ms.

## Paint Order

Cat painting uses one union:

1. build the transformed near contour;
2. build the transformed far contour;
3. append them into one full-Cat silhouette;
4. scale that silhouette to `haloScale` around the Cat logical center;
5. paint one low-alpha source-colored halo/shadow with a 2.2 logical pt soft
   blur behind all Cat parts;
6. paint the existing far and near body paths;
7. paint the language glyph last.

The glow uses the same mirror, facing compression, heading rotation, startle
point transform, display transform, and 49 x 49 canvas as the body. The glyph
does not receive glow color, scale, or rotation.

## Coalescing and Reduced Motion

Rapid source flips continue to use the existing one active/one pending startle
state. They update only the latest source and replace the next 50 ms reaction.
Fur and halo share that state and cannot desynchronize or stack.

Reduced Motion returns:

- `dorsalLift = 0`;
- `haloScale = 1`;
- `haloAlpha = 0`;
- no active glow path.

Initial, duplicate, and Unknown transitions produce no fur/glow frame.

## Alternatives Considered

1. **Restore the old three-wave halo.** Directly conflicts with the requested
   cohesive Cat silhouette and risks strobe/afterimage behavior. Rejected.
2. **Add detached fur spikes plus a ring glow.** Increases visibility but
   breaks the continuous approved contour and reads as an icon effect.
   Rejected.
3. **Raise the existing dorsal contour and pair it with one synchronized
   full-silhouette glow.** Preserves motion/composition and makes source color
   legible. Selected.

## Test Contract

Pure tests prove:

- dorsal peak is exactly 2.4 pt and endpoints remain exactly neutral;
- halo starts at scale 1/alpha 0 and ends at scale 1/alpha 0;
- halo peak is <=1.12 and <=0.16;
- exact final/rest source state produces no glow;
- latest coalesced source selects the final glow color;
- Reduced Motion produces no fur/glow;
- one state has no queue or multiple glow frames.

Native tests prove:

- 2.4 pt dorsal changed-pixel count exceeds the old 1.8 pt reference;
- one full-silhouette halo exists only outside/behind the body edge;
- source halo color is slate for `A` and rose for `한`;
- halo paint cannot replace or tint the glyph;
- no halo pixels exist at reaction 0 or 300 ms;
- 1x/2x, both facings and sources, and all 8 pursuit headings stay inside
  49 x 49 with `outsidePixels == 0`;
- existing tail/startle 48/264/64 cases and 7 pt/33 x 31 geometry remain green.

## Scope

In scope: pure startle frame extension, dorsal peak revision, one source-colored
full-Cat halo, painter integration, native pixels, docs, and safe tests.

Out of scope: release/install, live process/config changes, old ambient halo,
new settings, Dog/Turtle effects, cursor/caret APIs, and panel/canvas resizing.
