// Copyright OpenCircle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BusStop.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class ABusVehicle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBusServed, ABusStop*, Stop, ABusVehicle*, Bus);

/**
 *  A bus stop. A bus serves it by stopping inside the zone with a door open for DwellTime.
 */
UCLASS()
class ABusStop : public AActor
{
	GENERATED_BODY()

public:

	ABusStop();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category="Bus Stop")
	const FText& GetStopName() const { return StopName; }

	void SetStopName(const FText& InName) { StopName = InName; }

	/** 0..1 progress of the current dwell */
	UFUNCTION(BlueprintPure, Category="Bus Stop")
	float GetDwellProgress() const { return DwellTimer / FMath::Max(DwellTime, KINDA_SMALL_NUMBER); }

	UFUNCTION(BlueprintPure, Category="Bus Stop")
	bool IsServed() const { return bServed; }

	UPROPERTY(BlueprintAssignable, Category="Bus Stop")
	FOnBusServed OnBusServed;

protected:

	UPROPERTY(VisibleAnywhere, Category="Bus Stop")
	TObjectPtr<UBoxComponent> Zone;

	UPROPERTY(VisibleAnywhere, Category="Bus Stop")
	TObjectPtr<UStaticMeshComponent> Marker;

	UPROPERTY(EditAnywhere, Category="Bus Stop")
	FText StopName = NSLOCTEXT("BusStop", "Default", "Bus Stop");

	/** Seconds stopped with doors open to serve the stop */
	UPROPERTY(EditAnywhere, Category="Bus Stop")
	float DwellTime = 3.f;

	/** Counts as stopped below this speed */
	UPROPERTY(EditAnywhere, Category="Bus Stop")
	float StoppedSpeedKmh = 2.f;

private:

	TWeakObjectPtr<ABusVehicle> LastBus;
	float DwellTimer = 0.f;
	bool bServed = false;
};
