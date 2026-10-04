// Copyright OpenCircle. All Rights Reserved.

#include "Vehicles/BusWheels.h"

UBusWheelFront::UBusWheelFront()
{
	AxleType = EAxleType::Front;
	WheelRadius = 52.7f;
	WheelWidth = 28.f;
	WheelMass = 60.f;
	bAffectedBySteering = true;
	bAffectedByEngine = false;
	bAffectedByHandbrake = false;
	MaxSteerAngle = 38.f;
	MaxBrakeTorque = 9000.f;
	// Front axle ~4.5 t (2.25 t per wheel), ~9x a car wheel
	SuspensionMaxRaise = 12.f;
	SuspensionMaxDrop = 12.f;
	// SpringRate ~ N/cm: wheel load (N) / 5 cm sag
	SpringRate = 4500.f;
	SpringPreload = 50.f;
	CorneringStiffness = 1200.f;
}

UBusWheelRear::UBusWheelRear()
{
	AxleType = EAxleType::Rear;
	WheelRadius = 52.7f;
	WheelWidth = 56.f;
	WheelMass = 110.f;
	bAffectedBySteering = false;
	bAffectedByEngine = true;
	bAffectedByHandbrake = true;
	MaxSteerAngle = 0.f;
	MaxBrakeTorque = 12000.f;
	MaxHandBrakeTorque = 20000.f;
	// Rear axle ~6.5 t (3.25 t per wheel)
	SuspensionMaxRaise = 12.f;
	SuspensionMaxDrop = 12.f;
	SpringRate = 6500.f;
	SpringPreload = 50.f;
	CorneringStiffness = 1600.f;
}
