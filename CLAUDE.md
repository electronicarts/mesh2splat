# CLAUDE.md

Guidance for Claude Code (claude.ai/code) and other AI assistants working in this repository.

## What this project is

Mesh2Splat converts textured triangle meshes (glTF/GLB) into 3D gaussian splats, and ships an
OpenGL viewer that renders both the source mesh and the resulting splat with PBR relighting. The
conversion runs on the GPU: it is a rasterization pass, not an optimization loop, which is why it
takes seconds rather than hours.

## Build

```bash
# Viewer + tests (default). Needs GLFW, GLEW and OpenGL.
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Tests only: no GPU, no window server, no system GL packages required.
cmake -B build -DMESH2SPLAT_BUILD_APP=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

On Windows `run_build_release.bat` / `run_build_debug.bat` wrap the same commands. Prebuilt GLFW
and GLEW binaries for MSVC live in `thirdParty/`; on Linux they come from the distro packages
listed in the README.

Prefer the app-off configuration when a change is CPU-side only — it builds in seconds and works
in a sandbox.

## Layout

```
src/
  main.cpp                    Window, camera and frame loop
  glewGlfwHandlers/           GLEW/GLFW setup
  imGuiUi/                    All ImGui panels and the UI-owned settings state
  renderer/
    renderer.cpp              Owns the passes and the render context
    guiRendererConcreteMediator.cpp   Mediator: turns UI state into renderer events
    renderPasses/             One class per pass (conversion, splatting, shadows, radix sort)
  parsers/parsers.cpp         PLY read/write in all three export formats
  utils/
    SceneManager.cpp          glTF loading, scene graph flattening, PLY export entry point
    coordinateSystem.hpp      Export-time world-convention conversion (header-only)
    normalizedUvUnwrapping.cpp  xatlas UV unwrapping
  shaders/
    conversion/               Mesh -> gaussian conversion (VS/GS/FS)
    rendering/                Splat rendering, shadows, radix sort
tests/                        Hermetic CPU-side unit tests
thirdParty/                   Vendored dependencies; do not edit
```

The UI never talks to the renderer directly. `ImGuiUI` holds settings and intent flags,
`GuiRendererConcreteMediator::update()` polls them and emits `EventType` events, and the renderer
acts on those. New UI-driven behaviour follows that path rather than reaching across.

## Conventions that are easy to get wrong

**Quaternion packing.** `GaussianDataSSBO::rotation` is a `glm::vec4` whose components
`(.x, .y, .z, .w)` hold `(w, x, y, z)` — scalar first, matching the PLY `rot_0..rot_3` order of the
reference 3DGS format. This is set up in `converterGS.glsl` (`vec4(q.w, q.x, q.y, q.z)`) and relied
on by `parsers.cpp`. Assuming `(x, y, z, w)` produces a splat whose point cloud looks right but
whose gaussians are individually mis-oriented, so it survives casual inspection.

**World coordinate system.** Conversion preserves the source glTF frame exactly (right-handed,
+Y up, +Z towards the viewer). Nothing is recentered, rescaled or flipped. Rotation into the
COLMAP/OpenCV frame is opt-in at export time via `utils::CoordinateSystem`; see the README's
Coordinate System section. Keep `GltfYUp` the default — changing it would silently break every
existing pipeline.

**Export is not the viewport.** `SceneManager::exportPly` reads the gaussian SSBO directly, so the
gizmo's model-to-world transform is *not* baked into the saved PLY. The gaussian scale slider is
applied at write time through the scale multiplier and is likewise not stored in the buffer.

**Flat gaussians.** The converter emits surfels: the third scale is fixed at `1e-7` in
`converterGS.glsl`. Code touching covariance must stay well conditioned at that magnitude.

**Shader hot reload.** Shaders are re-read from disk about once a second while the app runs
(`EventType::CheckShaderUpdate`), so GLSL edits do not need a rebuild.

## Testing

`tests/` depends only on the vendored header-only glm — no GPU, no network, no test framework to
fetch. Add CPU-side math there and keep it that way so it stays runnable in CI and in sandboxes.

Assert the property, not a transcription of the implementation. The coordinate tests, for example,
check that the frame change has determinant +1, that the quaternion path agrees with the matrix
path, and that the covariance transforms as `R Σ Rᵀ` — each of which fails loudly if the packing or
multiplication order is wrong, whereas a hardcoded expected quaternion would not.

CI (`.github/workflows/ci.yml`) runs the tests on Linux, Windows and macOS, and builds the full
viewer on Linux.

## Working style in this repo

- Match the surrounding style: 4 spaces, `camelCase` members, brace on its own line in `.cpp`.
- Keep the EA copyright header at the top of new source files.
- Do not edit anything under `thirdParty/`.
- New export options get a default that preserves current output. Downstream pipelines consume
  these PLYs and a silent format change is worse than no feature.
- Prefer extending the existing pass/mediator structure over adding new cross-cutting paths.
