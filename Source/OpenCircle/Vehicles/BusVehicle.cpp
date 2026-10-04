// Copyright OpenCircle. All Rights Reserved.

#include "Vehicles/BusVehicle.h"
#include "Vehicles/BusWheels.h"
#include "Vehicles/BusAirSystemComponent.h"
#include "Vehicles/BusControlsComponent.h"
#include "Vehicles/BusTelemetryComponent.h"
#include "Vehicles/BusDrivetrainComponent.h"
#include "Vehicles/BusDriverPoseComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "HAL/IConsoleManager.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/CollisionProfile.h"

namespace BusVehicle
{
	// Chaos wheel order; bone names match Tools/Blender/make_bus.py
	const FName WheelBones[] = { TEXT("Wheel_FL"), TEXT("Wheel_FR"), TEXT("Wheel_RL"), TEXT("Wheel_RR") };
	constexpr float CmPerSecToKmh = 0.036f;
	static TAutoConsoleVariable<int32> CVarStartCamera(TEXT("bus.StartCamera"), 0, TEXT("Camera the bus starts with: 0 chase, 1 cockpit, 2 driver view"));

	// Wheel centres in cm (Tata LPO 1612: wheelbase 620, tyre radius 52.7). Set in code rather than read
	// from bones, so a skeleton import scale can never misplace the wheels. Kerb side is -Y.
	const FVector WheelPositions[] = {
		FVector(367.5f, -103.f, 52.7f), FVector(367.5f, 103.f, 52.7f),
		FVector(-252.5f, -98.f, 52.7f), FVector(-252.5f, 98.f, 52.7f) };
}

