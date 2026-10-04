// Copyright OpenCircle. All Rights Reserved.

#include "Vehicles/BusVehicle.h"
#include "Vehicles/BusWheels.h"
#include "Vehicles/BusAirSystemComponent.h"
#include "Vehicles/BusControlsComponent.h"
#include "Vehicles/BusTelemetryComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
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
	SetCamera(EBusCamera::Chase);

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
}

void ABusVehicle::UpdateDriving(float DeltaTime)
{
	UChaosWheeledVehicleMovementComponent* Movement = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
	const float ForwardKmh = GetSpeedKmh();
	const float Speed = FMath::Abs(ForwardKmh);

	// Nobody driving: stay parked
	if (!IsPlayerControlled())
	{
		Movement->SetThrottleInput(0.f);
		Movement->SetBrakeInput(0.f);     // brake pedal at a standstill = reverse in Chaos
		Movement->SetHandbrakeInput(true);
		return;
	}

	float Brake = FMath::Pow(FMath::Clamp(BrakeInput, 0.f, 1.f), BrakeCurveExponent);
	// Engine braking only when rolling forward; while reversing the brake pedal drives backwards
	if (ThrottleInput < 0.05f && ForwardKmh > 10.f)
	{
		Brake = FMath::Max(Brake, EngineBrake);
	}

	// Door interlock: no driving off with doors open
	const bool bDoorsOpen = AreDoorsOpen();
	const float Throttle = bDoorsOpen ? 0.f : ThrottleInput;
	// Hold with the handbrake: at a standstill Chaos treats the brake pedal as reverse
	if (bDoorsOpen)
	{
		Brake = 0.f;
	}

	const float SteerScale = FMath::Lerp(1.f, HighSpeedSteeringScale, FMath::Clamp(Speed / SteeringFadeSpeedKmh, 0.f, 1.f));

	if (ForwardKmh > 1.f)
	{
		AirSystem->ConsumeBrake(BrakeInput, DeltaTime);   // S acts as reverse when stopped; that uses no air
	}
	Movement->SetThrottleInput(Throttle);
	Movement->SetBrakeInput(Brake);
	Movement->SetSteeringInput(SteerInput * SteerScale);
	Movement->SetHandbrakeInput(bDoorsOpen || AirSystem->IsParkingBrakeApplied());
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
		const FRotator Rotation = Wheel ? FRotator(Wheel->GetRotationAngle(), Wheel->GetSteerAngle(), 0.f)   // same convention as Chaos VehicleAnimationInstance : FRotator::ZeroRotator;
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
	return CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent())->GetEngineRotationSpeed();
}

int32 ABusVehicle::GetCurrentGear() const
{
	return GetVehicleMovementComponent()->GetCurrentGear();
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
}

void ABusVehicle::OnThrottle(const FInputActionValue& Value) { ThrottleInput = Value.Get<float>(); }
void ABusVehicle::OnBrake(const FInputActionValue& Value) { BrakeInput = Value.Get<float>(); }
void ABusVehicle::OnSteer(const FInputActionValue& Value) { SteerInput = Value.Get<float>(); }

void ABusVehicle::OnLook(const FInputActionValue& Value)
{
	const FVector2D Look = Value.Get<FVector2D>();
	AddControllerYawInput(Look.X);
	AddControllerPitchInput(-Look.Y);
}

void ABusVehicle::OnShiftGear(int32 Delta)
{
	if (bManualGearbox)
	{
		UChaosVehicleMovementComponent* Movement = GetVehicleMovementComponent();
		Movement->SetTargetGear(Movement->GetTargetGear() + Delta, false);
	}
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

	BindPress(CameraAction, [this] { SetCamera(Camera == EBusCamera::Chase ? EBusCamera::Cockpit : EBusCamera::Chase); });
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
	BindPress(GearboxModeAction, [this]
	{
		bManualGearbox = !bManualGearbox;
		GetVehicleMovementComponent()->SetUseAutomaticGears(!bManualGearbox);
	});

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
