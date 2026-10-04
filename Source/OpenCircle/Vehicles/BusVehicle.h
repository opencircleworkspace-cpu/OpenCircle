// Copyright OpenCircle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "BusVehicle.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class USpringArmComponent;
class UCameraComponent;
class USpotLightComponent;
class UInputAction;
class UInputMappingContext;
class UBusAirSystemComponent;
class UBusControlsComponent;
class UBusTelemetryComponent;
class UBusDrivetrainComponent;
class UBusDriverPoseComponent;
class ABusStop;
struct FInputActionValue;

UENUM(BlueprintType)
enum class EBusCamera : uint8
{
	Chase,
	Cockpit,
	/** Inside the saloon, behind and beside the driver */
	DriverView
};

/** A hand on the steering rim: where it holds (wheel-relative clock angle) and any re-grip in progress */
struct FBusHandGrip
{
	float Home = 0.f;           // clock angle it returns to (deg, 0 = top)
	float Anchor = 0.f;         // held position on the rim, relative to the wheel
	float From = 0.f;
	float Alpha = 1.f;          // < 1 while moving to a new grip
};

/** One hinged door: its mesh and how far open it is */
USTRUCT()
struct FBusDoor
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;

	bool bWantsOpen = false;
	float OpenAlpha = 0.f;
};

/**
 *  The bus. The skeletal mesh is the chassis Chaos simulates (bones mark wheel positions);
 *  body, doors and wheels are static meshes moved from code, so no Anim Blueprint is needed.
 *  Single-player driver controls; gameplay state is kept on the actor so it can replicate later.
 */
UCLASS()
class ABusVehicle : public AWheeledVehiclePawn
{
	GENERATED_BODY()

public:

	ABusVehicle();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;

	UFUNCTION(BlueprintPure, Category="Bus")
	float GetSpeedKmh() const;

	UFUNCTION(BlueprintPure, Category="Bus")
	float GetEngineRpm() const;

	UFUNCTION(BlueprintPure, Category="Bus")
	int32 GetCurrentGear() const;

	UFUNCTION(BlueprintPure, Category="Bus")
	bool IsManualGearbox() const;

	UFUNCTION(BlueprintPure, Category="Bus")
	bool AreDoorsOpen() const;

	UFUNCTION(BlueprintPure, Category="Bus")
	bool IsDoorOpen(int32 Index) const { return Doors.IsValidIndex(Index) && Doors[Index].OpenAlpha > 0.f; }

	UFUNCTION(BlueprintCallable, Category="Bus")
	void ToggleDoor(int32 Index);

	/** Feeds driver inputs from code (scripted tests, AI). Same path as the player's keys. */
	void SetDriverInputs(float Throttle, float Brake, float Steer) { ThrottleInput = Throttle; BrakeInput = Brake; SteerInput = Steer; }

	float GetThrottleInput() const { return ThrottleInput; }
	float GetBrakeInput() const { return BrakeInput; }
	float GetSteerInput() const { return SteerInput; }

	/** Inputs and per-wheel ground contact, for the debug HUD */
	FString GetDebugString() const;

	UBusDrivetrainComponent* GetDrivetrain() const { return Drivetrain; }
	UBusAirSystemComponent* GetAirSystem() const { return AirSystem; }
	UBusControlsComponent* GetControls() const { return Controls; }

	/** Set by ABusStop while the bus is inside a stop zone */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Bus")
	TObjectPtr<ABusStop> CurrentStop;

protected:

	// ---- Components ----
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus")
	TObjectPtr<UStaticMeshComponent> DoorFront;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus")
	TObjectPtr<UStaticMeshComponent> DoorRear;

