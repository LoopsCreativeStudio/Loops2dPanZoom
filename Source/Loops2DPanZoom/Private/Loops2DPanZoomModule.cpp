// Copyright 2026 Loops Creative Studio. All Rights Reserved.

#include "Loops2DPanZoomModule.h"
#include "Loops2DPanZoomInputProcessor.h"
#include "Loops2DPanZoomSubsystem.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "Editor.h"
#include "ToolMenus.h"
#include "Loops2DPanZoomStyle.h"
#include "Framework/Application/SlateApplication.h"
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

	// Fonctionne seulement pour 5.8
	// static FEditorViewportClient* GetViewportClientFromContext(const FToolMenuContext& Context)
	// {
	// 	if (TSharedPtr<FEditorViewportClient> ContextClient = UUnrealEdViewportToolbarContext::GetEditorViewportClient(Context))
	// 	{
	// 		return ContextClient.Get();
	// 	}
	// 	return GetActiveEditorViewportClient();
	// }

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

	InputProcessor = MakeShared<FLoops2DPanZoomInputProcessor>();
	FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
		this, &FLoops2DPanZoomModule::RegisterToolbarExtension));

	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
	SequencerCreatedHandle = SequencerModule.RegisterOnSequencerCreated(
		FOnSequencerCreated::FDelegate::CreateRaw(this, &FLoops2DPanZoomModule::OnSequencerCreated));
}

void FLoops2DPanZoomModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("Sequencer"))
	{
		FModuleManager::GetModuleChecked<ISequencerModule>("Sequencer").UnregisterOnSequencerCreated(SequencerCreatedHandle);
	}

	UnregisterToolbarExtension();

	if (FSlateApplication::IsInitialized() && InputProcessor.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
	InputProcessor.Reset();

	FLoops2DPanZoomStyle::Shutdown();
}

void FLoops2DPanZoomModule::OnSequencerCreated(TSharedRef<ISequencer> InSequencer)
{
	InSequencer->OnCameraCut().AddRaw(this, &FLoops2DPanZoomModule::OnSequencerCameraCut);
	InSequencer->OnGlobalTimeChanged().AddRaw(this, &FLoops2DPanZoomModule::OnSequencerGlobalTimeChanged);
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
	RefreshFollowCameraCutForAllViewports();
}

void FLoops2DPanZoomModule::OnSequencerGlobalTimeChanged()
{
	RefreshFollowCameraCutForAllViewports();
}

void FLoops2DPanZoomModule::RefreshFollowCameraCutForAllViewports()
{
	if (!GEditor)
	{
		return;
	}

	if (ULoops2DPanZoomSubsystem* Subsystem = GEditor->GetEditorSubsystem<ULoops2DPanZoomSubsystem>())
	{
		for (FEditorViewportClient* Candidate : GEditor->GetAllViewportClients())
		{
			if (Candidate)
			{
				Subsystem->TickFollowCameraCut(Candidate);
			}
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
		// Empty label: icon-only in the toolbar, matching the look of most other viewport toolbar buttons.
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
	static const FText BaseTooltip = LOCTEXT("Loops2DPanZoomToggleTooltip",
		"Loops 2D Pan/Zoom\n"
		"'/' to toggle\n"
		"Shift+'/' to reset the view\n"
		"Alt+MMB drag to pan (tilts the camera in place)\n"
		"Alt+RMB drag to zoom (adjusts FOV / ortho zoom)\n"
		"Numpad 4/6/8/2 to pan, Numpad +/- to zoom\n"
		"Numpad * to toggle zoom+pan between their current values and 100%\n");

	if (FEditorViewportClient* Client = Loops2DPanZoom::GetActiveEditorViewportClient())
	{
		if (Client->AllowsCinematicControl() && Client->IsLevelEditorClient())
		{
			const FLevelEditorViewportClient* LevelViewportClient = static_cast<FLevelEditorViewportClient*>(Client);
			if (LevelViewportClient->IsLockedToCinematic())
			{
				return FText::Format(
					LOCTEXT("Loops2DPanZoomToggleTooltipWithWarning", "{0}\n{1}"),
					BaseTooltip,
					LOCTEXT("Loops2DPanZoomCinematicWarning", "WARNING: Pan/Zoom blocked by camera cut - disable Allow Cinematic Control")
				);
			}
		}
	}

	//TODO : Add check Camera Pilot

	return BaseTooltip;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLoops2DPanZoomModule, Loops2DPanZoom)

