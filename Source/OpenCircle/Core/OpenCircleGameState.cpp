// Copyright OpenCircle. All Rights Reserved.

#include "Core/OpenCircleGameState.h"
#include "Net/UnrealNetwork.h"

void AOpenCircleGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AOpenCircleGameState, MatchPhase);
	DOREPLIFETIME(AOpenCircleGameState, RaceStartServerTime);
}

void AOpenCircleGameState::SetMatchPhase(EMatchPhase NewPhase)
{
	if (!HasAuthority() || MatchPhase == NewPhase)
	{
		return;
	}

	MatchPhase = NewPhase;

	if (MatchPhase == EMatchPhase::Racing)
	{
		RaceStartServerTime = GetServerWorldTimeSeconds();
	}

	// RepNotify does not run on the server, so call it manually
	OnRep_MatchPhase();
}

float AOpenCircleGameState::GetRaceElapsedTime() const
{
	if (MatchPhase != EMatchPhase::Racing)
	{
		return 0.f;
	}

	return static_cast<float>(GetServerWorldTimeSeconds() - RaceStartServerTime);
}

void AOpenCircleGameState::OnRep_MatchPhase()
{
	OnMatchPhaseChanged.Broadcast(MatchPhase);
}