	/** Visual wheels, in Chaos wheel order (FL, FR, RL, RR) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus")
	TArray<TObjectPtr<UStaticMeshComponent>> WheelMeshes;

	// ---- Cab controls (meshes from Tools/Blender/interior, origins at their pivots) ----
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus|Cab") TObjectPtr<UStaticMeshComponent> CabColumnMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus|Cab") TObjectPtr<UStaticMeshComponent> SteeringWheelMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus|Cab") TObjectPtr<UStaticMeshComponent> GearLeverMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus|Cab") TObjectPtr<UStaticMeshComponent> PedalClutchMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus|Cab") TObjectPtr<UStaticMeshComponent> PedalBrakeMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus|Cab") TObjectPtr<UStaticMeshComponent> PedalAcceleratorMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus|Cab") TObjectPtr<UStaticMeshComponent> DashboardMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus|Cab") TObjectPtr<UStaticMeshComponent> NeedleTachoMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus|Cab") TObjectPtr<UStaticMeshComponent> NeedleSpeedoMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus|Cab") TObjectPtr<UStaticMeshComponent> NeedleAirMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus|Cab") TObjectPtr<UStaticMeshComponent> NeedleFuelMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus|Interior") TObjectPtr<UStaticMeshComponent> InteriorSeatsMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus|Interior") TObjectPtr<UStaticMeshComponent> InteriorFittingsMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus")
	TObjectPtr<USpringArmComponent> ChaseArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus")
	TObjectPtr<UCameraComponent> ChaseCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus")
	TObjectPtr<UCameraComponent> CockpitCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus")
	TObjectPtr<UCameraComponent> DriverViewCamera;

	/** Seated driver character (procedural pose) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus|Driver")
	TObjectPtr<UBusDriverPoseComponent> DriverMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus")
	TObjectPtr<USpotLightComponent> HeadlightLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus")
	TObjectPtr<USpotLightComponent> HeadlightRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus")
	TObjectPtr<UBusAirSystemComponent> AirSystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus")
	TObjectPtr<UBusControlsComponent> Controls;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus")
	TObjectPtr<UBusTelemetryComponent> Telemetry;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus")
	TObjectPtr<UBusDrivetrainComponent> Drivetrain;

	// ---- Meshes ----
	UPROPERTY(EditDefaultsOnly, Category="Bus|Meshes")
	TObjectPtr<UStaticMesh> FrontWheelMesh;

	/** Dual rear tyres */
	UPROPERTY(EditDefaultsOnly, Category="Bus|Meshes")
	TObjectPtr<UStaticMesh> RearWheelMesh;

	// ---- Driving feel ----
	/** Steering multiplier at SteeringFadeSpeedKmh and above */
	UPROPERTY(EditAnywhere, Category="Bus|Driving", meta=(ClampMin="0.1", ClampMax="1"))
	float HighSpeedSteeringScale = 0.4f;

	UPROPERTY(EditAnywhere, Category="Bus|Driving")
	float SteeringFadeSpeedKmh = 60.f;

	/** Automatic: hold the pedal this long at a standstill to swap drive/reverse */
	UPROPERTY(EditAnywhere, Category="Bus|Driving")
	float DirectionChangeHoldTime = 0.4f;

	// ---- Cab animation ----
	/** Steering wheel spin axis (bus space): rim tilted 32 deg like a bus wheel */
	UPROPERTY(EditAnywhere, Category="Bus|Cab") FVector SteeringWheelAxis = FVector(-0.53f, 0.f, 0.848f);
	/** Steering wheel rotation at full lock, degrees (about 1.25 turns each way) */
	UPROPERTY(EditAnywhere, Category="Bus|Cab") float SteeringWheelLockAngle = 450.f;
	/** Pedal travel at full press, degrees */
	UPROPERTY(EditAnywhere, Category="Bus|Cab") float PedalTravel = 16.f;
	/** Gear lever tilt per gate step: X = fore/aft rows, Y = across gates */
	UPROPERTY(EditAnywhere, Category="Bus|Cab") FVector2D GearLeverThrow = FVector2D(9.f, 5.f);
	/** Gauge needle spin axis (instrument panel normal) and full-scale sweep */
	UPROPERTY(EditAnywhere, Category="Bus|Cab") FVector GaugeAxis = FVector(-0.906f, 0.f, 0.423f);
	UPROPERTY(EditAnywhere, Category="Bus|Cab") float GaugeSweep = 240.f;
	/** Full-scale values: speedo km/h, tacho rpm, air bar */
	UPROPERTY(EditAnywhere, Category="Bus|Cab") FVector GaugeMax = FVector(120.f, 3000.f, 10.f);
	/** Fuel level 0..1 (no fuel model yet) */
	UPROPERTY(EditAnywhere, Category="Bus|Cab") float FuelLevel = 0.75f;

	// ---- Driver ----
	/** Driver pelvis on the seat (body mesh space, cm) */
	UPROPERTY(EditAnywhere, Category="Bus|Driver") FVector DriverPelvis = FVector(468.f, 75.f, 170.f);
	/** Wrist sits this far back from the rim toward the driver */
	UPROPERTY(EditAnywhere, Category="Bus|Driver") float GripOffset = 5.f;
	/** A hand lets go and re-grips once carried this far (deg) from its home position */
	UPROPERTY(EditAnywhere, Category="Bus|Driver") float RegripAngle = 70.f;
	/** Seconds for one hand-over-hand re-grip */
	UPROPERTY(EditAnywhere, Category="Bus|Driver") float RegripTime = 0.28f;
	/** Torso lean per m/s2 of acceleration, degrees */
	UPROPERTY(EditAnywhere, Category="Bus|Driver") float LeanPerAccel = 1.6f;
	/** Seconds for the left hand to reach the gear lever */
	UPROPERTY(EditAnywhere, Category="Bus|Driver") float ShiftReachTime = 0.25f;

	// ---- Doors and kneeling ----
	UPROPERTY(EditAnywhere, Category="Bus|Doors")
	float DoorOpenAngle = 85.f;

