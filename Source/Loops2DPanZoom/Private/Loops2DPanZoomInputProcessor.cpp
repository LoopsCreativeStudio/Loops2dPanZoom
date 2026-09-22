// Copyright 2026 Loops Creative Studio. All Rights Reserved.

#include "Loops2DPanZoomInputProcessor.h"
#include "Loops2DPanZoomSubsystem.h"
#include "EditorViewportClient.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "SEditorViewport.h"
#include "Loops2DPanZoomCommands.h"
#include "Loops2DPanZoomSettings.h"
#include "EditorModeManager.h"
#include "EditMode/ControlRigEditMode.h"

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

	static bool CanUseModeShortcuts(FSlateApplication& SlateApp)
	{
		if (!GEditor || !SlateApp.IsActive() || SlateApp.GetActiveModalWindow()) { return false; }
		// Protect text entry without requiring viewport focus.
		for (TSharedPtr<SWidget> Widget = SlateApp.GetKeyboardFocusedWidget(); Widget; Widget = Widget->GetParentWidget())
		{
			if (Widget->GetTypeAsString().Contains(TEXT("EditableText"))) { return false; }
		}
		return !GetDefault<ULoops2DPanZoomSettings>()->bAnimationModeOnly
			|| GLevelEditorModeTools().IsModeActive(FControlRigEditMode::ModeName);
	}
}

void FLoops2DPanZoomInputProcessor::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
	// Key-up is lost on application switches; don't leave the modifier stuck.
	if (!SlateApp.IsActive())
	{
		bPanZoomKeyDown = false;
		bPanZoomKeyUsedWithMouse = false;
		PanZoomKeyViewportClient = nullptr;
		PanZoomHeldKey = EKeys::Invalid;
		bIsPanning = bIsZooming = false;
		CapturedViewportClient = nullptr;
	}
	// Cancel the gesture across mode/focus changes, but consume its releases.
	if (bPanZoomKeyDown && !Loops2DPanZoomInput::CanUseModeShortcuts(SlateApp))
	{
		bPanZoomGestureCancelled = true;
	}

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

	// Consume repeats only for a press we accepted; don't start mid-hold on a mode switch.
	if (bPanZoomKeyDown && Key == PanZoomHeldKey) { return true; }
	const FInputChord Chord(Key, InKeyEvent.IsShiftDown(), InKeyEvent.IsControlDown(), InKeyEvent.IsAltDown(), InKeyEvent.IsCommandDown());
	const auto& Commands = FLoops2DPanZoomCommands::Get();
	const bool bReset = Commands.Reset->HasActiveChord(Chord);
	if (bReset || Commands.ToggleAndDrag->HasActiveChord(Chord))
	{
		if (bPanZoomKeyDown || InKeyEvent.IsRepeat() || !Loops2DPanZoomInput::CanUseModeShortcuts(SlateApp)) { return false; }
		ULoops2DPanZoomSubsystem* Subsystem = GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>();
		FEditorViewportClient* Client = Loops2DPanZoomInput::GetActiveEditorViewportClient();
		if (!Subsystem || !Client) { return false; }
		bPanZoomKeyDown = true;
		PanZoomHeldKey = Key;
		PanZoomKeyViewportClient = Client;
		bPanZoomGestureCancelled = bReset;
		bPanZoomKeyUsedWithMouse = !SlateApp.GetPressedMouseButtons().IsEmpty();
		if (bReset) { Subsystem->Reset(Client); }
		return true;
	}

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

bool FLoops2DPanZoomInputProcessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() != PanZoomHeldKey || !bPanZoomKeyDown)
	{
		return false;
	}

	const bool bWasTap = !bPanZoomKeyUsedWithMouse && !bPanZoomGestureCancelled
		&& Loops2DPanZoomInput::CanUseModeShortcuts(SlateApp);
	bPanZoomKeyDown = false;
	bPanZoomKeyUsedWithMouse = false;

	// Toggle only if the original viewport is still the active target.
	ULoops2DPanZoomSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>() : nullptr;
	FEditorViewportClient* Client = Loops2DPanZoomInput::GetActiveEditorViewportClient();
	if (bWasTap && Subsystem && Client && Client == PanZoomKeyViewportClient)
	{
		Subsystem->ToggleEnabled(Client);
	}
	PanZoomKeyViewportClient = nullptr;
	PanZoomHeldKey = EKeys::Invalid;
	return true;
}

bool FLoops2DPanZoomInputProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (!bPanZoomKeyDown || bPanZoomGestureCancelled || !Loops2DPanZoomInput::CanUseModeShortcuts(SlateApp))
	{
		return false;
	}
	// Any click while the key is held means this was not a tap.
	bPanZoomKeyUsedWithMouse = true;

	const FKey Button = MouseEvent.GetEffectingButton();
	if (Button != EKeys::MiddleMouseButton && Button != EKeys::RightMouseButton)
	{
		return false;
	}

	ULoops2DPanZoomSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>() : nullptr;
	FEditorViewportClient* Client = Loops2DPanZoomInput::GetActiveEditorViewportClient();
	if (!Subsystem || !Client || Client != PanZoomKeyViewportClient)
	{
		return false;
	}

	// Preprocessors run before Slate routes the click, so focus alone doesn't
	// prove the pointer is over the viewport. Leave clicks elsewhere alone.
	const TSharedPtr<SEditorViewport> ViewportWidget = Client->GetEditorViewportWidget();
	if (!ViewportWidget || !ViewportWidget->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
	{
		return false;
	}

	// Dragging enables the mode, like Maya; no separate enable step needed.
	Subsystem->SetEnabled(Client, true);
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

	// Releasing the key stops adjusting, but the drag stays consumed until
	// mouse-up so the viewport doesn't see moves for a press it never got.
	if (!bPanZoomKeyDown || bPanZoomGestureCancelled || !Loops2DPanZoomInput::CanUseModeShortcuts(SlateApp)
		|| !GEditor->GetAllViewportClients().Contains(CapturedViewportClient))
	{
		return true;
	}

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
