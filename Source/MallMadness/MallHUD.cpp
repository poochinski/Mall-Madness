#include "MallHUD.h"

#include "MallBuilder.h"
#include "MallInteractable.h"
#include "MallMaterials.h"
#include "MallPlayer.h"
#include "MallWeapons.h"
#include "Camera/CameraComponent.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"

namespace
{
	UFont* HudFont(bool bBold)
	{
		return bBold ? GEngine->GetLargeFont() : GEngine->GetMediumFont();
	}

	const FLinearColor White(0.95f, 0.94f, 0.9f, 1.f);
	const FLinearColor Dim(0.65f, 0.65f, 0.62f, 1.f);
	const FLinearColor Accent(1.0f, 0.72f, 0.28f, 1.f);
	const FLinearColor Danger(0.95f, 0.25f, 0.18f, 1.f);
	const FLinearColor Panel(0.02f, 0.02f, 0.025f, 0.55f);
}

void AMallHUD::BeginPlay()
{
	Super::BeginPlay();
	// Radial vignette (alpha rises toward the edges) and a soft-edged scope mask.
	const int32 N = 256;
	TArray<FColor> V, Sc;
	V.SetNum(N * N);
	Sc.SetNum(N * N);
	for (int32 Y = 0; Y < N; ++Y)
	{
		for (int32 X = 0; X < N; ++X)
		{
			const float DX = (X + 0.5f) / N * 2 - 1, DY = (Y + 0.5f) / N * 2 - 1;
			const float R = FMath::Sqrt(DX * DX + DY * DY);
			V[Y * N + X] = FColor(255, 255, 255, uint8(FMath::Clamp((R - 0.45f) / 0.75f, 0.f, 1.f) * 255));
			const float Edge = FMath::Clamp((R - 0.90f) / 0.08f, 0.f, 1.f);
			Sc[Y * N + X] = FColor(0, 0, 0, uint8(Edge * 255));
		}
	}
	Vignette = MallTex::MakeTexture(V, N, true);
	ScopeMask = MallTex::MakeTexture(Sc, N, true);
}

void AMallHUD::Rect(float X, float Y, float W, float H, const FLinearColor& C)
{
	FCanvasTileItem T(FVector2D(X, Y), FVector2D(W, H), C);
	T.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(T);
}

void AMallHUD::Line(float X0, float Y0, float X1, float Y1, const FLinearColor& C, float T)
{
	FCanvasLineItem L(FVector2D(X0, Y0), FVector2D(X1, Y1));
	L.SetColor(C);
	L.LineThickness = T;
	Canvas->DrawItem(L);
}

FVector2D AMallHUD::Measure(const FString& Str, float Size, bool bBold) const
{
	UFont* F = HudFont(bBold);
	if (!F || !Canvas)
	{
		return FVector2D(Str.Len() * Size * 0.55f, Size * 1.2f);
	}
	const float Scale = Size / float(FMath::Max(1, F->LegacyFontSize));
	float W = 0.f, H = 0.f;
	Canvas->TextSize(F, Str, W, H, Scale, Scale);
	return FVector2D(W, H);
}

FVector2D AMallHUD::Txt(const FString& Str, float X, float Y, float Size, const FLinearColor& C, bool bBold, float AlignX, bool bShadow)
{
	const float Px = Size * S;
	UFont* F = HudFont(bBold);
	const FVector2D Dim2 = Measure(Str, Px, bBold);
	if (!F)
	{
		return Dim2;
	}
	const float Scale = Px / float(FMath::Max(1, F->LegacyFontSize));
	FCanvasTextItem T(FVector2D(X - Dim2.X * AlignX, Y), FText::FromString(Str), F, C);
	T.Scale = FVector2D(Scale, Scale);
	if (bShadow)
	{
		T.EnableShadow(FLinearColor(0, 0, 0, 0.7f * C.A), FVector2D(1.5f, 1.5f));
	}
	Canvas->DrawItem(T);
	return Dim2;
}

