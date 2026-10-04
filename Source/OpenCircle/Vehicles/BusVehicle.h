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
class ABusStop;
struct FInputActionValue;

UENUM(BlueprintType)
enum class EBusCamera : uint8
{
	Chase,
	Cockpit
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
	bool IsManualGearbox() const { return bManualGearbox; }

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus")
	TObjectPtr<USpringArmComponent> ChaseArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus")
	TObjectPtr<UCameraComponent> ChaseCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bus")
	TObjectPtr<UCameraComponent> CockpitCamera;

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

	// ---- Meshes ----
	UPROPERTY(EditDefaultsOnly, Category="Bus|Meshes")
	TObjectPtr<UStaticMesh> FrontWheelMesh;

	/** Dual rear tyres */
	UPROPERTY(EditDefaultsOnly, Category="Bus|Meshes")
	TObjectPtr<UStaticMesh> RearWheelMesh;

	// ---- Driving feel ----
	/** Brake response curve exponent: >1 makes light taps soft and full press strong */
	UPROPERTY(EditAnywhere, Category="Bus|Driving")
	float BrakeCurveExponent = 2.0f;

	/** Steering multiplier at SteeringFadeSpeedKmh and above */
	UPROPERTY(EditAnywhere, Category="Bus|Driving", meta=(ClampMin="0.1", ClampMax="1"))
	float HighSpeedSteeringScale = 0.4f;

	UPROPERTY(EditAnywhere, Category="Bus|Driving")
	float SteeringFadeSpeedKmh = 60.f;

	/** Brake applied when coasting off-throttle above 10 km/h (engine braking / retarder) */
	UPROPERTY(EditAnywhere, Category="Bus|Driving", meta=(ClampMin="0", ClampMax="1"))
	float EngineBrake = 0.05f;

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

private:

	void UpdateDriving(float DeltaTime);
	void UpdateDoors(float DeltaTime);
	void UpdateWheelVisuals();
	void CacheWheelBases();
	void SetCamera(EBusCamera NewCamera);

	void OnThrottle(const FInputActionValue& Value);
	void OnBrake(const FInputActionValue& Value);
	void OnSteer(const FInputActionValue& Value);
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
	bool bManualGearbox = false;
	EBusCamera Camera = EBusCamera::Chase;
};
