// Copyright 2026 Loops Creative Studio. All Rights Reserved.

#include "Loops2DPanZoomModule.h"
#include "Loops2DPanZoomInputProcessor.h"
#include "Loops2DPanZoomSubsystem.h"
#include "Loops2DPanZoomCommands.h"
#include "Loops2DPanZoomSettings.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "Editor.h"
#include "ToolMenus.h"
#include "Loops2DPanZoomStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "ISequencerModule.h"
#include "ISequencer.h"
#include "Modules/ModuleManager.h"
#include "ToolMenuContext.h"
#include "ToolMenuDelegates.h"
#include "ViewportToolbar/UnrealEdViewportToolbarContext.h"
#include "SEditorViewport.h"

#define LOCTEXT_NAMESPACE "FLoops2DPanZoomModule"

namespace Loops2DPanZoom
{
	static const FName ToolbarMenuName("LevelEditor.ViewportToolbar");
	static const FName ToolbarSectionName("Loops2DPanZoom");

	static float GetKeyPanStepPixels()
	{
		const ULoops2DPanZoomSettings* Settings = GetDefault<ULoops2DPanZoomSettings>();
		return Settings ? Settings->KeyboardPanStepPixels : 20.0f;
	}

	static float GetKeyZoomStep()
	{
		const ULoops2DPanZoomSettings* Settings = GetDefault<ULoops2DPanZoomSettings>();
		return Settings ? Settings->KeyboardZoomStep : 0.12f;
	}

	static FEditorViewportClient* GetActiveEditorViewportClient()
	{
		if (!GEditor)
		{
			return nullptr;
		}
		FViewport* ActiveVP = GEditor->GetActiveViewport();
		if (ActiveVP)
		{
			for (FEditorViewportClient* Candidate : GEditor->GetAllViewportClients())
			{
				if (Candidate && Candidate->Viewport == ActiveVP)
				{
					return Candidate;
				}
			}
		}
		for (FEditorViewportClient* Candidate : GEditor->GetAllViewportClients())
		{
			if (Candidate)
			{
				return Candidate;
			}
		}
		return nullptr;
	}

	static bool IsTextEntryWidget(const TSharedPtr<SWidget>& Widget)
	{
		if (!Widget.IsValid())
		{
			return false;
		}
		const FString TypeName = Widget->GetType().ToString();
		return TypeName.Contains(TEXT("EditableText"))
			|| TypeName.Contains(TEXT("SearchBox"))
			|| TypeName.Contains(TEXT("SpinBox"));
	}

	static bool IsViewportWidgetLaidOut(const TSharedPtr<SWidget>& Widget)
	{
		if (!Widget.IsValid())
		{
			return false;
		}
		const FVector2D Size = FVector2D(Widget->GetTickSpaceGeometry().GetLocalSize());
		return Size.X > 1.0f && Size.Y > 1.0f;
	}

	constexpr int32 CursorFallbackAncestorDepth = 3;

	static FEditorViewportClient* GetViewportClientUnderCursor()
	{
		if (!GEditor || !FSlateApplication::IsInitialized())
		{
			return nullptr;
		}

		const FVector2D CursorPosition = FVector2D(FSlateApplication::Get().GetCursorPos());

		FEditorViewportClient* BestClient = nullptr;
		int32 BestDepth = MAX_int32;

		for (FEditorViewportClient* Candidate : GEditor->GetAllViewportClients())
		{
			if (!Candidate)
			{
				continue;
			}

			TSharedPtr<SWidget> Widget = Candidate->GetEditorViewportWidget();
			if (!IsViewportWidgetLaidOut(Widget))
			{
				continue;
			}

			for (int32 Depth = 0; Widget.IsValid() && Depth <= CursorFallbackAncestorDepth; ++Depth)
			{
				if (Widget->GetTickSpaceGeometry().IsUnderLocation(CursorPosition))
				{
					if (Depth < BestDepth)
					{
						BestDepth = Depth;
						BestClient = Candidate;
					}
					break;
				}
				Widget = Widget->GetParentWidget();
			}
		}

		return BestClient;
	}

	constexpr int32 FocusAncestorSearchDepth = 12;