bool AMallHUD::Button(const FString& Label, float X, float Y, float W, float H, EMallButton Id, bool bPrimary)
{
	const FBox2D Box(FVector2D(X, Y), FVector2D(X + W, Y + H));
	const bool bHover = Box.IsInside(Mouse);
	Rect(X, Y, W, H, bPrimary ? (bHover ? FLinearColor(1.f, 0.78f, 0.35f, 0.95f) : FLinearColor(0.9f, 0.62f, 0.2f, 0.9f)) : (bHover ? FLinearColor(1, 1, 1, 0.22f) : FLinearColor(1, 1, 1, 0.08f)));
	Line(X, Y + H, X + W, Y + H, bPrimary ? FLinearColor(0, 0, 0, 0.3f) : FLinearColor(1, 1, 1, 0.25f), 1.f);
	const FVector2D D = Measure(Label, 20 * S, true);
	Txt(Label, X + W * 0.5f, Y + (H - D.Y) * 0.5f, 20, bPrimary ? FLinearColor(0.05f, 0.04f, 0.03f) : White, true, 0.5f, !bPrimary);
	Buttons.Add({Box, Id});
	return bHover;
}

EMallButton AMallHUD::ButtonAt(const FVector2D& P) const
{
	for (const TPair<FBox2D, EMallButton>& B : Buttons)
	{
		if (B.Key.IsInside(P))
		{
			return B.Value;
		}
	}
	return EMallButton::None;
}

void AMallHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	S = Canvas->ClipY / 1080.f;
	Buttons.Reset();
	if (APlayerController* PC = GetOwningPlayerController())
	{
		float MX = 0, MY = 0;
		if (PC->GetMousePosition(MX, MY))
		{
			Mouse = FVector2D(MX, MY);
		}
	}
	AMallGameMode* GM = GetWorld()->GetAuthGameMode<AMallGameMode>();
	AMallPlayer* P = GM ? GM->GetPlayer() : nullptr;
	if (!GM || !P)
	{
		return;
	}
	if (GM->State == EMallState::Playing || GM->State == EMallState::Paused || GM->State == EMallState::Dead)
	{
		DrawGameplay(GM, P);
	}
	if (GM->State != EMallState::Playing)
	{
		DrawMenu(GM, P);
	}
}

void AMallHUD::DrawScope()
{
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float D = H * 0.96f;
	const float X0 = (W - D) * 0.5f, Y0 = (H - D) * 0.5f;
	Rect(0, 0, X0 + 1, H, FLinearColor::Black);
	Rect(X0 + D - 1, 0, W - X0 - D + 1, H, FLinearColor::Black);
	Rect(X0, 0, D, Y0 + 1, FLinearColor::Black);
	Rect(X0, Y0 + D - 1, D, H - Y0 - D + 1, FLinearColor::Black);
	FCanvasTileItem Mask(FVector2D(X0, Y0), ScopeMask->GetResource(), FVector2D(D, D), FLinearColor::White);
	Mask.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Mask);
	const float CX = W * 0.5f, CY = H * 0.5f, R = D * 0.47f;
	const FLinearColor Ret(0.02f, 0.02f, 0.02f, 0.95f);
	// Thick outer posts, fine centre cross, mil dots.
	Line(CX - R, CY, CX - R * 0.32f, CY, Ret, 5.f * S);
	Line(CX + R * 0.32f, CY, CX + R, CY, Ret, 5.f * S);
	Line(CX, CY + R * 0.32f, CX, CY + R, Ret, 5.f * S);
	Line(CX - R * 0.32f, CY, CX + R * 0.32f, CY, Ret, 1.4f * S);
	Line(CX, CY - R, CX, CY + R * 0.32f, Ret, 1.4f * S);
	for (int32 I = 1; I <= 4; ++I)
	{
		const float O = R * 0.075f * I;
		Rect(CX + O - 2 * S, CY - 2 * S, 4 * S, 4 * S, Ret);
		Rect(CX - O - 2 * S, CY - 2 * S, 4 * S, 4 * S, Ret);
		Rect(CX - 2 * S, CY + O - 2 * S, 4 * S, 4 * S, Ret);
	}
	Rect(CX - 1.5f * S, CY - 1.5f * S, 3 * S, 3 * S, FLinearColor(0.9f, 0.1f, 0.05f, 0.9f));
}