ABusVehicle::ABusVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	GetMesh()->SetGenerateOverlapEvents(true);
	// Mass must not come from the physics-asset box volume (that would be ~90 t)
	GetMesh()->BodyInstance.SetMassOverride(11000.f, true);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetMesh());
	// Query-only: characters can walk inside later, but the shell never fights the chassis physics body
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BodyMesh->SetCollisionResponseToAllChannels(ECR_Block);
	BodyMesh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
	BodyMesh->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
	BodyMesh->BodyInstance.bAutoWeld = false;
	// Vehicle object type, which the wheel suspension traces ignore (they must only see the road)
	BodyMesh->SetCollisionObjectType(ECC_Vehicle);

	// Hinges at the rear edge of each door opening, kerb side (UE -Y)
	DoorFront = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorFront"));
	DoorFront->SetupAttachment(BodyMesh);
	DoorFront->SetRelativeLocation(FVector(295.f, -131.f, 0.f));
	DoorRear = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorRear"));
	DoorRear->SetupAttachment(BodyMesh);
	DoorRear->SetRelativeLocation(FVector(-90.f, -131.f, 0.f));
	for (UStaticMeshComponent* Door : { DoorFront.Get(), DoorRear.Get() })
	{
		Door->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Door->BodyInstance.bAutoWeld = false;
	}

	// Cab controls: pivots from Tools/Blender/interior/cab_controls.py (Blender -> UE: x100, Y flipped)
	auto MakeCab = [this](const TCHAR* Name, const FVector& Location)
	{
		UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(BodyMesh);
		Mesh->SetRelativeLocation(Location);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return Mesh;
	};
	CabColumnMesh = MakeCab(TEXT("CabColumn"), FVector::ZeroVector);
	SteeringWheelMesh = MakeCab(TEXT("SteeringWheel"), FVector(520.f, 75.f, 186.f));
	GearLeverMesh = MakeCab(TEXT("GearLever"), FVector(492.f, 36.f, 145.f));
	PedalClutchMesh = MakeCab(TEXT("PedalClutch"), FVector(528.f, 55.f, 108.f));
	PedalBrakeMesh = MakeCab(TEXT("PedalBrake"), FVector(528.f, 74.f, 108.f));
	PedalAcceleratorMesh = MakeCab(TEXT("PedalAccelerator"), FVector(528.f, 93.f, 108.f));
	DashboardMesh = MakeCab(TEXT("Dashboard"), FVector::ZeroVector);
	NeedleTachoMesh = MakeCab(TEXT("NeedleTacho"), FVector(549.64f, 66.5f, 176.17f));
	NeedleSpeedoMesh = MakeCab(TEXT("NeedleSpeedo"), FVector(549.64f, 83.5f, 176.17f));
	NeedleAirMesh = MakeCab(TEXT("NeedleAir"), FVector(548.79f, 53.5f, 174.36f));
	NeedleFuelMesh = MakeCab(TEXT("NeedleFuel"), FVector(548.79f, 96.5f, 174.36f));
	InteriorSeatsMesh = MakeCab(TEXT("InteriorSeats"), FVector::ZeroVector);
	InteriorFittingsMesh = MakeCab(TEXT("InteriorFittings"), FVector::ZeroVector);

	DriverMesh = CreateDefaultSubobject<UBusDriverPoseComponent>(TEXT("DriverMesh"));
	DriverMesh->SetupAttachment(BodyMesh);

	for (const FName& Bone : BusVehicle::WheelBones)
	{
		UStaticMeshComponent* Wheel = CreateDefaultSubobject<UStaticMeshComponent>(*(Bone.ToString() + TEXT("Mesh")));
		Wheel->SetupAttachment(GetMesh());
		Wheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WheelMeshes.Add(Wheel);
	}

	ChaseArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ChaseArm"));
	ChaseArm->SetupAttachment(GetMesh());
	ChaseArm->TargetArmLength = 1600.f;
	ChaseArm->SocketOffset = FVector(0.f, 0.f, 350.f);
	ChaseArm->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
	ChaseArm->bUsePawnControlRotation = true;
	ChaseArm->bEnableCameraLag = true;
	ChaseArm->CameraLagSpeed = 6.f;
	ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
	ChaseCamera->SetupAttachment(ChaseArm);

	// Driver sits on the right (India), eye height above the raised floor
	CockpitCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CockpitCamera"));
	CockpitCamera->SetupAttachment(GetMesh());
	CockpitCamera->SetRelativeLocation(FVector(470.f, 75.f, 230.f));
	CockpitCamera->bUsePawnControlRotation = true;
	CockpitCamera->SetAutoActivate(false);

	// Saloon view: standing in the aisle behind-left of the driver, looking at him and the road
	DriverViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("DriverViewCamera"));
	DriverViewCamera->SetupAttachment(GetMesh());
	DriverViewCamera->SetRelativeLocation(FVector(415.f, 5.f, 245.f));
	DriverViewCamera->SetRelativeRotation(UKismetMathLibrary::FindLookAtRotation(FVector(415.f, 5.f, 245.f), FVector(560.f, 85.f, 175.f)));
	DriverViewCamera->SetFieldOfView(75.f);
	DriverViewCamera->SetAutoActivate(false);

	HeadlightLeft = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadlightLeft"));
	HeadlightLeft->SetupAttachment(GetMesh());
	HeadlightLeft->SetRelativeLocation(FVector(605.f, -85.f, 90.f));
	HeadlightRight = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadlightRight"));
	HeadlightRight->SetupAttachment(GetMesh());
	HeadlightRight->SetRelativeLocation(FVector(605.f, 85.f, 90.f));
	for (USpotLightComponent* Light : { HeadlightLeft.Get(), HeadlightRight.Get() })
	{
		Light->SetRelativeRotation(FRotator(-4.f, 0.f, 0.f));
		Light->SetIntensity(40000.f);
		Light->SetAttenuationRadius(4000.f);
		Light->SetOuterConeAngle(30.f);
		Light->SetVisibility(false);
	}

	AirSystem = CreateDefaultSubobject<UBusAirSystemComponent>(TEXT("AirSystem"));
	Controls = CreateDefaultSubobject<UBusControlsComponent>(TEXT("Controls"));
	Telemetry = CreateDefaultSubobject<UBusTelemetryComponent>(TEXT("Telemetry"));
	Drivetrain = CreateDefaultSubobject<UBusDrivetrainComponent>(TEXT("Drivetrain"));

	bUseControllerRotationYaw = false;

	UChaosWheeledVehicleMovementComponent* Movement = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());

	Movement->WheelSetups.SetNum(UE_ARRAY_COUNT(BusVehicle::WheelBones));
	for (int32 i = 0; i < Movement->WheelSetups.Num(); ++i)
	{
		Movement->WheelSetups[i].WheelClass = i < 2 ? UBusWheelFront::StaticClass() : UBusWheelRear::StaticClass();
		Movement->WheelSetups[i].BoneName = TEXT("Root");     // Chaos requires a bone; Root is at the origin
		Movement->WheelSetups[i].AdditionalOffset = BusVehicle::WheelPositions[i];
	}

	// Heavy, slow, long braking: loaded bus around 11 t, low centre of mass
	Movement->Mass = 11000.f;
	Movement->ChassisHeight = 260.f;
	Movement->ChassisWidth = 258.f;
	Movement->DragCoefficient = 0.7f;
	Movement->WheelTraceCollisionResponses.SetResponse(ECC_Vehicle, ECR_Ignore);
	Movement->WheelTraceCollisionResponses.SetResponse(ECC_Pawn, ECR_Ignore);
	Movement->bEnableCenterOfMassOverride = true;
	Movement->CenterOfMassOverride = FVector(0.f, 0.f, 90.f);

	// Diesel: strong low-down torque, falls off at the top
	Movement->EngineSetup.MaxTorque = 2200.f;
	Movement->EngineSetup.MaxRPM = 2600.f;
	Movement->EngineSetup.EngineIdleRPM = 700.f;
	Movement->EngineSetup.EngineRevUpMOI = 20.f;
	Movement->EngineSetup.EngineRevDownRate = 300.f;
	FRichCurve* Torque = Movement->EngineSetup.TorqueCurve.GetRichCurve();
	Torque->Reset();
	Torque->AddKey(0.f, 0.75f);
	Torque->AddKey(1000.f, 1.f);
	Torque->AddKey(1800.f, 0.95f);
	Torque->AddKey(2600.f, 0.6f);

	// Engine, clutch, gearbox and brakes are simulated by UBusDrivetrainComponent
	Movement->bMechanicalSimEnabled = false;

	Movement->DifferentialSetup.DifferentialType = EVehicleDifferential::RearWheelDrive;
	Movement->TransmissionSetup.bUseAutomaticGears = true;
	Movement->TransmissionSetup.FinalRatio = 5.8f;
	Movement->TransmissionSetup.ForwardGearRatios = { 6.0f, 3.4f, 2.0f, 1.3f, 1.0f };
	Movement->TransmissionSetup.ReverseGearRatios = { 6.0f };
	Movement->TransmissionSetup.ChangeUpRPM = 2200.f;
	Movement->TransmissionSetup.ChangeDownRPM = 1100.f;
	Movement->TransmissionSetup.GearChangeTime = 0.6f;

	Movement->SteeringSetup.SteeringType = ESteeringType::AngleRatio;
	Movement->SteeringSetup.AngleRatio = 0.6f;

	// Heavy controls: slow steering and pedal build-up
	Movement->SteeringInputRate.RiseRate = 1.2f;
	Movement->SteeringInputRate.FallRate = 1.8f;
	Movement->ThrottleInputRate.RiseRate = 1.5f;
	Movement->BrakeInputRate.RiseRate = 2.f;
}

