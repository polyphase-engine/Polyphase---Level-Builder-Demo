/**
 * @file LevelBuilderKitEditor.h
 * @brief Dedicated "Kit Editor" window — full authoring UI for kits,
 *        promoted out of modular's Piece Properties accordion.
 *
 * Provides:
 *  - Active kit dropdown + New / Delete buttons
 *  - Kit-level metadata editor (kitId / version / author / license /
 *    description / preview image / homepage)
 *  - Pieces list with add / remove
 *  - Per-piece editor: name / asset / category / icon / size
 *  - Save Kit button (flushes to kit.json via Kit_Save)
 *
 * Uses the v10 (R4) piece-mutation surface and the v6 metadata surface.
 * Sockets editing stays in modular's Piece Properties accordion for now
 * (its drag handles + viewport-overlay integration are modular-specific).
 *
 * Editor-only — entire surface gated #if EDITOR.
 */

#pragma once

#if EDITOR

#include <stdint.h>

struct EditorUIHooks;

namespace LevelBuilderKitEditor
{
    // Register the dockable window with the engine's EditorUIHooks.
    // Call from core's RegisterEditorUI alongside the main level
    // builder window. The window opens via the Addons menu and via
    // viewport-mode activation.
    void Register(EditorUIHooks* hooks, uint64_t hookId);

    // Drop cached engine-hooks pointer. The engine calls
    // RemoveAllHooks(hookId) for us before the DLL unloads, so we
    // don't need to unregister the window individually.
    void Unregister();
}

#endif // EDITOR
