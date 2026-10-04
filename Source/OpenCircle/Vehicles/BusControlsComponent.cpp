// Copyright OpenCircle. All Rights Reserved.

#include "Vehicles/BusControlsComponent.h"
#include "Components/MeshComponent.h"
#include "Components/LightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

UBusControlsComponent::UBusControlsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UBusControlsComponent::Initialize(UMeshComponent* LampMesh, const TArray<ULightComponent*>& InHeadlights)
{
	LeftLamp = MakeLamp(LampMesh, LeftIndicatorSlot);
	RightLamp = MakeLamp(LampMesh, RightIndicatorSlot);
	HeadLamp = MakeLamp(LampMesh, HeadlightSlot);
	Headlights.Reset();
	Headlights.Append(InHeadlights);
	ApplyLamps();
}

UMaterialInstanceDynamic* UBusControlsComponent::MakeLamp(UMeshComponent* Mesh, FName Slot)
{
	const int32 Index = Mesh ? Mesh->GetMaterialIndex(Slot) : INDEX_NONE;
	return Index == INDEX_NONE ? nullptr : Mesh->CreateAndSetMaterialInstanceDynamic(Index);
}

void UBusControlsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Indicator == EBusIndicator::Off)
	{
		return;
	}

	BlinkTimer += DeltaTime;
	if (BlinkTimer >= BlinkInterval)
	{
		BlinkTimer = 0.f;
		bBlinkOn = !bBlinkOn;
		OnBlink.Broadcast(bBlinkOn);
		ApplyLamps();
	}
}

void UBusControlsComponent::ToggleIndicator(EBusIndicator Side)
{
	Indicator = Indicator == Side ? EBusIndicator::Off : Side;
	bBlinkOn = Indicator != EBusIndicator::Off;
	BlinkTimer = 0.f;
	ApplyLamps();
}

void UBusControlsComponent::SetHorn(bool bOn)
{
	OnHorn.Broadcast(bOn);
}

void UBusControlsComponent::ToggleHeadlights()
{
	bHeadlights = !bHeadlights;
	ApplyLamps();
}

void UBusControlsComponent::ToggleWipers()
{
	bWipers = !bWipers;
	OnWipers.Broadcast(bWipers);
}

void UBusControlsComponent::ApplyLamps()
{
	const bool bLeft = bBlinkOn && (Indicator == EBusIndicator::Left || Indicator == EBusIndicator::Hazard);
	const bool bRight = bBlinkOn && (Indicator == EBusIndicator::Right || Indicator == EBusIndicator::Hazard);
	const FLinearColor Amber(1.f, 0.45f, 0.f);

	if (LeftLamp)
	{
		LeftLamp->SetVectorParameterValue(TEXT("Emissive"), bLeft ? Amber * LampGlow : FLinearColor::Black);
	}
	if (RightLamp)
	{
		RightLamp->SetVectorParameterValue(TEXT("Emissive"), bRight ? Amber * LampGlow : FLinearColor::Black);
	}
	if (HeadLamp)
	{
		HeadLamp->SetVectorParameterValue(TEXT("Emissive"), bHeadlights ? FLinearColor(1.f, 0.95f, 0.8f) * LampGlow : FLinearColor::Black);
	}
	for (ULightComponent* Light : Headlights)
	{
		if (Light)
		{
			Light->SetVisibility(bHeadlights);
		}
	}
}
