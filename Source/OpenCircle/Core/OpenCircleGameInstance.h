// Copyright OpenCircle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "OpenCircleGameInstance.generated.h"

/**
 *  Lives for the whole session and survives map travel.
 *  Home for online sessions and settings carried from the lobby into a match.
 */
UCLASS()
class UOpenCircleGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:

	/** Seed for the city generator, chosen in the lobby (Phase 4) */
	UPROPERTY(BlueprintReadWrite, Category="Match")
	int32 CitySeed = 0;
};
