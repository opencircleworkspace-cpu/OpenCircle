// Copyright OpenCircle. All Rights Reserved.

#include "Player/DiverAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void UDiverAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwningCharacter = Cast<ACharacter>(TryGetPawnOwner());
	MovementComponent = OwningCharacter ? OwningCharacter->GetCharacterMovement() : nullptr;
}

void UDiverAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!MovementComponent)
	{
		return;
	}

	CachedVelocity = MovementComponent->Velocity;
	CachedAcceleration = MovementComponent->GetCurrentAcceleration();
	CachedMovementMode = MovementComponent->MovementMode;
}

void UDiverAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	GroundSpeed = CachedVelocity.Size2D();
	SwimSpeed = CachedVelocity.Size();
	VerticalSpeed = CachedVelocity.Z;
	bIsSwimming = CachedMovementMode == MOVE_Swimming;
	bIsFalling = CachedMovementMode == MOVE_Falling;
	bIsAccelerating = !CachedAcceleration.IsNearlyZero();
}
