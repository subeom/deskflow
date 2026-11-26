# Input Source Body Color

**Date:** 2026-11-26

**Status:** Approved for immediate implementation

**Base:** `a555ea1ac` on
`subeomlee-microsoft-five-idle-cat-tail-motions`

## Goal

Make the active keyboard source recognizable from the companion body without
changing the existing glyph, motion, timing, placement, or shape contracts.
Korean keeps the established off-white body. Latin/English uses a muted
Russian-blue gray body with a darker navy glyph.

## Source palette

Body colors do not change with display tone. The existing tone variants remain
responsible for glyph contrast and glow hue.

| Source role | Automatic | Light | Dark |
| --- | --- | --- | --- |
| Korean body | `#F2F3F5` (242, 243, 245) | `#F2F3F5` | `#F2F3F5` |
| Latin body | `#A8BAC7` (168, 186, 199) | `#A8BAC7` | `#A8BAC7` |
| Korean glyph | `#884E62` (136, 78, 98) | `#7A3E52` (122, 62, 82) | `#663144` (102, 49, 68) |
| Latin glyph | `#30425A` (48, 66, 90) | `#334A64` (51, 74, 100) | `#283B52` (40, 59, 82) |
| Edge | `#44505F` (68, 80, 95) | `#44505F` | `#44505F` |

The Latin/English body is cool by construction: blue is greater than both red
and green, with `blue - red == 31` and `blue - green == 13`. Its glyph contrast
against `#A8BAC7` is:

| Tone | Contrast |
| --- | ---: |
| Light | 4.56:1 |
| Automatic | 5.12:1 |
| Dark | 5.72:1 |

The unchanged Korean glyph colors retain 5.71:1, 7.17:1, and 9.06:1 contrast
against `#F2F3F5`. The unchanged edge color retains 4.11:1 contrast against the
Latin body and stronger contrast against the Korean body.

## Model and painter behavior

`MacInputIndicatorColorModel` owns two explicit body colors:
`koreanBody` and `latinBody`. It also owns the source-aware glyph and shared
edge colors. The former 8% glyph/body blend is removed.

`macInputIndicatorBodyColor(source, tone)` returns:

- `koreanBody` for `InputIndicatorSource::Korean2Set`;
- `latinBody` for `InputIndicatorSource::LatinABC` and preserves the existing
  non-Korean fallback for `Unknown`.

The panel continues to create the previous and current source body colors,
then blends them with the existing eased `transitionProgress`. No opacity,
`200 ms` duration, source state, redraw interval, glyph crossfade, or geometry
field changes.

Body opacity remains `60..100%`, using the existing body/detail split:

- `macInputIndicatorRenderAlpha` supplies the body opacity;
- glyph and halo detail retain the existing `+0.10`, clamped to `1.0`;
- Cat's far/near layering and all shared edge strokes remain unchanged.

The startle/reaction glow continues to use
`macInputIndicatorGlyphColor(latest coalesced source, tone)`. It therefore
remains source-aligned while the body and glyph finish their independent
200 ms crossfade. The Latin glow becomes the selected darker navy hue; the
Korean glow remains rose/mauve.

## Release note wording

> Input source recognition is now visible in the companion body: Korean
> remains exact off-white `#F2F3F5`, while Latin/English uses muted Russian
> blue-gray `#A8BAC7` with a contrast-safe navy glyph. The existing 200 ms
> source crossfade, opacity, geometry, and motion are unchanged.

## Preserved behavior

This revision must not alter:

- 49 x 49 pt canvas, 33 x 31 pt anchor, 8/9 pt offsets, 7 pt cursor gap, or
  1.10 visual scale;
- Cat, Dog, or Turtle artwork, labels, clipping, idle motion, gait, facing
  compression, body lean, or eight-heading rotation;
- fixed positions or Cursor Follow Approach;
- 50%, 100%, and 200% follow-speed movement caps;
- exact 50 ms source coalescing and 0/70/160/300 ms reaction envelope;
- one full-silhouette, source-colored, 2.2 pt glow behind the body;
- 2.4 pt Cat dorsal lift, Reduced Motion behavior, label uprightness, or
  source-change repaint behavior;
- settings, localized text, IPC, target selection, or release packaging.

## Test contract

### Pure color

For all three tones:

- Korean body RGB equals 242/255, 243/255, 245/255 exactly;
- Latin body RGB equals 168/255, 186/255, 199/255 exactly;
- Korean and Latin body values are materially distinct and the Latin body is
  visibly cool;
- Latin glyph/body contrast is at least 4.5:1;
- Korean glyph/body contrast is at least 4.5:1;
- body blending is finite, bounded, and exact at transition progress 0 and 1;
- body alpha remains 0.60, 0.65, and 1.00 for opacity 60, 65, and 100;
- edge/body contrast remains visible for the Latin body.

### Native raster

Controlled AppKit raster tests cover:

- Cat, Dog, and Turtle;
- Latin and Korean;
- 1x and 2x backing scales;
- both facings and eight body headings;
- the existing Follow Approach speed/heading path at 50%, 100%, and 200%.

For every raster case:

- at least one core body pixel matches the selected source RGB within 8-bit
  rounding and retains the requested body alpha;
- Korean and Latin alpha-weighted body pixels differ meaningfully;
- glyph ink remains upright and wholly inside its species silhouette, with
  width/height stable within one 1x/2x raster pixel across heading;
- paint, body, and glyph pixels stay inside the 49 x 49 canvas;
- `outsidePixels == 0`;
- the startle halo remains source-colored and behind the body; normalized
  hue-channel assertions allow at most 0.06 for 8-bit antialiasing, while
  keeping Latin navy and Korean rose separable.

### Regression gates

The focused gate is 15/15. The safe full CTest gate is 158/158, with Geometry
isolated for WindowServer; only `OSXClipboardTests` and `OSXKeyStateTests`
remain excluded. `InputIndicatorGeometryTests` must pass 14 consecutive
times. Translation output is restored before commit and the worktree is clean.

## Scope

In scope: the macOS input-indicator color model, body selection, navy glyph
tones, pure color assertions, controlled 1x/2x raster coverage, Follow
Approach integration evidence, and design/plan documentation.

Out of scope: settings, new UI text, translations, geometry, artwork,
animation timing, source/coalescing state, follow-speed behavior, live package
mutation, release, install, push, and pull request.

## Decision

Use explicit source body fields in the pure color model and retain the
existing panel crossfade. This separates visual source identity from glyph
color, avoids a barely visible tint, keeps Cat far/near shading and all
motion/geometry code unchanged, and lets pure tests enforce exact RGB and
contrast.
