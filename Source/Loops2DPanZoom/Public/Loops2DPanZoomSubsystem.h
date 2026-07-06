#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Loops2DPanZoomSubsystem.generated.h"

class FEditorViewportClient;
class SWidget;


USTRUCT()
struct FLoops2DPanZoomState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector2D PanOffset = FVector2D::ZeroVector;
	UPROPERTY()
	float Zoom = 1.0f;
	UPROPERTY()
	float PreToggleZoom = 1.0f;
	UPROPERTY()
	FVector2D PreTogglePanOffset = FVector2D::ZeroVector;
	UPROPERTY()
	bool bEnabled = false;
	UPROPERTY()
	bool bHasBase = false;
	UPROPERTY()
	FVector BaseLocation = FVector::ZeroVector;
	UPROPERTY()
	FRotator BaseRotation = FRotator::ZeroRotator;
	UPROPERTY()
	float BaseFOV = 90.0f;
	UPROPERTY()
	float BaseOrthoZoom = 0.0f;
	UPROPERTY()
	bool bWasDepthOfFieldEnabled = true;

	TSharedPtr<SWidget> OverlayWidget;
};

// Drives 2D Pan/Zoom per-viewport, keyed by the FEditorViewportClient pointer.
UCLASS()
class LOOPS2DPANZOOM_API ULoops2DPanZoomSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

	public:
		bool IsEnabled(const FEditorViewportClient* ViewportClient) const;
		void SetEnabled(FEditorViewportClient* ViewportClient, bool bEnabled);
		void ToggleEnabled(FEditorViewportClient* ViewportClient);
		void Pan(FEditorViewportClient* ViewportClient, const FVector2D& ScreenDelta, const FIntPoint& ViewportSize);
		void Zoom(FEditorViewportClient* ViewportClient, float DeltaZoom);
		void Reset(FEditorViewportClient* ViewportClient);
		void ToggleZoomTo100Percent(FEditorViewportClient* ViewportClient);
		bool GetOverlayInfo(const FEditorViewportClient* ViewportClient, float& OutZoomPercent, FVector2D& OutCropSize, FVector2D& OutCropCenterOffset) const;
		void TickFollowCameraCut(FEditorViewportClient* ViewportClient);
		void NotifyCameraCut(UObject* CameraObject);

	private:
		FLoops2DPanZoomState& GetState(FEditorViewportClient* ViewportClient);
		const FLoops2DPanZoomState* FindState(const FEditorViewportClient* ViewportClient) const;

		void CaptureBaseIfNeeded(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State);
		void ApplyToCamera(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State);
		void RestoreCamera(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State);

		void AddOverlayIfNeeded(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State);
		void RemoveOverlayIfNeeded(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State);

		TMap<FEditorViewportClient*, FLoops2DPanZoomState> ViewportStates;
		TWeakObjectPtr<UObject> LastCameraCutObject;
};
