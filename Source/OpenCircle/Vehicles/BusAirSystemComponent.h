// Copyright OpenCircle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BusAirSystemComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAirHiss);

/**
 *  Air brake system: a compressor refills the tank, braking and doors use air.
 *  Low pressure locks the spring (parking) brakes until the tank recovers.
 */
UCLASS(ClassGroup=(Bus), meta=(BlueprintSpawnableComponent))
class UBusAirSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UBusAirSystemComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Air used by the service brake this frame (Brake 0..1) */
	void ConsumeBrake(float Brake, float DeltaTime);

	/** One door open or close stroke */
	void ConsumeDoorStroke();

	/** Toggles the parking brake. Release is refused below ReleasePressure. */
	UFUNCTION(BlueprintCallable, Category="Bus|Air")
	void ToggleParkingBrake();

	/** True when the parking brake is set or spring brakes are forced on by low air */
	UFUNCTION(BlueprintPure, Category="Bus|Air")
	bool IsParkingBrakeApplied() const { return bParkingBrake || Pressure < SpringBrakePressure; }

	UFUNCTION(BlueprintPure, Category="Bus|Air")
	float GetPressure() const { return Pressure; }

	UFUNCTION(BlueprintPure, Category="Bus|Air")
	bool IsLowPressure() const { return Pressure < WarningPressure; }

	/** Fires when air is released (parking brake, doors); hook up the hiss sound in Blueprint */
	UPROPERTY(BlueprintAssignable, Category="Bus|Air")
	FOnAirHiss OnAirHiss;

protected:

	/** Tank pressure in bar */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Bus|Air")
	float Pressure = 6.0f;

	UPROPERTY(EditAnywhere, Category="Bus|Air")
	float MaxPressure = 8.5f;

	/** Below this the dashboard warns */
	UPROPERTY(EditAnywhere, Category="Bus|Air")
	float WarningPressure = 5.5f;

	/** Below this the spring brakes lock on */
	UPROPERTY(EditAnywhere, Category="Bus|Air")
	float SpringBrakePressure = 4.0f;

	/** Parking brake can only be released at or above this */
	UPROPERTY(EditAnywhere, Category="Bus|Air")
	float ReleasePressure = 5.0f;

	/** Compressor refill, bar per second */
	UPROPERTY(EditAnywhere, Category="Bus|Air")
	float CompressorRate = 0.25f;

	/** Air used at full brake, bar per second */
	UPROPERTY(EditAnywhere, Category="Bus|Air")
	float BrakeUseRate = 0.35f;

	UPROPERTY(EditAnywhere, Category="Bus|Air")
	float DoorStrokeCost = 0.15f;

	UPROPERTY(VisibleInstanceOnly, Category="Bus|Air")
	bool bParkingBrake = false;
};
