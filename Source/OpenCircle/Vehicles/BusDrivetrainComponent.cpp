// Copyright OpenCircle. All Rights Reserved.

#include "Vehicles/BusDrivetrainComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"

namespace BusDrivetrain
{
	constexpr float RpmToRadS = PI / 30.f;
	constexpr float RadSToRpm = 30.f / PI;
	constexpr float CmPerSecToKmh = 0.036f;
	/** Clutch counts as synchronised (lockable) within this slip */
	constexpr float LockSlipRpm = 40.f;
	constexpr int32 SubSteps = 4;
}

UBusDrivetrainComponent::UBusDrivetrainComponent()
{
	PrimaryComponentTick.bCanEverTick = false;     // driven by ABusVehicle::Tick, before it reads the results

	// Cummins 6BTAA 5.9 full-load curve (Nm): peak 490 @ 1400-1600, 400 @ 2400 (135 bhp)
	TorqueCurve = { {600.f, 260.f}, {800.f, 330.f}, {1000.f, 400.f}, {1200.f, 460.f}, {1400.f, 490.f},
		{1600.f, 490.f}, {1800.f, 470.f}, {2000.f, 445.f}, {2200.f, 420.f}, {2400.f, 400.f}, {2600.f, 370.f} };
	// 6-speed bus gearbox; with the 5.857 axle: ~12 km/h in 1st, ~100 km/h in 6th at 2400 rpm
	ForwardRatios = { 6.72f, 4.10f, 2.47f, 1.56f, 1.0f, 0.78f };
}

void UBusDrivetrainComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bStartRunning)
	{
		EngineState = EBusEngineState::Running;
		EngineRpm = IdleRpm;
	}
	if (Mode == EBusGearboxMode::Automatic)
	{
		Gear = 1;     // automatic sits in drive
	}
}

UChaosWheeledVehicleMovementComponent* UBusDrivetrainComponent::GetMovement() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UChaosWheeledVehicleMovementComponent>() : nullptr;
}

float UBusDrivetrainComponent::GetCurveTorque(float Rpm) const
{
	if (TorqueCurve.Num() == 0)
	{
		return 0.f;
	}
	if (Rpm <= TorqueCurve[0].X)
	{
		return TorqueCurve[0].Y * FMath::Clamp(Rpm / TorqueCurve[0].X, 0.f, 1.f);
	}
	for (int32 i = 1; i < TorqueCurve.Num(); ++i)
	{
		if (Rpm <= TorqueCurve[i].X)
		{
			const float Alpha = (Rpm - TorqueCurve[i - 1].X) / (TorqueCurve[i].X - TorqueCurve[i - 1].X);
			return FMath::Lerp(TorqueCurve[i - 1].Y, TorqueCurve[i].Y, Alpha);
		}
	}
	return TorqueCurve.Last().Y;
}

float UBusDrivetrainComponent::GetTotalRatio(int32 InGear) const
{
	if (InGear < 0)
	{
		return -ReverseRatio * FinalDriveRatio;
	}
	return ForwardRatios.IsValidIndex(InGear - 1) ? ForwardRatios[InGear - 1] * FinalDriveRatio : 0.f;
}

float UBusDrivetrainComponent::GetDrivenWheelRpm(const UChaosWheeledVehicleMovementComponent* Movement) const
{
	float Sum = 0.f;
	int32 Count = 0;
	for (int32 Index : DrivenWheels)
	{
		if (Movement->Wheels.IsValidIndex(Index) && Movement->Wheels[Index])
		{
			// Chaos reports rad/s, positive rolling forward
			Sum += Movement->Wheels[Index]->GetWheelAngularVelocity() * BusDrivetrain::RadSToRpm;
			++Count;
		}
	}
	return Count > 0 ? Sum / Count : 0.f;
}

