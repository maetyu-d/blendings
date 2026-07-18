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

## Workspaces

- Main: build coloured pipe networks that route drops through taps, gates, filters, teleports, clocks, modulation, and playable discs.
- Carousel: arrange tuned synth, SuperCollider, and Pure Data tones around rotating fields. Planks extend from any point on a carousel rim and provide an empty endpoint for a tone or another carousel.
- Pipe: compose spatial pipe structures across the faces and layers of a 3D grid, with programmable sound at each playable disc.
- Disc elements: combine nested worlds, SuperCollider, Pure Data, SCsheet, Orca, Carousel, and Pipe structures inside a single disc.

Mouse-wheel zoom, middle-drag or Command-drag panning, project load/save, undo, appearance controls, and WAV recording are available throughout the relevant workspaces.

`JUCE_PATH` and `BLENDINGS_SC_ROOT` may also be supplied as environment variables. The embedded Pure Data source is pinned from [pure-data/pure-data](https://github.com/pure-data/pure-data) and retains its upstream license.

## Audio Smoke Test

```sh
cmake --build build --target SCAudioSmoke --config Release
./build/SCAudioSmoke
```

The smoke test prepares the embedded SuperCollider engine, triggers disc-style events, renders audio, and prints peak/RMS levels.

Carousel and Pipe workspace checks are available as the `CarouselSmoke` and `PipeWorkspaceSmoke` build targets. The Pd editor round-trip regression suite is in `scripts/pd_editor_roundtrip_test.js`.

## License

Blendings is distributed as a combined work under the GNU General Public
License, version 3. See [LICENSE](LICENSE) for the complete terms.

Blendings includes or interoperates with third-party free-software components.
The original Orca MIT copyright and license notice is retained in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), alongside the applicable
SuperCollider and Purr Data/Pure Data licensing details. Release app bundles
include both documents in their Resources directory.
