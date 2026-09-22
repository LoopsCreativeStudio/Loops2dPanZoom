// Copyright 2026 Loops Creative Studio. All Rights Reserved.
#pragma once
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

class FLoops2DPanZoomCommands : public TCommands<FLoops2DPanZoomCommands>
{
public:
	FLoops2DPanZoomCommands() : TCommands(TEXT("Loops2DPanZoom"), NSLOCTEXT("Loops2DPanZoom", "Commands", "Loops 2D Pan/Zoom"), NAME_None, FAppStyle::GetAppStyleSetName()) {}
	virtual void RegisterCommands() override;
	TSharedPtr<FUICommandInfo> ToggleAndDrag;
	TSharedPtr<FUICommandInfo> Reset;
	TSharedPtr<FUICommandInfo> ZoomIn;
	TSharedPtr<FUICommandInfo> ZoomOut;
	TSharedPtr<FUICommandInfo> PanLeft;
	TSharedPtr<FUICommandInfo> PanRight;
	TSharedPtr<FUICommandInfo> PanUp;
	TSharedPtr<FUICommandInfo> PanDown;
	TSharedPtr<FUICommandInfo> ToggleZoom100;
	TSharedPtr<FUICommandInfo> ToggleControlLock;
};
