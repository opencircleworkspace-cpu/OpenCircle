// Copyright OpenCircle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OpenCircleGameMode.generated.h"

/**
 *  Server-only match rules. Wires up the project GameState, PlayerState and PlayerController.
 */
UCLASS()
class AOpenCircleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	AOpenCircleGameMode();
};
