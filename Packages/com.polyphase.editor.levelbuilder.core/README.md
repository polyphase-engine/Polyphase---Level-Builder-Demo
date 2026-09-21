# `com.polyphase.editor.levelbuilder.core`

**Status:** Functional (ABI v13) · **Layer:** framework · **Hot-reload safe:** yes

The framework half of the Polyphase Level Builder. Owns the dockable
**Level Builder** window, the shared kit registry, the placement preview,
and the C-ABI sibling addons plug into. Ships no tools / brushes / snap
providers of its own — those live in the siblings
(`modular`, `grid`, `tool.core`).

If you just want to place pieces, use [modular](../com.polyphase.editor.levelbuilder.modular/README.md)
or [grid](../com.polyphase.editor.levelbuilder.grid/README.md). This addon
is what they sit on top of.

---

## What it owns

- **Level Builder window** — Brush / Palette / Placement / Debug tabs,
  plus dynamic extension tabs registered by siblings (the old static
  "Mode" tab was retired in favor of viewport-mode activation through
  the engine's `AddViewportMode` hook).
- **Kit Editor window** — separate dockable window for kit-level
  metadata + piece add/remove + per-piece editor + Save (uses the v10
  R4 mutation surface).
- **Tool / brush / snap-provider / palette registries** — name-keyed maps
  with an `active` selection. Siblings register at `OnLoad`, unregister
  at `OnUnload`.
- **Kit registry (`LBKitRegistry`)** — every kit in `<project>/Kits/` and
  `<project>/Assets/Kits/` loaded once into core, surfaced to siblings via
  the `Kit_*` C-ABI. Folder-mode storage (`kit.json` + `pieces/*.json`)
  and Phase S2 `.kit` zip import / export live here too.
- **Viewport preview + raycast dispatch** — polls the engine's
  `Viewport_RaycastUnderMouse` each frame, runs the active snap provider,
  fires click callbacks to the active tool.
- **Brush spawn-fn router** — generic brushes (Line / Box / …) from
  `tool.core` commit through whichever sibling owns the active tool.

## Public C ABI

The single exported entry point is `LevelBuilderCore_GetAPI` returning a
`LevelBuilderCoreAPI*`. **All sibling addons must:**

- Resolve it via `GetProcAddress` / `dlsym` — **no link-time dependency**
  on the core import library. The
  [`LevelBuilderCoreLoader.{h,cpp}`](../com.polyphase.editor.levelbuilder.modular/Source/LevelBuilderCoreLoader.h)
  pair in any sibling is the canonical loader.
- Strict-equality check `apiVersion == LEVEL_BUILDER_CORE_API_VERSION`.
  Mismatch → null the cached pointer and bail. Siblings built against
  a newer header than the loaded core will silently short-circuit
  their `Register*` calls (manifests as "tab vanished from Level Builder
  window with no error log") — see `Cross-cutting #4` in
  [`../../Documentation/Developers/Packages.md`](../../Documentation/Developers/Packages.md).
- Null-check every function pointer before calling — newer fields land
  at the end of the struct, older cores leave them `nullptr`.

The header lives at
[`Source/LevelBuilderCoreAPI.h`](Source/LevelBuilderCoreAPI.h) and is
**copied verbatim** into each sibling addon's `Source/` folder. Bumps:

| Version | Highlight                                                                                                    |
| ------- | ------------------------------------------------------------------------------------------------------------ |
| v13     | Rebuild-from-world hook registry (`RegisterRebuildFromWorldFn`) — siblings rebuild their placed-piece registry from the live scene tree when a viewport mode activates. |
| v12     | `LBVisitFn` callback receives the placed piece's asset name as an additional argument — lets Replace eyedropper identify what was clicked. |
| v11     | `sourceAssetFilter` argument on `LBEnumeratePlacementsFn` — Replace narrows its swap query by source asset.   |
| v10     | R4 piece-level mutation surface (`Kit_AddPiece` / `Kit_RemovePiece`, per-piece string setters, kit-level CRUD). |
| v7      | Paint brush drag input (`Viewport_IsLmbDown`/`Rmb`); placed-piece enumeration callback registry.             |
| v6      | Kit sharing metadata (`Kit_GetMetaInfo` / `SetMetaString`), `.kit` zip pack/unpack.                          |
| v5      | Socket mutation (`Kit_AddSocket` / `SetSocketName` / `Position` / `Rotation` / `Type` / `CompatibleTypes`).  |
| v4      | Brush spawn-fn registry (`RegisterSpawnFn` / `GetSpawnFnForActiveTool`).                                      |
| v3      | Core-owned kit registry (`Kit_GetCount` / `GetPieceInfo` / `SetPieceSize` / `Save` / `Reload`).               |
| v2      | Viewport hover/click dispatch (`Viewport_GetHoverHit` / `RegisterToolViewportInput`).                        |

## Build

```bat
:: From a Developer Command Prompt for VS 2022
cd Packages\com.polyphase.editor.levelbuilder.core
build.bat              REM Debug + Release
build.bat Debug        REM Debug only
```

Linux: `./build.sh`. Output lands at
`build/Windows/x64/<config>/com.polyphase.editor.levelbuilder.core.dll`.

Or open the editor: **Tools → Addons → Reload Native Addons** triggers
a `cmake --build` for every addon that has `resolveMode: "source"` in
its `package.json`.

After rebuilding core you **must rebuild every sibling addon** — the
strict-equality `apiVersion` check will silently disable any sibling
linked against an older header.

## Source layout

```
Source/
├── ComPolyphaseEditorLevelbuilderCore.cpp   ← plugin entry (OnLoad/OnUnload)
├── LevelBuilderCoreAPI.{h,cpp}              ← exported C ABI (canonical copy)
├── LevelBuilderInterfaces.h                 ← Tool / Brush / SnapProvider bases
├── LevelBuilderRegistry.{h,cpp}             ← registries + active state
├── LevelBuilderPalette.{h,cpp}              ← palette items + filter
├── LevelBuilderContext.h                    ← engine API + raycast hit
├── LevelBuilderEditorUI.{h,cpp}             ← Level Builder dockable window
├── LevelBuilderKitEditor.{h,cpp}            ← Kit Editor dockable window
├── LBKitTypes.{h,cpp}                       ← LBKit / LBPiece / LBSocket
├── LBKitJson.{h,cpp}                        ← single-file + folder-mode JSON I/O
├── LBKitRegistry.{h,cpp}                    ← LBKit registry + project root
├── LBKitShare.{h,cpp}                       ← .kit zip pack/unpack (S2)
└── LBKitZip.{h,cpp}                         ← bundled miniz wrapper
```

## Where to read next

- [`Source/LevelBuilderCoreAPI.h`](Source/LevelBuilderCoreAPI.h) — the
  single source of truth for what siblings can call.
- [`../../Documentation/Developers/GettingStarted.md`](../../Documentation/Developers/GettingStarted.md)
  — quick orientation for someone working on the level-builder bundle.
- [`../../Documentation/Developers/Packages.md`](../../Documentation/Developers/Packages.md)
  — full per-package status and roadmap.
- [`../../Documentation/Developers/CoreKitsRefactor.md`](../../Documentation/Developers/CoreKitsRefactor.md)
  — why the kit registry moved into core (R1–R4).
