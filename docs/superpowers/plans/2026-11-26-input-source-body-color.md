# Input Source Body Color Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Korean companions keep exact `#F2F3F5` bodies while Latin companions use `#A8BAC7` bodies and contrast-safe navy glyphs without changing motion, transition, or geometry.

**Architecture:** Extend the pure macOS color model with explicit Korean and Latin body fields, then select the current and previous body colors through the existing 200 ms panel crossfade. Controlled AppKit fixtures render exact source colors at 1x/2x for all species, facings, headings, and Follow Approach states; no state-machine or geometry ownership moves.

**Tech Stack:** C++23, Objective-C++, AppKit, Qt 6/QTest, CMake/Ninja, CTest.

---

## File map

- `docs/superpowers/specs/2026-11-26-input-source-body-color-design.md`
  owns the exact palette, invariants, scope, and evidence contract.
- `docs/superpowers/plans/2026-11-26-input-source-body-color.md`
  owns task order, commands, and expected test states.
- `src/lib/gui/input/MacInputIndicatorPanel.h`
  owns the pure source body/glyph palette and source selection.
- `src/unittests/gui/input/InputIndicatorGeometryTests.h`
  declares the source-body raster regression.
- `src/unittests/gui/input/InputIndicatorGeometryTests.mm`
  owns the pure color checks, AppKit raster fixture fills, all-species
  heading/body matrix, and Follow Approach pixel assertions.
- `src/lib/gui/input/MacInputIndicatorPanel.mm`
  consumes the helper but should need no implementation edit because it
  already crossfades `macInputIndicatorBodyColor` for previous/current
  sources.

## Exact palette

```cpp
constexpr MacInputIndicatorRgb kKoreanBody{
    .red = 242.0 / 255.0,
    .green = 243.0 / 255.0,
    .blue = 245.0 / 255.0,
};
constexpr MacInputIndicatorRgb kLatinBody{
    .red = 168.0 / 255.0,
    .green = 186.0 / 255.0,
    .blue = 199.0 / 255.0,
};
```

Latin glyphs are Light `(51, 74, 100)`, Automatic `(48, 66, 90)`, and Dark
`(40, 59, 82)`. Korean glyphs and `#44505F` edges are unchanged.

---

### Task 1: Add red exact-color and raster tests

**Files:**
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.h`
- Modify: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`

- [ ] **Step 1: Declare the source-body raster test**

Add this slot after the palette slot:

```cpp
void companionPaletteUsesSourceBodiesAndReadableGlyph();
void companionSourceBodiesRasterDistinctAtOneAndTwoX();
```

Rename the existing palette implementation from
`companionPaletteUsesNeutralBodyAndReadableGlyph` to the first name.

- [ ] **Step 2: Extend raster fixtures with controlled body fill**

Add optional parameters at the end of `catBodyBitmap`:

```cpp
std::optional<MacInputIndicatorRgb> bodyColor = std::nullopt,
double bodyAlpha = 1.0
```

Add the same parameters at the end of `companionBodyBitmap`. Preserve every
existing caller by keeping defaults. Replace the black fill near the end of
each fixture with:

```cpp
NSColor *bodyFill =
    bodyColor
        ? [NSColor colorWithSRGBRed:bodyColor->red
                              green:bodyColor->green
                               blue:bodyColor->blue
                              alpha:bodyAlpha]
        : [NSColor blackColor];
[bodyFill setFill];
```

Use the same fill for every Cat path. Do not change stroke, glow, transform,
scale, or glyph fixture behavior.

- [ ] **Step 3: Add a premultiplied color difference helper**

Below `bitmapChangedPixels`, add:

