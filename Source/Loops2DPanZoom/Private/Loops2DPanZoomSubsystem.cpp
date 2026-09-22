// Copyright 2026 Loops Creative Studio. All Rights Reserved.

#include "Loops2DPanZoomSubsystem.h"
#include "Loops2DPanZoomOverlay.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "IAssetViewport.h"
#include "SLevelViewport.h"
#include "Editor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "EditorModeManager.h"
#include "ControlRig.h"
#include "Rigs/RigHierarchy.h"
#include "IControlRigObjectBinding.h"
#include "EditMode/ControlRigEditMode.h"

namespace Loops2DPanZoomLimits
{
	constexpr float MinFOV = 5.0f;
	constexpr float MaxFOV = 170.0f;
	constexpr float DollyDistancePerZoomDoubling = 400.0f;
	constexpr float MinZoom = 0.05f;
	constexpr float MaxZoom = 20000.0f;

	constexpr float AnimControlLockMoveTolerance = 0.01f;
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

	RebaseOnDrivingCameraChange(ViewportClient, State);

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

			NewLocation += Forward * (Loops2DPanZoomLimits::DollyDistancePerZoomDoubling * FMath::Log2(ExcessZoomRatio));
		}

		ViewportClient->SetViewRotation(NewRotation);
		ViewportClient->SetViewLocation(NewLocation);

		if (IsViewLockedToActor(ViewportClient))
		{
			FRotator ModifierRotation = State.BaseRotation;
			ModifierRotation.Yaw = FRotator::NormalizeAxis(ModifierRotation.Yaw + State.PanOffset.X);
			ModifierRotation.Pitch = FMath::Clamp(ModifierRotation.Pitch + State.PanOffset.Y, -89.9f, 89.9f);

			FVector ModifierLocation = State.BaseLocation;
			if (!FMath::IsNearlyEqual(SafeZoom, 1.0f, 0.0001f))
			{
				ModifierLocation += ModifierRotation.Vector() * (Loops2DPanZoomLimits::DollyDistancePerZoomDoubling * FMath::Log2(SafeZoom));
			}

			ApplyCinematicCameraModifier(ViewportClient, State, ModifierLocation, ModifierRotation);
		}
		else if (State.bCameraModifierActive)
		{
			RestoreCinematicCameraModifier(State);
		}
	}
	else
	{
		const FRotationMatrix RotMatrix(State.BaseRotation);
		const FVector Right = RotMatrix.GetScaledAxis(EAxis::Y);
		const FVector Up = RotMatrix.GetScaledAxis(EAxis::Z);
		const FVector NewLocation = State.BaseLocation + Right * State.PanOffset.X + Up * State.PanOffset.Y;
		ViewportClient->SetViewLocation(NewLocation);
		ViewportClient->SetOrthoZoom(State.BaseOrthoZoom / SafeZoom);

		if (State.bCameraModifierActive)
		{
			RestoreCinematicCameraModifier(State);
		}
	}
	ViewportClient->EngineShowFlags.SetDepthOfField(false);
	ViewportClient->Invalidate();
}

bool ULoops2DPanZoomSubsystem::IsViewLockedToActor(const FEditorViewportClient* ViewportClient) const
{
	if (!ViewportClient || !ViewportClient->IsLevelEditorClient())
	{
		return false;
	}
	const FLevelEditorViewportClient* LevelViewportClient = static_cast<const FLevelEditorViewportClient*>(ViewportClient);

	const bool bCinematicLocked = ViewportClient->AllowsCinematicControl() && LevelViewportClient->IsLockedToCinematic();
	const bool bAnyActorLocked = LevelViewportClient->IsAnyActorLocked();

	return bCinematicLocked || bAnyActorLocked;
}

