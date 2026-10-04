// Copyright OpenCircle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BusDrivetrainComponent.generated.h"

class UChaosWheeledVehicleMovementComponent;

UENUM(BlueprintType)
enum class EBusEngineState : uint8
{
	Off,
	Cranking,
	Running
};

UENUM(BlueprintType)
enum class EBusGearboxMode : uint8
{
	/** Automated manual (AMT): computer works clutch and shifts */
	Automatic,
	/** Driver works clutch and H-pattern shifts */
	Manual
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBusGearChanged, int32, NewGear);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBusDrivetrainEvent);

/**
 *  Bus drivetrain: turbo diesel, dry clutch, 6-speed gearbox, final drive and service brakes.
 *  Replaces the Chaos mechanical simulation; drives the wheels through per-wheel drive/brake torque.
 *  Defaults model a Tata LPO 1612 (Cummins 6BTAA 5.9: 135 bhp @ 2400, 490 Nm @ 1400-1600).
 *  Gear index: -1 reverse, 0 neutral, 1..N forward.
 */
UCLASS(ClassGroup=(Bus), meta=(BlueprintSpawnableComponent))
class UBusDrivetrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UBusDrivetrainComponent();

	virtual void BeginPlay() override;

	/** Runs the drivetrain and writes wheel torques. Throttle/Brake/Clutch 0..1, Handbrake holds driven wheels. */
	void Update(float DeltaTime, float Throttle, float Brake, float Clutch, bool bHandbrake);

	// ---- Driver commands ----
	UFUNCTION(BlueprintCallable, Category="Bus|Drivetrain")
	void ToggleIgnition();

	/** Manual: next/previous gear in R-N-1..N order. Needs the clutch pressed. */
	UFUNCTION(BlueprintCallable, Category="Bus|Drivetrain")
	void RequestShift(int32 Delta);

	UFUNCTION(BlueprintCallable, Category="Bus|Drivetrain")
	void ToggleGearboxMode();

	UFUNCTION(BlueprintCallable, Category="Bus|Drivetrain")
	void ToggleExhaustBrake() { bExhaustBrake = !bExhaustBrake; }

	// ---- State ----
	UFUNCTION(BlueprintPure, Category="Bus|Drivetrain") float GetEngineRpm() const { return EngineRpm; }
	UFUNCTION(BlueprintPure, Category="Bus|Drivetrain") int32 GetGear() const { return Gear; }
	UFUNCTION(BlueprintPure, Category="Bus|Drivetrain") EBusEngineState GetEngineState() const { return EngineState; }
	UFUNCTION(BlueprintPure, Category="Bus|Drivetrain") EBusGearboxMode GetGearboxMode() const { return Mode; }
	UFUNCTION(BlueprintPure, Category="Bus|Drivetrain") float GetTurboBoost() const { return Boost; }
	UFUNCTION(BlueprintPure, Category="Bus|Drivetrain") float GetClutchEngagement() const { return ClutchEngagement; }
	UFUNCTION(BlueprintPure, Category="Bus|Drivetrain") bool IsClutchLocked() const { return bClutchLocked; }
	UFUNCTION(BlueprintPure, Category="Bus|Drivetrain") bool IsShifting() const { return ShiftTimer > 0.f; }
	UFUNCTION(BlueprintPure, Category="Bus|Drivetrain") bool IsExhaustBrakeOn() const { return bExhaustBrake; }
	UFUNCTION(BlueprintPure, Category="Bus|Drivetrain") float GetEngineTorque() const { return EngineTorque; }

	/** Automatic: select drive or reverse (only when stopped; caller checks speed) */
	void SelectAutoDirection(bool bReverse);

	UPROPERTY(BlueprintAssignable, Category="Bus|Drivetrain") FOnBusGearChanged OnGearChanged;
	UPROPERTY(BlueprintAssignable, Category="Bus|Drivetrain") FOnBusDrivetrainEvent OnStall;
	/** Shift refused (clutch not pressed / too fast for reverse); play a gear grind */
	UPROPERTY(BlueprintAssignable, Category="Bus|Drivetrain") FOnBusDrivetrainEvent OnGearGrind;