```cpp
int bitmapPremultipliedColorDifferencePixels(
    NSBitmapImageRep *from,
    NSBitmapImageRep *to
)
{
  if (!from || !to ||
      from.pixelsWide != to.pixelsWide ||
      from.pixelsHigh != to.pixelsHigh) {
    qFatal("raster color comparison requires equal bitmaps");
  }
  int changed = 0;
  for (NSInteger y = 0; y < from.pixelsHigh; ++y) {
    for (NSInteger x = 0; x < from.pixelsWide; ++x) {
      const NSColor *left = [from colorAtX:x y:y];
      const NSColor *right = [to colorAtX:x y:y];
      const double difference =
          std::abs(
              left.redComponent * left.alphaComponent -
              right.redComponent * right.alphaComponent
          ) +
          std::abs(
              left.greenComponent * left.alphaComponent -
              right.greenComponent * right.alphaComponent
          ) +
          std::abs(
              left.blueComponent * left.alphaComponent -
              right.blueComponent * right.alphaComponent
          );
      if (difference > 0.01) {
        ++changed;
      }
    }
  }
  return changed;
}
```

The helper therefore fails the complete test immediately on an invalid input,
rather than silently treating mismatched dimensions as equal.

- [ ] **Step 4: Replace the pure palette assertions**

In `companionPaletteUsesSourceBodiesAndReadableGlyph`, assert exact
`koreanBody` and `latinBody` values for Automatic, Light, and Dark. Assert
`latinBody.blue - latinBody.red >= 31.0 / 255.0 - 1e-12`,
`latinBody.blue - latinBody.green >= 13.0 / 255.0 - 1e-12`, source-helper
selection, and both source contrast checks `>= 4.5`.

Use the exact expected Latin glyph checks:

```cpp
QCOMPARE(
    macInputIndicatorGlyphColor(
        Source::LatinABC, InputIndicatorTone::Light
    ).red,
    51.0 / 255.0
);
```

Add exact green/blue and Automatic/Dark checks the same way. Assert all three
edge-to-Latin-body contrast ratios exceed 4.0.

For transition progress values `{0.0, 0.25, 0.5, 0.75, 1.0}`, use
`macInputIndicatorBlendColor` on the Korean and Latin bodies; assert finite,
bounded channels, exact progress-0 Korean RGB, and exact progress-1 Latin RGB.
Keep `macInputIndicatorRenderAlpha(60/65/100)` body assertions in this test.

- [ ] **Step 5: Add the all-species source raster matrix**

Implement `companionSourceBodiesRasterDistinctAtOneAndTwoX` with:

```cpp
companions = {Cat, Dog, Turtle}
sources = {LatinABC, Korean2Set}
facings = {true, false}
headings = {
  0, pi/4, pi/2, 3*pi/4,
  pi, 5*pi/4, 3*pi/2, 7*pi/4
}
scales = {1, 2}
```

Render one body for each case with its `macInputIndicatorBodyColor`,
`bodyAlpha = 0.65`, and the matching glyph. Assert:

1. at least 10 core pixels match the exact RGB within `2.0 / 255.0` and alpha
   within 0.01;
2. no body or glyph pixel falls outside the 49 x 49 canvas;
3. no glyph pixel with alpha above 0.02 lands where the body is absent;
4. glyph alpha bounds width and height stay within one pixel across heading
   for the same species/source/facing/scale, allowing 8-bit subpixel
   placement.

For each geometry tuple, render Latin then Korean and assert:

```cpp
bitmapPremultipliedColorDifferencePixels(latin, korean) >= 25;
```

Track and print `cases`, `matchedBodyPixels`,
`alphaWeightedDifferencePixels`, `outsidePixels`, and
`glyphOutsidePixels`. Require exactly 192 cases and zero outside pixels.

- [ ] **Step 6: Extend the Follow Approach raster test**

In `followApproachCompanionSpeedRasterMatrix`, pass
`macInputIndicatorBodyColor(Source::LatinABC, Automatic)` and
`macInputIndicatorRenderAlpha(65).body` to each reconstructed Cat/Dog/Turtle
body. Extend its result type with `exactBodyPixels`; count pixels within
2/255 RGB and 0.01 alpha. Require at least 10 exact body pixels per 1x/2x
case and keep every existing movement-cap and outside-canvas assertion.

