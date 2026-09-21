// Copyright 2026 Loops Creative Studio. All Rights Reserved.
#include "Loops2DPanZoomCommands.h"
#define LOCTEXT_NAMESPACE "Loops2DPanZoomCommands"
void FLoops2DPanZoomCommands::RegisterCommands()
{
	UI_COMMAND(ToggleAndDrag, "Toggle / Hold to Pan and Zoom", "Tap to toggle; hold with middle/right mouse to pan/zoom. Subject to the Animation Mode Only preference.", EUserInterfaceActionType::Button, FInputChord(EKeys::Backslash), FInputChord(EKeys::Slash));
	UI_COMMAND(Reset, "Reset Pan and Zoom", "Restore centered pan and 100% zoom. Subject to the Animation Mode Only preference.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::Backslash), FInputChord(EModifierKey::Shift, EKeys::Slash));
}
#undef LOCTEXT_NAMESPACE
