// Copyright OpenCircle. All Rights Reserved.

#include "Core/OpenCirclePlayerState.h"
#include "Net/UnrealNetwork.h"

void AOpenCirclePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AOpenCirclePlayerState, BusRole);
	DOREPLIFETIME(AOpenCirclePlayerState, DuoIndex);
}

void AOpenCirclePlayerState::SetBusRole(EBusRole NewRole)
{
	if (!HasAuthority() || BusRole == NewRole)
	{
		return;
	}

	BusRole = NewRole;

	// RepNotify does not run on the server, so call it manually
	OnRep_BusRole();
}

void AOpenCirclePlayerState::SetDuoIndex(int32 NewDuoIndex)
{
	if (HasAuthority())
	{
		DuoIndex = NewDuoIndex;
	}
}

void AOpenCirclePlayerState::OnRep_BusRole()
{
	OnBusRoleChanged.Broadcast(BusRole);
}