void AMallHUD::DrawCrosshair(AMallPlayer* P)
{
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float CX = W * 0.5f, CY = H * 0.5f;
	const float Ads = P->GetAdsAlpha();
	const FMallWeaponDef& D = P->CurrentDef();

	if (P->IsScoped())
	{
		DrawScope();
	}
	else if (Ads < 0.6f && !P->IsBusy())
	{
		// Gap tracks the real spread cone.
		const float Fov = P->Camera->FieldOfView;
		const float PxPerRad = (W * 0.5f) / FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f));
		const float Gap = FMath::Clamp(FMath::Tan(FMath::DegreesToRadians(P->CurrentSpread())) * PxPerRad, 4.f * S, 120.f * S);
		const float L = 9.f * S;
		const FLinearColor C(1, 1, 1, 0.85f * (1.f - Ads / 0.6f));
		const FLinearColor Sh(0, 0, 0, 0.5f * (1.f - Ads / 0.6f));
		auto Tick = [&](float X0, float Y0, float X1, float Y1)
		{
			Line(X0 + 1, Y0 + 1, X1 + 1, Y1 + 1, Sh, 2.f * S);
			Line(X0, Y0, X1, Y1, C, 2.f * S);
		};
		Tick(CX - Gap - L, CY, CX - Gap, CY);
		Tick(CX + Gap, CY, CX + Gap + L, CY);
		Tick(CX, CY + Gap, CX, CY + Gap + L);
		if (D.Pellets == 1)
		{
			Tick(CX, CY - Gap - L, CX, CY - Gap);
		}
		Rect(CX - 1, CY - 1, 2, 2, C);
	}

	if (P->HitMarkerTime > 0.f)
	{
		const float A = FMath::Clamp(P->HitMarkerTime / 0.18f, 0.f, 1.f);
		const FLinearColor C = P->bHitMarkerKill ? FLinearColor(0.95f, 0.2f, 0.15f, A) : (P->bHitMarkerHead ? FLinearColor(1.f, 0.8f, 0.3f, A) : FLinearColor(1, 1, 1, A));
		const float I = 7.f * S, O = 15.f * S;
		Line(CX - O, CY - O, CX - I, CY - I, C, 2.2f * S);
		Line(CX + O, CY - O, CX + I, CY - I, C, 2.2f * S);
		Line(CX - O, CY + O, CX - I, CY + I, C, 2.2f * S);
		Line(CX + O, CY + O, CX + I, CY + I, C, 2.2f * S);
	}
}

