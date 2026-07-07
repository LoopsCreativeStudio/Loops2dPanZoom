// Copyright 2026 Loops Creative Studio. All Rights Reserved.

#include "Loops2DPanZoomSubsystem.h"
#include "Loops2DPanZoomOverlay.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "IAssetViewport.h"
#include "SLevelViewport.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"

namespace Loops2DPanZoomLimits
{
	// Default Unreal value FOV in viewport, if is possible to change that ?
	constexpr float MinFOV = 5.0f;
	constexpr float MaxFOV = 170.0f;

	constexpr float DollyTraceDistance = 1000000.0f; 
	constexpr float DollyFallbackDistance = 100000.0f; 
}

namespace Loops2DPanZoomSequencer
{
	static UCameraComponent* GetDrivingCameraComponent(FEditorViewportClient* ViewportClient)
	{
		if (!ViewportClient || !ViewportClient->IsLevelEditorClient())
		{
			return nullptr;
		}
		return static_cast<FLevelEditorViewportClient*>(ViewportClient)->GetCameraComponentForView();
	}

	static UCameraComponent* ResolveCameraComponent(UObject* CameraObject)
	{
		if (!CameraObject)
		{
			return nullptr;
		}
		if (UCameraComponent* AsComponent = Cast<UCameraComponent>(CameraObject))
		{
			return AsComponent;
		}
		if (AActor* AsActor = Cast<AActor>(CameraObject))
		{
			return AsActor->FindComponentByClass<UCameraComponent>();
		}
		return nullptr;
	}
}

FLoops2DPanZoomState& ULoops2DPanZoomSubsystem::GetState(FEditorViewportClient* ViewportClient)
{
	return ViewportStates.FindOrAdd(ViewportClient);
}

const FLoops2DPanZoomState* ULoops2DPanZoomSubsystem::FindState(const FEditorViewportClient* ViewportClient) const
{
	return ViewportStates.Find(const_cast<FEditorViewportClient*>(ViewportClient));
}

bool ULoops2DPanZoomSubsystem::IsEnabled(const FEditorViewportClient* ViewportClient) const
{
	const FLoops2DPanZoomState* State = FindState(ViewportClient);
	return State && State->bEnabled;
}

void ULoops2DPanZoomSubsystem::CaptureBaseIfNeeded(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State)
{
	if (State.bHasBase || !ViewportClient)
	{
		return;
	}

	UCameraComponent* DrivingCamera = Loops2DPanZoomSequencer::ResolveCameraComponent(LastCameraCutObject.Get());
	if (!DrivingCamera)
	{
		DrivingCamera = Loops2DPanZoomSequencer::GetDrivingCameraComponent(ViewportClient);
	}

	if (DrivingCamera)
	{
		FMinimalViewInfo ViewInfo;
		DrivingCamera->GetCameraView(0.0f, ViewInfo);
		State.BaseLocation = ViewInfo.Location;
		State.BaseRotation = ViewInfo.Rotation;
		State.BaseFOV = ViewInfo.FOV;
	}
	else
	{
		State.BaseLocation = ViewportClient->GetViewLocation();
		State.BaseRotation = ViewportClient->GetViewRotation();
		State.BaseFOV = ViewportClient->ViewFOV;
	}

	State.BaseOrthoZoom = ViewportClient->GetOrthoZoom();
	State.bWasDepthOfFieldEnabled = ViewportClient->EngineShowFlags.DepthOfField;
	State.bHasBase = true;
}

