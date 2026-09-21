// Copyright 2026 Loops Creative Studio. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Framework/Application/IInputProcessor.h"

class FEditorViewportClient;

class FLoops2DPanZoomInputProcessor : public IInputProcessor
{
	public:
		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
		virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
		virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
		virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
		virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
		virtual const TCHAR* GetDebugName() const override { return TEXT("Loops2DPanZoom"); }

	private:
		// Hotkey \ is both a hold-modifier (drag to pan/zoom) and a tap-toggle, so
		// the toggle is deferred to key-up and cancelled by any mouse use while held.
		bool bPanZoomKeyDown = false;
		bool bPanZoomGestureCancelled = false;
		FKey PanZoomHeldKey;
		bool bPanZoomKeyUsedWithMouse = false;
		FEditorViewportClient* PanZoomKeyViewportClient = nullptr;
		bool bIsPanning = false;
		bool bIsZooming = false;
		FEditorViewportClient* CapturedViewportClient = nullptr;
		FVector2D LastScreenPos = FVector2D::ZeroVector;
};
