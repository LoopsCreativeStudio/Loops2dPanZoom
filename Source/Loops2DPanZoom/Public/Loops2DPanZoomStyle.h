#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/** Registers the plugin's own Slate style set so custom toolbar icons can be loaded from Resources/Icons. */
class FLoops2DPanZoomStyle
{
	public:
		static void Initialize();
		static void Shutdown();
		static FName GetStyleSetName();
		static const ISlateStyle& Get();

	private:
		static TSharedRef<class FSlateStyleSet> Create();
		static TSharedPtr<class FSlateStyleSet> StyleInstance;
};