	/** Seconds for a full open or close */
	UPROPERTY(EditAnywhere, Category="Bus|Doors")
	float DoorTravelTime = 1.2f;

	/** Doors only open below this speed */
	UPROPERTY(EditAnywhere, Category="Bus|Doors")
	float DoorMaxSpeedKmh = 3.f;

	/** Body drop toward the kerb while stopped with doors open */
	UPROPERTY(EditAnywhere, Category="Bus|Doors")
	float KneelDrop = 8.f;

	UPROPERTY(EditAnywhere, Category="Bus|Doors")
	float KneelRoll = 2.f;

	// ---- Input ----
	UPROPERTY(EditDefaultsOnly, Category="Bus|Input")
	TObjectPtr<UInputMappingContext> DriverContext;

	UPROPERTY(EditDefaultsOnly, Category="Bus|Input") TObjectPtr<UInputAction> ThrottleAction;
	UPROPERTY(EditDefaultsOnly, Category="Bus|Input") TObjectPtr<UInputAction> BrakeAction;
	UPROPERTY(EditDefaultsOnly, Category="Bus|Input") TObjectPtr<UInputAction> SteerAction;
	UPROPERTY(EditDefaultsOnly, Category="Bus|Input") TObjectPtr<UInputAction> LookAction;
	UPROPERTY(EditDefaultsOnly, Category="Bus|Input") TObjectPtr<UInputAction> CameraAction;
	UPROPERTY(EditDefaultsOnly, Category="Bus|Input") TObjectPtr<UInputAction> ParkingBrakeAction;
	UPROPERTY(EditDefaultsOnly, Category="Bus|Input") TObjectPtr<UInputAction> DoorFrontAction;
	UPROPERTY(EditDefaultsOnly, Category="Bus|Input") TObjectPtr<UInputAction> DoorRearAction;
	UPROPERTY(EditDefaultsOnly, Category="Bus|Input") TObjectPtr<UInputAction> IndicatorLeftAction;
	UPROPERTY(EditDefaultsOnly, Category="Bus|Input") TObjectPtr<UInputAction> IndicatorRightAction;
	UPROPERTY(EditDefaultsOnly, Category="Bus|Input") TObjectPtr<UInputAction> HazardAction;
	UPROPERTY(EditDefaultsOnly, Category="Bus|Input") TObjectPtr<UInputAction> HornAction;
	UPROPERTY(EditDefaultsOnly, Category="Bus|Input") TObjectPtr<UInputAction> HeadlightsAction;
	UPROPERTY(EditDefaultsOnly, Category="Bus|Input") TObjectPtr<UInputAction> WipersAction;
	UPROPERTY(EditDefaultsOnly, Category="Bus|Input") TObjectPtr<UInputAction> GearUpAction;
	UPROPERTY(EditDefaultsOnly, Category="Bus|Input") TObjectPtr<UInputAction> GearDownAction;
	UPROPERTY(EditDefaultsOnly, Category="Bus|Input") TObjectPtr<UInputAction> GearboxModeAction;
	UPROPERTY(EditDefaultsOnly, Category="Bus|Input") TObjectPtr<UInputAction> ClutchAction;
	UPROPERTY(EditDefaultsOnly, Category="Bus|Input") TObjectPtr<UInputAction> IgnitionAction;
	UPROPERTY(EditDefaultsOnly, Category="Bus|Input") TObjectPtr<UInputAction> ExhaustBrakeAction;

private:

	void UpdateDriving(float DeltaTime);
	void UpdateDoors(float DeltaTime);
	void UpdateWheelVisuals();
	void UpdateCabVisuals(float DeltaTime);
	void UpdateDriver(float DeltaTime);
	void CacheWheelBases();
	void SetCamera(EBusCamera NewCamera);

	void OnThrottle(const FInputActionValue& Value);
	void OnBrake(const FInputActionValue& Value);
	void OnSteer(const FInputActionValue& Value);
	void OnClutch(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnShiftGear(int32 Delta);

	UPROPERTY(Transient)
	TArray<FBusDoor> Doors;

	TArray<FVector> WheelBases;
	FVector BodyBaseLocation = FVector::ZeroVector;
	float KneelAlpha = 0.f;

	float ThrottleInput = 0.f;
	float BrakeInput = 0.f;
	float SteerInput = 0.f;
	float ClutchInput = 0.f;
	float DirectionHoldTimer = 0.f;
	FRotator GearLeverRotation = FRotator::ZeroRotator;
	float SteeringWheelAngle = 0.f;
	float ShiftHandAlpha = 0.f;
	FBusHandGrip Hands[2];
	FVector LastLocalVelocity = FVector::ZeroVector;
	FVector2D BodyLean = FVector2D::ZeroVector;
	float RightFootBrakeAlpha = 0.f;
	EBusCamera Camera = EBusCamera::Chase;
};
