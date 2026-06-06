# Shader Viewer

Lightweight Windows Direct3D 11 HLSL material/shader previewer for fast shader iteration.

## Features

- D3D11 viewport with orbit camera and default sphere/cube/plane meshes
- Runtime HLSL compilation with hot reload on save
- PBR-ish pixel shader (GGX specular, Schlick Fresnel, Smith geometry)
- Material texture binding with sRGB/linear handling
- Packed ORM/RMA/MRA channel mapping UI
- Recursive asset scan of a UE project folder (read-only)
- OBJ/FBX/glTF/glb import via Assimp
- Unreal `.uasset` listing + Python export helper (no direct `.uasset` decoding)

## Requirements

- Windows 10/11
- Visual Studio 2022 with **Desktop development with C++**
- Windows 10/11 SDK
- CMake 3.20+
- Git (for FetchContent dependencies)

## Build

```powershell
cd C:\Users\singerie\Documents\Cursor\temp\ShaderViewer
cmake -B out/build -G "Visual Studio 17 2022" -A x64
cmake --build out/build --config Release
```

First configure downloads ImGui, DirectXTex, and Assimp via FetchContent and may take several minutes.

Debug build:

```powershell
cmake --build build --config Debug
```

## Run

```powershell
.\out\build\Release\ShaderViewer.exe
```

### Automated smoke test

```powershell
powershell -ExecutionPolicy Bypass -File tools\smoke_test.ps1
```

### Dev loop (C++ changes — auto relaunch)

One-shot rebuild + relaunch (use after C++ fixes):

```powershell
powershell -ExecutionPolicy Bypass -File tools\dev_run.ps1
```

Leave running in a terminal — rebuilds and relaunches when `src/`, `shaders/`, or `CMakeLists.txt` change:

```powershell
powershell -ExecutionPolicy Bypass -File tools\dev_watch.ps1
```

**What is dynamic without restart:**
- HLSL / `.hlsli` — hot-reload in the running app (~200ms after save)
- UI sliders, view tabs, materials, camera — immediate

**What needs rebuild + relaunch:**
- C++ changes — `dev_run.ps1` or `dev_watch.ps1` handles this automatically

Verifies: HLSL offline compile, app launch, bad-shader survival, shader restore/reload, and local test assets.

Local test assets live in `assets/test_material/` and are scanned automatically in addition to the Berserk project root.

Shaders are copied to `build/Release/shaders/` on build. Edit:

- `shaders/material_ps.hlsl`
- `shaders/material_vs.hlsl`
- `shaders/common.hlsli`

Save in Cursor/VS Code and the app recompiles automatically.

## Default asset scan path

By default the app scans:

`C:\Users\singerie\Documents\Cursor\Berserk`

Skipped folders: `Binaries`, `Intermediate`, `DerivedDataCache`, `.git`, `Build`.

Useful scan areas include `Content`, `SourceArt`, `Saved/ShaderViewerExports`, and any folder containing image/model files.

Change the scan root in the **Assets** panel or edit the default in code (`Types.h` / `AppState`).

## Exporting Unreal textures

The viewer does **not** decode `.uasset` files directly. Instead:

1. Open the Berserk project in Unreal Editor.
2. Run the Python script from **Output Log**:

```python
py "C:/Users/singerie/Documents/Cursor/temp/ShaderViewer/tools/ExportUnrealTextures.py"
```

Or via command line:

```powershell
& "C:\Program Files\Epic Games\UE_5.x\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "C:\Users\singerie\Documents\Cursor\Berserk\Berserk.uproject" `
  -ExecutePythonScript="C:\Users\singerie\Documents\Cursor\temp\ShaderViewer\tools\ExportUnrealTextures.py"
```

Exports go to:

`C:\Users\singerie\Documents\Cursor\Berserk\Saved\ShaderViewerExports`

Click **Rescan Berserk Assets** in the app after export.

## Controls

| Input | Action |
|-------|--------|
| Left mouse drag | Orbit |
| Right mouse drag | Pan |
| Mouse wheel | Zoom |
| Reset Camera button | Reset view |

## Shader hot reload

1. `FileWatcher` monitors the `shaders/` directory using `ReadDirectoryChangesW`.
2. On `.hlsl` / `.hlsli` write events, the main loop calls `ShaderCompiler::ReloadIfNeeded`.
3. `D3DCompileFromFile` recompiles VS/PS with a custom `ID3DInclude` handler for `#include`.
4. On success, new shader objects replace the old ones and a timestamp is recorded.
5. On failure, the last valid shaders remain bound and errors appear in the **Shaders** panel.

Manual reload: **Reload Shaders Now**.

## Shader resource bindings

| Slot | Name |
|------|------|
| t0 | AlbedoMap |
| t1 | NormalMap |
| t2 | RoughnessMap |
| t3 | MetallicMap |
| t4 | AOMap |
| t5 | EmissiveMap |
| t6 | HeightMap |
| t7 | PackedMaskMap |
| s0 | LinearWrapSampler |
| s1 | LinearClampSampler |
| b0 | FrameCB |
| b1 | ObjectCB |
| b2 | MaterialCB |

## Known limitations

- No direct `.uasset` texture decoding; use the export script.
- No HDRI/image-based lighting in MVP.
- Tangent generation uses Assimp `CalcTangentSpace` for imports; procedural meshes use a Lengyel-style fallback (not full MikkTSpace).
- EXR/HDR loading depends on DirectXTex support; some formats may fail gracefully.
- Berserk project `Content/` may be empty until assets are added or exported.
- Height/displacement maps are bound but not parallax-displaced in the default shader.

## Project layout

```
ShaderViewer/
  src/           C++ application code
  shaders/       HLSL sources (hot-reloaded)
  tools/         Unreal export script
  assets/        Optional local assets
  CMakeLists.txt
  README.md
```
