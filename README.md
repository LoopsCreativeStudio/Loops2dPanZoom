# Loops 2D Pan/Zoom - Unreal Engine Editor Plugin

"2D Pan/Zoom" in the Unreal editor viewport: zoom/offset the view to inspect a detail.

**Description:** Loops2DPanZoom drives the viewport's camera non-destructively to pan/zoom in place, similar to Maya's 2D Pan/Zoom, keeping selection, gizmos, and Control Rig in sync (at the cost of slight perspective distortion at large values). It can also auto-lock the camera onto the selected Control Rig control, and it works while the viewport is locked to a Sequencer camera cut.

## Requirements

- Unreal Engine 5.8 or 5.7, Windows (Win64).
- The **Control Rig** plugin must be enabled it is required by the auto-focus feature.

## Installation

1. Copy the `Loops2DPanZoom/` folder into `YourProject/Plugins/`.
2. Open the project (or right-click the `.uproject` → "Generate Visual Studio
   project files") and compile (Development Editor).
3. Edit > Plugins > enable "Loops 2D Pan/Zoom", then restart the editor.

## Usage

- **Toggle**: Numpad `/`, or the 2D Pan/Zoom button in the viewport toolbar (the
  button acts on whichever viewport it's in, and focuses it so shortcuts work
  right away).
- **Pan**: `Alt + Middle-click` drag, or Numpad `4`/`6`/`8`/`2`.
- **Zoom**: `Alt + Right-click` drag (drag right = zoom in), or Numpad `+`/`-`.
- **Toggle zoom+pan to 100% / centered**: Numpad `*`. Pressing it again
  restores whatever zoom/pan you had before, so it doubles as a reset.
- **Auto focus on selected control**: Numpad `.` — locks and follows the camera
  onto the selected Control Rig control.

## Settings

Under **Editor Preferences > Plugins > Loops 2D Pan/Zoom**:

- **Mouse Drag** — which mouse buttons pan and zoom, and which modifier key
  starts a drag (Alt, Ctrl, Shift or Cmd).
- **Sensitivity** — mouse pan and zoom multipliers, keyboard pan step in pixels,
  and keyboard zoom step. Changes take effect immediately.

Every keyboard shortcut is rebindable under **Editor Preferences > Keyboard
Shortcuts**, in the "Loops 2D Pan/Zoom" section.

## Sequencer and cinematic cameras

Pan/Zoom works while the viewport is locked to a Sequencer camera cut or to a
manually piloted actor. In those cases the offset is applied as a temporary,
fully reversible modifier on the camera actually driving the view, so:

- The framing offset stays locked to the shot's camera as it animates.
- A cut to a different camera re-bases the offset onto the new one.
- Switching Pan/Zoom off returns the camera exactly to where the shot puts it on
  the current frame, nothing is left baked in.