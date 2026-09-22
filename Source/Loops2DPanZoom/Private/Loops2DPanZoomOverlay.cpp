// Copyright 2026 Loops Creative Studio. All Rights Reserved.

#include "Loops2DPanZoomOverlay.h"
#include "Loops2DPanZoomSubsystem.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "Editor.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "Loops2DPanZoomOverlay"

namespace Loops2DPanZoomOverlayLayout
{
	constexpr float FrameWidth = 140.0f;
	constexpr float FrameHeight = 80.0f;
	constexpr float Margin = 12.0f;
	constexpr float TextHeight = 20.0f;
}

void SLoops2DPanZoomOverlay::Construct(const FArguments& InArgs, FEditorViewportClient* InViewportClient)
{
	ViewportClient = InViewportClient;

	SetVisibility(EVisibility::HitTestInvisible);
}

FVector2D SLoops2DPanZoomOverlay::ComputeDesiredSize(float) const
{
	return FVector2D(Loops2DPanZoomOverlayLayout::FrameWidth + Loops2DPanZoomOverlayLayout::Margin * 2.0f,
		Loops2DPanZoomOverlayLayout::FrameHeight + Loops2DPanZoomOverlayLayout::Margin * 2.0f + Loops2DPanZoomOverlayLayout::TextHeight * 2.0f);
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
	bool bIsAnimControlLockActive = false;
	if (!Subsystem || !Subsystem->GetOverlayInfo(ViewportClient, ZoomPercent, CropSize, CropCenterOffset, bIsAnimControlLockActive))
	{
		return LayerId;
	}

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const float FrameLeft = Loops2DPanZoomOverlayLayout::Margin;
	const float FrameTop = Loops2DPanZoomOverlayLayout::Margin + Loops2DPanZoomOverlayLayout::TextHeight;

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(FVector2D(Loops2DPanZoomOverlayLayout::FrameWidth,
			Loops2DPanZoomOverlayLayout::FrameHeight), FSlateLayoutTransform(FVector2D(FrameLeft, FrameTop))),
		FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor(0.0f, 0.0f, 0.0f, 0.35f)
	);

	{
		const TArray<FVector2D> OuterBorderPoints = {
			FVector2D(FrameLeft, FrameTop),
			FVector2D(FrameLeft + Loops2DPanZoomOverlayLayout::FrameWidth, FrameTop),
			FVector2D(FrameLeft + Loops2DPanZoomOverlayLayout::FrameWidth, FrameTop + Loops2DPanZoomOverlayLayout::FrameHeight),
			FVector2D(FrameLeft, FrameTop + Loops2DPanZoomOverlayLayout::FrameHeight),
			FVector2D(FrameLeft, FrameTop)
		};

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			OuterBorderPoints,
			ESlateDrawEffect::None,
			FLinearColor(1.0f, 1.0f, 1.0f, 0.7f),
			true,
			1.5f
		);
	}

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

	const FLinearColor FrameColor = bIsAnimControlLockActive
		? FLinearColor(1.0f, 0.85f, 0.0f, 1.0f)
		: FLinearColor(33.0f / 255.0f, 166.0f / 255.0f, 227.0f / 255.0f, 1.0f);

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(),
		Points,
		ESlateDrawEffect::None,
		FrameColor,
		true,
		2.0f
	);

	FNumberFormattingOptions ZoomNumberFormat;
	ZoomNumberFormat.SetMaximumFractionalDigits(0);
	const FString ZoomText = FText::Format(
		LOCTEXT("OverlayZoomLabel", "Loops2DPanZoom: {0}%"),
		FText::AsNumber(ZoomPercent, &ZoomNumberFormat)).ToString();
	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId + 2,
		AllottedGeometry.ToPaintGeometry(FVector2D(Loops2DPanZoomOverlayLayout::FrameWidth, Loops2DPanZoomOverlayLayout::TextHeight), FSlateLayoutTransform(FVector2D(FrameLeft, FrameTop - Loops2DPanZoomOverlayLayout::TextHeight))),
		ZoomText,
		FCoreStyle::GetDefaultFontStyle("Bold", 8),
		ESlateDrawEffect::None,
		FLinearColor::White
	);

	if (bIsAnimControlLockActive)
	{
		const FString LockText = LOCTEXT("OverlayAutoFocus", "Auto Focus on Control").ToString();

		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId + 2,
			AllottedGeometry.ToPaintGeometry(FVector2D(Loops2DPanZoomOverlayLayout::FrameWidth, Loops2DPanZoomOverlayLayout::TextHeight), FSlateLayoutTransform(FVector2D(FrameLeft, FrameTop + Loops2DPanZoomOverlayLayout::FrameHeight))),
			LockText,
			FCoreStyle::GetDefaultFontStyle("Bold", 8),
			ESlateDrawEffect::None,
			FLinearColor(1.0f, 0.85f, 0.0f, 1.0f)
		);
	}

	return LayerId + 3;
}

#undef LOCTEXT_NAMESPACE