- [ ] **Step 7: Build and prove red**

Run:

```bash
cmake --build /tmp/deskflow-cat-fur-glow-650d-release \
  --config Release --parallel 8 \
  --target InputIndicatorGeometryTests
ctest --test-dir /tmp/deskflow-cat-fur-glow-650d-release \
  -R '^InputIndicatorGeometryTests$' --output-on-failure
```

Expected: FAIL because `koreanBody`/`latinBody` do not exist, exact Latin
glyph/body values do not match, and source bodies remain 8% tint.

---

### Task 2: Implement explicit source body selection

**Files:**
- Modify: `src/lib/gui/input/MacInputIndicatorPanel.h`

- [ ] **Step 1: Extend the model**

Replace the single body field with:

```cpp
struct MacInputIndicatorColorModel
{
  MacInputIndicatorRgb koreanBody;
  MacInputIndicatorRgb latinBody;
  MacInputIndicatorRgb edge;
  MacInputIndicatorRgb latinGlyph;
  MacInputIndicatorRgb koreanGlyph;
};
```

- [ ] **Step 2: Set exact tone values**

In `macInputIndicatorColorModel`, define `koreanBody` and `latinBody` using the
palette at the top of this plan. Every tone returns the same two body values
and the same existing edge. Set Latin glyphs to:

```cpp
Light:      51/255,  74/255, 100/255
Automatic:  48/255,  66/255,  90/255
Dark:       40/255,  59/255,  82/255
```

Leave every Korean glyph channel unchanged.

- [ ] **Step 3: Select instead of tinting**

Replace the 8% blend in `macInputIndicatorBodyColor` with:

```cpp
return source ==
        deskflow::input::InputIndicatorSource::Korean2Set
    ? model.koreanBody
    : model.latinBody;
```

Do not change the panel's calls, blend fraction, alpha, draw order, halo
color, or `transitionProgress`.

- [ ] **Step 4: Run the focused Geometry test**

Run:

```bash
cmake --build /tmp/deskflow-cat-fur-glow-650d-release \
  --config Release --parallel 8 \
  --target InputIndicatorGeometryTests
ctest --test-dir /tmp/deskflow-cat-fur-glow-650d-release \
  -R '^InputIndicatorGeometryTests$' --output-on-failure
```

Expected: PASS, including exact RGB, contrast, 192 all-species heading cases,
Follow Approach exact body pixels, and `outsidePixels == 0`.

---

### Task 3: Review invariants and complete the focused gate

**Files:**
- Review: `src/lib/gui/input/MacInputIndicatorPanel.h`
- Review: `src/lib/gui/input/MacInputIndicatorPanel.mm`
- Review: `src/unittests/gui/input/InputIndicatorGeometryTests.mm`
- Review: `git diff`

- [ ] **Step 1: Inspect the semantic diff**

Run:

```bash
git --no-pager diff --check
git --no-pager diff --stat
git --no-pager diff -- \
  src/lib/gui/input/MacInputIndicatorPanel.h \
  src/lib/gui/input/MacInputIndicatorPanel.mm \
  src/unittests/gui/input/InputIndicatorGeometryTests.h \
  src/unittests/gui/input/InputIndicatorGeometryTests.mm
```

Confirm that `MacInputIndicatorPanel.mm` has no transition, opacity, geometry,
halo ordering, or source-state edit.

- [ ] **Step 2: Build all 15 focused targets**

Enumerate the established build gate:

```bash
ctest --test-dir /tmp/deskflow-cat-fur-glow-650d-release -N
cmake --build /tmp/deskflow-cat-fur-glow-650d-release \
  --config Release --parallel 8 --target \
  InputIndicatorSettingsTests \
  SettingsPagesTests \
  InputIndicatorPolicyTests \
  CoreProcessInputIndicatorTests \
  InputIndicatorIpcTests \
  OSXInputIndicatorRelayTests \
  OSXInputSourceTests \
  InputIndicatorGeometryTests \
  KoreanLookTests \
  DialogLayoutTests \
  MainShellTests \
  DesignTokensTests \
  StatusColorsTests \
  QuitStateTests \
  TranslationTests
```

