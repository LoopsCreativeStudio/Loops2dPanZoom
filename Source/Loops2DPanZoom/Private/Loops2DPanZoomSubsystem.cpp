// Copyright 2026 Loops Creative Studio. All Rights Reserved.

#include "Loops2DPanZoomSubsystem.h"
#include "Loops2DPanZoomOverlay.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "IAssetViewport.h"
#include "SLevelViewport.h"
#include "Editor.h"
#include "SceneView.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "EditorModeManager.h"
#include "ControlRig.h"
#include "Rigs/RigHierarchy.h"
#include "IControlRigObjectBinding.h"
#include "EditMode/ControlRigEditMode.h"

/*
 * Perspective pan/zoom is applied through FEditorViewportClient::ViewModifiers
 * when Unreal builds each FSceneView. Previously we wrote the viewport's FOV,
 * rotation and location directly, which competed with Sequencer updating those
 * same properties while Allow Cinematic Control was enabled.
 *
 * ModifyView adjusts the evaluated view parameters, leaving the perspective
 * viewport camera properties and camera actor/lens settings under Unreal's
 * control. CalcSceneView is also used for picking and gizmos, so those views
 * receive the same adjustment as rendering; a render-only effect would not.
 *
 * Each calculation starts from that viewport's current camera. Camera movement
 * and cuts therefore no longer need a global last-camera object, Sequencer event
 * subscriptions or a separate follow-camera refresh pass. Removing that manual
 * synchronization and perspective transform restoration accounts for most of
 * the deleted code. The modifier still needs explicit removal on disable and
 * viewport cleanup, and the cached controlling-view FOV needs a neutral refresh.
 *
 * Orthographic views retain their transform-based implementation. DOF is still
 * temporarily disabled and restored. Perspective pan remains rotational, not
 * an off-axis crop; other view modifiers (including camera blends) can conflict
 * through callback ordering.
 */
namespace Loops2DPanZoomLimits
{
	constexpr float MinZoom = 0.05f;
	constexpr float MaxZoom = 20000.0f;
}

FLoops2DPanZoomState& ULoops2DPanZoomSubsystem::GetState(FEditorViewportClient* ViewportClient)
{
	if (GEditor && !ViewportListChangedHandle.IsValid())
	{
		ViewportListChangedHandle = GEditor->OnViewportClientListChanged().AddUObject(this, &ULoops2DPanZoomSubsystem::RemoveClosedViewports);
	}
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

	State.BaseLocation = ViewportClient->GetViewLocation();
	State.BaseRotation = ViewportClient->GetViewRotation();
	State.BaseFOV = ViewportClient->ViewFOV;
	State.BaseOrthoZoom = ViewportClient->GetOrthoZoom();
	State.bHasBase = true;
}

void ULoops2DPanZoomSubsystem::ModifyView(FEditorViewportViewModifierParams& Params)
{
	FEditorViewportClient* ViewportClient = Params.ViewportClient;
	FLoops2DPanZoomState* State = ViewportStates.Find(ViewportClient);
	if (!State || !ViewportClient || !ViewportClient->IsPerspective())
	{
		return;
	}

	// CalcSceneView caches the modified FOV in ControllingActorViewInfo. Start
	// from ViewFOV on EVERY call so picking/repeated draws cannot compound zoom.
	// This property is maintained by the editor/Sequencer and is never written here.
	Params.ViewInfo.FOV = ViewportClient->ViewFOV;
	State->BaseLocation = Params.ViewInfo.Location;
	State->BaseRotation = Params.ViewInfo.Rotation;
	State->BaseFOV = Params.ViewInfo.FOV;
	if (!State->bEnabled && !State->bAnimControlLockEnabled)
	{
		return;
	}

	if (State->bAnimControlLockEnabled)
	{
		FTransform ControlTransform;
		if (GetSelectedControlWorldTransform(ControlTransform))
		{
			UpdateAnimControlLockPan(ViewportClient, *State, ControlTransform.GetLocation());
		}
	}

	const float HalfFOV = FMath::DegreesToRadians(FMath::Clamp(State->BaseFOV, 0.001f, 179.0f) * 0.5f);
	Params.ViewInfo.FOV = FMath::RadiansToDegrees(2.0f * FMath::Atan(FMath::Tan(HalfFOV) / FMath::Max(State->Zoom, 0.01f)));
	FRotator NewRotation = State->BaseRotation;
	NewRotation.Yaw = FRotator::NormalizeAxis(NewRotation.Yaw + State->PanOffset.X);
	NewRotation.Pitch = FMath::Clamp(NewRotation.Pitch + State->PanOffset.Y, -89.9f, 89.9f);
	Params.ViewInfo.Rotation = NewRotation;
}

