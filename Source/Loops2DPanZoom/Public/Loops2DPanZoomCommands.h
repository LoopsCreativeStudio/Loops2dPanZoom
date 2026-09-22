// Copyright 2026 Loops Creative Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Loops2DPanZoomStyle.h"

class FLoops2DPanZoomCommands : public TCommands<FLoops2DPanZoomCommands>
{
	public:
		FLoops2DPanZoomCommands();

		virtual void RegisterCommands() override;

		TSharedPtr<FUICommandInfo> ToggleEnabled;
		TSharedPtr<FUICommandInfo> ToggleZoomTo100;
		TSharedPtr<FUICommandInfo> PanLeft;
		TSharedPtr<FUICommandInfo> PanRight;
		TSharedPtr<FUICommandInfo> PanUp;
		TSharedPtr<FUICommandInfo> PanDown;
		TSharedPtr<FUICommandInfo> ZoomIn;
		TSharedPtr<FUICommandInfo> ZoomOut;
		TSharedPtr<FUICommandInfo> ToggleAnimControlLock;
};
