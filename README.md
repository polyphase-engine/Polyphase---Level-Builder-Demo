# Polyphase Level Builder Demo

An example Polyphase project set up with the Level Builder addons and a couple of kits to play with. Open it, pick a kit, and start dropping pieces into the scene.

[![Level Builder demo video](https://img.youtube.com/vi/THCoGaFr7J0/hqdefault.jpg)](https://www.youtube.com/watch?v=THCoGaFr7J0)

Demo video: https://www.youtube.com/watch?v=THCoGaFr7J0

## What the Level Builder is

The Level Builder is a set of native editor addons for putting levels together out of reusable pieces. You work from a **kit**, which is a list of pieces (walls, floors, blocks, props). Each piece points at a StaticMesh or a Scene asset in the project, and can carry sockets so it snaps onto its neighbours.

It all runs inside the editor. There's a dockable Level Builder window with a palette, brush settings and placement options, plus a Kit Editor window for building your own kits without hand-editing JSON. Placement happens in the viewport: hover, click, press `R` to rotate 90 degrees.

The addons are editor-only. Nothing from them ends up in a packaged game, only the nodes you placed.

## Packages in this repo

Both live under `Packages/` and were installed through the editor's addon manager, in source mode. The editor compiles them the first time it loads the project.

### com.polyphase.editor.levelbuilder.core

The framework everything else plugs into. It doesn't place anything on its own. What it does own:

- the Level Builder window (Brush, Palette, Placement and Debug tabs, plus any tabs other addons register)
- the Kit Editor window, for kit metadata, adding and removing pieces, sockets, and saving
- the kit registry, which loads every kit found in `Kits/` and `Assets/Kits/`
- the placement preview and the viewport raycast that feeds clicks to whichever tool is active
- `.kit` export and import, a zip of a kit folder you can hand to someone else

The other addons talk to core through a small C API that they look up at runtime, so core can hot-reload without taking them down with it.

Source and full README: [Packages/com.polyphase.editor.levelbuilder.core](Packages/com.polyphase.editor.levelbuilder.core)

### com.polyphase.editor.levelbuilder.tool.core

The brushes. There are seven:

| Brush | How you use it | What you get |
| --- | --- | --- |
| Line | two clicks | pieces stepped along a line |
| Box | two clicks | pieces around the outline of a rectangle |
| BoxFill | two clicks | a filled rectangle |
| NoiseFill | two clicks | a noise-driven scatter inside a rectangle |
| Paint | click and drag | pieces sprayed under the cursor |
| Replace | click | swaps placed pieces within a radius |
| MaskFill | two clicks | placement driven by the RGB channels of a PNG |

Every brush can take a list of pieces and pick one at random per spawn, and can be gated by a PNG mask. Two masks are included in `Kits/Brushes/`. A whole stroke is one undo step, so a single Ctrl+Z takes it back.

The brushes don't know anything about kits or snapping. They ask core which placement tool is active and hand each spawn off to it.

Source and full README: [Packages/com.polyphase.editor.levelbuilder.tool.core](Packages/com.polyphase.editor.levelbuilder.tool.core)

### Placement addons

Because the brushes hand off to a placement tool, you need at least one of these installed to actually put pieces down. They aren't checked into this repo yet. Add them the same way, through the addon manager:

| Package | What it does |
| --- | --- |
| [com.polyphase.editor.levelbuilder.modular](https://github.com/Polyphase-Labs/com.polyphase.editor.levelbuilder.modular) | Socket-snapped placement for modular kits: walls, floors, doors, props. |
| [com.polyphase.editor.levelbuilder.grid](https://github.com/Polyphase-Labs/com.polyphase.editor.levelbuilder.grid) | Uniform grid placement, block-builder style. No sockets, just cells. |

## Kits included

| Kit | Where | Notes |
| --- | --- | --- |
| KenneyPlatformerKit | `Assets/Kits/KenneyPlatformerKit/` | 45 pieces: grass blocks, slopes, corners, overhangs, a barrel, a chest. One JSON file per piece, with preview thumbnails. |
| Prototype | `Assets/Kits/Prototype/` | Walls, corners, a doorway and a coin. The walls have sockets, so this is the one to try snapping with. |
| Example Dungeon Kit | `Kits/ExampleDungeonKit.json` | A single-file kit that's handy as a reference for the JSON format. Copy it when you start your own. |

The models in the Kenney kits are from [Kenney](https://kenney.nl) and are CC0.

## Getting started

You need the Polyphase Engine editor and, on Windows, Visual Studio 2022 so the addons can compile. Linux uses the `build.sh` in each package.

1. Clone the repo and open `Level Builder.octp` in the editor.
2. Let the addons build. If something doesn't show up, run **Tools > Addons > Reload Native Addons** and check the console.
3. Open `Assets/Scenes/SC_Default`.
4. Go to **Addons > Level Builder > Open Level Builder** and dock the window wherever you like.
5. Choose a placement mode from the viewport mode picker at the top right of the viewport.
6. Pick a kit and a piece, then click in the viewport. `R` rotates the preview.
7. Switch to the Brush tab to try Line, BoxFill, Paint and the rest.

If you rebuild core by hand, rebuild the other addons afterwards. They check the core API version on load and quietly switch themselves off if it doesn't match, which looks like a tab going missing with nothing in the log.

## Making your own kit

Open **Addons > Level Builder > Open Kit Editor**, create a kit, add a piece, and drag a StaticMesh or Scene from the asset browser onto it. Auto-Detect Size fills in the bounds for meshes. Add sockets if you want the piece to snap, then hit Save Kit. The kit is written as a folder with a `kit.json` and one file per piece, which keeps diffs small when more than one person is adding pieces.

Editing the JSON directly works too. Click **Reload Kits From Disk** afterwards and the changes show up without restarting.

## Project layout

```
Assets/
  Kits/          kits that ship with their own models and previews
  Materials/
  Scenes/        SC_Default
Kits/            loose kit JSON files and brush mask PNGs
Packages/        the Level Builder addons
Settings/        build profiles and the installed addon list
```
