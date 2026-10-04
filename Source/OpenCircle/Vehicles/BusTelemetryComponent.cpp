// Copyright OpenCircle. All Rights Reserved.

#include "Vehicles/BusTelemetryComponent.h"
#include "Vehicles/BusVehicle.h"
#include "Vehicles/BusAirSystemComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Kismet/KismetSystemLibrary.h"

namespace BusTelemetry
{
	static TAutoConsoleVariable<int32> CVarTelemetry(TEXT("bus.Telemetry"), 0, TEXT("1 = record bus state to Saved/Logs/BusTelemetry.csv"));
	static TAutoConsoleVariable<int32> CVarAutoTest(TEXT("bus.AutoTest"), 0, TEXT("1 = scripted drive test (records telemetry, quits afterwards in -game)"));

	// Scripted test: (end time s, throttle, brake, steer)
	struct FStep { float End, Throttle, Brake, Steer; };
	const FStep Steps[] = { { 2.f, 0.f, 0.f, 0.f }, { 8.f, 1.f, 0.f, 0.f }, { 12.f, 1.f, 0.f, 0.6f }, { 16.f, 0.f, 1.f, 0.f }, { 18.f, 0.f, 0.f, 0.f } };
}

UBusTelemetryComponent::UBusTelemetryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UBusTelemetryComponent::BeginPlay()
{
	Super::BeginPlay();
	FilePath = FPaths::ProjectLogDir() / TEXT("BusTelemetry.csv");
}

void UBusTelemetryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ABusVehicle* Bus = Cast<ABusVehicle>(GetOwner());
	if (!Bus || !Bus->IsPlayerControlled())
	{
		return;
	}

	const bool bAutoTest = BusTelemetry::CVarAutoTest.GetValueOnGameThread() != 0;
	if (bAutoTest)
	{
		TestTime += DeltaTime;
		RunAutoTest(TestTime);
	}

	if (bAutoTest || BusTelemetry::CVarTelemetry.GetValueOnGameThread() != 0)
	{
		SampleTimer += DeltaTime;
		if (SampleTimer >= SampleInterval)
		{
			SampleTimer = 0.f;
			WriteSample();
		}
	}
}

void UBusTelemetryComponent::RunAutoTest(float Time)
{
	ABusVehicle* Bus = CastChecked<ABusVehicle>(GetOwner());
	for (const BusTelemetry::FStep& Step : BusTelemetry::Steps)
	{
		if (Time < Step.End)
		{
			Bus->SetDriverInputs(Step.Throttle, Step.Brake, Step.Steer);
			return;
		}
	}

	Bus->SetDriverInputs(0.f, 0.f, 0.f);
	IConsoleManager::Get().FindConsoleVariable(TEXT("bus.AutoTest"))->Set(0);
	UE_LOG(LogTemp, Warning, TEXT("BUS_AUTOTEST_DONE %s"), *FilePath);
	if (!GIsEditor)
	{
		UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
	}
}

void UBusTelemetryComponent::WriteSample()
{
	const ABusVehicle* Bus = CastChecked<ABusVehicle>(GetOwner());
	const UChaosWheeledVehicleMovementComponent* Movement = CastChecked<UChaosWheeledVehicleMovementComponent>(Bus->GetVehicleMovementComponent());
	const USkeletalMeshComponent* Chassis = Bus->GetMesh();

	if (!bHeaderWritten)
	{
		FString Header = TEXT("time,throttle,brake,steer,park,simulating,mass,speed_kmh,rpm,gear,x,y,z,pitch,roll,yaw,wheels");
		for (int32 i = 0; i < Movement->WheelSetups.Num(); ++i)
		{
			Header += FString::Printf(TEXT(",w%d_air,w%d_susp,w%d_rpm"), i, i, i);
		}
		FFileHelper::SaveStringToFile(Header + LINE_TERMINATOR, *FilePath);
		bHeaderWritten = true;
	}

	const FVector Location = Bus->GetActorLocation();
	const FRotator Rotation = Bus->GetActorRotation();
	FString Line = FString::Printf(TEXT("%.2f,%.2f,%.2f,%.2f,%d,%d,%.0f,%.1f,%.0f,%d,%.0f,%.0f,%.0f,%.1f,%.1f,%.1f,%d"),
		Bus->GetWorld()->GetTimeSeconds(), Bus->GetThrottleInput(), Bus->GetBrakeInput(), Bus->GetSteerInput(),
		Bus->GetAirSystem()->IsParkingBrakeApplied() ? 1 : 0, Chassis->IsSimulatingPhysics() ? 1 : 0, Chassis->GetMass(),
		Bus->GetSpeedKmh(), Bus->GetEngineRpm(), Bus->GetCurrentGear(),
		Location.X, Location.Y, Location.Z, Rotation.Pitch, Rotation.Roll, Rotation.Yaw, Movement->Wheels.Num());

	for (const UChaosVehicleWheel* Wheel : Movement->Wheels)
	{
		Line += Wheel ? FString::Printf(TEXT(",%d,%.1f,%.0f"), Wheel->IsInAir() ? 1 : 0, Wheel->GetSuspensionOffset(),
			Wheel->GetWheelAngularVelocity() * 9.549f) : TEXT(",,,");
	}
	FFileHelper::SaveStringToFile(Line + LINE_TERMINATOR, *FilePath, FFileHelper::EEncodingOptions::AutoDetect,
		&IFileManager::Get(), FILEWRITE_Append);
}
