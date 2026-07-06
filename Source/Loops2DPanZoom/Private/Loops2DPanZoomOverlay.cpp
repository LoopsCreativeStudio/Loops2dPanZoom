#include "Loops2DPanZoomOverlay.h"
#include "Loops2DPanZoomSubsystem.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "Editor.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace Loops2DPanZoomOverlayLayout
{
	//Default Layout
	constexpr float FrameWidth = 140.0f;
	constexpr float FrameHeight = 80.0f;
	constexpr float Margin = 12.0f;
	constexpr float TextHeight = 20.0f;
}

void SLoops2DPanZoomOverlay::Construct(const FArguments& InArgs, FEditorViewportClient* InViewportClient)
{
	ViewportClient = InViewportClient;
}

FVector2D SLoops2DPanZoomOverlay::ComputeDesiredSize(float) const
{
	return FVector2D(Loops2DPanZoomOverlayLayout::FrameWidth + Loops2DPanZoomOverlayLayout::Margin * 2.0f,
		Loops2DPanZoomOverlayLayout::FrameHeight + Loops2DPanZoomOverlayLayout::Margin * 2.0f + Loops2DPanZoomOverlayLayout::TextHeight);
}

int32 SLoops2DPanZoomOverlay::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (!ViewportClient || !GEditor)
	{
		return LayerId;
	}

	ULoops2DPanZoomSubsystem* Subsystem = GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>();

	float ZoomPercent = 100.0f;
	FVector2D CropSize = FVector2D(1.0f, 1.0f);
	FVector2D CropCenterOffset = FVector2D::ZeroVector;
	if (!Subsystem || !Subsystem->GetOverlayInfo(ViewportClient, ZoomPercent, CropSize, CropCenterOffset))
	{
		return LayerId;
	}

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const float FrameLeft = Loops2DPanZoomOverlayLayout::Margin;
	const float FrameTop = Loops2DPanZoomOverlayLayout::Margin + Loops2DPanZoomOverlayLayout::TextHeight;

	// Outer reference frame: represents the original (un-zoomed) framing.
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(FVector2D(Loops2DPanZoomOverlayLayout::FrameWidth, 
			Loops2DPanZoomOverlayLayout::FrameHeight), FSlateLayoutTransform(FVector2D(FrameLeft, FrameTop))),
		FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor(0.0f, 0.0f, 0.0f, 0.35f)
	);

	bool bShowCinematicWarning = false;
	if (ViewportClient->AllowsCinematicControl() && ViewportClient->IsLevelEditorClient())
	{
		FLevelEditorViewportClient* LevelViewportClient = static_cast<FLevelEditorViewportClient*>(ViewportClient);
		bShowCinematicWarning = LevelViewportClient->IsLockedToCinematic();
	}

	if (bShowCinematicWarning)
	{
		static const TArray<FString> WarningLines = {
			TEXT("Pan/Zoom blocked by camera cut"),
			TEXT("Disable Allow Cinematic Control")
		};

		const FVector2D WarningLineSize(Loops2DPanZoomOverlayLayout::FrameWidth, Loops2DPanZoomOverlayLayout::TextHeight);
		const float WarningTop = FrameTop;
		const FLinearColor WarningColor(1.0f, 0.65f, 0.0f, 1.0f);

		for (int32 LineIndex = 0; LineIndex < WarningLines.Num(); ++LineIndex)
		{
			FSlateDrawElement::MakeText(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(WarningLineSize, FSlateLayoutTransform(FVector2D(FrameLeft, WarningTop + Loops2DPanZoomOverlayLayout::TextHeight * LineIndex))),
				WarningLines[LineIndex],
				FCoreStyle::GetDefaultFontStyle("Bold", 8),
				ESlateDrawEffect::None,
				WarningColor
			);
		}
	}
	else
	{
		const FVector2D InnerSize(Loops2DPanZoomOverlayLayout::FrameWidth * CropSize.X, Loops2DPanZoomOverlayLayout::FrameHeight * CropSize.Y);
		const FVector2D InnerCenter(
			FrameLeft + Loops2DPanZoomOverlayLayout::FrameWidth * (0.5f + CropCenterOffset.X),
			FrameTop + Loops2DPanZoomOverlayLayout::FrameHeight * (0.5f + CropCenterOffset.Y)
		);
		const FVector2D InnerTopLeft = InnerCenter - InnerSize * 0.5f;

		TArray<FVector2D> Points;
		Points.Add(InnerTopLeft);
		Points.Add(InnerTopLeft + FVector2D(InnerSize.X, 0.0f));
		Points.Add(InnerTopLeft + InnerSize);
		Points.Add(InnerTopLeft + FVector2D(0.0f, InnerSize.Y));
		Points.Add(InnerTopLeft);

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			FLinearColor(33.0f / 255.0f, 166.0f / 255.0f, 227.0f / 255.0f, 1.0f),
			true,
			2.0f
		);
	}
	const FString ZoomText = FString::Printf(TEXT("Loops2DPanZoom: %.0f%%"), ZoomPercent);
	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId + 2,
		AllottedGeometry.ToPaintGeometry(FVector2D(Loops2DPanZoomOverlayLayout::FrameWidth, Loops2DPanZoomOverlayLayout::TextHeight), FSlateLayoutTransform(FVector2D(FrameLeft, FrameTop - Loops2DPanZoomOverlayLayout::TextHeight))),
		ZoomText,
		FCoreStyle::GetDefaultFontStyle("Bold", 8),
		ESlateDrawEffect::None,
		FLinearColor::White
	);
	return LayerId + 3;
}