void AMallHUD::DrawMarker(AMallGameMode* GM, AMallPlayer* P)
{
	FVector Loc;
	if (!GM->ObjectiveMarker(Loc) || P->IsScoped())
	{
		return;
	}
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const FVector Eye = P->GetEyeLocation();
	const FVector Fwd = P->Camera->GetForwardVector();
	const FVector To = (Loc + FVector(0, 0, 60)) - Eye;
	FVector Proj = Canvas->Project(Loc + FVector(0, 0, 60));
	bool bBehind = FVector::DotProduct(To, Fwd) < 0.f;
	float X = Proj.X, Y = Proj.Y;
	if (bBehind || X < 40 * S || X > W - 40 * S || Y < 60 * S || Y > H - 80 * S)
	{
		// Clamp to the screen edge in the direction of the target.
		const FVector Local = P->Camera->GetComponentTransform().InverseTransformVectorNoScale(To);
		FVector2D Dir(Local.Y, -Local.Z);
		Dir = Dir.GetSafeNormal();
		if (Dir.IsNearlyZero()) Dir = FVector2D(0, 1);
		const float R = FMath::Min(W, H) * 0.42f;
		X = W * 0.5f + Dir.X * R;
		Y = H * 0.5f + Dir.Y * R;
	}
	const float Dist = To.Size() / 100.f;
	const float A = FMath::Clamp((Dist - 2.f) / 4.f, 0.f, 1.f) * 0.85f;
	if (A <= 0.01f)
	{
		return;
	}
	const FLinearColor C(1.f, 0.78f, 0.35f, A);
	const float R = 7.f * S;
	Line(X, Y - R, X + R, Y, C, 2.f * S);
	Line(X + R, Y, X, Y + R, C, 2.f * S);
	Line(X, Y + R, X - R, Y, C, 2.f * S);
	Line(X - R, Y, X, Y - R, C, 2.f * S);
	Txt(FString::Printf(TEXT("%d m"), FMath::RoundToInt(Dist)), X, Y + 10 * S, 14, FLinearColor(1, 0.9f, 0.7f, A), true, 0.5f);
	if (FMath::Abs(To.Z) > 300.f)
	{
		Txt(To.Z > 0 ? TEXT("UPSTAIRS") : TEXT("DOWNSTAIRS"), X, Y + 28 * S, 11, FLinearColor(1, 0.9f, 0.7f, A * 0.8f), false, 0.5f);
	}
}

