// Copyright 2026 Loops Creative Studio. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Types/SlateEnums.h"

class ISequencer;
class FEditorViewportClient;
struct FToolMenuContext;

class FLoops2DPanZoomModule : public IModuleInterface
{
	public:
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;

	private:
		void RegisterToolbarExtension();
		void UnregisterToolbarExtension();
		void OnToggleClicked(const FToolMenuContext& InContext);
		ECheckBoxState GetToggleCheckState(const FToolMenuContext& InContext) const;
		FText GetToggleTooltipText() const;
		
		// Event Sequencer
		void OnSequencerCreated(TSharedRef<ISequencer> InSequencer);
		void OnSequencerCameraCut(UObject* CameraObject, bool bJumpCut);
		void OnSequencerGlobalTimeChanged();
		void RefreshFollowCameraCutForAllViewports();
		void ProcessPendingFollowCameraCutRefresh();
		
		bool bFollowCameraCutRefreshPending = false;
		FDelegateHandle EndFrameDelegateHandle;

		TSharedPtr<class FLoops2DPanZoomInputProcessor> InputProcessor;
		FDelegateHandle SequencerCreatedHandle;
};
