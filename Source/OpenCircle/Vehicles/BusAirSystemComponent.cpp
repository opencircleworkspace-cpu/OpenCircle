// Copyright OpenCircle. All Rights Reserved.

#include "Vehicles/BusAirSystemComponent.h"

UBusAirSystemComponent::UBusAirSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UBusAirSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Pressure = FMath::Min(MaxPressure, Pressure + CompressorRate * DeltaTime);
}

void UBusAirSystemComponent::ConsumeBrake(float Brake, float DeltaTime)
{
	Pressure = FMath::Max(0.f, Pressure - Brake * BrakeUseRate * DeltaTime);
}

void UBusAirSystemComponent::ConsumeDoorStroke()
{
	Pressure = FMath::Max(0.f, Pressure - DoorStrokeCost);
	OnAirHiss.Broadcast();
}

void UBusAirSystemComponent::ToggleParkingBrake()
{
	if (bParkingBrake && Pressure < ReleasePressure)
	{
		return;
	}

	bParkingBrake = !bParkingBrake;
	OnAirHiss.Broadcast();
}