void ABusVehicle::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	for (int32 i = 0; i < WheelMeshes.Num(); ++i)
	{
		WheelMeshes[i]->SetStaticMesh(i < 2 ? FrontWheelMesh : RearWheelMesh);
	}
	CacheWheelBases();
	UpdateWheelVisuals();
}

void ABusVehicle::BeginPlay()
{
	Super::BeginPlay();

	// Saved Blueprint/level data can leave the chassis kinematic; the vehicle needs it simulating
	USkeletalMeshComponent* Chassis = GetMesh();
	Chassis->SetCollisionProfileName(UCollisionProfile::Vehicle_ProfileName);
	Chassis->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Chassis->SetAllBodiesSimulatePhysics(true);
	Chassis->SetSimulatePhysics(true);
	Chassis->SetMassOverrideInKg(NAME_None, CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent())->Mass, true);
	Chassis->WakeAllRigidBodies();

	Doors = { FBusDoor{ DoorFront }, FBusDoor{ DoorRear } };
	BodyBaseLocation = BodyMesh->GetRelativeLocation();
	CacheWheelBases();
	Controls->Initialize(BodyMesh, { HeadlightLeft.Get(), HeadlightRight.Get() });
	SetCamera(static_cast<EBusCamera>(FMath::Clamp(BusVehicle::CVarStartCamera.GetValueOnGameThread(), 0, 2)));
	DriverMesh->InitializeSeat(DriverPelvis);

	const FBox BodyBox = GetMesh()->Bodies.Num() > 0 ? GetMesh()->Bodies[0]->GetBodyBounds() : FBox(ForceInit);
	UE_LOG(LogTemp, Warning, TEXT("BUS_PHYS %s simulating=%d bodies=%d body_min=%s body_max=%s actor=%s"), *GetName(),
		GetMesh()->IsSimulatingPhysics(), GetMesh()->Bodies.Num(), *(BodyBox.Min - GetActorLocation()).ToCompactString(),
		*(BodyBox.Max - GetActorLocation()).ToCompactString(), *GetActorLocation().ToCompactString());
}

