#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MallGameMode.h"
#include "MallHUD.generated.h"

class UTexture2D;
class AMallPlayer;
struct FSlateFontInfo;

UCLASS()
class AMallHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

	EMallButton ButtonAt(const FVector2D& P) const;

private:
	void DrawGameplay(AMallGameMode* GM, AMallPlayer* P);
	void DrawCrosshair(AMallPlayer* P);
	void DrawScope();
	void DrawMarker(AMallGameMode* GM, AMallPlayer* P);
	void DrawMenu(AMallGameMode* GM, AMallPlayer* P);
	void DrawControls(float X, float Y);
	void DrawSettings(float X, float Y);

	void Rect(float X, float Y, float W, float H, const FLinearColor& C);
	void Line(float X0, float Y0, float X1, float Y1, const FLinearColor& C, float T = 2.f);
	FVector2D Txt(const FString& S, float X, float Y, float Size, const FLinearColor& C, bool bBold = false, float AlignX = 0.f, bool bShadow = true);
	FVector2D Measure(const FString& S, float Size, bool bBold) const;
	bool Button(const FString& Label, float X, float Y, float W, float H, EMallButton Id, bool bPrimary = false);

	float S = 1.f;   // UI scale relative to 1080p
	FVector2D Mouse = FVector2D::ZeroVector;
	TArray<TPair<FBox2D, EMallButton>> Buttons;

	UPROPERTY() TObjectPtr<UTexture2D> Vignette;
	UPROPERTY() TObjectPtr<UTexture2D> ScopeMask;
};
