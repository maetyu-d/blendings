# Blendings

Created by [matd.space](https://matd.space).

A spatial music environment for building pipe networks, routing drops, and triggering discs containing SuperCollider, Pure Data, SCsheet, Orca, Carousel, nested-world, and pipe-world elements.

## Requirements

- CMake 3.22 or newer
- A C++17 toolchain
- A JUCE checkout
- The SuperCollider host runtime used by the embedded audio engine
- Git submodules initialized for the bundled Pure Data engine

## Run

```sh
git clone --recurse-submodules https://github.com/maetyu-d/blendings.git
cd blendings
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DJUCE_PATH=/path/to/JUCE \
  -DBLENDINGS_SC_ROOT=/path/to/supercollider-host
cmake --build build --target Blendings --config Release
open "build/Blendings_artefacts/Release/Blendings.app"
```

## Controls

- Select tool: click a disc to select it, or double-click to open its data pane.
- Draw tool: drag on the canvas to preview a path, release to commit it.
- Edit tool: drag visible nodes to reshape an existing path.
- Disc tool: click to place a disc and open its data pane.
- Add Element: adds a placeholder musical/sound element to the selected disc.
- Nested World: adds/enters an inner world on the selected disc.
- Exit World: returns to the parent world.
- SC Code: adds a SuperCollider code element to the selected disc. The pane accepts a SynthDef body, or a full `SynthDef` using `__name__` as the generated synth name. Duration accepts seconds, or `-` for unlimited/code-decided; `-` sends `sustain = -1`.
- Fire Disc: triggers the selected disc through the embedded SuperCollider engine. If the disc contains a nested world, its inner discs fire too.
- Erase tool: click a path or disc to remove it.
- Snap: snaps close points to the grid while drawing or editing.
- Mouse wheel: zooms around the pointer.
- Middle-drag or Command-drag: pans the canvas.
- Undo and clear are available from the Edit menu.

`JUCE_PATH` and `BLENDINGS_SC_ROOT` may also be supplied as environment variables. The embedded Pure Data source is pinned from [pure-data/pure-data](https://github.com/pure-data/pure-data) and retains its upstream license.

## Audio Smoke Test

```sh
cmake --build build --target SCAudioSmoke --config Release
./build/SCAudioSmoke
```

The smoke test prepares the embedded SuperCollider engine, triggers disc-style events, renders audio, and prints peak/RMS levels.