void ULoops2DPanZoomSubsystem::ApplyToCamera(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State)
{
	if (!ViewportClient || !State.bHasBase)
	{
		return;
	}

	if (!ViewportClient->IsPerspective())
	{
		const float SafeZoom = FMath::Max(State.Zoom, 0.01f);
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

void ULoops2DPanZoomSubsystem::RestoreCamera(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State)
{
	if (!ViewportClient || !State.bHasBase || !State.ViewModifierHandle.IsValid())
	{
		return;
	}

	ViewportClient->EngineShowFlags.SetDepthOfField(State.bWasDepthOfFieldEnabled);

	if (!ViewportClient->IsPerspective())
	{
		ViewportClient->SetViewLocation(State.BaseLocation);
		ViewportClient->SetOrthoZoom(State.BaseOrthoZoom);
	}
	else if (ViewportClient->Viewport)
	{
		// Flush the controlling actor's cached FOV with a neutral modifier before
		// detaching it, including when playback/realtime is paused.
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			ViewportClient->Viewport, ViewportClient->GetScene(), ViewportClient->EngineShowFlags));
		ViewportClient->CalcSceneView(&ViewFamily);
	}
	ViewportClient->ViewModifiers.Remove(State.ViewModifierHandle);
	State.ViewModifierHandle.Reset();
	ViewportClient->Invalidate();
}

void ULoops2DPanZoomSubsystem::Deinitialize()
{
	if (GEditor)
	{
		GEditor->OnViewportClientListChanged().Remove(ViewportListChangedHandle);
		ViewportListChangedHandle.Reset();
		for (TPair<FEditorViewportClient*, FLoops2DPanZoomState>& Pair : ViewportStates)
		{
			if (GEditor->GetAllViewportClients().Contains(Pair.Key))
			{
				Pair.Value.bEnabled = false;
				Pair.Value.bAnimControlLockEnabled = false;
				RestoreCamera(Pair.Key, Pair.Value);
				RemoveOverlayIfNeeded(Pair.Key, Pair.Value);
			}
		}
	}
	ViewportStates.Empty();
	Super::Deinitialize();
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
		// Save DOF on each off-to-on transition even if a base already exists.
		// ApplyToCamera disables it; RestoreCamera restores this saved setting.
		State.bWasDepthOfFieldEnabled = ViewportClient->EngineShowFlags.DepthOfField;
		State.ViewModifierHandle = ViewportClient->ViewModifiers.AddUObject(this, &ULoops2DPanZoomSubsystem::ModifyView);

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

bool ULoops2DPanZoomSubsystem::GetOverlayInfo(const FEditorViewportClient* ViewportClient, float& OutZoomPercent, FVector2D& OutCropSize, FVector2D& OutCropCenterOffset, bool& OutIsAnimControlLockActive, FString& OutAnimControlLockControlName) const
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
	OutAnimControlLockControlName = State->AnimControlLockControlName;

	return true;
}

void ULoops2DPanZoomSubsystem::ToggleEnabled(FEditorViewportClient* ViewportClient)
{
	SetEnabled(ViewportClient, !IsEnabled(ViewportClient));
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
		// Perspective projection scale is proportional to 1/tan(FOV/2), so a
		// zoom factor Z gives effective FOV = 2*atan(tan(baseFOV/2)/Z).
		// Match ModifyView's zoom instead of the old baseFOV/Z approximation,
		// then use that angle for the existing degrees-per-pixel pan sensitivity.
		// Read live ViewFOV so animated lenses and camera cuts affect sensitivity.
		// This is angular pan sensitivity, not an exact screen-space translation.
		const float HalfFOV = FMath::DegreesToRadians(FMath::Clamp(ViewportClient->ViewFOV, 0.001f, 179.0f) * 0.5f);
		const float CurrentFOV = FMath::RadiansToDegrees(2.0f * FMath::Atan(FMath::Tan(HalfFOV) / SafeZoom));
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


// TODO : Extract ControlRig function to Loops2DPanZoomControlRig
bool ULoops2DPanZoomSubsystem::GetSelectedControlWorldTransform(FTransform& OutTransform, FName* OutControlName) const
{
	FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(
		GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));
	if (!ControlRigEditMode)
	{
		return false;
	}

	TMap<UControlRig*, TArray<FRigElementKey>> SelectedControls;
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
		// Nothing selected: the shortcut/restore is a no-op.
		return false;
	}

	SetEnabled(ViewportClient, true);
	State.bAnimControlLockEnabled = true;
	State.AnimControlLockControlName = ControlName.ToString();
	UpdateAnimControlLockPan(ViewportClient, State, ControlWorldTransform.GetLocation());
	RefreshOverlayPresence(ViewportClient, State);
	ApplyToCamera(ViewportClient, State);
	return true;
}

void ULoops2DPanZoomSubsystem::DisableAnimControlLock(FEditorViewportClient* ViewportClient, FLoops2DPanZoomState& State)
{
	// Unlock: leave the camera exactly where it currently sits.
	State.bAnimControlLockEnabled = false;
	State.AnimControlLockControlName.Empty();
	RefreshOverlayPresence(ViewportClient, State);
}

void ULoops2DPanZoomSubsystem::RemoveClosedViewports()
{
	// Viewport clients are not UObjects. Drop closed viewports before dereferencing
	// their keys (for example after changing the editor viewport layout).
	for (auto It = ViewportStates.CreateIterator(); It; ++It)
	{
		if (!GEditor || !GEditor->GetAllViewportClients().Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
}

void ULoops2DPanZoomSubsystem::TickAllAnimControlLocks()
{
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
		// Selection lost: freeze the camera(s) at their last known aim rather than moving them.
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

		State.AnimControlLockControlName = ControlName.ToString();
		UpdateAnimControlLockPan(ViewportClient, State, ControlWorldTransform.GetLocation());
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