void ULoops2DPanZoomSubsystem::ApplyToCamera(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State)
{
	if (!ViewportClient || !State.bHasBase)
	{
		return;
	}

	const float SafeZoom = FMath::Max(State.Zoom, 0.01f);

	if (ViewportClient->IsPerspective())
	{
		FRotator NewRotation = State.BaseRotation;
		NewRotation.Yaw = FRotator::NormalizeAxis(NewRotation.Yaw + State.PanOffset.X);
		NewRotation.Pitch = FMath::Clamp(NewRotation.Pitch + State.PanOffset.Y, -89.9f, 89.9f);
		ViewportClient->ViewFOV = FMath::Clamp(State.BaseFOV / SafeZoom, Loops2DPanZoomLimits::MinFOV, Loops2DPanZoomLimits::MaxFOV);

		FVector NewLocation = State.BaseLocation;
		const float FOVLimitedZoom = State.BaseFOV / Loops2DPanZoomLimits::MinFOV;
		if (SafeZoom > FOVLimitedZoom)
		{
			const float ExcessZoomRatio = SafeZoom / FOVLimitedZoom;
			const FVector Forward = NewRotation.Vector();

			// TODO : Improve this Part
			float TargetDistance = Loops2DPanZoomLimits::DollyFallbackDistance;
			if (UWorld* World = ViewportClient->GetWorld())
			{
				const FVector TraceStart = State.BaseLocation;
				const FVector TraceEnd = TraceStart + Forward * Loops2DPanZoomLimits::DollyTraceDistance;
				FCollisionQueryParams QueryParams(TEXT("Loops2DPanZoomDollyTrace"), /*bTraceComplex=*/false);

				FHitResult Hit;
				if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
				{
					TargetDistance = Hit.Distance;
				}
			}

			const float NewDistance = TargetDistance / ExcessZoomRatio;
			NewLocation += Forward * (TargetDistance - NewDistance);
		}

		ViewportClient->SetViewRotation(NewRotation);
		ViewportClient->SetViewLocation(NewLocation);
	}
	else
	{
		const FRotationMatrix RotMatrix(State.BaseRotation);
		const FVector Right = RotMatrix.GetScaledAxis(EAxis::Y);
		const FVector Up = RotMatrix.GetScaledAxis(EAxis::Z);
		const FVector NewLocation = State.BaseLocation + Right * State.PanOffset.X + Up * State.PanOffset.Y;
		ViewportClient->SetViewLocation(NewLocation);
		ViewportClient->SetOrthoZoom(State.BaseOrthoZoom / SafeZoom);
	}
	ViewportClient->EngineShowFlags.SetDepthOfField(false);
	ViewportClient->Invalidate();
}

// void ULoops2DPanZoomSubsystem::ApplyToCamera(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State)
// {
// 	if (!ViewportClient || !State.bHasBase)
// 	{
// 		return;
// 	}

// 	const float SafeZoom = FMath::Max(State.Zoom, 0.01f);

// 	if (ViewportClient->IsPerspective())
// 	{
// 		FRotator NewRotation = State.BaseRotation;
// 		NewRotation.Yaw = FRotator::NormalizeAxis(NewRotation.Yaw + State.PanOffset.X);
// 		NewRotation.Pitch = FMath::Clamp(NewRotation.Pitch + State.PanOffset.Y, -89.9f, 89.9f);
// 		ViewportClient->ViewFOV = FMath::Clamp(State.BaseFOV / SafeZoom, Loops2DPanZoomLimits::MinFOV, Loops2DPanZoomLimits::MaxFOV);
// 		ViewportClient->SetViewRotation(NewRotation);
// 	}
// 	else
// 	{
// 		const FRotationMatrix RotMatrix(State.BaseRotation);
// 		const FVector Right = RotMatrix.GetScaledAxis(EAxis::Y);
// 		const FVector Up = RotMatrix.GetScaledAxis(EAxis::Z);
// 		const FVector NewLocation = State.BaseLocation + Right * State.PanOffset.X + Up * State.PanOffset.Y;
// 		ViewportClient->SetViewLocation(NewLocation);
// 		ViewportClient->SetOrthoZoom(State.BaseOrthoZoom / SafeZoom);
// 	}
// 	ViewportClient->EngineShowFlags.SetDepthOfField(false);
// 	ViewportClient->Invalidate();
// }

void ULoops2DPanZoomSubsystem::RestoreCamera(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State)
{
	if (!ViewportClient || !State.bHasBase)
	{
		return;
	}

	ViewportClient->EngineShowFlags.SetDepthOfField(State.bWasDepthOfFieldEnabled);

	ViewportClient->SetViewRotation(State.BaseRotation);
	ViewportClient->SetViewLocation(State.BaseLocation);
	ViewportClient->ViewFOV = State.BaseFOV;
	ViewportClient->SetOrthoZoom(State.BaseOrthoZoom);
	ViewportClient->Invalidate();
}

