// Copyright OpenCircle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Core/BusTypes.h"
#include "OpenCirclePlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBusRoleChanged, EBusRole, NewRole);

/**
 *  Per-player match data: role and duo (bus) index. Server-authoritative, replicated.
 */
UCLASS()
class AOpenCirclePlayerState : public APlayerState
{
	GENERATED_BODY()

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Sets the role. Server only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Bus")
	void SetBusRole(EBusRole NewRole);

	/** Sets which duo/bus this player belongs to. Server only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Bus")
	void SetDuoIndex(int32 NewDuoIndex);

	UFUNCTION(BlueprintPure, Category="Bus")
	EBusRole GetBusRole() const { return BusRole; }

	UFUNCTION(BlueprintPure, Category="Bus")
	int32 GetDuoIndex() const { return DuoIndex; }

	/** Fires on server and clients when the role changes */
	UPROPERTY(BlueprintAssignable, Category="Bus")
	FOnBusRoleChanged OnBusRoleChanged;

protected:

	UPROPERTY(ReplicatedUsing=OnRep_BusRole, BlueprintReadOnly, Category="Bus")
	EBusRole BusRole = EBusRole::None;

	/** INDEX_NONE until assigned to a duo */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Bus")
	int32 DuoIndex = INDEX_NONE;

	UFUNCTION()
	void OnRep_BusRole();
};