	static bool IsViewportWidgetFocused(const TSharedPtr<SEditorViewport>& ViewportWidget)
	{
		if (!ViewportWidget.IsValid())
		{
			return false;
		}
		if (ViewportWidget->HasAnyUserFocusOrFocusedDescendants())
		{
			return true;
		}
		if (!FSlateApplication::IsInitialized())
		{
			return false;
		}

		const TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetUserFocusedWidget(0);
		if (!FocusedWidget.IsValid())
		{
			return false;
		}

		TSharedPtr<SWidget> Ancestor = ViewportWidget->GetParentWidget();
		for (int32 Depth = 0; Ancestor.IsValid() && Depth < FocusAncestorSearchDepth; ++Depth)
		{
			if (Ancestor == FocusedWidget)
			{
				return true;
			}
			Ancestor = Ancestor->GetParentWidget();
		}

		return false;
	}

	static FEditorViewportClient* GetFocusedEditorViewportClient()
	{
		if (!GEditor)
		{
			return nullptr;
		}
		for (FEditorViewportClient* Candidate : GEditor->GetAllViewportClients())
		{
			if (!Candidate)
			{
				continue;
			}
			if (IsViewportWidgetFocused(Candidate->GetEditorViewportWidget()))
			{
				return Candidate;
			}
		}

		if (FSlateApplication::IsInitialized() && IsTextEntryWidget(FSlateApplication::Get().GetUserFocusedWidget(0)))
		{
			return nullptr;
		}

		return GetViewportClientUnderCursor();
	}

	static void StepPan(const FVector2D& ScreenDelta)
	{
		FEditorViewportClient* Client = GetFocusedEditorViewportClient();
		if (!Client || !Client->Viewport || !GEditor)
		{
			return;
		}
		if (ULoops2DPanZoomSubsystem* Subsystem = GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>())
		{
			Subsystem->Pan(Client, ScreenDelta, Client->Viewport->GetSizeXY());
		}
	}

	static void StepZoom(float DeltaZoom)
	{
		FEditorViewportClient* Client = GetFocusedEditorViewportClient();
		if (!Client || !GEditor)
		{
			return;
		}
		if (ULoops2DPanZoomSubsystem* Subsystem = GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>())
		{
			Subsystem->Zoom(Client, DeltaZoom);
		}
	}

	static FEditorViewportClient* GetViewportClientFromContext(const FToolMenuContext& Context)
	{
		if (const UUnrealEdViewportToolbarContext* ContextObject = Context.FindContext<UUnrealEdViewportToolbarContext>())
		{
			if (TSharedPtr<SEditorViewport> ViewportWidget = ContextObject->Viewport.Pin())
			{
				if (TSharedPtr<FEditorViewportClient> Client = ViewportWidget->GetViewportClient())
				{
					return Client.Get();
				}
			}
		}
		return GetActiveEditorViewportClient();
	}

	static void FocusViewport(FEditorViewportClient* ViewportClient)
	{
		if (!ViewportClient)
		{
			return;
		}
		if (TSharedPtr<SEditorViewport> EditorViewportWidget = ViewportClient->GetEditorViewportWidget())
		{
			FSlateApplication::Get().SetKeyboardFocus(EditorViewportWidget, EFocusCause::SetDirectly);
		}
	}
}

void FLoops2DPanZoomModule::StartupModule()
{
	FLoops2DPanZoomStyle::Initialize();
	FLoops2DPanZoomCommands::Register();

	CommandList = MakeShared<FUICommandList>();
	BindCommands();

	InputProcessor = MakeShared<FLoops2DPanZoomInputProcessor>(CommandList.ToSharedRef());
	FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
		this, &FLoops2DPanZoomModule::RegisterToolbarExtension));

	if (ISequencerModule* SequencerModule = FModuleManager::Get().LoadModulePtr<ISequencerModule>("Sequencer"))
	{
		SequencerCreatedHandle = SequencerModule->RegisterOnSequencerCreated(
			FOnSequencerCreated::FDelegate::CreateRaw(this, &FLoops2DPanZoomModule::OnSequencerCreated));
	}
}

void FLoops2DPanZoomModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("Sequencer") && SequencerCreatedHandle.IsValid())
	{
		FModuleManager::GetModuleChecked<ISequencerModule>("Sequencer").UnregisterOnSequencerCreated(SequencerCreatedHandle);
	}
	SequencerCreatedHandle.Reset();

	for (const TPair<TWeakPtr<ISequencer>, FDelegateHandle>& Binding : SequencerCameraCutHandles)
	{
		if (const TSharedPtr<ISequencer> Sequencer = Binding.Key.Pin())
		{
			Sequencer->OnCameraCut().Remove(Binding.Value);
		}
	}
	SequencerCameraCutHandles.Empty();

	UnregisterToolbarExtension();

	if (FSlateApplication::IsInitialized() && InputProcessor.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
	InputProcessor.Reset();
	CommandList.Reset();

	FLoops2DPanZoomCommands::Unregister();
	FLoops2DPanZoomStyle::Shutdown();
}

