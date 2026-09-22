// Copyright 2026 Loops Creative Studio. All Rights Reserved.

#include "Loops2DPanZoomSettings.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
DEFINE_LOG_CATEGORY_STATIC(LogLoops2DPanZoomSettings, Log, All);
#endif

ULoops2DPanZoomSettings::ULoops2DPanZoomSettings()
	: PanMouseButton(EKeys::MiddleMouseButton)
	, ZoomMouseButton(EKeys::RightMouseButton)
	, DragModifierKey(EKeys::LeftAlt)
	, MousePanSensitivity(1.0f)
	, MouseZoomSensitivity(1.0f)
	, KeyboardPanStepPixels(20.0f)
	, KeyboardZoomStep(0.12f)
{
	CategoryName = TEXT("Plugins");
}

bool ULoops2DPanZoomSettings::IsSupportedModifierKey(const FKey& Key)
{
	return Key == EKeys::LeftAlt || Key == EKeys::RightAlt
		|| Key == EKeys::LeftControl || Key == EKeys::RightControl
		|| Key == EKeys::LeftShift || Key == EKeys::RightShift
		|| Key == EKeys::LeftCommand || Key == EKeys::RightCommand;
}

#if WITH_EDITOR
void ULoops2DPanZoomSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName ChangedProperty = PropertyChangedEvent.GetPropertyName();
	if (ChangedProperty == GET_MEMBER_NAME_CHECKED(ULoops2DPanZoomSettings, DragModifierKey) && !IsSupportedModifierKey(DragModifierKey))
	{
		UE_LOG(LogLoops2DPanZoomSettings, Warning,
			TEXT("Loops 2D Pan/Zoom: Drag Modifier Key is set to '%s', which isn't one of the four supported modifiers (Alt / Ctrl / Shift / Cmd, either side). Mouse-drag Pan/Zoom won't trigger until this is changed back to one of those."),
			*DragModifierKey.ToString());
	}
}
#endif
