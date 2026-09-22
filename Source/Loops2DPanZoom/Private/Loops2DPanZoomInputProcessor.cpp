// Copyright 2026 Loops Creative Studio. All Rights Reserved.

#include "Loops2DPanZoomInputProcessor.h"
#include "Loops2DPanZoomSubsystem.h"
#include "Loops2DPanZoomSettings.h"
#include "EditorViewportClient.h"
#include "SEditorViewport.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"

namespace Loops2DPanZoomInput
{
	static FEditorViewportClient* GetViewportClientUnderScreenPosition(const FVector2D& ScreenPosition)
	{
		if (!GEditor)
		{
			return nullptr;
		}
		for (FEditorViewportClient* Candidate : GEditor->GetAllViewportClients())
		{
			if (!Candidate)
			{
				continue;
			}
			if (TSharedPtr<SEditorViewport> Widget = Candidate->GetEditorViewportWidget())
			{
				if (Widget->GetTickSpaceGeometry().IsUnderLocation(ScreenPosition))
				{
					return Candidate;
				}
			}
		}
		return nullptr;
	}

	static bool IsSuspended()
	{
		const ULoops2DPanZoomSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>() : nullptr;
		return Subsystem && Subsystem->IsSuspendedForPlayInEditor();
	}

	static bool IsViewportClientStillAlive(FEditorViewportClient* Client)
	{
		return Client && GEditor && GEditor->GetAllViewportClients().Contains(Client);
	}

	static bool IsModifierKeyDown(const FInputEvent& Event, const FKey& ModifierKey)
	{
		if (!ULoops2DPanZoomSettings::IsSupportedModifierKey(ModifierKey))
		{
			return false;
		}
		if (ModifierKey == EKeys::LeftAlt || ModifierKey == EKeys::RightAlt)
		{
			return Event.IsAltDown();
		}
		if (ModifierKey == EKeys::LeftControl || ModifierKey == EKeys::RightControl)
		{
			return Event.IsControlDown();
		}
		if (ModifierKey == EKeys::LeftShift || ModifierKey == EKeys::RightShift)
		{
			return Event.IsShiftDown();
		}
		return Event.IsCommandDown();
	}
}

FLoops2DPanZoomInputProcessor::FLoops2DPanZoomInputProcessor(TSharedRef<FUICommandList> InCommandList)
	: CommandList(InCommandList)
{
}

void FLoops2DPanZoomInputProcessor::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
	if (IsEngineExitRequested())
	{
		return;
	}

	if (GEditor)
	{
		if (ULoops2DPanZoomSubsystem* Subsystem = GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>())
		{
			Subsystem->TickAllAnimControlLocks();
			Subsystem->TickAllFollowCameraCuts();
		}
	}
}

bool FLoops2DPanZoomInputProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (Loops2DPanZoomInput::IsSuspended())
	{
		return false;
	}

	return CommandList->ProcessCommandBindings(InKeyEvent);
}

bool FLoops2DPanZoomInputProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	const ULoops2DPanZoomSettings* Settings = GetDefault<ULoops2DPanZoomSettings>();
	if (!Settings || Loops2DPanZoomInput::IsSuspended() || !Loops2DPanZoomInput::IsModifierKeyDown(MouseEvent, Settings->DragModifierKey))
	{
		return false;
	}

	const FKey Button = MouseEvent.GetEffectingButton();
	const bool bIsPanButton = Button == Settings->PanMouseButton;
	const bool bIsZoomButton = Button == Settings->ZoomMouseButton;
	if (!bIsPanButton && !bIsZoomButton)
	{
		return false;
	}

	FEditorViewportClient* Client = Loops2DPanZoomInput::GetViewportClientUnderScreenPosition(MouseEvent.GetScreenSpacePosition());
	ULoops2DPanZoomSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>() : nullptr;
	if (!Subsystem || !Client || !Subsystem->IsEnabled(Client))
	{
		return false;
	}

	CapturedViewportClient = Client;
	LastScreenPos = MouseEvent.GetScreenSpacePosition();
	bIsPanning = bIsPanButton;
	bIsZooming = bIsZoomButton && !bIsPanButton;
	return true;
}

bool FLoops2DPanZoomInputProcessor::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (!bIsPanning && !bIsZooming)
	{
		return false;
	}

	const ULoops2DPanZoomSettings* Settings = GetDefault<ULoops2DPanZoomSettings>();
	const FKey Button = MouseEvent.GetEffectingButton();
	const bool bStoppingPan = bIsPanning && Settings && Button == Settings->PanMouseButton;
	const bool bStoppingZoom = bIsZooming && Settings && Button == Settings->ZoomMouseButton;

	if (bStoppingPan || bStoppingZoom)
	{
		bIsPanning = false;
		bIsZooming = false;
		CapturedViewportClient = nullptr;
		return true;
	}

	return false;
}

bool FLoops2DPanZoomInputProcessor::HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (!bIsPanning && !bIsZooming)
	{
		return false;
	}

	if (Loops2DPanZoomInput::IsSuspended() || !Loops2DPanZoomInput::IsViewportClientStillAlive(CapturedViewportClient))
	{
		bIsPanning = false;
		bIsZooming = false;
		CapturedViewportClient = nullptr;
		return false;
	}

	ULoops2DPanZoomSubsystem* Subsystem = GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>();
	if (!Subsystem)
	{
		return false;
	}

	const FVector2D CurrentScreenPos = MouseEvent.GetScreenSpacePosition();
	const FVector2D Delta = CurrentScreenPos - LastScreenPos;
	LastScreenPos = CurrentScreenPos;

	const ULoops2DPanZoomSettings* Settings = GetDefault<ULoops2DPanZoomSettings>();
	constexpr float BaseZoomSpeed = 0.01f;

	if (bIsPanning && CapturedViewportClient->Viewport)
	{
		const float PanSensitivity = Settings ? Settings->MousePanSensitivity : 1.0f;
		Subsystem->Pan(CapturedViewportClient, Delta * PanSensitivity, CapturedViewportClient->Viewport->GetSizeXY());
	}
	else if (bIsZooming)
	{
		const float ZoomSensitivity = Settings ? Settings->MouseZoomSensitivity : 1.0f;
		Subsystem->Zoom(CapturedViewportClient, Delta.X * BaseZoomSpeed * ZoomSensitivity);
	}

	return true;
}