void UBusDrivetrainComponent::Update(float DeltaTime, float Throttle, float Brake, float Clutch, bool bHandbrake)
{
	UChaosWheeledVehicleMovementComponent* Movement = GetMovement();
	if (!Movement || DeltaTime <= 0.f)
	{
		return;
	}

	const float SpeedKmh = Movement->GetForwardSpeed() * BusDrivetrain::CmPerSecToKmh;
	const float WheelRpm = GetDrivenWheelRpm(Movement);
	bManualClutchDown = Clutch > ClutchFreePoint;

	// ---- Gearbox: shift in progress sits in neutral, then engages the target ----
	if (ShiftTimer > 0.f)
	{
		ShiftTimer -= DeltaTime;
		if (ShiftTimer <= 0.f)
		{
			SetGear(PendingGear);
		}
	}
	ShiftCooldown = FMath::Max(0.f, ShiftCooldown - DeltaTime);

	const float Ratio = GetTotalRatio(Gear);
	const float GearboxRpm = WheelRpm * Ratio;      // engine-side speed of the clutch disc

	if (Mode == EBusGearboxMode::Automatic && EngineState == EBusEngineState::Running)
	{
		UpdateAutomatic(DeltaTime, Throttle, GearboxRpm, SpeedKmh);
	}

	// ---- Clutch engagement 0..1 ----
	float TargetEngagement;
	float EngageRate = 6.f;                 // pedal / actuator speed, engagement per second
	if (Gear == 0 || ShiftTimer > 0.f)
	{
		TargetEngagement = 0.f;
	}
	else if (Mode == EBusGearboxMode::Manual)
	{
		TargetEngagement = 1.f - FMath::SmoothStep(ClutchBitePoint, ClutchFreePoint, Clutch);
	}
	else
	{
		// Automated launch: engage progressively while the driver is on the throttle,
		// backing off only if the engine is pulled down toward a stall
		const float StallGuard = FMath::SmoothStep(StallRpm + 150.f, AutoLaunchStartRpm, EngineRpm);
		if (bClutchLocked)
		{
			TargetEngagement = 1.f;
		}
		else if (Throttle < 0.02f && FMath::Abs(SpeedKmh) < 2.f)
		{
			TargetEngagement = 0.f;        // no creep: stand still in gear without throttle
		}
		else
		{
			TargetEngagement = StallGuard;
			EngageRate = AutoLaunchRate;
		}
	}
	ClutchEngagement = FMath::FInterpConstantTo(ClutchEngagement, TargetEngagement, DeltaTime, EngageRate);
	const float ClutchTorqueCap = ClutchCapacity * ClutchEngagement;

	// ---- Turbo ----
	const float BoostTarget = EngineState == EBusEngineState::Running
		? Throttle * FMath::SmoothStep(BoostStartRpm, BoostFullRpm, EngineRpm) : 0.f;
	const float SpoolTime = BoostTarget > Boost ? SpoolUpTime : SpoolDownTime;
	Boost = FMath::FInterpTo(Boost, BoostTarget, DeltaTime, 1.f / FMath::Max(SpoolTime, 0.05f));

	// ---- Engine and clutch, substepped ----
	const float Dt = DeltaTime / BusDrivetrain::SubSteps;
	float ClutchTorque = 0.f;
	for (int32 Step = 0; Step < BusDrivetrain::SubSteps; ++Step)
	{
		// Fuel: driver throttle, idle governor floor, top governor cut
		float Fuel = 0.f;
		if (EngineState == EBusEngineState::Running)
		{
			const float IdleFuel = FMath::Clamp((IdleRpm - EngineRpm) / 150.f, 0.f, 1.f);
			const float ShiftCut = ShiftTimer > 0.f && Mode == EBusGearboxMode::Automatic ? 0.f : 1.f;
			Fuel = FMath::Max(Throttle * ShiftCut, IdleFuel);
			Fuel *= 1.f - FMath::SmoothStep(GovernorRpm, MaxRpm, EngineRpm);
		}

		const float Available = GetCurveTorque(EngineRpm) * (NaturallyAspiratedFraction + (1.f - NaturallyAspiratedFraction) * Boost);
		const float Friction = EngineRpm > 1.f ? FrictionTorqueBase + FrictionTorquePerRpm * EngineRpm : 0.f;
		const float Exhaust = bExhaustBrake && Throttle < 0.05f ? ExhaustBrakeTorquePerRpm * EngineRpm : 0.f;
		const float Starter = EngineState == EBusEngineState::Cranking ? StarterTorque : 0.f;
		EngineTorque = Available * Fuel + Starter - Friction - Exhaust;

		// Clutch: locked = engine turns with the wheels and passes its torque through
		const float Slip = EngineRpm - GearboxRpm;
		if (bClutchLocked)
		{
			const bool bAutoDeclutch = Mode == EBusGearboxMode::Automatic && GearboxRpm < AutoLaunchStartRpm;
			if (ClutchTorqueCap < FMath::Abs(EngineTorque) || ClutchEngagement < 0.95f || bAutoDeclutch)
			{
				bClutchLocked = false;
			}
		}
		else if (Mode == EBusGearboxMode::Automatic && GearboxRpm < AutoLaunchStartRpm)
		{
			// automated clutch never holds the engine below launch rpm (no stalling when stopping)
		}
		else if (ClutchEngagement > 0.95f && FMath::Abs(Slip) < BusDrivetrain::LockSlipRpm && ClutchTorqueCap >= FMath::Abs(EngineTorque))
		{
			bClutchLocked = true;
		}

		if (bClutchLocked)
		{
			EngineRpm = GearboxRpm;
			ClutchTorque = EngineTorque;
		}
		else
		{
			ClutchTorque = ClutchTorqueCap * FMath::Clamp(Slip / 60.f, -1.f, 1.f);
			EngineRpm += (EngineTorque - ClutchTorque) / EngineInertia * Dt * BusDrivetrain::RadSToRpm;
		}
		EngineRpm = FMath::Clamp(EngineRpm, 0.f, MaxRpm + 200.f);
	}

	// ---- Engine state ----
	if (EngineState == EBusEngineState::Cranking)
	{
		CrankTimer += DeltaTime;
		if (EngineRpm > 400.f && CrankTimer > 0.6f)
		{
			EngineState = EBusEngineState::Running;
		}
		else if (CrankTimer > 3.f)
		{
			EngineState = EBusEngineState::Off;       // failed start (e.g. in gear with the clutch up)
		}
	}
	else if (EngineState == EBusEngineState::Running && EngineRpm < StallRpm)
	{
		EngineState = EBusEngineState::Off;
		bClutchLocked = false;
		OnStall.Broadcast();
	}

	// ---- Wheels: drive torque on the driven axle, brakes everywhere ----
	const float WheelTorque = ClutchTorque * Ratio * DrivelineEfficiency;
	const float BrakeCurve = Brake * Brake;            // progressive pedal
	for (int32 i = 0; i < Movement->Wheels.Num(); ++i)
	{
		const bool bDriven = DrivenWheels.Contains(i);
		Movement->SetDriveTorque(bDriven ? WheelTorque / FMath::Max(1, DrivenWheels.Num()) : 0.f, i);

		float BrakeTorque = RollingResistanceTorque + BrakeCurve * (bDriven ? RearBrakeTorque : FrontBrakeTorque);
		if (bHandbrake && bDriven)
		{
			BrakeTorque += HandbrakeTorque;
		}
		Movement->SetBrakeTorque(BrakeTorque, i);
	}
}

