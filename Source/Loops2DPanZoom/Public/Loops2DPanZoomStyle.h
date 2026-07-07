// Copyright 2026 Loops Creative Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

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