void ULoops2DPanZoomSubsystem::ApplyCinematicCameraModifier(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State, const FVector& NewLocation, const FRotator& NewRotation)
{
	UCameraComponent* DrivingCamera = Loops2DPanZoomSequencer::ResolveCameraComponent(LastCameraCutObject.Get());
	if (!DrivingCamera)
	{
		DrivingCamera = Loops2DPanZoomSequencer::GetDrivingCameraComponent(ViewportClient);
	}

	if (!DrivingCamera)
	{
		return;
	}

	if (State.bCameraModifierActive && State.ModifiedCameraComponent.Get() != DrivingCamera)
	{
		RestoreCinematicCameraModifier(State);
	}

	if (!State.bCameraModifierActive)
	{
		State.BaseRelativeLocation = DrivingCamera->GetRelativeLocation();
		State.BaseRelativeRotation = DrivingCamera->GetRelativeRotation();
		State.bHasBaseRelativeTransform = true;
	}

	State.ModifiedCameraComponent = DrivingCamera;
	State.bCameraModifierActive = true;

	DrivingCamera->SetWorldLocationAndRotation(NewLocation, NewRotation);
}

void ULoops2DPanZoomSubsystem::RestoreCinematicCameraModifier(FLoops2DPanZoomState& State)
{
	if (State.bCameraModifierActive)
	{
		if (UCameraComponent* Camera = State.ModifiedCameraComponent.Get())
		{
			if (State.bHasBaseRelativeTransform)
			{
				Camera->SetRelativeLocationAndRotation(State.BaseRelativeLocation, State.BaseRelativeRotation);
			}
		}
	}
	State.bCameraModifierActive = false;
	State.ModifiedCameraComponent = nullptr;
	State.bHasBaseRelativeTransform = false;
}

