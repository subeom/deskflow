# Dog and Turtle Companion Animation Parity

**Date:** 2026-11-26

**Status:** Approved for immediate implementation

**Base:** `6da43d4f9388f7e56d3d7775e17cd6a86a08e0a9` after the live
companion follow-speed revision

## Goal

Bring Dog and Turtle to Cat-level animation quality while preserving distinct
species identity. All three companions share pursuit, idle scheduling, source
reaction timing, rotation, clipping, Reduced Motion, and 49 pt canvas
mechanics. Cat keeps its approved artwork and pixel contracts. Dog and Turtle
receive continuous species-specific Bézier silhouettes rather than reused Cat
shapes or the current primitive stacks.

## Shared behavior

Every Cat, Dog, and Turtle uses:

- the one shared velocity-bounded pursuit state implemented in
  `InputIndicatorCompanionPursuitState`;
- the live `50..200%` follow-speed multiplier;
- the 8 ms simulation cap, 48 pt Run distance threshold, 260 pt/s target
  speed threshold, 12 pt Run hysteresis, 180 ms Idle settle, 10 rad/s heading
  turn, no overshoot, and no teleport;
- state-preserving source, Space, and follow-speed reconfiguration;
- actual diagonal body-heading rotation;
- a 49 x 49 transparent canvas with the existing 33 x 31 visible anchor,
  8/9 pt artwork offsets, 7 pt cursor gap, and 1.10 visual scale;
- one upright, unmirrored `A` or `한` glyph centered in and clipped to the
  current species silhouette;
- five deterministic idle pattern IDs with no immediate repeat;
- one exact-source 300 ms reaction with 50 ms coalescing, 0/70/160/300 ms
  endpoints, one transformed full-silhouette source-colored glow, and no
  ambient, stacked, detached, or persistent halo;
- neutral/direct Reduced Motion placement with zero idle point movement,
  source reaction, or glow.

Cat remains the visual and behavior reference. Existing Cat artwork, idle-tail
pixel thresholds, dorsal 2.4 pt peak, slate/rose halo evidence, 264 base,
48 startle, and 64 heading cases are unchanged.

## Shared model

### Companion identity

`InputIndicatorCompanion` moves into `InputIndicatorGeometry.h`, where the pure
motion layer can use Cat, Dog, and Turtle without depending on the macOS panel
header.

### Idle scheduler

The existing deterministic no-repeat state becomes
`InputIndicatorCompanionIdleMotionState`. Its five IDs, durations, seed,
finite endpoints, and no-immediate-repeat selection stay identical:

| ID | Cat interpretation | Dog interpretation | Turtle interpretation |
| ---: | --- | --- | --- |
| 0 | relaxed tail sway | ear perk | head peek |
| 1 | tip flick | short tail flick/ear bounce | shell settle |
| 2 | curl/unfurl | ear flop | foot tuck |
| 3 | lazy sweep | tail wag | cautious head/tail shift |
| 4 | question lift | nose settle and tail curl | extended peek then retract |

Each species maps the same frame to a different continuous point deformation.
No species gets a queue, random per-frame choice, or its own independent
scheduler.

### Gait model

The pure companion motion pose includes:

- Idle, Walk, and Run phase;
- body bob/stretch;
- tail phase;
- limb stride;
- body lead/extension for Run.

Dog Walk is a readable alternating trot with opposite paw progression. Dog Run
extends the front/rear reach, lowers the torso, and increases tail angle while
preserving the same bounded position cap.

Turtle Walk is an alternating crawl with feet exposed in opposite pairs.
Turtle Run is a short shell-forward dash with faster art-cycle stride; it does
not change the physical movement model to teleport or bypass the speed cap.

Cat continues to select its approved Idle/Walk/Run artwork sheet.

### Source reaction and glow

The existing exact-source trigger, 300 ms envelope, coalescing state, final
source selection, and halo envelope are generalized to all companions under
source-compatible aliases. The shared reaction frame remains the one source
of truth.

- Dog uses the 0-70 ms 8% recoil/compression, then an ear perk and brief tail
  flick toward the shared 160 ms peak, returning to neutral by 300 ms.
- Turtle uses the same recoil/compression on the shell, a controlled head
  peek/extension at the peak, and shell/head retraction by 300 ms.
- Cat retains its 2.4 pt dorsal lift.

The glow is exactly one union path, scaled once to `haloScale`, painted in the
latest coalesced glyph color with a 2.2 logical pt zero-offset blur, then
painted behind the body. Body, stroke, and glyph use existing palette/opacity.
The final source color remains authoritative even during the 200 ms
body/label crossfade.

## Species artwork

### Dog

Dog is a continuous side-profile silhouette with:

