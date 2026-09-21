// Copyright 2026 Loops Creative Studio. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateTypes.h"

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
		
		TSharedPtr<class FLoops2DPanZoomInputProcessor> InputProcessor;
};
