// Copyright 2026 Loops Creative Studio. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Types/SlateEnums.h"

class ISequencer;
class FEditorViewportClient;
class FUICommandList;
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

		void BindCommands();

		void OnSequencerCreated(TSharedRef<ISequencer> InSequencer);
		void OnSequencerCameraCut(UObject* CameraObject, bool bJumpCut);

		TSharedPtr<class FLoops2DPanZoomInputProcessor> InputProcessor;
		TSharedPtr<FUICommandList> CommandList;
		FDelegateHandle SequencerCreatedHandle;

		TArray<TPair<TWeakPtr<ISequencer>, FDelegateHandle>> SequencerCameraCutHandles;
};