void ABusVehicle::CacheWheelBases()
{
	WheelBases.Reset();
	WheelBases.Append(BusVehicle::WheelPositions, UE_ARRAY_COUNT(BusVehicle::WheelPositions));
}

void ABusVehicle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateDriving(DeltaTime);
	UpdateDoors(DeltaTime);
	UpdateWheelVisuals();
	UpdateCabVisuals(DeltaTime);
	UpdateDriver(DeltaTime);
}

void ABusVehicle::UpdateDriving(float DeltaTime)
{
	UChaosWheeledVehicleMovementComponent* Movement = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
	const float ForwardKmh = GetSpeedKmh();
	const float Speed = FMath::Abs(ForwardKmh);

	// Nobody driving: parked, engine idling
	if (!IsPlayerControlled())
	{
		Drivetrain->Update(DeltaTime, 0.f, 0.f, 0.f, true);
		return;
	}

	float Throttle = ThrottleInput;
	float Brake = BrakeInput;

	// Automatic: S drives in reverse; holding a pedal at a standstill swaps drive/reverse
	if (Drivetrain->GetGearboxMode() == EBusGearboxMode::Automatic)
	{
		const bool bInReverse = Drivetrain->GetGear() < 0;
		if (Speed < 1.f && (bInReverse ? ThrottleInput : BrakeInput) > 0.5f && (bInReverse ? BrakeInput : ThrottleInput) < 0.05f)
		{
			DirectionHoldTimer += DeltaTime;
			if (DirectionHoldTimer > DirectionChangeHoldTime)
			{
				Drivetrain->SelectAutoDirection(!bInReverse);
				DirectionHoldTimer = 0.f;
			}
		}
		else
		{
			DirectionHoldTimer = 0.f;
		}
		if (bInReverse)
		{
			Throttle = BrakeInput;
			Brake = ThrottleInput;
		}
	}

	// Door interlock: no driving off with doors open
	const bool bDoorsOpen = AreDoorsOpen();
	if (bDoorsOpen)
	{
		Throttle = 0.f;
	}

	if (Speed > 1.f)
	{
		AirSystem->ConsumeBrake(Brake, DeltaTime);
	}
	Drivetrain->Update(DeltaTime, Throttle, Brake, ClutchInput, bDoorsOpen || AirSystem->IsParkingBrakeApplied());

	// Chaos's own engine is off; its inputs only keep the body awake (it sleeps slow vehicles with no input)
	Movement->SetThrottleInput(Throttle);
	Movement->SetBrakeInput(Brake);

	const float SteerScale = FMath::Lerp(1.f, HighSpeedSteeringScale, FMath::Clamp(Speed / SteeringFadeSpeedKmh, 0.f, 1.f));
	Movement->SetSteeringInput(SteerInput * SteerScale);
}

void ABusVehicle::UpdateDoors(float DeltaTime)
{
	const float Step = DeltaTime / FMath::Max(DoorTravelTime, KINDA_SMALL_NUMBER);
	for (FBusDoor& Door : Doors)
	{
		Door.OpenAlpha = FMath::Clamp(Door.OpenAlpha + (Door.bWantsOpen ? Step : -Step), 0.f, 1.f);
		const float Eased = FMath::InterpEaseInOut(0.f, 1.f, Door.OpenAlpha, 2.f);
		Door.Mesh->SetRelativeRotation(FRotator(0.f, Eased * DoorOpenAngle, 0.f));
	}

	const bool bKneel = AreDoorsOpen() && FMath::Abs(GetSpeedKmh()) < DoorMaxSpeedKmh;
	KneelAlpha = FMath::FInterpTo(KneelAlpha, bKneel ? 1.f : 0.f, DeltaTime, 2.f);
	BodyMesh->SetRelativeLocationAndRotation(BodyBaseLocation - FVector(0.f, 0.f, KneelDrop * KneelAlpha),
		FRotator(0.f, 0.f, -KneelRoll * KneelAlpha));
}