- a distinct long torso and belly contour;
- a raised muzzle/head and at least one readable ear contour;
- a tail as one continuous curved lobe;
- two visible near-paw lobes and smaller far paw hints;
- no rounded-rectangle body primitive, eye, pad, badge, or detached face
  object.

The artwork is represented by Bézier path commands and deformed before facing,
heading, and display transforms. Idle motion moves only approved local ear,
tail, and nose regions. Walk and Run move paws and torso in readable cycles.
Reaction lift is localized to ears/tail and does not resize the label.

### Turtle

Turtle is a continuous side-profile silhouette with:

- a domed shell with a readable belly seam;
- a forward head and short neck;
- a tapered tail;
- paired visible feet and smaller far-foot hints;
- no eye, pad, shell pattern decoration, or detached face object.

Idle motion moves only shell settle, head/neck, feet, or tail in species-
specific finite windows. Walk alternates foot exposure. Run keeps the shell
readable, brings it slightly forward, extends one foot pair, and speeds only
the art phase. Reaction moves the head/neck and shell recoil, then returns to
neutral.

## Paint order and label

For each species:

1. construct the species path in logical art coordinates;
2. apply pure motion, idle, and reaction point transforms;
3. apply mirror/facing compression and actual heading rotation;
4. append the body subpaths into one glow union;
5. scale the glow around the logical body center and paint it;
6. stroke/fill the body;
7. draw the upright language glyph inside a species-specific inner bounds and
   clip it to the current body path.

The glyph never receives the halo transform, reaction warp, mirror, or
rotation. Only its center/clip follows the transformed body.

## Architecture decision

Three approaches were considered:

1. **Copy Cat timing/state for Dog and Turtle.** Rejected because three
   schedulers would drift and source coalescing could split.
2. **Use pre-rendered Dog/Turtle sprite sheets.** Rejected because arbitrary
   headings, facing compression, reaction scale, and 1x/2x clipping would need
   many generated frames and separate glow masks.
3. **Shared pure timeline + species Bézier point warps.** Selected. It preserves
   Cat timing, gives Dog/Turtle distinct artwork, exposes finite pure inputs,
   and lets one painter layer handle facing, heading, glow, glyph, and canvas.

## Test contract

### Pure

Tests prove:

- all three companion identities return finite motion poses;
- Dog/Turtle Walk and Run frames differ and have finite 0..1 phase endpoints;
- all five idle IDs cover finite patterns, do not repeat immediately, and gate
  to neutral outside Idle or in Reduced Motion;
- Dog ear/tail and Turtle head/shell/feet reaction point transforms stay within
  approved bounds and return exactly to neutral at 300 ms;
- 50/100/200 pursuit caps still apply through the shared companion path;
- target-speed/distance/hysteresis, direct Reduced Motion placement, pointer
  and caret parity, and no overshoot remain unchanged;
- upright label body-center transform uses the same heading for all species
  without rotating glyph ink.

### Native raster

For each Dog and Turtle:

- 1x and 2x resting, all five idle motifs, Walk, Run, 0/160/300 reaction
  extrema;
- both facings, both languages, and eight headings;
- 49 x 49 canvas clipping with `outsidePixels == 0`;
- no glyph pixels outside the species body;
- halo-only pixels use the final slate/rose hue and no endpoint halo;
- idle and gait changed-pixel counts exceed zero at 1x and 2x.

The combined heading matrix is 128 new Dog/Turtle cases:
`2 species * 2 facings * 2 sources * 8 headings * 2 scales`.

### Panel

Fixed panel tests prove for Cat, Dog, and Turtle:

- large jumps traverse multiple intermediate positions;
- 50/100/200 per-frame caps remain bounded;
- live speed changes preserve the visible origin;
- source and Space changes do not snap;
- source reaction reaches peak and endpoint using one active state;
- Reduced Motion places directly with zero idle/reaction pixels.

### Regression gates

The focused gate uses the established 15 target set: settings, controller,
geometry, policy, palette/layout, translation, rendering, and Quit/state
targets. The full gate runs every safe registered CTest, excluding only
`OSXClipboardTests` and `OSXKeyStateTests`, with
`InputIndicatorGeometryTests` isolated for WindowServer.

## Scope

In scope: species-neutral companion model aliases, common panel animation
integration, 49 pt canvas for all, Dog/Turtle continuous artwork, five idle
motifs each, Walk/Run art, reaction/glow, upright clipped glyphs, tests,
docs, and local commits.

Out of scope: Cat artwork changes, new menu items, source/caret APIs, reduced
motion policy, companion color settings, eye/pad decorations, audio, particle
effects, sprite files, release, install, push, and pull request.
