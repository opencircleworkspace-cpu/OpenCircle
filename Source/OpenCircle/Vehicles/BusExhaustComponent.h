// Copyright OpenCircle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraComponent.h"
#include "BusExhaustComponent.generated.h"

class UBusDrivetrainComponent;

/**
 *  Diesel exhaust: drives a Niagara system's user parameters from engine load. Light haze at idle, a dark puff
 *  when the throttle snaps open (turbo lag), heavier smoke under load. Cosmetic only: runs locally on every machine.
 *
 *  Niagara system user parameters read (all optional): float SpawnRate, float Darkness (0 grey .. 1 black),
 *  float Size (multiplier), vector Velocity (world, cm/s: exhaust jet + bus motion).
 */
UCLASS(ClassGroup=(Bus), meta=(BlueprintSpawnableComponent))
class UBusExhaustComponent : public UNiagaraComponent
{
	GENERATED_BODY()

public:

	UBusExhaustComponent();

	void SetDrivetrain(UBusDrivetrainComponent* InDrivetrain) { Drivetrain = InDrivetrain; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 0..1 throttle from the bus this frame */
	float Throttle = 0.f;

protected:

	UPROPERTY(EditAnywhere, Category="Exhaust") FVector2D SpawnRate = FVector2D(6.f, 60.f);
	/** Extra particles/s during a throttle puff */
	UPROPERTY(EditAnywhere, Category="Exhaust") float PuffSpawnRate = 120.f;
	/** Puff decays over this many seconds */
	UPROPERTY(EditAnywhere, Category="Exhaust") float PuffTime = 0.8f;
	UPROPERTY(EditAnywhere, Category="Exhaust") FVector2D Darkness = FVector2D(0.15f, 0.55f);
	UPROPERTY(EditAnywhere, Category="Exhaust") FVector2D Size = FVector2D(0.6f, 1.6f);
	/** Exhaust gas speed out of the pipe at idle / full load (cm/s), along the component's +X */
	UPROPERTY(EditAnywhere, Category="Exhaust") FVector2D JetSpeed = FVector2D(150.f, 600.f);

private:

	TWeakObjectPtr<UBusDrivetrainComponent> Drivetrain;
	float LastThrottle = 0.f;
	float Puff = 0.f;
};
