// Copyright OpenCircle. All Rights Reserved.

#include "Vehicles/BusDriverPoseComponent.h"

UBusDriverPoseComponent::UBusDriverPoseComponent()
{
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCastShadow(true);
	PosedBones = { TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("head"),
		TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("upperarm_r"), TEXT("lowerarm_r"),
		TEXT("thigh_l"), TEXT("calf_l"), TEXT("thigh_r"), TEXT("calf_r") };
	for (const TCHAR* Side : { TEXT("l"), TEXT("r") })
	{
		for (const TCHAR* Finger : { TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") })
		{
			for (int32 Joint = 1; Joint <= 3; ++Joint)
			{
				PosedBones.Add(*FString::Printf(TEXT("%s_0%d_%s"), Finger, Joint, Side));
			}
		}
	}
}

void UBusDriverPoseComponent::InitializeSeat(const FVector& PelvisLocation)
{
	if (!GetSkinnedAsset())
	{
		return;
	}
	SetRelativeRotation(FRotator::ZeroRotator);
	SetRelativeLocation(FVector::ZeroVector);

	// Face the parent's +X: the toes (ball) point forward from the ankle in the reference pose
	const FVector Forward = GetBoneLocationByName(TEXT("ball_l"), EBoneSpaces::ComponentSpace)
		- GetBoneLocationByName(TEXT("foot_l"), EBoneSpaces::ComponentSpace);
	const float Yaw = -FMath::RadiansToDegrees(FMath::Atan2(Forward.Y, Forward.X));
	SetRelativeRotation(FRotator(0.f, Yaw, 0.f));

	const FVector Pelvis = GetRelativeRotation().RotateVector(GetBoneLocationByName(TEXT("pelvis"), EBoneSpaces::ComponentSpace));
	SetRelativeLocation(PelvisLocation - Pelvis);
	bSeated = true;
}

void UBusDriverPoseComponent::ResetPose()
{
	for (const FName& Bone : PosedBones)
	{
		ResetBoneTransformByName(Bone);
	}
}

void UBusDriverPoseComponent::SolveTwoBone(FName Upper, FName Lower, FName End, const FVector& Target, const FVector& PoleDirection)
{
	const FVector Root = GetBoneLocationByName(Upper, EBoneSpaces::WorldSpace);
	const FVector Joint = GetBoneLocationByName(Lower, EBoneSpaces::WorldSpace);
	const FVector Tip = GetBoneLocationByName(End, EBoneSpaces::WorldSpace);
	const float A = FVector::Dist(Root, Joint);
	const float B = FVector::Dist(Joint, Tip);
	if (A < KINDA_SMALL_NUMBER || B < KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Law of cosines: where the joint sits along the root->target line, and how far it bends out toward the pole
	FVector ToTarget = Target - Root;
	const float Dist = FMath::Clamp(ToTarget.Size(), FMath::Abs(A - B) + 0.1f, A + B - 0.1f);
	const FVector Dir = ToTarget.GetSafeNormal();
	const float Along = (A * A - B * B + Dist * Dist) / (2.f * Dist);
	const float Out = FMath::Sqrt(FMath::Max(A * A - Along * Along, 0.f));
	const FVector Bend = (PoleDirection - Dir * FVector::DotProduct(PoleDirection, Dir)).GetSafeNormal();
	const FVector NewJoint = Root + Dir * Along + Bend * Out;

	const FQuat UpperRot = FQuat::FindBetweenVectors(Joint - Root, NewJoint - Root)
		* GetBoneRotationByName(Upper, EBoneSpaces::WorldSpace).Quaternion();
	SetBoneRotationByName(Upper, UpperRot.Rotator(), EBoneSpaces::WorldSpace);

	const FVector MovedJoint = GetBoneLocationByName(Lower, EBoneSpaces::WorldSpace);
	const FVector MovedTip = GetBoneLocationByName(End, EBoneSpaces::WorldSpace);
	const FQuat LowerRot = FQuat::FindBetweenVectors(MovedTip - MovedJoint, Root + Dir * Dist - MovedJoint)
		* GetBoneRotationByName(Lower, EBoneSpaces::WorldSpace).Quaternion();
	SetBoneRotationByName(Lower, LowerRot.Rotator(), EBoneSpaces::WorldSpace);
}

void UBusDriverPoseComponent::UpdatePose(const FBusDriverTargets& Targets)
{
	if (!bSeated || !GetSkinnedAsset())
	{
		return;
	}
	ResetPose();

	const FTransform& Parent = GetAttachParent() ? GetAttachParent()->GetComponentTransform() : FTransform::Identity;
	auto Dir = [&Parent](const FVector& Local, float Side) { return Parent.TransformVectorNoScale(FVector(Local.X, Local.Y * Side, Local.Z)); };

	const FVector Right = Parent.GetUnitAxis(EAxis::Y);
	const FVector Forward = Parent.GetUnitAxis(EAxis::X);
	const FVector Up = Parent.GetUnitAxis(EAxis::Z);
	Time += Targets.DeltaTime;

	// Torso: base lean toward the wheel + vehicle forces + breathing, split over two spine bones
	const float Breath = FMath::Sin(Time * BreathsPerMinute / 60.f * 2.f * PI) * BreathAmplitude;
	for (const TCHAR* Bone : { TEXT("spine_01"), TEXT("spine_03") })
	{
		RotateBoneWorld(Bone, Right, (TorsoLean + Targets.LeanForward) * 0.5f + (Bone[6] == '3' ? Breath : 0.f));
		RotateBoneWorld(Bone, Forward, -Targets.LeanSide * 0.5f);
	}

	// Legs first (they do not move the torso), then arms; driver's left is -Y in vehicle space
	SolveTwoBone(TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), Targets.LeftFoot, Dir(KneePole, -1.f));
	SolveTwoBone(TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"), Targets.RightFoot, Dir(KneePole, 1.f));
	SolveTwoBone(TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"), Targets.LeftHand, Dir(ElbowPole, -1.f));
	SolveTwoBone(TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"), Targets.RightHand, Dir(ElbowPole, 1.f));

	CurlFingers(TEXT("l"), Targets.GripLeft);
	CurlFingers(TEXT("r"), Targets.GripRight);

	const FVector2D Look = UpdateIdleLook(Targets);
	RotateBoneWorld(TEXT("head"), Up, Look.X);
	RotateBoneWorld(TEXT("head"), Right, -Look.Y);
}

void UBusDriverPoseComponent::RotateBoneWorld(FName Bone, const FVector& Axis, float Degrees)
{
	const FQuat Delta(Axis.GetSafeNormal(), FMath::DegreesToRadians(Degrees));
	SetBoneRotationByName(Bone, (Delta * GetBoneRotationByName(Bone, EBoneSpaces::WorldSpace).Quaternion()).Rotator(),
		EBoneSpaces::WorldSpace);
}

void UBusDriverPoseComponent::CurlFingers(const TCHAR* Side, float Amount)
{
	// Curl axis runs across the knuckles (index base -> pinky base); each joint bends toward the palm
	const FVector Across = GetBoneLocationByName(*FString::Printf(TEXT("pinky_01_%s"), Side), EBoneSpaces::WorldSpace)
		- GetBoneLocationByName(*FString::Printf(TEXT("index_01_%s"), Side), EBoneSpaces::WorldSpace);
	const float SideSign = Side[0] == 'l' ? 1.f : -1.f;
	for (const TCHAR* Finger : { TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") })
	{
		for (int32 Joint = 1; Joint <= 3; ++Joint)
		{
			RotateBoneWorld(*FString::Printf(TEXT("%s_0%d_%s"), Finger, Joint, Side), Across,
				FingerCurlSign * SideSign * FingerCurl[Joint - 1] * Amount);
		}
	}
	// Thumb wraps under the rim: smaller bend about the same axis
	for (int32 Joint = 2; Joint <= 3; ++Joint)
	{
		RotateBoneWorld(*FString::Printf(TEXT("thumb_0%d_%s"), Joint, Side), Across, FingerCurlSign * SideSign * 25.f * Amount);
	}
}

FVector2D UBusDriverPoseComponent::UpdateIdleLook(const FBusDriverTargets& Targets)
{
	// Occasional glances: left mirror, right mirror, interior mirror; watch the door while at a stop
	if (Time > NextGlance)
	{
		static const FVector2D Glances[] = { FVector2D(-55.f, 0.f), FVector2D(50.f, -2.f), FVector2D(18.f, 12.f) };
		GlanceTarget = Glances[FMath::RandRange(0, 2)];
		GlanceEnd = Time + GlanceDuration;
		NextGlance = Time + FMath::FRandRange(GlanceInterval.X, GlanceInterval.Y);
	}
	FVector2D Target(Targets.HeadYaw, 0.f);
	if (Targets.bAtStop)
	{
		Target = FVector2D(-65.f, -5.f);                     // door is on the driver's left
	}
	else if (Time < GlanceEnd)
	{
		Target = GlanceTarget;
	}
	// Small living micro-movement
	Target += FVector2D(FMath::PerlinNoise1D(Time * 0.3f) * 4.f, FMath::PerlinNoise1D(Time * 0.23f + 7.f) * 2.5f);
	HeadLook = FMath::Vector2DInterpTo(HeadLook, Target, Targets.DeltaTime, 6.f);
	return HeadLook;
}
