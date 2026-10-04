// Copyright OpenCircle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BusControlsComponent.generated.h"

class UMeshComponent;
class UMaterialInstanceDynamic;
class ULightComponent;

UENUM(BlueprintType)
enum class EBusIndicator : uint8
{
	Off,
	Left,
	Right,
	Hazard
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBusToggle, bool, bOn);

/**
 *  Cab switches: indicators, hazards, horn, headlights, wipers.
 *  Drives lamp emissive through material slots and broadcasts events for audio.
 */
UCLASS(ClassGroup=(Bus), meta=(BlueprintSpawnableComponent))
class UBusControlsComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UBusControlsComponent();

	/** Binds lamp materials on the body mesh and the headlight light components */
	void Initialize(UMeshComponent* LampMesh, const TArray<ULightComponent*>& InHeadlights);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Bus|Controls")
	void ToggleIndicator(EBusIndicator Side);

	UFUNCTION(BlueprintCallable, Category="Bus|Controls")
	void SetHorn(bool bOn);

	UFUNCTION(BlueprintCallable, Category="Bus|Controls")
	void ToggleHeadlights();

	UFUNCTION(BlueprintCallable, Category="Bus|Controls")
	void ToggleWipers();

	UFUNCTION(BlueprintPure, Category="Bus|Controls")
	EBusIndicator GetIndicator() const { return Indicator; }

	UFUNCTION(BlueprintPure, Category="Bus|Controls")
	bool IsBlinkOn() const { return bBlinkOn; }

	UFUNCTION(BlueprintPure, Category="Bus|Controls")
	bool AreHeadlightsOn() const { return bHeadlights; }

	UFUNCTION(BlueprintPure, Category="Bus|Controls")
	bool AreWipersOn() const { return bWipers; }

	UPROPERTY(BlueprintAssignable, Category="Bus|Controls")
	FOnBusToggle OnHorn;

	/** Fires on every blink edge; use for the click sound */
	UPROPERTY(BlueprintAssignable, Category="Bus|Controls")
	FOnBusToggle OnBlink;

	UPROPERTY(BlueprintAssignable, Category="Bus|Controls")
	FOnBusToggle OnWipers;

protected:

	UPROPERTY(EditAnywhere, Category="Bus|Controls")
	float BlinkInterval = 0.4f;

	UPROPERTY(EditAnywhere, Category="Bus|Controls")
	float LampGlow = 25.f;

	/** Material slot names on the body mesh (from Tools/Blender/make_bus.py) */
	UPROPERTY(EditAnywhere, Category="Bus|Controls")
	FName LeftIndicatorSlot = TEXT("M_IndicatorL");

	UPROPERTY(EditAnywhere, Category="Bus|Controls")
	FName RightIndicatorSlot = TEXT("M_IndicatorR");

	UPROPERTY(EditAnywhere, Category="Bus|Controls")
	FName HeadlightSlot = TEXT("M_Light");

private:

	void ApplyLamps();
	UMaterialInstanceDynamic* MakeLamp(UMeshComponent* Mesh, FName Slot);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LeftLamp;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RightLamp;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HeadLamp;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ULightComponent>> Headlights;

	EBusIndicator Indicator = EBusIndicator::Off;
	bool bBlinkOn = false;
	bool bHeadlights = false;
	bool bWipers = false;
	float BlinkTimer = 0.f;
};
