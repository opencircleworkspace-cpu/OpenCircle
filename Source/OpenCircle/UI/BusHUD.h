// Copyright OpenCircle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BusHUD.generated.h"

/**
 *  Placeholder driver dashboard drawn on the canvas: speed, RPM, gear, air, doors, lamps, stop.
 *  Replace with a UMG dashboard later; it reads the same ABusVehicle getters.
 */
UCLASS()
class ABusHUD : public AHUD
{
	GENERATED_BODY()

public:

	virtual void DrawHUD() override;

protected:

	UPROPERTY(EditAnywhere, Category="HUD")
	float TextScale = 1.4f;
};
