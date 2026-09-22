// Copyright 2026 Loops Creative Studio. All Rights Reserved.
#include "Loops2DPanZoomCommands.h"
#define LOCTEXT_NAMESPACE "Loops2DPanZoomCommands"
void FLoops2DPanZoomCommands::RegisterCommands()
{
	UI_COMMAND(ToggleAndDrag, "Toggle / Hold to Pan and Zoom", "Tap to toggle; hold with middle/right mouse to pan/zoom. Subject to the Animation Mode Only preference.", EUserInterfaceActionType::Button, FInputChord(EKeys::Backslash), FInputChord(EKeys::Slash));
	UI_COMMAND(Reset, "Reset Pan and Zoom", "Restore centered pan and 100% zoom. Subject to the Animation Mode Only preference.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::Backslash), FInputChord(EModifierKey::Shift, EKeys::Slash));
	UI_COMMAND(ZoomIn, "Zoom In", "Zoom the view in. Requires Pan/Zoom to be active.", EUserInterfaceActionType::Button, FInputChord(EKeys::Add));
	UI_COMMAND(ZoomOut, "Zoom Out", "Zoom the view out. Requires Pan/Zoom to be active.", EUserInterfaceActionType::Button, FInputChord(EKeys::Subtract));
	UI_COMMAND(PanLeft, "Pan Left", "Pan the view left. Requires Pan/Zoom to be active.", EUserInterfaceActionType::Button, FInputChord(EKeys::NumPadFour));
	UI_COMMAND(PanRight, "Pan Right", "Pan the view right. Requires Pan/Zoom to be active.", EUserInterfaceActionType::Button, FInputChord(EKeys::NumPadSix));
	UI_COMMAND(PanUp, "Pan Up", "Pan the view up. Requires Pan/Zoom to be active.", EUserInterfaceActionType::Button, FInputChord(EKeys::NumPadEight));
	UI_COMMAND(PanDown, "Pan Down", "Pan the view down. Requires Pan/Zoom to be active.", EUserInterfaceActionType::Button, FInputChord(EKeys::NumPadTwo));
	UI_COMMAND(ToggleZoom100, "Toggle Zoom 100%", "Toggle zoom and pan between their current values and 100% centered. Requires Pan/Zoom to be active.", EUserInterfaceActionType::Button, FInputChord(EKeys::Multiply));
	UI_COMMAND(ToggleControlLock, "Toggle Control Rig Lock", "Lock the camera onto the selected Control Rig control. Subject to the Animation Mode Only preference.", EUserInterfaceActionType::Button, FInputChord(EKeys::Decimal));
}
#undef LOCTEXT_NAMESPACE