void FLoops2DPanZoomModule::BindCommands()
{
	const FLoops2DPanZoomCommands& Commands = FLoops2DPanZoomCommands::Get();

	const FCanExecuteAction CanExecuteWhenFocused = FCanExecuteAction::CreateLambda([]()
	{
		return Loops2DPanZoom::GetFocusedEditorViewportClient() != nullptr;
	});

	CommandList->MapAction(
		Commands.ToggleEnabled,
		FExecuteAction::CreateLambda([]()
		{
			if (FEditorViewportClient* Client = Loops2DPanZoom::GetFocusedEditorViewportClient())
			{
				if (ULoops2DPanZoomSubsystem* Subsystem = GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>())
				{
					Subsystem->ToggleEnabled(Client);
				}
			}
		}),
		CanExecuteWhenFocused
	);

	CommandList->MapAction(
		Commands.ToggleAnimControlLock,
		FExecuteAction::CreateLambda([]()
		{
			if (FEditorViewportClient* Client = Loops2DPanZoom::GetFocusedEditorViewportClient())
			{
				if (ULoops2DPanZoomSubsystem* Subsystem = GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>())
				{
					Subsystem->ToggleAnimControlLock(Client);
				}
			}
		}),
		CanExecuteWhenFocused
	);

	const FCanExecuteAction CanExecuteWhenEnabled = FCanExecuteAction::CreateLambda([]()
	{
		FEditorViewportClient* Client = Loops2DPanZoom::GetFocusedEditorViewportClient();
		const ULoops2DPanZoomSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>() : nullptr;
		return Client && Subsystem && Subsystem->IsEnabled(Client);
	});

	CommandList->MapAction(
		Commands.ToggleZoomTo100,
		FExecuteAction::CreateLambda([]()
		{
			if (FEditorViewportClient* Client = Loops2DPanZoom::GetFocusedEditorViewportClient())
			{
				if (ULoops2DPanZoomSubsystem* Subsystem = GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>())
				{
					Subsystem->ToggleZoomTo100Percent(Client);
				}
			}
		}),
		CanExecuteWhenEnabled
	);

	CommandList->MapAction(Commands.ZoomIn, FExecuteAction::CreateLambda([]() { Loops2DPanZoom::StepZoom(Loops2DPanZoom::GetKeyZoomStep()); }), CanExecuteWhenEnabled);
	CommandList->MapAction(Commands.ZoomOut, FExecuteAction::CreateLambda([]() { Loops2DPanZoom::StepZoom(-Loops2DPanZoom::GetKeyZoomStep()); }), CanExecuteWhenEnabled);

	CommandList->MapAction(Commands.PanLeft, FExecuteAction::CreateLambda([]() { Loops2DPanZoom::StepPan(FVector2D(Loops2DPanZoom::GetKeyPanStepPixels(), 0.0f)); }), CanExecuteWhenEnabled);
	CommandList->MapAction(Commands.PanRight, FExecuteAction::CreateLambda([]() { Loops2DPanZoom::StepPan(FVector2D(-Loops2DPanZoom::GetKeyPanStepPixels(), 0.0f)); }), CanExecuteWhenEnabled);
	CommandList->MapAction(Commands.PanUp, FExecuteAction::CreateLambda([]() { Loops2DPanZoom::StepPan(FVector2D(0.0f, Loops2DPanZoom::GetKeyPanStepPixels())); }), CanExecuteWhenEnabled);
	CommandList->MapAction(Commands.PanDown, FExecuteAction::CreateLambda([]() { Loops2DPanZoom::StepPan(FVector2D(0.0f, -Loops2DPanZoom::GetKeyPanStepPixels())); }), CanExecuteWhenEnabled);
}

void FLoops2DPanZoomModule::OnSequencerCreated(TSharedRef<ISequencer> InSequencer)
{
	const FDelegateHandle CameraCutHandle = InSequencer->OnCameraCut().AddRaw(this, &FLoops2DPanZoomModule::OnSequencerCameraCut);
	SequencerCameraCutHandles.Emplace(TWeakPtr<ISequencer>(InSequencer), CameraCutHandle);
}

void FLoops2DPanZoomModule::OnSequencerCameraCut(UObject* CameraObject, bool bJumpCut)
{
	if (GEditor)
	{
		if (ULoops2DPanZoomSubsystem* Subsystem = GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>())
		{
			Subsystem->NotifyCameraCut(CameraObject);
		}
	}
}