void UBusDrivetrainComponent::UpdateAutomatic(float DeltaTime, float Throttle, float GearboxRpm, float SpeedKmh)
{
	if (ShiftTimer > 0.f || ShiftCooldown > 0.f || Gear <= 0)
	{
		return;
	}

	const float EngineSideRpm = bClutchLocked ? EngineRpm : GearboxRpm;
	const float UpAt = FMath::Lerp(UpshiftRpm.X, UpshiftRpm.Y, Throttle);
	if (Gear < ForwardRatios.Num() && bClutchLocked && EngineSideRpm > UpAt)
	{
		StartShift(Gear + 1);
	}
	else if (Gear > 1 && (EngineSideRpm < DownshiftRpm || (Throttle > 0.95f && EngineSideRpm < KickdownRpm)))
	{
		// Only drop if the lower gear will not over-rev
		const float LowerRpm = EngineSideRpm * GetTotalRatio(Gear - 1) / GetTotalRatio(Gear);
		if (LowerRpm < GovernorRpm - 150.f)
		{
			StartShift(Gear - 1);
		}
	}
}

void UBusDrivetrainComponent::StartShift(int32 TargetGear)
{
	PendingGear = TargetGear;
	SetGear(0);
	ShiftTimer = ShiftTime;
	ShiftCooldown = ShiftTime + 1.f;    // no hunting between gears
}

void UBusDrivetrainComponent::SetGear(int32 NewGear)
{
	if (Gear != NewGear)
	{
		Gear = NewGear;
		bClutchLocked = false;
		if (NewGear != 0 || ShiftTimer <= 0.f)
		{
			OnGearChanged.Broadcast(Gear);
		}
	}
}

void UBusDrivetrainComponent::RequestShift(int32 Delta)
{
	if (Mode != EBusGearboxMode::Manual || ShiftTimer > 0.f)
	{
		return;
	}

	const int32 Target = FMath::Clamp(Gear + Delta, -1, ForwardRatios.Num());
	const UChaosWheeledVehicleMovementComponent* Movement = GetMovement();
	const float SpeedKmh = Movement ? Movement->GetForwardSpeed() * BusDrivetrain::CmPerSecToKmh : 0.f;
	const bool bReverseBlocked = Target < 0 && FMath::Abs(SpeedKmh) > ReverseMaxSpeedKmh;

	if (Target == Gear || (!bManualClutchDown && Target != 0) || bReverseBlocked)
	{
		if (Target != Gear)
		{
			OnGearGrind.Broadcast();
		}
		return;
	}
	StartShift(Target);
}

void UBusDrivetrainComponent::ToggleGearboxMode()
{
	Mode = Mode == EBusGearboxMode::Automatic ? EBusGearboxMode::Manual : EBusGearboxMode::Automatic;
}

void UBusDrivetrainComponent::ToggleIgnition()
{
	if (EngineState == EBusEngineState::Off)
	{
		EngineState = EBusEngineState::Cranking;
		CrankTimer = 0.f;
	}
	else
	{
		EngineState = EBusEngineState::Off;
	}
}

void UBusDrivetrainComponent::SelectAutoDirection(bool bReverse)
{
	const int32 Target = bReverse ? -1 : 1;
	if (Mode == EBusGearboxMode::Automatic && ShiftTimer <= 0.f && (Gear < 0) != bReverse)
	{
		StartShift(Target);
	}
}
