// Copyright 2026 Loops Creative Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Engine/DeveloperSettings.h"
#include "Loops2DPanZoomSettings.generated.h"

UCLASS(config = EditorPerProjectUserSettings, defaultconfig, meta = (DisplayName = "Loops 2D Pan/Zoom"))
class LOOPS2DPANZOOM_API ULoops2DPanZoomSettings : public UDeveloperSettings
{
	GENERATED_BODY()

	public:
		ULoops2DPanZoomSettings();

		UPROPERTY(EditAnywhere, config, Category = "Mouse Drag", meta = (DisplayName = "Pan Mouse Button"))
		FKey PanMouseButton;

		UPROPERTY(EditAnywhere, config, Category = "Mouse Drag", meta = (DisplayName = "Zoom Mouse Button"))
		FKey ZoomMouseButton;

		UPROPERTY(EditAnywhere, config, Category = "Mouse Drag", meta = (DisplayName = "Drag Modifier Key"))
		FKey DragModifierKey;

		UPROPERTY(EditAnywhere, config, Category = "Sensitivity", meta = (DisplayName = "Mouse Pan Sensitivity", ClampMin = "0.05", ClampMax = "10.0", UIMin = "0.1", UIMax = "3.0"))
		float MousePanSensitivity;

		UPROPERTY(EditAnywhere, config, Category = "Sensitivity", meta = (DisplayName = "Mouse Zoom Sensitivity", ClampMin = "0.05", ClampMax = "10.0", UIMin = "0.1", UIMax = "3.0"))
		float MouseZoomSensitivity;

		UPROPERTY(EditAnywhere, config, Category = "Sensitivity", meta = (DisplayName = "Keyboard Pan Step (pixels)", ClampMin = "1.0", ClampMax = "500.0", UIMin = "5.0", UIMax = "100.0"))
		float KeyboardPanStepPixels;

		UPROPERTY(EditAnywhere, config, Category = "Sensitivity", meta = (DisplayName = "Keyboard Zoom Step", ClampMin = "0.01", ClampMax = "1.0", UIMin = "0.02", UIMax = "0.5"))
		float KeyboardZoomStep;

		static bool IsSupportedModifierKey(const FKey& Key);

#if WITH_EDITOR
		virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
