// Copyright OpenCircle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CityTypes.generated.h"

UENUM(BlueprintType)
enum class ECityRoadType : uint8
{
	Highway,
	Street,
	/** Steep, narrow alternative to part of the highway (the "risky" route) */
	Shortcut
};

UENUM(BlueprintType)
enum class ECityLandmark : uint8
{
	House,
	BusStand,
	Chowk,
	Market,
	Temple,
	Dhaba,
	Depot
};

USTRUCT(BlueprintType)
struct FCityRoad
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="City") ECityRoadType Type = ECityRoadType::Street;
	/** Carriageway width (cm) */
	UPROPERTY(BlueprintReadOnly, Category="City") float Width = 600.f;
	/** Centreline in the city actor's local space (cm), evenly spaced, Z = road surface */
	UPROPERTY(BlueprintReadOnly, Category="City") TArray<FVector> Points;
	/** Per point: bit 0 parapet on the left edge, bit 1 on the right edge */
	UPROPERTY(BlueprintReadOnly, Category="City") TArray<uint8> Walls;
};

USTRUCT(BlueprintType)
struct FCityPlot
{
	GENERATED_BODY()

	/** Footprint centre at ground level */
	UPROPERTY(BlueprintReadOnly, Category="City") FVector Location = FVector::ZeroVector;
	/** Front faces the street */
	UPROPERTY(BlueprintReadOnly, Category="City") float Yaw = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="City") FVector2D Size = FVector2D(800.f, 800.f);
	UPROPERTY(BlueprintReadOnly, Category="City") ECityLandmark Use = ECityLandmark::House;
	/** 1 at the chowk, 0 at the town edge and beyond */
	UPROPERTY(BlueprintReadOnly, Category="City") float Centrality = 0.f;
	/** Point on the road this plot fronts */
	UPROPERTY(BlueprintReadOnly, Category="City") FVector RoadPoint = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category="City") bool bOnHighway = false;
};

USTRUCT(BlueprintType)
struct FCityStop
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="City") FTransform Transform;
	UPROPERTY(BlueprintReadOnly, Category="City") FText Name;
};

/** Everything the generator decides; pure data, identical on every machine for the same seed */
USTRUCT(BlueprintType)
struct FCityLayout
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="City") int32 Seed = 0;
	UPROPERTY(BlueprintReadOnly, Category="City") TArray<FCityRoad> Roads;
	UPROPERTY(BlueprintReadOnly, Category="City") TArray<FCityPlot> Plots;
	UPROPERTY(BlueprintReadOnly, Category="City") TArray<FCityStop> Stops;
	UPROPERTY(BlueprintReadOnly, Category="City") TArray<FTransform> Trees;
	/** Race start (A) and finish (B), facing along the highway */
	UPROPERTY(BlueprintReadOnly, Category="City") FTransform Start;
	UPROPERTY(BlueprintReadOnly, Category="City") FTransform Finish;
	UPROPERTY(BlueprintReadOnly, Category="City") FVector TownCentre = FVector::ZeroVector;
	/** River centreline, Z = water surface */
	UPROPERTY(BlueprintReadOnly, Category="City") TArray<FVector> River;
	/** Channel half width at each river point (cm); the water mesh is wider and the banks clip it */
	UPROPERTY(BlueprintReadOnly, Category="City") TArray<float> RiverHalfWidths;
};
