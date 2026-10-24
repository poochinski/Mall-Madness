#include "MallPlayerController.h"

#include "MallGameMode.h"
#include "MallHUD.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"

AMallPlayerController::AMallPlayerController()
{
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bShouldPerformFullTickWhenPaused = true;
}

bool AMallPlayerController::IsGameplayInputEnabled() const
{
	const AMallGameMode* GM = GetWorld()->GetAuthGameMode<AMallGameMode>();
	return GM && GM->State == EMallState::Playing && !bMenuMode;
}

void AMallPlayerController::SetMenuMode(bool bMenu)
{
	bMenuMode = bMenu;
	if (bMenu)
	{
		bShowMouseCursor = true;
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
	else
	{
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
		FocusGrace = 0.5f;
	}
}

void AMallPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	AMallGameMode* GM = GetWorld()->GetAuthGameMode<AMallGameMode>();
	if (!GM)
	{
		return;
	}

	if (WasInputKeyJustPressed(EKeys::Escape) || WasInputKeyJustPressed(EKeys::P))
	{
		if (GM->State == EMallState::Playing) GM->SetPaused(true);
		else if (GM->State == EMallState::Paused) GM->SetPaused(false);
	}

	// Pause automatically if the window loses focus mid-game.
	FocusGrace -= DeltaTime;
	static const bool bNoFocusPause = FParse::Param(FCommandLine::Get(), TEXT("MallTest")) || FParse::Param(FCommandLine::Get(), TEXT("MallNoFocusPause"));
	if (!bNoFocusPause && GM->State == EMallState::Playing && FocusGrace <= 0.f)
	{
		UGameViewportClient* VC = GetWorld()->GetGameViewport();
		if (VC && VC->Viewport && !VC->Viewport->HasFocus() && !VC->Viewport->HasMouseCapture())
		{
			GM->SetPaused(true);
		}
	}

	if (bMenuMode)
	{
		const bool bEnter = WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::SpaceBar);
		if (bEnter)
		{
			if (GM->State == EMallState::Title) GM->OnButton(EMallButton::Play);
			else if (GM->State == EMallState::Paused) GM->OnButton(EMallButton::Resume);
			else if (GM->State == EMallState::Dead || GM->State == EMallState::Won) GM->OnButton(EMallButton::Restart);
			return;
		}
		if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
		{
			float X = 0, Y = 0;
			if (GetMousePosition(X, Y))
			{
				if (AMallHUD* H = Cast<AMallHUD>(GetHUD()))
				{
					const EMallButton B = H->ButtonAt(FVector2D(X, Y));
					if (B != EMallButton::None)
					{
						GM->OnButton(B);
					}
				}
			}
		}
	}
}
