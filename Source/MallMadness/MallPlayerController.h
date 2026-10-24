#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MallPlayerController.generated.h"

UCLASS()
class AMallPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMallPlayerController();

	virtual void PlayerTick(float DeltaTime) override;
	bool IsGameplayInputEnabled() const;
	void SetMenuMode(bool bMenu);

private:
	bool bMenuMode = false;
	float FocusGrace = 0.f;
};
