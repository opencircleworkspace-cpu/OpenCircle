// Copyright OpenCircle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BusTypes.generated.h"

/** Seat a player holds in their duo's bus */
UENUM(BlueprintType)
enum class EBusRole : uint8
{
	None,
	Driver,
	Conductor
};

/** High-level flow of a match */
UENUM(BlueprintType)
enum class EMatchPhase : uint8
{
	Lobby,
	PreRace,
	Racing,
	Results
};
