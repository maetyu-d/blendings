# Blendings Architecture

Blendings is organised around a shared workspace model, multiple editors, and
one application shell. New behavior should be added to the narrowest owning
module rather than to `Main.cpp`.

## Shared foundations

- `AppTheme.*`: application colours and reusable JUCE control styling.
- `WorkspaceModel.*`: routes, discs, pipe tools, modulation data, clocks, and
  default SuperCollider, Pure Data, SCsheet, and Orca content.
- `SuperColliderTokeniser.*`: SuperCollider syntax classification and editor
  colour scheme.
- `InspectorStyle.h`: common inspector layout helpers.

## Editing surfaces

- `PipeToolSettings.h`: focused inspectors for taps, drains, quantum nodes,
  speed limits, waits, strikes, teleports, filters, and logic gates.
- `WorkspaceInspectors.h`: clocks, modulation, pipe, and selection inspectors.
- `CarouselEditorComponent.*`: Carousel document and editing surface.
- `PipeWorkspaceComponent.*`: three-dimensional Pipe workspace.
- `GridEditorComponent.*`, `GridModel.*`, and `GridInterpreter.*`: Orca grid
  editing and execution.
- `ScSheet*`: SCsheet model, formulas, playback score, and table UI.

## Audio

- `ScDiscAudioEngine.*`: disc-level audio coordination.
- `EmbeddedScAudioEngine.*`: embedded SuperCollider rendering.
- `PdAudioEngine.*`: embedded Pure Data rendering and patch lifecycle.

## Application shell

`Main.cpp` currently owns the main road canvas, floating element editors,
transport wiring, project persistence, startup flow, and JUCE application
lifecycle. Further consolidation should proceed in this order:

1. Move project serialization and validation behind a `ProjectDocument` API.
2. Move transport, clocks, drop simulation, and modulation evaluation into a
   workspace engine independent of JUCE components.
3. Move floating SC, Pd, SCsheet, and Orca windows into an editor-window module.
4. Leave `MainComponent` responsible only for layout, commands, and wiring.

Each extraction must keep the Release build and all smoke targets passing.