void ABusVehicle::UpdateWheelVisuals()
{
	const UChaosWheeledVehicleMovementComponent* Movement = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
	for (int32 i = 0; i < WheelMeshes.Num() && i < WheelBases.Num(); ++i)
	{
		const UChaosVehicleWheel* Wheel = Movement && Movement->Wheels.IsValidIndex(i) ? Movement->Wheels[i].Get() : nullptr;
		const float Offset = Wheel ? Wheel->GetSuspensionOffset() : 0.f;
		const FRotator Rotation = Wheel ? FRotator(Wheel->GetRotationAngle(), Wheel->GetSteerAngle(), 0.f) : FRotator::ZeroRotator;   // same convention as Chaos VehicleAnimationInstance
		WheelMeshes[i]->SetRelativeLocationAndRotation(WheelBases[i] + FVector(0.f, 0.f, Offset), Rotation);
		// Meshes are modelled for the kerb (-Y) side; mirror for the right-hand wheels
		WheelMeshes[i]->SetRelativeScale3D(FVector(1.f, WheelBases[i].Y > 0.f ? -1.f : 1.f, 1.f));
	}
}

float ABusVehicle::GetSpeedKmh() const
{
	return GetVehicleMovementComponent()->GetForwardSpeed() * BusVehicle::CmPerSecToKmh;
}

float ABusVehicle::GetEngineRpm() const
{
	return Drivetrain->GetEngineRpm();
}

int32 ABusVehicle::GetCurrentGear() const
{
	return Drivetrain->GetGear();
}

bool ABusVehicle::IsManualGearbox() const
{
	return Drivetrain->GetGearboxMode() == EBusGearboxMode::Manual;
}

bool ABusVehicle::AreDoorsOpen() const
{
	return Doors.ContainsByPredicate([](const FBusDoor& Door) { return Door.OpenAlpha > 0.f; });
}

void ABusVehicle::ToggleDoor(int32 Index)
{
	if (!Doors.IsValidIndex(Index))
	{
		return;
	}

	FBusDoor& Door = Doors[Index];
	if (!Door.bWantsOpen && FMath::Abs(GetSpeedKmh()) > DoorMaxSpeedKmh)
	{
		return;
	}

	Door.bWantsOpen = !Door.bWantsOpen;
	AirSystem->ConsumeDoorStroke();
}

void ABusVehicle::SetCamera(EBusCamera NewCamera)
{
	Camera = NewCamera;
	ChaseCamera->SetActive(Camera == EBusCamera::Chase);
	CockpitCamera->SetActive(Camera == EBusCamera::Cockpit);
	DriverViewCamera->SetActive(Camera == EBusCamera::DriverView);
}

void ABusVehicle::OnThrottle(const FInputActionValue& Value) { ThrottleInput = Value.Get<float>(); }
void ABusVehicle::OnBrake(const FInputActionValue& Value) { BrakeInput = Value.Get<float>(); }
void ABusVehicle::OnSteer(const FInputActionValue& Value) { SteerInput = Value.Get<float>(); }
void ABusVehicle::OnClutch(const FInputActionValue& Value) { ClutchInput = Value.Get<float>(); }

void ABusVehicle::OnLook(const FInputActionValue& Value)
{
	const FVector2D Look = Value.Get<FVector2D>();
	AddControllerYawInput(Look.X);
	AddControllerPitchInput(-Look.Y);
}

void ABusVehicle::OnShiftGear(int32 Delta)
{
	Drivetrain->RequestShift(Delta);
}

void ABusVehicle::PawnClientRestart()
{
	Super::PawnClientRestart();

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Input->ClearAllMappings();
			if (DriverContext)
			{
				Input->AddMappingContext(DriverContext, 0);
			}
		}
	}
}

void ABusVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}

	auto BindAxis = [Input, this](UInputAction* Action, void (ABusVehicle::*Handler)(const FInputActionValue&))
	{
		if (Action)
		{
			Input->BindAction(Action, ETriggerEvent::Triggered, this, Handler);
			Input->BindAction(Action, ETriggerEvent::Completed, this, Handler);
		}
	};
	auto BindPress = [Input, this](UInputAction* Action, TFunction<void()> Handler)
	{
		if (Action)
		{
			Input->BindActionValueLambda(Action, ETriggerEvent::Started, [Handler](const FInputActionValue&) { Handler(); });
		}
	};

	BindAxis(ThrottleAction, &ABusVehicle::OnThrottle);
	BindAxis(BrakeAction, &ABusVehicle::OnBrake);
	BindAxis(SteerAction, &ABusVehicle::OnSteer);
	if (LookAction)
	{
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABusVehicle::OnLook);
	}

	BindPress(CameraAction, [this] { SetCamera(static_cast<EBusCamera>((static_cast<uint8>(Camera) + 1) % 3)); });
	BindPress(ParkingBrakeAction, [this] { AirSystem->ToggleParkingBrake(); });
	BindPress(DoorFrontAction, [this] { ToggleDoor(0); });
	BindPress(DoorRearAction, [this] { ToggleDoor(1); });
	BindPress(IndicatorLeftAction, [this] { Controls->ToggleIndicator(EBusIndicator::Left); });
	BindPress(IndicatorRightAction, [this] { Controls->ToggleIndicator(EBusIndicator::Right); });
	BindPress(HazardAction, [this] { Controls->ToggleIndicator(EBusIndicator::Hazard); });
	BindPress(HeadlightsAction, [this] { Controls->ToggleHeadlights(); });
	BindPress(WipersAction, [this] { Controls->ToggleWipers(); });
	BindPress(GearUpAction, [this] { OnShiftGear(1); });
	BindPress(GearDownAction, [this] { OnShiftGear(-1); });
	BindPress(GearboxModeAction, [this] { Drivetrain->ToggleGearboxMode(); });
	BindPress(IgnitionAction, [this] { Drivetrain->ToggleIgnition(); });
	BindPress(ExhaustBrakeAction, [this] { Drivetrain->ToggleExhaustBrake(); });
	BindAxis(ClutchAction, &ABusVehicle::OnClutch);

	if (HornAction)
	{
		Input->BindActionValueLambda(HornAction, ETriggerEvent::Started, [this](const FInputActionValue&) { Controls->SetHorn(true); });
		Input->BindActionValueLambda(HornAction, ETriggerEvent::Completed, [this](const FInputActionValue&) { Controls->SetHorn(false); });
	}
}

FString ABusVehicle::GetDebugString() const
{
	const UChaosWheeledVehicleMovementComponent* Movement = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
	FString Wheels;
	for (const FChaosWheelSetup& Setup : Movement->WheelSetups)
	{
		const FVector P = Setup.AdditionalOffset;
		Wheels += FString::Printf(TEXT(" [%.0f,%.0f,%.0f]"), P.X, P.Y, P.Z);
	}
	const FVector Com = GetMesh()->GetCenterOfMass() - GetActorLocation();
	Wheels += FString::Printf(TEXT(" com[%.0f,%.0f,%.0f] roll %.1f |"), Com.X, Com.Y, Com.Z, GetActorRotation().Roll);
	for (const UChaosVehicleWheel* Wheel : Movement->Wheels)
	{
		Wheels += Wheel && !Wheel->IsInAir() ? FString::Printf(TEXT(" %.0f"), Wheel->GetSuspensionOffset()) : TEXT(" AIR");
	}
	return FString::Printf(TEXT("root %s | mass %.0f kg | in T%.1f B%.1f S%.1f | park %d | wheels(susp cm):%s | pitch %.1f"),
		*GetMesh()->GetSkinnedAsset()->GetComposedRefPoseMatrix(TEXT("Root")).Rotator().ToCompactString(), GetMesh()->GetMass(), ThrottleInput, BrakeInput, SteerInput, AirSystem->IsParkingBrakeApplied(), *Wheels, GetActorRotation().Pitch);
}

