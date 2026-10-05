// Copyright OpenCircle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "BusDriverPoseComponent.generated.h"

/** World-space targets the driver reaches for this frame */
USTRUCT(BlueprintType)
struct FBusDriverTargets
{
	GENERATED_BODY()

	/** Hand targets are the knuckles (not the wrist) when a finger direction is given */
	UPROPERTY(BlueprintReadWrite) FVector LeftHand = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite) FVector RightHand = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite) FVector LeftFoot = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite) FVector RightFoot = FVector::ZeroVector;
	/** Hand orientation: where the straight fingers point and which way the palm faces (zero = leave as is) */
	UPROPERTY(BlueprintReadWrite) FVector LeftFingers = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite) FVector LeftPalm = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite) FVector RightFingers = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite) FVector RightPalm = FVector::ZeroVector;
	/** Head turn from driving (steering), degrees (+ = driver's right) */
	UPROPERTY(BlueprintReadWrite) float HeadYaw = 0.f;
	/** Torso lean from vehicle forces, degrees (+ forward / + toward driver's right) */
	UPROPERTY(BlueprintReadWrite) float LeanForward = 0.f;
	UPROPERTY(BlueprintReadWrite) float LeanSide = 0.f;
	/** 0 open .. 1 fist round the wheel / knob */
	UPROPERTY(BlueprintReadWrite) float GripLeft = 1.f;
	UPROPERTY(BlueprintReadWrite) float GripRight = 1.f;
	/** Stopped at a stop with doors open: watch the door */
	UPROPERTY(BlueprintReadWrite) bool bAtStop = false;
	UPROPERTY(BlueprintReadWrite) float DeltaTime = 0.f;
};

/**
 *  Procedural seated driver: places a humanoid (UE mannequin bone names) on the seat and solves
 *  two-bone IK for both arms and legs toward the wheel, gear lever and pedals every frame.
 *  No animation assets needed; the owning vehicle supplies targets via UpdatePose.
 */
UCLASS(ClassGroup=(Bus), meta=(BlueprintSpawnableComponent))
class UBusDriverPoseComponent : public UPoseableMeshComponent
{
	GENERATED_BODY()

public:

	UBusDriverPoseComponent();

	/** Seat the mesh: faces local +X with its pelvis at PelvisLocation (parent space) */
	void InitializeSeat(const FVector& PelvisLocation);

	void UpdatePose(const FBusDriverTargets& Targets);

protected:

	/** Elbows point down, outward and back */
	UPROPERTY(EditAnywhere, Category="Driver") FVector ElbowPole = FVector(-0.4f, 0.6f, -1.f);
	/** Knees point forward and up */
	UPROPERTY(EditAnywhere, Category="Driver") FVector KneePole = FVector(1.f, 0.f, 0.6f);
	/** Torso lean toward the wheel, degrees */
	UPROPERTY(EditAnywhere, Category="Driver") float TorsoLean = 6.f;
	/** Finger joint bend at full grip, degrees (base, middle, tip); sign flips curl direction */
	UPROPERTY(EditAnywhere, Category="Driver|Hands") FVector FingerCurl = FVector(40.f, 55.f, 35.f);
	UPROPERTY(EditAnywhere, Category="Driver|Hands") float FingerCurlSign = 1.f;
	UPROPERTY(EditAnywhere, Category="Driver|Hands") float ThumbCurl = 25.f;
	UPROPERTY(EditAnywhere, Category="Driver|Idle") float BreathsPerMinute = 15.f;
	UPROPERTY(EditAnywhere, Category="Driver|Idle") float BreathAmplitude = 1.2f;
	/** Seconds between mirror glances (random in range) */
	UPROPERTY(EditAnywhere, Category="Driver|Idle") FVector2D GlanceInterval = FVector2D(5.f, 11.f);
	UPROPERTY(EditAnywhere, Category="Driver|Idle") float GlanceDuration = 0.9f;

private:

	void SolveTwoBone(FName Upper, FName Lower, FName End, const FVector& Target, const FVector& PoleDirection);
	void ResetPose();
	void RotateBoneWorld(FName Bone, const FVector& Axis, float Degrees);
	/** Current finger direction and palm normal of a hand, from its bones (thumb marks the palm side) */
	void HandFrame(const TCHAR* Side, FVector& OutFingers, FVector& OutPalm);
	FVector WristTarget(const TCHAR* Side, const FVector& Knuckles, const FVector& Fingers);
	void AlignHand(const TCHAR* Side, const FVector& Fingers, const FVector& Palm);
	void CurlFingers(const TCHAR* Side, float Amount);
	FVector2D UpdateIdleLook(const FBusDriverTargets& Targets);

	TArray<FName> PosedBones;
	bool bSeated = false;
	float Time = 0.f;
	float NextGlance = 4.f;
	float GlanceEnd = 0.f;
	FVector2D GlanceTarget = FVector2D::ZeroVector;
	FVector2D HeadLook = FVector2D::ZeroVector;
};