void ULoops2DPanZoomSubsystem::RestoreCamera(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State)
{
	if (!ViewportClient || !State.bHasBase)
	{
		return;
	}

	if (State.bCameraModifierActive)
	{
		RestoreCinematicCameraModifier(State);
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
		CaptureBaseIfNeeded(ViewportClient, State);

		RefreshOverlayPresence(ViewportClient, State);
		ApplyToCamera(ViewportClient, State);
	}
	else
	{
		if (State.bAnimControlLockEnabled)
		{
			DisableAnimControlLock(ViewportClient, State);
		}

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

void ULoops2DPanZoomSubsystem::RefreshOverlayPresence(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State)
{
	if (State.bEnabled || State.bAnimControlLockEnabled)
	{
		AddOverlayIfNeeded(ViewportClient, State);
	}
	else
	{
		RemoveOverlayIfNeeded(ViewportClient, State);
	}
}

bool ULoops2DPanZoomSubsystem::GetOverlayInfo(const FEditorViewportClient* ViewportClient, float& OutZoomPercent, FVector2D& OutCropSize, FVector2D& OutCropCenterOffset, bool& OutIsAnimControlLockActive) const
{
	const FLoops2DPanZoomState* State = FindState(ViewportClient);
	if (!State || !State->bHasBase || !ViewportClient || (!State->bEnabled && !State->bAnimControlLockEnabled))
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

	OutIsAnimControlLockActive = State->bAnimControlLockEnabled;

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

bool ULoops2DPanZoomSubsystem::RefreshBaseFromDrivingCameraIfChanged(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State)
{
	if (!ViewportClient)
	{
		return false;
	}

	UCameraComponent* DrivingCamera = Loops2DPanZoomSequencer::ResolveCameraComponent(LastCameraCutObject.Get());
	if (!DrivingCamera)
	{
		DrivingCamera = Loops2DPanZoomSequencer::GetDrivingCameraComponent(ViewportClient);
	}

	if (!DrivingCamera)
	{
		return false;
	}

	if (State.bCameraModifierActive)
	{
		return false;
	}

	FMinimalViewInfo ViewInfo;
	DrivingCamera->GetCameraView(0.0f, ViewInfo);

	constexpr float ExternalChangeLocationTolerance = 2.0f;
	constexpr float ExternalChangeRotationToleranceDegrees = 0.1f;
	constexpr float ExternalChangeFOVTolerance = 0.05f;

	if (State.bHasBase
		&& State.BaseLocation.Equals(ViewInfo.Location, ExternalChangeLocationTolerance)
		&& State.BaseRotation.Equals(ViewInfo.Rotation, ExternalChangeRotationToleranceDegrees)
		&& FMath::IsNearlyEqual(State.BaseFOV, ViewInfo.FOV, ExternalChangeFOVTolerance))
	{
		return false;
	}

	State.BaseLocation = ViewInfo.Location;
	State.BaseRotation = ViewInfo.Rotation;
	State.BaseFOV = ViewInfo.FOV;
	State.bHasBase = true;
	return true;
}

bool ULoops2DPanZoomSubsystem::RebaseOnDrivingCameraChange(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State)
{
	if (!ViewportClient || !State.bCameraModifierActive)
	{
		return false;
	}

	UCameraComponent* DrivingCamera = Loops2DPanZoomSequencer::ResolveCameraComponent(LastCameraCutObject.Get());
	if (!DrivingCamera)
	{
		DrivingCamera = Loops2DPanZoomSequencer::GetDrivingCameraComponent(ViewportClient);
	}

	if (!DrivingCamera || State.ModifiedCameraComponent.Get() == DrivingCamera)
	{
		return false;
	}

	RestoreCinematicCameraModifier(State);

	FMinimalViewInfo ViewInfo;
	DrivingCamera->GetCameraView(0.0f, ViewInfo);

	State.BaseLocation = ViewInfo.Location;
	State.BaseRotation = ViewInfo.Rotation;
	State.BaseFOV = ViewInfo.FOV;
	State.bHasBase = true;
	return true;
}

void ULoops2DPanZoomSubsystem::TickFollowCameraCut(FEditorViewportClient* ViewportClient)
{
	const FLoops2DPanZoomState* ConstState = FindState(ViewportClient);
	if (!ConstState || !ConstState->bEnabled || !ViewportClient)
	{
		return;
	}

	if (ConstState->bAnimControlLockEnabled)
	{
		return;
	}

	FLoops2DPanZoomState& State = GetState(ViewportClient);

	if (State.bCameraModifierActive && !IsViewLockedToActor(ViewportClient))
	{
		RestoreCinematicCameraModifier(State);
	}

	const bool bRebased = RebaseOnDrivingCameraChange(ViewportClient, State);
	const bool bRefreshed = RefreshBaseFromDrivingCameraIfChanged(ViewportClient, State);
	if (bRebased || bRefreshed)
	{
		ApplyToCamera(ViewportClient, State);
	}
}

void ULoops2DPanZoomSubsystem::PruneStaleViewportStates()
{
	if (!GEditor)
	{
		return;
	}

	if (LastPruneFrameCounter == GFrameCounter)
	{
		return;
	}
	LastPruneFrameCounter = GFrameCounter;

	const TArray<FEditorViewportClient*>& LiveClients = GEditor->GetAllViewportClients();

	for (auto It = ViewportStates.CreateIterator(); It; ++It)
	{
		if (!LiveClients.Contains(It->Key))
		{
			if (It->Value.bCameraModifierActive)
			{
				RestoreCinematicCameraModifier(It->Value);
			}
			It.RemoveCurrent();
		}
	}
}

void ULoops2DPanZoomSubsystem::TickAllFollowCameraCuts()
{
	if (bSuspendedForPlayInEditor)
	{
		return;
	}

	PruneStaleViewportStates();

	for (const TPair<FEditorViewportClient*, FLoops2DPanZoomState>& Pair : ViewportStates)
	{
		if (FEditorViewportClient* ViewportClient = Pair.Key)
		{
			TickFollowCameraCut(ViewportClient);
		}
	}
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

	const float ZoomFactor = FMath::Exp(DeltaZoom);
	State.Zoom = FMath::Clamp(State.Zoom * ZoomFactor, Loops2DPanZoomLimits::MinZoom, Loops2DPanZoomLimits::MaxZoom);

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

		if (State.bPreToggleAnimControlLockEnabled && !State.bAnimControlLockEnabled)
		{
			EnableAnimControlLock(ViewportClient, State);
		}
		State.bPreToggleAnimControlLockEnabled = false;
	}
	else
	{
		State.PreToggleZoom = State.Zoom;
		State.PreTogglePanOffset = State.PanOffset;
		State.Zoom = 1.0f;
		State.PanOffset = FVector2D::ZeroVector;

		State.bPreToggleAnimControlLockEnabled = State.bAnimControlLockEnabled;
		if (State.bAnimControlLockEnabled)
		{
			DisableAnimControlLock(ViewportClient, State);
		}
	}

	if (State.bEnabled)
	{
		ApplyToCamera(ViewportClient, State);
	}
}

bool ULoops2DPanZoomSubsystem::GetSelectedControlWorldTransform(FTransform& OutTransform, FName* OutControlName) const
{
	FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(
		GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));
	if (!ControlRigEditMode)
	{
		return false;
	}

	static TMap<UControlRig*, TArray<FRigElementKey>> SelectedControls;
	SelectedControls.Reset();
	ControlRigEditMode->GetAllSelectedControls(SelectedControls);

	for (const TPair<UControlRig*, TArray<FRigElementKey>>& Pair : SelectedControls)
	{
		UControlRig* ControlRig = Pair.Key;
		if (!ControlRig || Pair.Value.Num() == 0)
		{
			continue;
		}

		URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
		if (!Hierarchy)
		{
			continue;
		}

		FTransform ControlTransform = Hierarchy->GetGlobalTransform(Pair.Value[0]);

		if (TSharedPtr<IControlRigObjectBinding> Binding = ControlRig->GetObjectBinding())
		{
			if (USceneComponent* BoundComponent = Cast<USceneComponent>(Binding->GetBoundObject()))
			{
				ControlTransform = ControlTransform * BoundComponent->GetComponentTransform();
			}
		}

		OutTransform = ControlTransform;
		if (OutControlName)
		{
			*OutControlName = Pair.Value[0].Name;
		}
		return true;
	}

	return false;
}

bool ULoops2DPanZoomSubsystem::IsAnimControlLockEnabled(const FEditorViewportClient* ViewportClient) const
{
	const FLoops2DPanZoomState* State = FindState(ViewportClient);
	return State && State->bAnimControlLockEnabled;
}

void ULoops2DPanZoomSubsystem::UpdateAnimControlLockPan(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State, const FVector& ControlWorldLocation)
{
	if (!ViewportClient || !State.bHasBase)
	{
		return;
	}

	const FVector ToControl = ControlWorldLocation - State.BaseLocation;
	if (ToControl.IsNearlyZero())
	{
		return;
	}

	const FRotator AimRotation = ToControl.Rotation();
	State.PanOffset.X = FRotator::NormalizeAxis(AimRotation.Yaw - State.BaseRotation.Yaw);
	State.PanOffset.Y = AimRotation.Pitch - State.BaseRotation.Pitch;
}

void ULoops2DPanZoomSubsystem::ToggleAnimControlLock(FEditorViewportClient* ViewportClient)
{
	if (!ViewportClient)
	{
		return;
	}

	FLoops2DPanZoomState& State = GetState(ViewportClient);

	if (State.bAnimControlLockEnabled)
	{
		DisableAnimControlLock(ViewportClient, State);
	}
	else
	{
		EnableAnimControlLock(ViewportClient, State);
	}
}

bool ULoops2DPanZoomSubsystem::EnableAnimControlLock(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State)
{
	if (!ViewportClient)
	{
		return false;
	}

	FTransform ControlWorldTransform;
	FName ControlName;
	if (!GetSelectedControlWorldTransform(ControlWorldTransform, &ControlName))
	{
		return false;
	}

	CaptureBaseIfNeeded(ViewportClient, State);
	State.bAnimControlLockEnabled = true;
	State.LastAnimControlLockControlName = ControlName;
	State.LastAnimControlLockLocation = ControlWorldTransform.GetLocation();
	State.bHasLastAnimControlLockLocation = true;
	UpdateAnimControlLockPan(ViewportClient, State, ControlWorldTransform.GetLocation());
	RefreshOverlayPresence(ViewportClient, State);
	ApplyToCamera(ViewportClient, State);
	return true;
}

void ULoops2DPanZoomSubsystem::DisableAnimControlLock(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State)
{
	State.bAnimControlLockEnabled = false;
	State.LastAnimControlLockControlName = NAME_None;
	State.bHasLastAnimControlLockLocation = false;
	RefreshOverlayPresence(ViewportClient, State);
}

void ULoops2DPanZoomSubsystem::TickAllAnimControlLocks()
{
	if (bSuspendedForPlayInEditor)
	{
		return;
	}

	PruneStaleViewportStates();

	bool bAnyLockActive = false;
	for (const TPair<FEditorViewportClient*, FLoops2DPanZoomState>& Pair : ViewportStates)
	{
		if (Pair.Value.bAnimControlLockEnabled)
		{
			bAnyLockActive = true;
			break;
		}
	}
	if (!bAnyLockActive)
	{
		return;
	}

	FTransform ControlWorldTransform;
	FName ControlName;
	if (!GetSelectedControlWorldTransform(ControlWorldTransform, &ControlName))
	{
		return;
	}

	for (TPair<FEditorViewportClient*, FLoops2DPanZoomState>& Pair : ViewportStates)
	{
		FEditorViewportClient* ViewportClient = Pair.Key;
		FLoops2DPanZoomState& State = Pair.Value;
		if (!ViewportClient || !State.bAnimControlLockEnabled)
		{
			continue;
		}

		const bool bRebased = RebaseOnDrivingCameraChange(ViewportClient, State);

		const FVector ControlLocation = ControlWorldTransform.GetLocation();
		const bool bControlChanged = !State.bHasLastAnimControlLockLocation
			|| State.LastAnimControlLockControlName != ControlName
			|| !State.LastAnimControlLockLocation.Equals(ControlLocation, Loops2DPanZoomLimits::AnimControlLockMoveTolerance);

		if (!bRebased && !bControlChanged)
		{
			continue;
		}

		State.LastAnimControlLockControlName = ControlName;
		State.LastAnimControlLockLocation = ControlLocation;
		State.bHasLastAnimControlLockLocation = true;
		UpdateAnimControlLockPan(ViewportClient, State, ControlLocation);
		ApplyToCamera(ViewportClient, State);
	}
}

void ULoops2DPanZoomSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	BeginPlayInEditorHandle = FEditorDelegates::BeginPIE.AddUObject(this, &ULoops2DPanZoomSubsystem::OnBeginPlayInEditor);
	EndPlayInEditorHandle = FEditorDelegates::EndPIE.AddUObject(this, &ULoops2DPanZoomSubsystem::OnEndPlayInEditor);
}

