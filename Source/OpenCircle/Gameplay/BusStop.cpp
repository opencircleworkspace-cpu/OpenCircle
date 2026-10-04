// Copyright OpenCircle. All Rights Reserved.

#include "Gameplay/BusStop.h"
#include "Vehicles/BusVehicle.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

ABusStop::ABusStop()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;

	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	Zone->SetBoxExtent(FVector(800.f, 300.f, 250.f));
	Zone->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	RootComponent = Zone;

	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	Marker->SetupAttachment(Zone);
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABusStop::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TArray<AActor*> Buses;
	Zone->GetOverlappingActors(Buses, ABusVehicle::StaticClass());

	ABusVehicle* Bus = Buses.Num() > 0 ? CastChecked<ABusVehicle>(Buses[0]) : nullptr;
	if (LastBus.IsValid() && LastBus.Get() != Bus && LastBus->CurrentStop == this)
	{
		LastBus->CurrentStop = nullptr;    // bus left the zone
	}
	LastBus = Bus;
	for (AActor* Other : Buses)
	{
		ABusVehicle* Vehicle = CastChecked<ABusVehicle>(Other);
		if (Vehicle->CurrentStop == nullptr)
		{
			Vehicle->CurrentStop = this;
		}
	}

	if (!Bus)
	{
		DwellTimer = 0.f;
		bServed = false;      // ready for the next visit
		return;
	}

	const bool bDwelling = FMath::Abs(Bus->GetSpeedKmh()) < StoppedSpeedKmh && Bus->AreDoorsOpen();
	DwellTimer = bDwelling ? DwellTimer + DeltaTime : 0.f;
	if (!bServed && DwellTimer >= DwellTime)
	{
		bServed = true;
		OnBusServed.Broadcast(this, Bus);
	}
}