void AMallHUD::DrawGameplay(AMallGameMode* GM, AMallPlayer* P)
{
	const float W = Canvas->ClipX, H = Canvas->ClipY;

	// Damage / low-health vignette.
	const float Low = FMath::Clamp(1.f - P->Health / 45.f, 0.f, 1.f);
	const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 5.f);
	const float VA = FMath::Clamp(P->DamageFlash * 0.75f + Low * (0.35f + 0.2f * Pulse), 0.f, 0.95f);
	if (VA > 0.01f && Vignette)
	{
		FCanvasTileItem V(FVector2D(0, 0), Vignette->GetResource(), FVector2D(W, H), FLinearColor(0.55f, 0.02f, 0.01f, VA));
		V.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(V);
	}

	if (P->IsDead())
	{
		return;
	}

	DrawCrosshair(P);
	DrawMarker(GM, P);

	// Directional damage arcs.
	const float PlayerYaw = P->GetControlRotation().Yaw;
	for (const TPair<float, float>& DD : P->DamageDirs)
	{
		const float Rel = FMath::DegreesToRadians(DD.Key - PlayerYaw);
		const float A = FMath::Clamp(DD.Value / 1.4f, 0.f, 1.f);
		const float R = 150.f * S;
		for (int32 Seg = -3; Seg < 3; ++Seg)
		{
			const float A0 = Rel + Seg * 0.07f, A1 = Rel + (Seg + 1) * 0.07f;
			Line(W * 0.5f + FMath::Sin(A0) * R, H * 0.5f - FMath::Cos(A0) * R, W * 0.5f + FMath::Sin(A1) * R, H * 0.5f - FMath::Cos(A1) * R, FLinearColor(0.95f, 0.15f, 0.1f, A * 0.9f), 5.f * S);
		}
	}

	// Objective (top-left).
	const float Mg = 40.f * S;
	Txt(TEXT("OBJECTIVE"), Mg, Mg, 13, Accent, true);
	Txt(GM->ObjectiveText(), Mg, Mg + 20 * S, 19, White, false);
	if (GM->Objective == 2)
	{
		const float BW = 360.f * S;
		Rect(Mg, Mg + 52 * S, BW, 5 * S, FLinearColor(1, 1, 1, 0.15f));
		Rect(Mg, Mg + 52 * S, BW * GM->TransmitProgress(), 5 * S, Accent);
	}

	// Health and meds (bottom-left).
	const float BY = H - 86.f * S;
	Txt(FString::Printf(TEXT("%d"), FMath::CeilToInt(P->Health)), Mg, BY - 8 * S, 34, P->Health < 35 ? Danger : White, true);
	Txt(TEXT("HEALTH"), Mg + 70 * S, BY + 4 * S, 12, Dim, true);
	const float HW = 260.f * S;
	Rect(Mg, BY + 36 * S, HW, 6 * S, FLinearColor(1, 1, 1, 0.15f));
	Rect(Mg, BY + 36 * S, HW * P->Health / P->MaxHealth, 6 * S, P->Health < 35 ? Danger : FLinearColor(0.9f, 0.9f, 0.86f, 0.9f));
	Txt(FString::Printf(TEXT("MEDKITS  %d     [Q] use"), P->Meds), Mg, BY + 50 * S, 13, P->Meds > 0 ? White : Dim, false);
	if (P->HealProgress() > 0.f)
	{
		Rect(W * 0.5f - 100 * S, H * 0.6f, 200 * S, 4 * S, FLinearColor(1, 1, 1, 0.2f));
		Rect(W * 0.5f - 100 * S, H * 0.6f, 200 * S * P->HealProgress(), 4 * S, FLinearColor(0.5f, 0.95f, 0.6f, 0.9f));
		Txt(TEXT("APPLYING MEDKIT"), W * 0.5f, H * 0.6f + 8 * S, 13, White, true, 0.5f);
	}

	// Weapon and ammo (bottom-right).
	const FMallWeaponDef& D = P->CurrentDef();
	const int32 Slot = P->GetSlot();
	const float RX = W - Mg;
	const FString MagStr = FString::Printf(TEXT("%d"), P->Mag[Slot]);
	const FVector2D ResSize = Txt(FString::Printf(TEXT("/ %d"), P->Reserve[Slot]), RX, BY + 6 * S, 20, Dim, false, 1.f);
	const bool bLowMag = P->Mag[Slot] <= FMath::Max(1, D.MagSize / 4);
	Txt(MagStr, RX - ResSize.X - 8 * S, BY - 12 * S, 40, P->Mag[Slot] == 0 ? Danger : (bLowMag ? Accent : White), true, 1.f);
	Txt(D.Name.ToUpper(), RX, BY - 34 * S, 14, White, true, 1.f);
	float SX = RX;
	for (int32 I = 3; I >= 0; --I)
	{
		const FString Label = FString::Printf(TEXT("%d"), I + 1);
		const FLinearColor C = I == Slot ? Accent : (P->bOwned[I] ? FLinearColor(1, 1, 1, 0.75f) : FLinearColor(1, 1, 1, 0.18f));
		const FVector2D Sz = Txt(Label, SX, BY + 46 * S, 14, C, true, 1.f);
		SX -= Sz.X + 14 * S;
	}
	if (P->IsReloading())
	{
		Txt(TEXT("RELOADING"), W * 0.5f, H * 0.5f + 40 * S, 13, White, true, 0.5f);
		Rect(W * 0.5f - 50 * S, H * 0.5f + 60 * S, 100 * S, 3 * S, FLinearColor(1, 1, 1, 0.2f));
		Rect(W * 0.5f - 50 * S, H * 0.5f + 60 * S, 100 * S * P->ReloadProgress(), 3 * S, White);
	}
	else if (P->Mag[Slot] == 0)
	{
		Txt(P->Reserve[Slot] > 0 ? TEXT("[R] RELOAD") : TEXT("NO AMMO"), W * 0.5f, H * 0.5f + 40 * S, 13, Danger, true, 0.5f);
	}
	else if (bLowMag && P->Reserve[Slot] > 0)
	{
		Txt(TEXT("[R] RELOAD"), W * 0.5f, H * 0.5f + 40 * S, 12, FLinearColor(1, 0.8f, 0.4f, 0.7f), true, 0.5f);
	}

	// Interaction prompt.
	if (P->Focus)
	{
		const FString Verb = P->Focus->UseTime > 0.f ? TEXT("Hold E") : TEXT("E");
		const FString Str = FString::Printf(TEXT("%s  -  %s"), *Verb, *P->Focus->Prompt);
		const FVector2D Sz = Measure(Str, 18 * S, true);
		const float PY = H * 0.5f + 80 * S;
		Rect(W * 0.5f - Sz.X * 0.5f - 14 * S, PY - 6 * S, Sz.X + 28 * S, Sz.Y + 12 * S, Panel);
		Txt(Str, W * 0.5f, PY, 18, White, true, 0.5f);
		if (P->Focus->UseTime > 0.f && P->InteractHold > 0.f)
		{
			Rect(W * 0.5f - Sz.X * 0.5f - 14 * S, PY + Sz.Y + 6 * S, (Sz.X + 28 * S) * FMath::Clamp(P->InteractHold / P->Focus->UseTime, 0.f, 1.f), 3 * S, Accent);
		}
	}

	// Notifications (right, stacked above the ammo readout).
	float NY = BY - 90 * S;
	for (int32 I = GM->Notes.Num() - 1; I >= 0; --I)
	{
		const FMallNote& N = GM->Notes[I];
		const float A = FMath::Clamp(FMath::Min(N.Time / 0.2f, (N.Duration - N.Time) / 0.5f), 0.f, 1.f);
		FLinearColor C = FLinearColor(N.Color);
		C.A = A;
		Txt(N.Text, RX, NY, 15, C, false, 1.f);
		NY -= 26 * S;
	}

	if (GM->BigMessageTime > 0.f)
	{
		const float A = FMath::Clamp(FMath::Min((3.5f - GM->BigMessageTime) / 0.3f, GM->BigMessageTime / 0.6f), 0.f, 1.f);
		Txt(GM->BigMessage, W * 0.5f, H * 0.24f, 30, FLinearColor(1, 0.85f, 0.55f, A), true, 0.5f);
	}
}