void ULoops2DPanZoomSubsystem::OnBeginPlayInEditor(const bool bIsSimulating)
{
	SuspendForPlayInEditor();
}

void ULoops2DPanZoomSubsystem::OnEndPlayInEditor(const bool bIsSimulating)
{
	bSuspendedForPlayInEditor = false;
}

void ULoops2DPanZoomSubsystem::SuspendForPlayInEditor()
{
	if (bSuspendedForPlayInEditor)
	{
		return;
	}
	bSuspendedForPlayInEditor = true;

	PruneStaleViewportStates();

	TArray<FEditorViewportClient*> ToSwitchOff;
	for (const TPair<FEditorViewportClient*, FLoops2DPanZoomState>& Pair : ViewportStates)
	{
		if (Pair.Key && (Pair.Value.bEnabled || Pair.Value.bAnimControlLockEnabled))
		{
			ToSwitchOff.Add(Pair.Key);
		}
	}

	for (FEditorViewportClient* ViewportClient : ToSwitchOff)
	{
		FLoops2DPanZoomState* State = ViewportStates.Find(ViewportClient);
		if (!State)
		{
			continue;
		}

		if (State->bEnabled)
		{
			SetEnabled(ViewportClient, false);
		}
		else if (State->bAnimControlLockEnabled)
		{
			DisableAnimControlLock(ViewportClient, *State);
		}
	}
}

void ULoops2DPanZoomSubsystem::Deinitialize()
{
	FEditorDelegates::BeginPIE.Remove(BeginPlayInEditorHandle);
	FEditorDelegates::EndPIE.Remove(EndPlayInEditorHandle);

	for (TPair<FEditorViewportClient*, FLoops2DPanZoomState>& Pair : ViewportStates)
	{
		if (Pair.Value.bCameraModifierActive)
		{
			RestoreCinematicCameraModifier(Pair.Value);
		}
	}

	Super::Deinitialize();
}
