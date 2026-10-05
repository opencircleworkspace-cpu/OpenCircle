// Copyright OpenCircle. All Rights Reserved.

#include "Vehicles/BusExhaustComponent.h"
#include "Vehicles/BusDrivetrainComponent.h"

UBusExhaustComponent::UBusExhaustComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void UBusExhaustComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const UBusDrivetrainComponent* Engine = Drivetrain.Get();
	const float Rpm = Engine ? Engine->GetEngineRpm() : 0.f;
	if (!Engine || Rpm < 100.f)
	{
		SetVariableFloat(TEXT("SpawnRate"), 0.f);            // engine off
		return;
	}
	// Load = throttle weighted by revs; a sudden throttle rise gives the classic black diesel puff
	const float Revs = FMath::Clamp(Rpm / 2700.f, 0.f, 1.f);
	const float Load = FMath::Clamp(Throttle * (0.4f + 0.6f * Revs), 0.f, 1.f);
	if (Throttle - LastThrottle > 0.25f)
	{
		Puff = 1.f;
	}
	LastThrottle = Throttle;
	Puff = FMath::Max(Puff - DeltaTime / FMath::Max(PuffTime, 0.05f), 0.f);

	const FVector Jet = GetComponentTransform().GetUnitAxis(EAxis::X) * FMath::Lerp(JetSpeed.X, JetSpeed.Y, Load);
	const FVector BusVelocity = GetOwner() ? GetOwner()->GetVelocity() * 0.3f : FVector::ZeroVector;    // smoke lags behind the bus
	SetVariableFloat(TEXT("SpawnRate"), FMath::Lerp(SpawnRate.X, SpawnRate.Y, Load) + PuffSpawnRate * Puff);
	SetVariableFloat(TEXT("Darkness"), FMath::Clamp(FMath::Lerp(Darkness.X, Darkness.Y, Load) + Puff * 0.4f, 0.f, 1.f));
	SetVariableFloat(TEXT("Size"), FMath::Lerp(Size.X, Size.Y, FMath::Max(Load, Puff)));
	SetVariableVec3(TEXT("Velocity"), Jet + BusVelocity);
}