void AMallHUD::DrawControls(float X, float Y)
{
	const TCHAR* Rows[][2] = {
		{TEXT("W A S D"), TEXT("Move")}, {TEXT("Mouse"), TEXT("Look")}, {TEXT("Shift"), TEXT("Sprint")}, {TEXT("Space"), TEXT("Jump")},
		{TEXT("C / Ctrl"), TEXT("Crouch")}, {TEXT("LMB"), TEXT("Fire")}, {TEXT("RMB (hold)"), TEXT("Aim / scope")}, {TEXT("R"), TEXT("Reload")},
		{TEXT("E"), TEXT("Interact / loot")}, {TEXT("1-4, Wheel"), TEXT("Weapons")}, {TEXT("Q"), TEXT("Use medkit")}, {TEXT("Esc"), TEXT("Pause")}};
	for (int32 I = 0; I < 12; ++I)
	{
		const float CX = X + (I % 2) * 250 * S;
		const float CY = Y + (I / 2) * 26 * S;
		Txt(Rows[I][0], CX, CY, 14, Accent, true);
		Txt(Rows[I][1], CX + 110 * S, CY, 14, White, false);
	}
}

void AMallHUD::DrawSettings(float X, float Y)
{
	const float BW = 36 * S, BH = 30 * S;
	Txt(TEXT("MOUSE SENSITIVITY"), X, Y + 5 * S, 14, Dim, true);
	Button(TEXT("-"), X + 210 * S, Y, BW, BH, EMallButton::SensDown);
	Txt(FString::Printf(TEXT("%.2f"), AMallGameMode::Sensitivity), X + 290 * S, Y + 4 * S, 16, White, true, 0.5f);
	Button(TEXT("+"), X + 330 * S, Y, BW, BH, EMallButton::SensUp);
	Txt(TEXT("VOLUME"), X, Y + 45 * S, 14, Dim, true);
	Button(TEXT("-"), X + 210 * S, Y + 40 * S, BW, BH, EMallButton::VolDown);
	Txt(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(AMallGameMode::Volume * 100)), X + 290 * S, Y + 44 * S, 16, White, true, 0.5f);
	Button(TEXT("+"), X + 330 * S, Y + 40 * S, BW, BH, EMallButton::VolUp);
}