void ABusVehicle::UpdateCabVisuals(float DeltaTime)
{
	// Steering wheel follows the front wheel angle (steering ratio feel without extra state)
	const UChaosWheeledVehicleMovementComponent* Movement = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
	const UChaosVehicleWheel* Front = Movement && Movement->Wheels.Num() > 0 ? Movement->Wheels[0].Get() : nullptr;
	const float MaxSteer = Front ? FMath::Max(Front->MaxSteerAngle, 1.f) : 38.f;
	const float TargetAngle = Front ? Front->GetSteerAngle() / MaxSteer * SteeringWheelLockAngle : 0.f;
	SteeringWheelAngle = FMath::FInterpTo(SteeringWheelAngle, TargetAngle, DeltaTime, 12.f);
	SteeringWheelMesh->SetRelativeRotation(FQuat(SteeringWheelAxis.GetSafeNormal(), FMath::DegreesToRadians(SteeringWheelAngle)));

	// Pedals hinge at the floor about Y
	const float Brake = Drivetrain->GetGear() < 0 && !IsManualGearbox() ? ThrottleInput : BrakeInput;
	const float Throttle = Drivetrain->GetGear() < 0 && !IsManualGearbox() ? BrakeInput : ThrottleInput;
	PedalClutchMesh->SetRelativeRotation(FRotator(-PedalTravel * ClutchInput, 0.f, 0.f));
	PedalBrakeMesh->SetRelativeRotation(FRotator(-PedalTravel * Brake, 0.f, 0.f));
	PedalAcceleratorMesh->SetRelativeRotation(FRotator(-PedalTravel * Throttle, 0.f, 0.f));

	// Gear lever in the R-1-3-5 / 2-4-6 gate; neutral sits between the rows
	const int32 Gear = Drivetrain->IsShifting() ? 0 : Drivetrain->GetGear();
	float Row = 0.f, Gate = 0.f;
	if (Gear < 0)
	{
		Row = 1.f;
		Gate = -1.5f;
	}
	else if (Gear > 0)
	{
		Row = Gear % 2 == 1 ? 1.f : -1.f;
		Gate = (Gear - 1) / 2 - 0.5f;
	}
	const FRotator Target(-Row * GearLeverThrow.X, 0.f, -Gate * GearLeverThrow.Y);
	GearLeverRotation = FMath::RInterpTo(GearLeverRotation, Target, DeltaTime, 10.f);
	GearLeverMesh->SetRelativeRotation(GearLeverRotation);

	// Gauges: fraction of full scale -> sweep about the instrument panel normal
	const FVector Axis = GaugeAxis.GetSafeNormal();
	auto SetNeedle = [&](UStaticMeshComponent* Needle, float Fraction)
	{
		Needle->SetRelativeRotation(FQuat(Axis, FMath::DegreesToRadians(GaugeSweep * FMath::Clamp(Fraction, 0.f, 1.f))));
	};
	SetNeedle(NeedleSpeedoMesh, FMath::Abs(GetSpeedKmh()) / GaugeMax.X);
	SetNeedle(NeedleTachoMesh, GetEngineRpm() / GaugeMax.Y);
	SetNeedle(NeedleAirMesh, AirSystem->GetPressure() / GaugeMax.Z);
	SetNeedle(NeedleFuelMesh, FuelLevel);
}

