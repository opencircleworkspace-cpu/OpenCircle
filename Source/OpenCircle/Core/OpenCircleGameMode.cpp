// Copyright OpenCircle. All Rights Reserved.

#include "Core/OpenCircleGameMode.h"
#include "Core/OpenCircleGameState.h"
#include "Core/OpenCirclePlayerState.h"
#include "Player/OpenCirclePlayerController.h"
#include "Player/OpenCircleCharacter.h"
#include "UI/BusHUD.h"

AOpenCircleGameMode::AOpenCircleGameMode()
{
	GameStateClass = AOpenCircleGameState::StaticClass();
	PlayerStateClass = AOpenCirclePlayerState::StaticClass();
	PlayerControllerClass = AOpenCirclePlayerController::StaticClass();
	DefaultPawnClass = AOpenCircleCharacter::StaticClass();
	HUDClass = ABusHUD::StaticClass();
}
