// Copyright 2026 Loops Creative Studio. All Rights Reserved.

#include "Loops2DPanZoomStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "Brushes/SlateImageBrush.h"

TSharedPtr<FSlateStyleSet> FLoops2DPanZoomStyle::StyleInstance = nullptr;

void FLoops2DPanZoomStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FLoops2DPanZoomStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		StyleInstance.Reset();
	}
}

FName FLoops2DPanZoomStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("Loops2DPanZoomStyle"));
	return StyleSetName;
}

const ISlateStyle& FLoops2DPanZoomStyle::Get()
{
	return *StyleInstance;
}

TSharedRef<FSlateStyleSet> FLoops2DPanZoomStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet(GetStyleSetName()));

	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Loops2DPanZoom")))
	{
		Style->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
	}

	const FVector2D IconSize(40.0f, 40.0f);
	Style->Set("Loops2DPanZoom.Icon", new FSlateImageBrush(Style->RootToContentDir(TEXT("Icons/Loops2DPanZoom"), TEXT(".png")), IconSize));

	return Style;
}
