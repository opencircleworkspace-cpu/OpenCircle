// Copyright OpenCircle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/BusTypes.h"
#include "OpenCircleGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchPhaseChanged, EMatchPhase, NewPhase);

/**
 *  Match-wide state every client can read: current phase and race timer.
 */
UCLASS()
class AOpenCircleGameState : public AGameStateBase
{
	GENERATED_BODY()

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Moves the match to a new phase. Server only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Match")
	void SetMatchPhase(EMatchPhase NewPhase);

	UFUNCTION(BlueprintPure, Category="Match")
	EMatchPhase GetMatchPhase() const { return MatchPhase; }

	/** Seconds since the race started, 0 if not racing */
	UFUNCTION(BlueprintPure, Category="Match")
	float GetRaceElapsedTime() const;

	UPROPERTY(BlueprintAssignable, Category="Match")
	FOnMatchPhaseChanged OnMatchPhaseChanged;

protected:

	UPROPERTY(ReplicatedUsing=OnRep_MatchPhase, BlueprintReadOnly, Category="Match")
	EMatchPhase MatchPhase = EMatchPhase::Lobby;

	/** Server world time when Racing began */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Match")
	double RaceStartServerTime = 0.0;

	UFUNCTION()
	void OnRep_MatchPhase();
};
