// Copyright 2026 Loops Creative Studio. All Rights Reserved.

#include "Loops2DPanZoomCommands.h"

#define LOCTEXT_NAMESPACE "Loops2DPanZoomCommands"

FLoops2DPanZoomCommands::FLoops2DPanZoomCommands()
	: TCommands<FLoops2DPanZoomCommands>(
		TEXT("Loops2DPanZoom"),
		LOCTEXT("Loops2DPanZoomCommandsContext", "Loops 2D Pan/Zoom"),
		NAME_None,
		FLoops2DPanZoomStyle::GetStyleSetName())
{
}

void FLoops2DPanZoomCommands::RegisterCommands()
{
	UI_COMMAND(ToggleEnabled, "Toggle Pan/Zoom", "Enable or disable 2D Pan/Zoom on the active viewport", EUserInterfaceActionType::Button, FInputChord(EKeys::Divide));
	UI_COMMAND(ToggleZoomTo100, "Toggle Zoom 100%", "Toggle zoom and pan between their current values and 100%", EUserInterfaceActionType::Button, FInputChord(EKeys::Multiply));
	UI_COMMAND(PanLeft, "Pan Left", "Pan the view left", EUserInterfaceActionType::Button, FInputChord(EKeys::NumPadFour));
	UI_COMMAND(PanRight, "Pan Right", "Pan the view right", EUserInterfaceActionType::Button, FInputChord(EKeys::NumPadSix));
	UI_COMMAND(PanUp, "Pan Up", "Pan the view up", EUserInterfaceActionType::Button, FInputChord(EKeys::NumPadEight));
	UI_COMMAND(PanDown, "Pan Down", "Pan the view down", EUserInterfaceActionType::Button, FInputChord(EKeys::NumPadTwo));
	UI_COMMAND(ZoomIn, "Zoom In", "Zoom the view in", EUserInterfaceActionType::Button, FInputChord(EKeys::Add));
	UI_COMMAND(ZoomOut, "Zoom Out", "Zoom the view out", EUserInterfaceActionType::Button, FInputChord(EKeys::Subtract));
	UI_COMMAND(ToggleAnimControlLock, "Toggle Control Rig Lock", "Lock the camera to the selected Control Rig control", EUserInterfaceActionType::Button, FInputChord(EKeys::Decimal));
}

#undef LOCTEXT_NAMESPACE
