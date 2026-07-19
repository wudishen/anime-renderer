# PROJECT_CONTEXT.md

Context dump for coding agents continuing work on **Anime Renderer** across machines/sessions.

## What this project is

Desktop OpenGL viewport/editor for modeling workflows (anime-oriented). Users can:

- Navigate and edit a 3D scene
- Import `.obj` meshes
- Manage **scene cameras** as saved viewports
- Attach **reference background images** per camera
- Save/load the full scene to `.animescene` files

Repo: `https://github.com/wudishen/anime-renderer.git` (branch `main`)

## Stack

| Layer | Choice |
|--------|--------|
| Language | C++20 |
| Graphics | OpenGL 4.6 Core |
| Window/input | GLFW 3.4 |
| GL loader | Manual GLAD under `include/` + `src/glad.c` |
| Math | GLM 1.0.1 |
| UI | Dear ImGui 1.91.8 (GLFW + OpenGL3 backends) |
| Scene JSON | nlohmann/json 3.11.3 |
| Images | `include/stb_image.h` |
| Build | CMake ≥ 3.22 → MSVC / Visual Studio generator on Windows |
| OS focus | Windows (native file dialogs via `comdlg32`) |

Dependencies (except GLAD/stb) are pulled with CMake `FetchContent`.

## Build & run

```powershell
cd <repo-root>
cmake -S . -B build
cmake --build build --config Debug
```

Executable: `build/Debug/AnimeRenderer.exe`

**Working directory matters for shaders.** Prefer:

- Run from repo root, or
- VS debugger cwd is set to `${CMAKE_SOURCE_DIR}`, or
- VS Code task `.vscode/tasks.json` (`run AnimeRenderer (Debug)`) uses workspace cwd

Post-build copies shaders to `build/Debug/shaders/` so double-clicking the exe also works.

## Source layout

```
src/
  main.cpp                 # App loop: input, render, view camera, tools
  engine/
    Window.*               # GLFW window + framebuffer size
    Camera.*               # Legacy orbit camera (UNUSED by main now; safe to delete later)
  rendering/
    Shader.*               # Program + uniforms (mat4/vec3/vec2/float/int)
    Mesh.*                 # Triangle mesh VAO/VBO
    Grid.*                 # XZ floor grid + colored X/Z axes
    CameraBackground.*     # Per-camera reference image texture
    CameraBackgroundRenderer.*  # Screen-space textured quad
    shaders/
      basic.vert/.frag     # Solid-color lit-by-uniform meshes
      bg.vert/.frag        # Background image overlay
  scene/
    Scene.*                # Object list, selection, active view camera
    SceneObject.h          # Mesh | Camera + transform + optional background
    CameraFactory.*        # Create free/preset cameras + gizmo mesh
    Picking.*              # Ray-triangle pick (meshes only; cameras excluded)
  tools/
    TransformTool.*        # Translate / Rotate modes (axis constraints, Shift precision)
  io/
    FileDialog.*           # Win32 open/save for obj/scene/image
    ObjLoader.*            # Wavefront OBJ → mesh (needs `f` faces)
    SceneFile.*            # Versioned .animescene JSON save/load + migrations
  ui/
    MainMenu.*             # File / Edit / Add / Layout + message popups
    ObjectPanel.*          # Right-hand object list + camera background controls
    ObjectContextMenu.*    # Translate/Rotate/Scale/Delete/Go to View
include/                   # glad + stb_image.h
```

## Core design decisions (do not casually undo)

1. **Cameras are saved viewports**, not a separate “editor camera.”
   - Startup creates a default `Camera` and sets it as `activeViewCameraId`.
   - Navigation (middle-pan, right-orbit, scroll-zoom) always edits the **active** scene camera.
   - Switch views via Objects list → right-click → **Go to View**.
   - There is **no** “Leave View” / Esc-to-exit-camera.

2. **Cameras are not pickable in the viewport.** Select only from the Objects list. `Picking.cpp` skips `IsCamera()`.

3. **Active view camera gizmo is not drawn** (you’re looking through it). Other cameras still draw as cyan gizmos.

4. **Transforms** live on `SceneObject::transform` (`glm::mat4`). Translate mutates translation; rotate composes around object origin. Scale tool is **menu stub only**.

5. **OBJ import** requires face lines (`f`). Vertex-only files show an Import Error popup.

## Controls (current)

| Input | Action |
|--------|--------|
| Middle-drag | Pan active camera |
| Right-drag | Orbit active camera (small click without drag opens context menu on pick hit) |
| Scroll | Dolly active camera along forward |
| Left-click viewport | Select mesh (not cameras) |
| Objects list LMB | Select any object |
| Objects list RMB | Context menu |
| Context: Translate / Rotate | Enter tool mode; mouse moves object; **X/Y/Z** constrain; **Shift** precision; LMB/Enter confirm; Esc/RMB cancel |
| Context: Go to View | Only on cameras — make that camera the active viewport |
| File → Save / Open | `.animescene` |
| File → Import | `.obj` |
| Add → Camera → … | Free + Top/Bottom/Left/Right/Front/Back presets |

## Camera background images (reference)

When a **camera** is selected in the Objects panel:

- **Upload Image…** / **Clear**
- Opacity, Position (NDC offset), Scale, Enabled

Drawn **only when that camera is the active view**, behind the 3D scene (depth off + alpha blend). Aspect-correct fit to viewport.

Saved on the camera as `background` in `.animescene` (image path + params). Load warns if the image file is missing.

## Scene file format

- Extension: `.animescene` (JSON)
- Magic: `"AnimeRendererScene"`
- Current `formatVersion`: **2** (v1→v2 migrator exists; background fields optional)
- Compatibility rules in `src/io/SceneFile.*`:
  - Older file → migrate forward in memory, warn
  - Newer file than app → refuse with clear error
  - Unknown object `type` → skip + warn

When adding breaking schema changes: bump `kCurrentFormatVersion` and add a `MigrateToNextVersion` case.

## UI layout

- Top: ImGui main menu bar
- Right: fixed **Objects** panel (~260px)
- Bottom-left: transform tool status (when active)
- Top-left: `View: <camera name>`

## Conventions for agents

- Prefer small, focused changes; match existing style.
- New `.cpp` files must be added to `CMakeLists.txt` `add_executable(...)`.
- New shaders: add under `src/rendering/shaders/` **and** to the POST_BUILD copy list.
- Windows dialogs live in `FileDialog.cpp`; keep `OFN_NOCHANGEDIR`.
- Do not reintroduce a dual “editor orbit camera vs scene camera” model unless the user asks.
- `src/engine/Camera.*` is leftover from early orbit-camera days and is **not linked** in `CMakeLists.txt` currently — ignore or delete in a cleanup PR.

## Likely next features (not done)

- Scale transform tool
- Non-Windows file dialogs
- Better TRS decomposition (separate position/rotation/scale)
- Materials / lighting beyond flat `uColor`
- Undo stack
- Proper `.gitignore` if build artifacts are still tracked on GitHub

## Quick health check after clone

1. Configure + Debug build succeeds
2. Run exe → see grid, triangle, `[View] Camera` in list
3. Add → Camera → Top Camera → Go to View
4. Select a camera → Upload background image → adjust opacity/position/scale
5. File → Save As → Open the file again