void ABusVehicle::UpdateDriver(float DeltaTime)
{
	if (!DriverMesh->GetSkinnedAsset())
	{
		return;
	}

	FBusDriverTargets Targets;

	// Hands on the rim, carried round by the wheel; hand-over-hand re-grip when one is carried too far
	const FTransform Wheel = SteeringWheelMesh->GetComponentTransform();
	const FVector Normal = Wheel.TransformVectorNoScale(SteeringWheelAxis.GetSafeNormal());      // toward the driver
	const FVector Side = GetMesh()->GetComponentTransform().GetUnitAxis(EAxis::Y);
	const FVector Up = FVector::CrossProduct(Side, Normal).GetSafeNormal();                     // rim top
	auto RimPoint = [&](float ClockDeg, float Lift)
	{
		const float A = FMath::DegreesToRadians(ClockDeg);
		return Wheel.GetLocation() + (Up * FMath::Cos(A) + Side * FMath::Sin(A)) * 24.5f + Normal * (GripOffset + Lift);
	};
	if (Hands[0].Home == 0.f)
	{
		Hands[0].Home = Hands[0].Anchor = -60.f;       // 10 o'clock
		Hands[1].Home = Hands[1].Anchor = 60.f;        // 2 o'clock
	}
	const bool bAnyMoving = Hands[0].Alpha < 1.f || Hands[1].Alpha < 1.f;
	FVector HandPos[2];
	for (int32 i = 0; i < 2; ++i)
	{
		FBusHandGrip& H = Hands[i];
		if (H.Alpha >= 1.f && !bAnyMoving && FMath::Abs(H.Anchor + SteeringWheelAngle - H.Home) > RegripAngle)
		{
			H.From = H.Anchor;                         // let go: reach back toward home
			H.Anchor = H.Home - SteeringWheelAngle;
			H.Alpha = 0.f;
		}
		if (H.Alpha < 1.f)
		{
			H.Alpha = FMath::Min(1.f, H.Alpha + DeltaTime / FMath::Max(RegripTime, 0.05f));
			const float T = FMath::SmoothStep(0.f, 1.f, H.Alpha);
			HandPos[i] = RimPoint(FMath::Lerp(H.From, H.Anchor, T) + SteeringWheelAngle, 9.f * FMath::Sin(PI * H.Alpha));
		}
		else
		{
			HandPos[i] = RimPoint(H.Anchor + SteeringWheelAngle, 0.f);
		}
	}
	Targets.LeftHand = HandPos[0];
	Targets.RightHand = HandPos[1];
	Targets.GripLeft = Hands[0].Alpha < 1.f ? 0.2f : 1.f;
	Targets.GripRight = Hands[1].Alpha < 1.f ? 0.2f : 1.f;

	// Left hand to the gear knob while shifting or with the clutch down in manual
	const bool bShifting = Drivetrain->IsShifting() || (IsManualGearbox() && ClutchInput > 0.5f);
	ShiftHandAlpha = FMath::FInterpConstantTo(ShiftHandAlpha, bShifting ? 1.f : 0.f, DeltaTime, 1.f / FMath::Max(ShiftReachTime, 0.05f));
	const FVector Knob = GearLeverMesh->GetComponentTransform().TransformPosition(FVector(-10.f, -3.f, 62.f));
	Targets.LeftHand = FMath::Lerp(Targets.LeftHand, Knob + FVector(0.f, 0.f, 6.f), FMath::SmoothStep(0.f, 1.f, ShiftHandAlpha));

	// Feet on the pedal pads (pad centre in pedal mesh space: hinged pads leaning 55 deg forward)
	auto Pad = [](const UStaticMeshComponent* Pedal, float Height)
	{
		return Pedal->GetComponentTransform().TransformPosition(FVector(Height * 0.29f, 0.f, Height * 0.41f + 4.f));
	};
	const bool bInReverse = Drivetrain->GetGear() < 0 && !IsManualGearbox();
	const float Brake = bInReverse ? ThrottleInput : BrakeInput;
	const float Throttle = bInReverse ? BrakeInput : ThrottleInput;
	RightFootBrakeAlpha = FMath::FInterpTo(RightFootBrakeAlpha, Brake > Throttle ? 1.f : 0.f, DeltaTime, 10.f);
	Targets.RightFoot = FMath::Lerp(Pad(PedalAcceleratorMesh, 27.f), Pad(PedalBrakeMesh, 17.f), RightFootBrakeAlpha);
	Targets.LeftFoot = Pad(PedalClutchMesh, 17.f);

	Targets.HeadYaw = SteeringWheelAngle / FMath::Max(SteeringWheelLockAngle, 1.f) * 30.f;

	// Body thrown by the bus: forward under braking, outward in turns (smoothed acceleration, m/s2)
	const FVector LocalVelocity = GetActorTransform().InverseTransformVectorNoScale(GetVelocity());
	const FVector Accel = DeltaTime > 0.f ? (LocalVelocity - LastLocalVelocity) / DeltaTime * 0.01f : FVector::ZeroVector;
	LastLocalVelocity = LocalVelocity;
	const FVector2D TargetLean(FMath::Clamp(-Accel.X * LeanPerAccel, -8.f, 8.f), FMath::Clamp(-Accel.Y * LeanPerAccel, -8.f, 8.f));
	BodyLean = FMath::Vector2DInterpTo(BodyLean, TargetLean, DeltaTime, 3.f);
	Targets.LeanForward = BodyLean.X;
	Targets.LeanSide = BodyLean.Y;
	Targets.GripLeft = FMath::Lerp(Targets.GripLeft, 0.85f, ShiftHandAlpha);      // knob is a smaller grip
	Targets.bAtStop = CurrentStop && AreDoorsOpen() && FMath::Abs(GetSpeedKmh()) < 2.f;
	Targets.DeltaTime = DeltaTime;
	DriverMesh->UpdatePose(Targets);
}