void AMallHUD::DrawMenu(AMallGameMode* GM, AMallPlayer* P)
{
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float L = W * 0.08f;

	if (GM->State == EMallState::Title)
	{
		Rect(0, 0, W, H, FLinearColor(0.01f, 0.01f, 0.015f, 0.55f));
		Rect(0, 0, W * 0.48f, H, FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
		float Y = H * 0.12f;
		Txt(TEXT("LAST CALL"), L, Y, 84, White, true);
		Y += 100 * S;
		Txt(TEXT("AT LARKSPUR GALLERIA"), L + 4 * S, Y, 26, Accent, true);
		Y += 58 * S;
		const TCHAR* Premise[] = {
			TEXT("The evacuation sirens stopped hours ago. You didn't make the last bus out."),
			TEXT("The mall's emergency radio can still reach the rescue teams - if someone"),
			TEXT("replaces its blown fuse. Scavenge the stores, arm yourself, call for"),
			TEXT("extraction, and survive long enough to walk out the front doors.")};
		for (const TCHAR* Line2 : Premise)
		{
			Txt(Line2, L, Y, 17, FLinearColor(0.85f, 0.84f, 0.8f), false);
			Y += 26 * S;
		}
		Y += 26 * S;
		Button(TEXT("PLAY"), L, Y, 220 * S, 54 * S, EMallButton::Play, true);
		Button(TEXT("QUIT"), L + 236 * S, Y, 120 * S, 54 * S, EMallButton::Quit);
		Y += 90 * S;
		DrawControls(L, Y);
		Y += 170 * S;
		DrawSettings(L, Y);
		Txt(TEXT("Click PLAY or press Enter. The mouse is captured while playing; Esc releases it."), L, H - 50 * S, 13, Dim, false);
		return;
	}

	Rect(0, 0, W, H, FLinearColor(0.0f, 0.0f, 0.0f, GM->State == EMallState::Paused ? 0.6f : 0.5f));
	float Y = H * 0.18f;
	if (GM->State == EMallState::Paused)
	{
		Txt(TEXT("PAUSED"), L, Y, 60, White, true);
		Y += 90 * S;
		Txt(GM->ObjectiveText(), L, Y, 18, Accent, false);
		Y += 50 * S;
		Button(TEXT("RESUME"), L, Y, 220 * S, 50 * S, EMallButton::Resume, true);
		Button(TEXT("RESTART"), L + 236 * S, Y, 160 * S, 50 * S, EMallButton::Restart);
		Button(TEXT("QUIT"), L + 412 * S, Y, 120 * S, 50 * S, EMallButton::Quit);
		Y += 90 * S;
		DrawSettings(L, Y);
		Y += 100 * S;
		DrawControls(L, Y);
		return;
	}

	const bool bWon = GM->State == EMallState::Won;
	Txt(bWon ? TEXT("YOU ESCAPED") : TEXT("YOU DIED"), L, Y, 72, bWon ? FLinearColor(0.85f, 0.95f, 0.8f) : Danger, true);
	Y += 100 * S;
	Txt(bWon ? TEXT("The rescue chopper lifts off over the parking lot. Larkspur Galleria falls silent behind you.")
		: TEXT("The mall claims another shopper. The radio is still waiting."), L, Y, 18, White, false);
	Y += 46 * S;
	const int32 Mins = int32(GM->PlayTime) / 60, Secs = int32(GM->PlayTime) % 60;
	Txt(FString::Printf(TEXT("Time  %d:%02d      Kills  %d      Shots fired  %d"), Mins, Secs, P->Kills, P->ShotsFired), L, Y, 16, Dim, true);
	Y += 60 * S;
	Button(TEXT("RESTART"), L, Y, 220 * S, 54 * S, EMallButton::Restart, true);
	Button(TEXT("QUIT"), L + 236 * S, Y, 120 * S, 54 * S, EMallButton::Quit);
	Txt(TEXT("Press Enter to restart"), L, Y + 70 * S, 13, Dim, false);
}
