// Copyright OpenCircle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BusTelemetryComponent.generated.h"

/**
 *  Records bus state to Saved/Logs/BusTelemetry.csv and can run a scripted drive test.
 *  Console: bus.Telemetry 1 (record), bus.AutoTest 1 (scripted drive, then quit when run with -game).
 *  Headless run: UnrealEditor.exe OpenCircle.uproject /Game/Maps/Lvl_BusTest -game -windowed -ResX=800 -ResY=450 -ExecCmds="bus.AutoTest 1"
 */
UCLASS(ClassGroup=(Bus), meta=(BlueprintSpawnableComponent))
class UBusTelemetryComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UBusTelemetryComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	UPROPERTY(EditAnywhere, Category="Telemetry")
	float SampleInterval = 0.2f;

private:

	void WriteSample();
	void RunAutoTest(float Time);

	FString FilePath;
	float SampleTimer = 0.f;
	float TestTime = 0.f;
	bool bHeaderWritten = false;
};