void ULoops2DPanZoomSubsystem::SetEnabled(FEditorViewportClient* ViewportClient, bool bEnabled)
{
	if (!ViewportClient)
	{
		return;
	}

	FLoops2DPanZoomState& State = GetState(ViewportClient);
	if (State.bEnabled == bEnabled)
	{
		return;
	}

	State.bEnabled = bEnabled;
	if (bEnabled)
	{
		//I don't force the "Allow Cinematic Control" setting to be disabled because it's too unstable.
		CaptureBaseIfNeeded(ViewportClient, State);

		AddOverlayIfNeeded(ViewportClient, State);
		ApplyToCamera(ViewportClient, State);
	}
	else
	{
		RestoreCamera(ViewportClient, State);
		RemoveOverlayIfNeeded(ViewportClient, State);

		State.bHasBase = false;
	}
}

void ULoops2DPanZoomSubsystem::AddOverlayIfNeeded(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State)
{
	if (State.OverlayWidget.IsValid() || !ViewportClient || !ViewportClient->IsLevelEditorClient())
	{
		return;
	}
	TSharedPtr<SEditorViewport> EditorViewportWidget = ViewportClient->GetEditorViewportWidget();
	TSharedPtr<SLevelViewport> LevelViewportWidget = StaticCastSharedPtr<SLevelViewport>(EditorViewportWidget);
	if (!LevelViewportWidget.IsValid())
	{
		return;
	}
	TSharedRef<SLoops2DPanZoomOverlay> Overlay = SNew(SLoops2DPanZoomOverlay, ViewportClient);
	LevelViewportWidget->AddOverlayWidget(Overlay);
	State.OverlayWidget = Overlay;
}

void ULoops2DPanZoomSubsystem::RemoveOverlayIfNeeded(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State)
{
	if (!State.OverlayWidget.IsValid()){return;}
	if (ViewportClient && ViewportClient->IsLevelEditorClient())
	{
		TSharedPtr<SEditorViewport> EditorViewportWidget = ViewportClient->GetEditorViewportWidget();
		TSharedPtr<SLevelViewport> LevelViewportWidget = StaticCastSharedPtr<SLevelViewport>(EditorViewportWidget);
		if (LevelViewportWidget.IsValid())
		{
			LevelViewportWidget->RemoveOverlayWidget(State.OverlayWidget.ToSharedRef());
		}
	}

	State.OverlayWidget.Reset();
}

bool ULoops2DPanZoomSubsystem::GetOverlayInfo(const FEditorViewportClient* ViewportClient, float& OutZoomPercent, FVector2D& OutCropSize, FVector2D& OutCropCenterOffset) const
{
	const FLoops2DPanZoomState* State = FindState(ViewportClient);
	if (!State || !State->bEnabled || !State->bHasBase || !ViewportClient)
	{
		return false;
	}

	OutZoomPercent = State->Zoom * 100.0f;

	const float CropFraction = FMath::Clamp(1.0f / FMath::Max(State->Zoom, 0.01f), 0.01f, 1.0f);
	OutCropSize = FVector2D(CropFraction, CropFraction);

	const float ReferenceScale = ViewportClient->IsPerspective()
		? FMath::Max(State->BaseFOV, 1.0f)
		: FMath::Max(State->BaseOrthoZoom, 1.0f);
	OutCropCenterOffset = FVector2D(
		FMath::Clamp(State->PanOffset.X / ReferenceScale, -0.5f, 0.5f),
		FMath::Clamp(-State->PanOffset.Y / ReferenceScale, -0.5f, 0.5f)
	);

	return true;
}

void ULoops2DPanZoomSubsystem::ToggleEnabled(FEditorViewportClient* ViewportClient)
{
	SetEnabled(ViewportClient, !IsEnabled(ViewportClient));
}

void ULoops2DPanZoomSubsystem::NotifyCameraCut(UObject* CameraObject)
{
	LastCameraCutObject = CameraObject;
}