Adjust only if the enumerated target name differs; do not silently replace a
coverage area. Run the corresponding 15 CTest entries together and require
15/15 PASS.

- [ ] **Step 3: Review planned scope against the spec**

Check every spec section against the diff and test log. There must be no
settings, localized strings, geometry, motion, IPC, or package change and no
remaining 8% body tint.

---

### Task 4: Run 14 repeated Geometry passes

**Files:**
- Modify: none

- [ ] **Step 1: Run 14 isolated consecutive passes**

```bash
for run in $(seq 1 14); do
  echo "Geometry pass $run/14"
  ctest --test-dir /tmp/deskflow-cat-fur-glow-650d-release \
    -R '^InputIndicatorGeometryTests$' \
    --output-on-failure || exit 1
done
```

Expected: 14/14 PASS. Preserve the terminal output showing case count 192,
zero outside pixels, zero glyph-outside pixels, and positive exact/mismatch
body-pixel totals.

- [ ] **Step 2: Stop on any instability**

If a pass fails, fix the deterministic cause and restart the 14-run loop from
pass 1. Do not average away failures or waive a WindowServer test.

---

### Task 5: Run the full safe gate

**Files:**
- Modify: none

- [ ] **Step 1: Enumerate and split CTest**

Run:

```bash
ctest --test-dir /tmp/deskflow-cat-fur-glow-650d-release -N
```

The safe full gate must contain 158 tests: run every registered test except
`InputIndicatorGeometryTests`, `OSXClipboardTests`, and `OSXKeyStateTests`,
then run `InputIndicatorGeometryTests` isolated:

```bash
ctest --test-dir /tmp/deskflow-cat-fur-glow-650d-release \
  -E 'InputIndicatorGeometryTests|OSXClipboardTests|OSXKeyStateTests' \
  --output-on-failure --parallel 8
ctest --test-dir /tmp/deskflow-cat-fur-glow-650d-release \
  -R '^InputIndicatorGeometryTests$' --output-on-failure
```

Expected: 157 safe non-Geometry tests plus 1 isolated Geometry test: 158/158.

- [ ] **Step 2: Restore generated files**

Inspect status:

```bash
git --no-pager status --short
```

Restore only generated translation files that the build changed. Do not revert
task files or unrelated user edits.

---

### Task 6: Commit and hand off the final SHA

**Files:**
- Modify: the five task files from the file map

- [ ] **Step 1: Final consistency checks**

Run:

```bash
git --no-pager diff --check
git --no-pager status --short
git --no-pager diff --stat
```

Expected: no whitespace error; only the two docs, panel header, test header,
and test implementation are changed.

- [ ] **Step 2: Commit the completed revision**

```bash
git add \
  docs/superpowers/specs/2026-11-26-input-source-body-color-design.md \
  docs/superpowers/plans/2026-11-26-input-source-body-color.md \
  src/lib/gui/input/MacInputIndicatorPanel.h \
  src/unittests/gui/input/InputIndicatorGeometryTests.h \
  src/unittests/gui/input/InputIndicatorGeometryTests.mm
git commit \
  -m "feat(input): distinguish keyboard source bodies" \
  -m "Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>"
```

- [ ] **Step 3: Verify the persistent result**

Run:

```bash
git --no-pager status --short --branch
git --no-pager log -1 --format='%H%n%s%n%b'
```

Expected: clean branch and the new local SHA with the Copilot trailer. Report
the SHA, exact body/glyph RGB values, contrast evidence, focused 15/15, full
158/158, Geometry 14/14, 192 raster cases, and `outsidePixels == 0`. Explicitly
state that it supersedes the prior v2.0.13 candidate and that no push,
release, install, or live mutation occurred.
