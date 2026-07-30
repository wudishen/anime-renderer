# Loop-Blinn Bézier stroke — handoff

Context for continuing curve-stroke work on another machine.

## What is implemented (on this branch tip)

1. **Loop cubic subdivision** ([`bezier_cubic_subdivision.plan.md`](bezier_cubic_subdivision.plan.md))  
   Loop-type cubics are de Casteljau–split (midpoint, max depth 5) before Loop-Blinn meshing so the algebraic phantom “second stroke” inside the control hull is removed.

2. **Per-leaf CPU stroke scale** ([`cpu_leaf_stroke_scale.plan.md`](cpu_leaf_stroke_scale.plan.md))  
   Each leaf stores `fScale ≈ |f(P+N)|` (CPU). Fragment shader uses `abs(f)/vFScale` for width instead of `fwidth(f)`, so tiny subdivided leaves do not collapse to a hairline.

## Key files

- [`src/rendering/LoopBlinnCubic.cpp`](../../src/rendering/LoopBlinnCubic.cpp) / [`.h`](../../src/rendering/LoopBlinnCubic.h) — subdivision, hull pad, `EstimateLeafFScale`, vertex `fScale`
- [`src/rendering/shaders/cubic_bezier.vert`](../../src/rendering/shaders/cubic_bezier.vert) / [`.frag`](../../src/rendering/shaders/cubic_bezier.frag) — `aFScale` / `vFScale`, no `fwidth`

## Known pitfalls (do not repeat)

- Flooring `|∇f|` / `fwidth` in the fragment shader to “fix thin strokes” can make **entire hull triangles fill**. Prefer CPU `fScale`.
- After changing shaders, copy them to `build/Debug/shaders/` (or rebuild so CMake copies them) before testing the exe.

## Suggested next checks

- Looping / self-overlapping Bézier: no phantom stroke; width roughly even.
- Mild S-curves and B-splines unchanged.
- If width is still uneven, inspect `EstimateLeafFScale` (sample at more than one `t`, or use a smaller/larger normal offset than 1 plane unit).
