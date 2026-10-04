// Copyright OpenCircle. All Rights Reserved.

#include "UI/BusHUD.h"
#include "Vehicles/BusVehicle.h"
#include "Vehicles/BusAirSystemComponent.h"
#include "Vehicles/BusControlsComponent.h"
#include "Vehicles/BusDrivetrainComponent.h"
#include "Gameplay/BusStop.h"
#include "Components/SkeletalMeshComponent.h"

void ABusHUD::DrawHUD()
{
	Super::DrawHUD();

	const ABusVehicle* Bus = Cast<ABusVehicle>(GetOwningPawn());
	if (!Bus)
	{
		return;
	}

	const UBusAirSystemComponent* Air = Bus->GetAirSystem();
	const UBusControlsComponent* Controls = Bus->GetControls();
	const int32 Gear = Bus->GetCurrentGear();
	const FString GearText = Gear < 0 ? TEXT("R") : Gear == 0 ? TEXT("N") : FString::FromInt(Gear);
	const TCHAR* Blink[] = { TEXT(""), TEXT("<<"), TEXT(">>"), TEXT("<< HAZARD >>") };

	TArray<TPair<FString, FLinearColor>> Lines;
	auto Add = [&Lines](const FString& Text, const FLinearColor& Color = FLinearColor::White) { Lines.Emplace(Text, Color); };

	const UBusDrivetrainComponent* Drive = Bus->GetDrivetrain();
	const TCHAR* EngineText[] = { TEXT("ENGINE OFF (I to start)"), TEXT("CRANKING"), TEXT("running") };
	Add(FString::Printf(TEXT("%3.0f km/h   %4.0f rpm   Gear %s%s %s"), FMath::Abs(Bus->GetSpeedKmh()), Bus->GetEngineRpm(),
		*GearText, Drive->IsShifting() ? TEXT(" (shifting)") : TEXT(""), Bus->IsManualGearbox() ? TEXT("[manual]") : TEXT("[auto]")));
	Add(FString::Printf(TEXT("Engine %s  %3.0f Nm  turbo %3.0f%%  clutch %3.0f%%%s%s"), EngineText[static_cast<int32>(Drive->GetEngineState())],
		Drive->GetEngineTorque(), Drive->GetTurboBoost() * 100.f, Drive->GetClutchEngagement() * 100.f,
		Drive->IsClutchLocked() ? TEXT(" locked") : TEXT(""), Drive->IsExhaustBrakeOn() ? TEXT("  EXHAUST BRAKE") : TEXT("")),
		Drive->GetEngineState() == EBusEngineState::Running ? FLinearColor::White : FLinearColor::Red);
	Add(FString::Printf(TEXT("Air %.1f bar"), Air->GetPressure()), Air->IsLowPressure() ? FLinearColor::Red : FLinearColor::White);
	if (Air->IsParkingBrakeApplied())
	{
		Add(TEXT("PARKING BRAKE"), FLinearColor::Red);
	}
	Add(FString::Printf(TEXT("Doors  front %s  rear %s"), Bus->IsDoorOpen(0) ? TEXT("OPEN") : TEXT("shut"),
		Bus->IsDoorOpen(1) ? TEXT("OPEN") : TEXT("shut")), Bus->AreDoorsOpen() ? FLinearColor::Yellow : FLinearColor::White);
	if (Controls->GetIndicator() != EBusIndicator::Off && Controls->IsBlinkOn())
	{
		Add(Blink[static_cast<int32>(Controls->GetIndicator())], FLinearColor(1.f, 0.5f, 0.f));
	}
	Add(FString::Printf(TEXT("Lights %s   Wipers %s"), Controls->AreHeadlightsOn() ? TEXT("on") : TEXT("off"),
		Controls->AreWipersOn() ? TEXT("on") : TEXT("off")));
	if (const ABusStop* Stop = Bus->CurrentStop)
	{
		Add(Stop->IsServed() ? FString::Printf(TEXT("%s: served"), *Stop->GetStopName().ToString())
			: FString::Printf(TEXT("%s: stop and open doors (%.0f%%)"), *Stop->GetStopName().ToString(), Stop->GetDwellProgress() * 100.f),
			Stop->IsServed() ? FLinearColor::Green : FLinearColor::Yellow);
	}
	if (!Bus->GetMesh()->IsSimulatingPhysics())
	{
		Add(FString::Printf(TEXT("PHYSICS NOT SIMULATING (bodies %d): check SK_Bus physics asset"), Bus->GetMesh()->Bodies.Num()), FLinearColor::Red);
	}
	Add(Bus->GetDebugString(), FLinearColor(0.4f, 0.9f, 1.f));
	Add(TEXT("W/S pedals (auto: hold S stopped = reverse)  A/D steer  M gearbox  Q/E gears  Shift clutch  I ignition  B exhaust brake  L lights  1/2 doors  C camera"),
		FLinearColor(0.7f, 0.7f, 0.7f));

	float Y = 30.f;
	for (const TPair<FString, FLinearColor>& Line : Lines)
	{
		DrawText(Line.Key, Line.Value, 30.f, Y, nullptr, TextScale);
		Y += 22.f * TextScale;
	}
}