void FLoops2DPanZoomModule::RegisterToolbarExtension()
{
	if (!UToolMenus::IsToolMenuUIEnabled())
	{
		return;
	}

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(Loops2DPanZoom::ToolbarMenuName);
	if (!Menu)
	{
		return;
	}

	FToolMenuSection& Section = Menu->FindOrAddSection(Loops2DPanZoom::ToolbarSectionName);

	FToolMenuEntry ToggleEntry = FToolMenuEntry::InitToolBarButton(
		"Loops2DPanZoomToggle",
		FToolUIActionChoice(FToolUIAction(
			FToolMenuExecuteAction::CreateRaw(this, &FLoops2DPanZoomModule::OnToggleClicked),
			FToolMenuCanExecuteAction(),
			FToolMenuGetActionCheckState::CreateRaw(this, &FLoops2DPanZoomModule::GetToggleCheckState)
		)),
		FText::GetEmpty(),
		TAttribute<FText>::CreateRaw(this, &FLoops2DPanZoomModule::GetToggleTooltipText),
		FSlateIcon(FLoops2DPanZoomStyle::GetStyleSetName(), "Loops2DPanZoom.Icon"),
		EUserInterfaceActionType::ToggleButton
	);
	Section.AddEntry(ToggleEntry);
}

void FLoops2DPanZoomModule::UnregisterToolbarExtension()
{
	if (UToolMenus* ToolMenus = UToolMenus::TryGet())
	{
		ToolMenus->RemoveSection(Loops2DPanZoom::ToolbarMenuName, Loops2DPanZoom::ToolbarSectionName);
	}
}

void FLoops2DPanZoomModule::OnToggleClicked(const FToolMenuContext& InContext)
{
	if (GEditor)
	{
		if (FEditorViewportClient* Client = Loops2DPanZoom::GetViewportClientFromContext(InContext))
		{
			if (ULoops2DPanZoomSubsystem* Subsystem = GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>())
			{
				Subsystem->ToggleEnabled(Client);
			}
			Loops2DPanZoom::FocusViewport(Client);
		}
	}
}

ECheckBoxState FLoops2DPanZoomModule::GetToggleCheckState(const FToolMenuContext& InContext) const
{
	if (GEditor)
	{
		if (FEditorViewportClient* Client = Loops2DPanZoom::GetViewportClientFromContext(InContext))
		{
			if (const ULoops2DPanZoomSubsystem* Subsystem = GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>())
			{
				return Subsystem->IsEnabled(Client) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			}
		}
	}
	return ECheckBoxState::Unchecked;
}

FText FLoops2DPanZoomModule::GetToggleTooltipText() const
{
	const FLoops2DPanZoomCommands& Commands = FLoops2DPanZoomCommands::Get();

	TArray<FString> Lines;
	Lines.Add(LOCTEXT("Loops2DPanZoomToggleTooltipTitle", "Loops 2D Pan/Zoom").ToString());
	Lines.Add(FText::Format(LOCTEXT("ToggleLine", "{0} to toggle"), Commands.ToggleEnabled->GetInputText()).ToString());
	Lines.Add(LOCTEXT("DragLine", "Configurable mouse drag to pan/zoom (see Editor Preferences > Plugins > Loops 2D Pan/Zoom)").ToString());
	Lines.Add(FText::Format(LOCTEXT("PanKeysLine", "{0}/{1}/{2}/{3} to pan"),
		Commands.PanLeft->GetInputText(), Commands.PanRight->GetInputText(),
		Commands.PanUp->GetInputText(), Commands.PanDown->GetInputText()).ToString());
	Lines.Add(FText::Format(LOCTEXT("ZoomKeysLine", "{0}/{1} to zoom"), Commands.ZoomIn->GetInputText(), Commands.ZoomOut->GetInputText()).ToString());
	Lines.Add(FText::Format(LOCTEXT("ZoomToggleLine", "{0} to toggle zoom+pan between their current values and 100%"), Commands.ToggleZoomTo100->GetInputText()).ToString());
	Lines.Add(FText::Format(LOCTEXT("AnimLockLine", "{0} to lock the camera to the selected Control Rig control"), Commands.ToggleAnimControlLock->GetInputText()).ToString());
	Lines.Add(LOCTEXT("CinematicLine", "Also works while locked to a cinematic camera cut or piloting an actor").ToString());
	Lines.Add(LOCTEXT("RebindLine", "All keyboard shortcuts are rebindable in Editor Preferences > Keyboard Shortcuts (Loops 2D Pan/Zoom)").ToString());

	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLoops2DPanZoomModule, Loops2DPanZoom)