void ULoops2DPanZoomSubsystem::TickFollowCameraCut(FEditorViewportClient* ViewportClient)
{
	const FLoops2DPanZoomState* ConstState = FindState(ViewportClient);
	if (!ConstState || !ConstState->bEnabled || !ViewportClient)
	{
		return;
	}

	UCameraComponent* DrivingCamera = Loops2DPanZoomSequencer::ResolveCameraComponent(LastCameraCutObject.Get());
	if (!DrivingCamera)
	{
		DrivingCamera = Loops2DPanZoomSequencer::GetDrivingCameraComponent(ViewportClient);
	}

	if (!DrivingCamera)
	{
		return;
	}

	FLoops2DPanZoomState& State = GetState(ViewportClient);

	FMinimalViewInfo ViewInfo;
	DrivingCamera->GetCameraView(0.0f, ViewInfo);
	State.BaseLocation = ViewInfo.Location;
	State.BaseRotation = ViewInfo.Rotation;
	State.BaseFOV = ViewInfo.FOV;
	State.bHasBase = true;

	ApplyToCamera(ViewportClient, State);
}

void ULoops2DPanZoomSubsystem::Pan(FEditorViewportClient* ViewportClient, const FVector2D& ScreenDelta, const FIntPoint& ViewportSize)
{
	if (!ViewportClient || ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		return;
	}

	FLoops2DPanZoomState& State = GetState(ViewportClient);
	CaptureBaseIfNeeded(ViewportClient, State);
	const float SafeZoom = FMath::Max(State.Zoom, 0.01f);

	if (ViewportClient->IsPerspective())
	{
		const float CurrentFOV = FMath::Clamp(State.BaseFOV / SafeZoom, Loops2DPanZoomLimits::MinFOV, Loops2DPanZoomLimits::MaxFOV);
		const float DegreesPerPixel = CurrentFOV / static_cast<float>(ViewportSize.X);
		State.PanOffset.X -= ScreenDelta.X * DegreesPerPixel;
		State.PanOffset.Y += ScreenDelta.Y * DegreesPerPixel;
	}
	else
	{
		const float OrthoWidth = FMath::Max(State.BaseOrthoZoom / SafeZoom, 1.0f);
		const float UnitsPerPixel = OrthoWidth / static_cast<float>(ViewportSize.X);
		State.PanOffset.X -= ScreenDelta.X * UnitsPerPixel;
		State.PanOffset.Y -= ScreenDelta.Y * UnitsPerPixel;
	}

	if (State.bEnabled)
	{
		ApplyToCamera(ViewportClient, State);
	}
}

void ULoops2DPanZoomSubsystem::Zoom(FEditorViewportClient* ViewportClient, float DeltaZoom)
{
	if (!ViewportClient)
	{
		return;
	}

	FLoops2DPanZoomState& State = GetState(ViewportClient);
	CaptureBaseIfNeeded(ViewportClient, State);

	constexpr float MinZoom = 0.05f;
	constexpr float MaxZoom = 200.0f;
	const float ZoomFactor = FMath::Exp(DeltaZoom);
	State.Zoom = FMath::Clamp(State.Zoom * ZoomFactor, MinZoom, MaxZoom);

	if (State.bEnabled)
	{
		ApplyToCamera(ViewportClient, State);
	}
}

void ULoops2DPanZoomSubsystem::ToggleZoomTo100Percent(FEditorViewportClient* ViewportClient)
{
	if (!ViewportClient)
	{
		return;
	}

	FLoops2DPanZoomState& State = GetState(ViewportClient);
	CaptureBaseIfNeeded(ViewportClient, State);

	constexpr float HundredPercentTolerance = 0.001f;
	if (FMath::IsNearlyEqual(State.Zoom, 1.0f, HundredPercentTolerance))
	{
		State.Zoom = State.PreToggleZoom;
		State.PanOffset = State.PreTogglePanOffset;
	}
	else
	{
		State.PreToggleZoom = State.Zoom;
		State.PreTogglePanOffset = State.PanOffset;
		State.Zoom = 1.0f;
		State.PanOffset = FVector2D::ZeroVector;
	}

	if (State.bEnabled)
	{
		ApplyToCamera(ViewportClient, State);
	}
}

void ULoops2DPanZoomSubsystem::Reset(FEditorViewportClient* ViewportClient)
{
	if (!ViewportClient){return;}

	FLoops2DPanZoomState& State = GetState(ViewportClient);
	State.PanOffset = FVector2D::ZeroVector;
	State.Zoom = 1.0f;

	if (State.bEnabled){ApplyToCamera(ViewportClient, State);}
}