protected:

	// ---- Engine ----
	/** Full-load torque (Nm) against rpm; linear between points */
	UPROPERTY(EditAnywhere, Category="Bus|Engine")
	TArray<FVector2D> TorqueCurve;

	UPROPERTY(EditAnywhere, Category="Bus|Engine") float IdleRpm = 650.f;
	/** Governor starts cutting fuel here and reaches zero at MaxRpm */
	UPROPERTY(EditAnywhere, Category="Bus|Engine") float GovernorRpm = 2500.f;
	UPROPERTY(EditAnywhere, Category="Bus|Engine") float MaxRpm = 2700.f;
	/** Below this with the clutch locked the engine stalls */
	UPROPERTY(EditAnywhere, Category="Bus|Engine") float StallRpm = 380.f;
	/** Engine + flywheel inertia, kg m2 */
	UPROPERTY(EditAnywhere, Category="Bus|Engine") float EngineInertia = 1.2f;
	/** Internal friction and pumping: A + B * rpm (Nm). Gives engine braking. */
	UPROPERTY(EditAnywhere, Category="Bus|Engine") float FrictionTorqueBase = 30.f;
	UPROPERTY(EditAnywhere, Category="Bus|Engine") float FrictionTorquePerRpm = 0.035f;
	/** Exhaust brake retarding torque per rpm when off-throttle */
	UPROPERTY(EditAnywhere, Category="Bus|Engine") float ExhaustBrakeTorquePerRpm = 0.12f;
	UPROPERTY(EditAnywhere, Category="Bus|Engine") float StarterTorque = 150.f;
	UPROPERTY(EditAnywhere, Category="Bus|Engine") bool bStartRunning = true;

	// ---- Turbo ----
	/** Fraction of curve torque available with no boost */
	UPROPERTY(EditAnywhere, Category="Bus|Turbo", meta=(ClampMin="0", ClampMax="1")) float NaturallyAspiratedFraction = 0.65f;
	/** Boost needs exhaust flow: none below this rpm, full from BoostFullRpm */
	UPROPERTY(EditAnywhere, Category="Bus|Turbo") float BoostStartRpm = 900.f;
	UPROPERTY(EditAnywhere, Category="Bus|Turbo") float BoostFullRpm = 1500.f;
	UPROPERTY(EditAnywhere, Category="Bus|Turbo") float SpoolUpTime = 0.8f;
	UPROPERTY(EditAnywhere, Category="Bus|Turbo") float SpoolDownTime = 0.4f;

	// ---- Clutch ----
	/** Torque the clutch can carry fully engaged, Nm */
	UPROPERTY(EditAnywhere, Category="Bus|Clutch") float ClutchCapacity = 900.f;
	/** Pedal travel (0 up, 1 down) where the clutch starts to bite and where it is fully free */
	UPROPERTY(EditAnywhere, Category="Bus|Clutch") float ClutchBitePoint = 0.65f;
	UPROPERTY(EditAnywhere, Category="Bus|Clutch") float ClutchFreePoint = 0.85f;
	/** Automatic launch: clutch engagement rises between these engine rpm */
	UPROPERTY(EditAnywhere, Category="Bus|Clutch") float AutoLaunchStartRpm = 750.f;
	UPROPERTY(EditAnywhere, Category="Bus|Clutch") float AutoLaunchFullRpm = 1300.f;
	/** Automatic launch: clutch engagement gained per second while on the throttle */
	UPROPERTY(EditAnywhere, Category="Bus|Clutch") float AutoLaunchRate = 1.2f;

	// ---- Gearbox ----
	UPROPERTY(EditAnywhere, Category="Bus|Gearbox") TArray<float> ForwardRatios;
	UPROPERTY(EditAnywhere, Category="Bus|Gearbox") float ReverseRatio = 6.3f;
	UPROPERTY(EditAnywhere, Category="Bus|Gearbox") float FinalDriveRatio = 5.857f;
	UPROPERTY(EditAnywhere, Category="Bus|Gearbox") float DrivelineEfficiency = 0.9f;
	/** Time in neutral during a shift (synchromesh, unsynchronised trucks are longer) */
	UPROPERTY(EditAnywhere, Category="Bus|Gearbox") float ShiftTime = 0.7f;
	/** Reverse only engages below this speed */
	UPROPERTY(EditAnywhere, Category="Bus|Gearbox") float ReverseMaxSpeedKmh = 3.f;
	/** Automatic shift points: light throttle .. full throttle */
	UPROPERTY(EditAnywhere, Category="Bus|Gearbox") FVector2D UpshiftRpm = FVector2D(1500.f, 2250.f);
	UPROPERTY(EditAnywhere, Category="Bus|Gearbox") float DownshiftRpm = 1000.f;
	/** Full throttle below this rpm drops a gear (kickdown) */
	UPROPERTY(EditAnywhere, Category="Bus|Gearbox") float KickdownRpm = 1350.f;

	// ---- Brakes (Nm per wheel) ----
	UPROPERTY(EditAnywhere, Category="Bus|Brakes") float FrontBrakeTorque = 9000.f;
	UPROPERTY(EditAnywhere, Category="Bus|Brakes") float RearBrakeTorque = 12000.f;
	UPROPERTY(EditAnywhere, Category="Bus|Brakes") float HandbrakeTorque = 20000.f;
	/** Tyre rolling resistance as a constant drag torque per wheel */
	UPROPERTY(EditAnywhere, Category="Bus|Brakes") float RollingResistanceTorque = 120.f;

	/** Chaos wheel indices driven by the rear axle */
	UPROPERTY(EditAnywhere, Category="Bus|Drivetrain") TArray<int32> DrivenWheels = { 2, 3 };

private:

	float GetCurveTorque(float Rpm) const;
	float GetTotalRatio(int32 InGear) const;
	float GetDrivenWheelRpm(const UChaosWheeledVehicleMovementComponent* Movement) const;
	void UpdateAutomatic(float DeltaTime, float Throttle, float GearboxRpm, float SpeedKmh);
	void SetGear(int32 NewGear);
	void StartShift(int32 TargetGear);

	UChaosWheeledVehicleMovementComponent* GetMovement() const;

	EBusEngineState EngineState = EBusEngineState::Off;
	EBusGearboxMode Mode = EBusGearboxMode::Automatic;
	float EngineRpm = 0.f;
	float EngineTorque = 0.f;
	float Boost = 0.f;
	float ClutchEngagement = 0.f;
	float ShiftTimer = 0.f;
	float ShiftCooldown = 0.f;
	float CrankTimer = 0.f;
	int32 Gear = 0;
	int32 PendingGear = 0;
	bool bClutchLocked = false;
	bool bExhaustBrake = false;
	bool bManualClutchDown = false;
};
