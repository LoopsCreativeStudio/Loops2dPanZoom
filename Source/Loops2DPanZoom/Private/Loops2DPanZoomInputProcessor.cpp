// Copyright 2026 Loops Creative Studio. All Rights Reserved.

#include "Loops2DPanZoomInputProcessor.h"
#include "Loops2DPanZoomSubsystem.h"
#include "EditorViewportClient.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"

namespace Loops2DPanZoomInput
{
	static FEditorViewportClient* GetActiveEditorViewportClient()
	{
		if (!GEditor)
		{
			return nullptr;
		}
		FViewport* ActiveVP = GEditor->GetActiveViewport();
		if (ActiveVP)
		{
			for (FEditorViewportClient* Candidate : GEditor->GetAllViewportClients())
			{
				if (Candidate && Candidate->Viewport == ActiveVP)
				{
					return Candidate;
				}
			}
		}
		for (FEditorViewportClient* Candidate : GEditor->GetAllViewportClients())
		{
			if (Candidate)
			{
				return Candidate;
			}
		}
		return nullptr;
	}
}

void FLoops2DPanZoomInputProcessor::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
	if (GEditor)
	{
		if (ULoops2DPanZoomSubsystem* Subsystem = GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>())
		{
			Subsystem->TickAllAnimControlLocks();
		}
	}
}

bool FLoops2DPanZoomInputProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::Decimal)
	{
		ULoops2DPanZoomSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>() : nullptr;
		FEditorViewportClient* Client = Loops2DPanZoomInput::GetActiveEditorViewportClient();
		if (!Subsystem || !Client)
		{
			return false;
		}

		Subsystem->ToggleAnimControlLock(Client);
		return true;
	}

	if (Key == EKeys::Slash || Key == EKeys::Divide)
	{
		ULoops2DPanZoomSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>() : nullptr;
		FEditorViewportClient* Client = Loops2DPanZoomInput::GetActiveEditorViewportClient();
		if (!Subsystem || !Client)
		{
			return false;
		}

		if (InKeyEvent.IsShiftDown())
		{
			Subsystem->Reset(Client);
		}
		else
		{
			Subsystem->ToggleEnabled(Client);
		}
		return true;
	}

	const bool bIsZoomKey = Key == EKeys::Add || Key == EKeys::Subtract;
	const bool bIsPanKey = Key == EKeys::NumPadFour || Key == EKeys::NumPadSix
		|| Key == EKeys::NumPadEight || Key == EKeys::NumPadTwo;
	const bool bIsZoomToggleKey = Key == EKeys::Multiply;
	if (!bIsZoomKey && !bIsPanKey && !bIsZoomToggleKey)
	{
		return false;
	}

	ULoops2DPanZoomSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>() : nullptr;
	FEditorViewportClient* Client = Loops2DPanZoomInput::GetActiveEditorViewportClient();
	if (!Subsystem || !Client || !Subsystem->IsEnabled(Client))
	{
		return false;
	}

	if (bIsZoomToggleKey)
	{
		Subsystem->ToggleZoomTo100Percent(Client);
		return true;
	}

	if (bIsZoomKey)
	{
		constexpr float KeyZoomStep = 0.12f;
		Subsystem->Zoom(Client, Key == EKeys::Add ? KeyZoomStep : -KeyZoomStep);
		return true;
	}

	if (Client->Viewport)
	{
		constexpr float KeyPanStepPixels = 20.0f;
		FVector2D ScreenDelta = FVector2D::ZeroVector;
		if (Key == EKeys::NumPadFour)
		{
			ScreenDelta = FVector2D(KeyPanStepPixels, 0.0f);
		}
		else if (Key == EKeys::NumPadSix)
		{
			ScreenDelta = FVector2D(-KeyPanStepPixels, 0.0f);
		}
		else if (Key == EKeys::NumPadEight)
		{
			ScreenDelta = FVector2D(0.0f, KeyPanStepPixels);
		}
		else if (Key == EKeys::NumPadTwo)
		{
			ScreenDelta = FVector2D(0.0f, -KeyPanStepPixels);
		}

		Subsystem->Pan(Client, ScreenDelta, Client->Viewport->GetSizeXY());
	}
	return true;
}

bool FLoops2DPanZoomInputProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (!MouseEvent.IsAltDown())
	{
		return false;
	}

	const FKey Button = MouseEvent.GetEffectingButton();
	if (Button != EKeys::MiddleMouseButton && Button != EKeys::RightMouseButton)
	{
		return false;
	}

	ULoops2DPanZoomSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>() : nullptr;
	FEditorViewportClient* Client = Loops2DPanZoomInput::GetActiveEditorViewportClient();
	if (!Subsystem || !Client || !Subsystem->IsEnabled(Client))
	{
		return false;
	}

	CapturedViewportClient = Client;
	LastScreenPos = MouseEvent.GetScreenSpacePosition();
	bIsPanning = (Button == EKeys::MiddleMouseButton);
	bIsZooming = (Button == EKeys::RightMouseButton);
	return true;
}

bool FLoops2DPanZoomInputProcessor::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (!bIsPanning && !bIsZooming)
	{
		return false;
	}

	const FKey Button = MouseEvent.GetEffectingButton();
	const bool bStoppingPan = bIsPanning && Button == EKeys::MiddleMouseButton;
	const bool bStoppingZoom = bIsZooming && Button == EKeys::RightMouseButton;

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
	if ((!bIsPanning && !bIsZooming) || !CapturedViewportClient || !GEditor)
	{
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

	if (bIsPanning && CapturedViewportClient->Viewport)
	{
		Subsystem->Pan(CapturedViewportClient, Delta, CapturedViewportClient->Viewport->GetSizeXY());
	}
	else if (bIsZooming)
	{
		const float ZoomSpeed = 0.01f;
		Subsystem->Zoom(CapturedViewportClient, Delta.X * ZoomSpeed);
	}

	return true;
}
