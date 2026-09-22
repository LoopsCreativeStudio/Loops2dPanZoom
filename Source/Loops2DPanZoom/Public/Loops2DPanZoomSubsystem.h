// Copyright 2026 Loops Creative Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Loops2DPanZoomSubsystem.generated.h"

class FEditorViewportClient;
class SWidget;
class UCameraComponent;

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
	UPROPERTY()
	bool bAnimControlLockEnabled = false;
	UPROPERTY()
	FName LastAnimControlLockControlName;

	FVector LastAnimControlLockLocation = FVector::ZeroVector;
	bool bHasLastAnimControlLockLocation = false;
	UPROPERTY()
	bool bPreToggleAnimControlLockEnabled = false;

	UPROPERTY()
	bool bCameraModifierActive = false;
	UPROPERTY()
	TWeakObjectPtr<UCameraComponent> ModifiedCameraComponent;

	UPROPERTY()
	FVector BaseRelativeLocation = FVector::ZeroVector;
	UPROPERTY()
	FRotator BaseRelativeRotation = FRotator::ZeroRotator;
	UPROPERTY()
	bool bHasBaseRelativeTransform = false;

	TSharedPtr<SWidget> OverlayWidget;
};

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
		void ToggleZoomTo100Percent(FEditorViewportClient* ViewportClient);
		bool GetOverlayInfo(const FEditorViewportClient* ViewportClient, float& OutZoomPercent, FVector2D& OutCropSize, FVector2D& OutCropCenterOffset, bool& OutIsAnimControlLockActive) const;
		void TickFollowCameraCut(FEditorViewportClient* ViewportClient);
		void TickAllFollowCameraCuts();
		void NotifyCameraCut(UObject* CameraObject);
		bool IsAnimControlLockEnabled(const FEditorViewportClient* ViewportClient) const;
		void ToggleAnimControlLock(FEditorViewportClient* ViewportClient);
		void TickAllAnimControlLocks();

		bool IsSuspendedForPlayInEditor() const { return bSuspendedForPlayInEditor; }

		virtual void Initialize(FSubsystemCollectionBase& Collection) override;
		virtual void Deinitialize() override;

	private:
		FLoops2DPanZoomState& GetState(FEditorViewportClient* ViewportClient);
		const FLoops2DPanZoomState* FindState(const FEditorViewportClient* ViewportClient) const;

		void CaptureBaseIfNeeded(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State);
		void ApplyToCamera(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State);
		void RestoreCamera(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State);

		bool RefreshBaseFromDrivingCameraIfChanged(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State);

		bool RebaseOnDrivingCameraChange(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State);

		bool IsViewLockedToActor(const FEditorViewportClient* ViewportClient) const;
		void ApplyCinematicCameraModifier(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State, const FVector& NewLocation, const FRotator& NewRotation);
		void RestoreCinematicCameraModifier(FLoops2DPanZoomState& State);

		void PruneStaleViewportStates();

		void AddOverlayIfNeeded(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State);
		void RemoveOverlayIfNeeded(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State);

		void RefreshOverlayPresence(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State);
		bool GetSelectedControlWorldTransform(FTransform& OutTransform, FName* OutControlName = nullptr) const;
		void UpdateAnimControlLockPan(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State, const FVector& ControlWorldLocation);
		bool EnableAnimControlLock(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State);
		void DisableAnimControlLock(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State);

		void OnBeginPlayInEditor(const bool bIsSimulating);
		void OnEndPlayInEditor(const bool bIsSimulating);
		void SuspendForPlayInEditor();

		TMap<FEditorViewportClient*, FLoops2DPanZoomState> ViewportStates;
		TWeakObjectPtr<UObject> LastCameraCutObject;

		bool bSuspendedForPlayInEditor = false;
		FDelegateHandle BeginPlayInEditorHandle;
		FDelegateHandle EndPlayInEditorHandle;

		uint64 LastPruneFrameCounter = MAX_uint64;
};
