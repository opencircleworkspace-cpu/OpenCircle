// Copyright OpenCircle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Engine/EngineTypes.h"
#include "DiverAnimInstance.generated.h"

class ACharacter;
class UCharacterMovementComponent;

/**
 *  Native base for the diver Anim Blueprint.
 *  Gathers movement state once per frame so the AnimGraph only reads values.
 */
UCLASS()
class UDiverAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

protected:

	/** Horizontal speed in cm/s */
	UPROPERTY(BlueprintReadOnly, Category="Movement")
	float GroundSpeed = 0.f;

	/** Full 3D speed in cm/s, used while swimming */
	UPROPERTY(BlueprintReadOnly, Category="Movement")
	float SwimSpeed = 0.f;

	/** Vertical velocity in cm/s (+up) */
	UPROPERTY(BlueprintReadOnly, Category="Movement")
	float VerticalSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Movement")
	bool bIsSwimming = false;

	UPROPERTY(BlueprintReadOnly, Category="Movement")
	bool bIsFalling = false;

	/** True while there is movement input */
	UPROPERTY(BlueprintReadOnly, Category="Movement")
	bool bIsAccelerating = false;

	virtual void NativeInitializeAnimation() override;
	/** Game thread: copy raw state from the owner (keep minimal) */
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** Worker thread: derive AnimGraph values from the copied state */
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

private:

	/** Raw state copied on the game thread */
	FVector CachedVelocity = FVector::ZeroVector;
	FVector CachedAcceleration = FVector::ZeroVector;
	TEnumAsByte<EMovementMode> CachedMovementMode = MOVE_None;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwningCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> MovementComponent;
};
