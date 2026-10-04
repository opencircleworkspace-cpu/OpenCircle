// Copyright OpenCircle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleWheel.h"
#include "BusWheels.generated.h"

/** Steered front wheel, Tata LPO 1612 size (10R20 tyre). */
UCLASS()
class UBusWheelFront : public UChaosVehicleWheel
{
	GENERATED_BODY()

public:
	UBusWheelFront();
};

/** Driven rear wheel (dual tyres modelled as one wide wheel). */
UCLASS()
class UBusWheelRear : public UChaosVehicleWheel
{
	GENERATED_BODY()

public:
	UBusWheelRear();
};
