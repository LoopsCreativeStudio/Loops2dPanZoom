# Loops 2D Pan/Zoom - Unreal Engine Editor Plugin

"2D Pan/Zoom" in the Unreal editor viewport: zoom/offset the
view to inspect a detail.

**Description:** Loops2DPanZoom drives the viewport's camera non-destructively to pan/zoom in place, similar to Maya's 2D Pan/Zoom, keeping selection, gizmos, and Control Rig in sync (at the cost of slight perspective distortion at large values). It can also auto-lock the camera onto the selected Control Rig control and follow sequencer camera cuts.

## Installation

1. Copy the `Loops2DPanZoom/` folder into `YourProject/Plugins/`.
2. Open the project (or right-click the `.uproject` → "Generate Visual Studio
   project files") and compile (Development Editor).
3. Edit > Plugins > check that "Loops 2D Pan/Zoom" is enabled.

## Shortcut preferences

In **Editor Preferences > Keyboard Shortcuts > Loops 2D Pan/Zoom**, customize
hotkeys for toggling and resetting pan and zoom.  Defaults to either `\` or `/`

**Editor Preferences > Plugins > Loops 2D Pan/Zoom > Animation Mode Only** is
on by default. Outside Animation Mode these shortcuts pass through to Unreal,
including its backslash preview mesh and Shift+backslash cycle commands.

## Usage

- **Toggle**: `\` key, or the 2D Pan/Zoom button in the viewport toolbar (the
  button acts on whichever viewport it's in, and focuses it so shortcuts work
  right away).
- **Pan**: hold `\` + middle mouse drag, or Numpad `4`/`6`/`8`/`2`.
- **Zoom**: hold `\` + right mouse drag (drag right = zoom in), or Numpad `+`/`-`.
- **Toggle zoom+pan to 100% / centered**: Numpad `*`. Pressing it again
  restores whatever zoom/pan you had before.
- **Reset** (zero offset, 100% zoom): `Shift + \`.
- **Auto focus on selected control**: `Numpad .`	Lock & Follow the camera to the selected Control Rig control

Holding `\` and dragging enables 2D Pan/Zoom automatically; tapping `\`
(without using the mouse) toggles between the original and adjusted view.
`Alt` + mouse is left to normal Unreal navigation.

Works in any editor mode (Select, Landscape, Rig Editing, etc.), and state is
stored per viewport, so one panel can be panned/zoomed while another stays
normal.

Pan/zoom also **follows Sequencer's live camera cut**: if the shot changes or
the camera moves while pan/zoom is enabled, the base view updates
continuously and the offset stays layered on top, including while the
viewport is locked to Camera Cuts.
