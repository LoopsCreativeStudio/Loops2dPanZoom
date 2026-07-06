#pragma once
#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class FEditorViewportClient;

class SLoops2DPanZoomOverlay : public SLeafWidget
{
	public:
		SLATE_BEGIN_ARGS(SLoops2DPanZoomOverlay) {}
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs, FEditorViewportClient* InViewportClient);
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
		virtual FVector2D ComputeDesiredSize(float) const override;

	private:
		FEditorViewportClient* ViewportClient = nullptr;
};
