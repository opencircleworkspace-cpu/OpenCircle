// Copyright OpenCircle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "World/CityTypes.h"
#include "CityDataAsset.generated.h"

class UStaticMesh;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct FCityTerrainSettings
{
	GENERATED_BODY()

	/** Square world size (cm) */
	UPROPERTY(EditAnywhere, Category="Terrain") float Size = 300000.f;
	/** Heightfield / render grid spacing (cm) */
	UPROPERTY(EditAnywhere, Category="Terrain") float CellSize = 500.f;
	UPROPERTY(EditAnywhere, Category="Terrain") float MountainHeight = 55000.f;
	/** Size of the largest landforms (cm) */
	UPROPERTY(EditAnywhere, Category="Terrain") float FeatureSize = 140000.f;
	UPROPERTY(EditAnywhere, Category="Terrain", meta=(ClampMin=1, ClampMax=10)) int32 Octaves = 6;
	/** 0 rolling hills .. 1 sharp Himalayan ridges */
	UPROPERTY(EditAnywhere, Category="Terrain", meta=(ClampMin=0, ClampMax=1)) float Ridged = 0.65f;
	/** Domain warp: bends ridges so they don't look like noise (cm) */
	UPROPERTY(EditAnywhere, Category="Terrain") float WarpStrength = 35000.f;
	/** River valley crossing the map along X */
	UPROPERTY(EditAnywhere, Category="Terrain") float ValleyWidth = 70000.f;
	UPROPERTY(EditAnywhere, Category="Terrain") float ValleyFloor = 0.15f;
	UPROPERTY(EditAnywhere, Category="Terrain") float RiverWidth = 6000.f;
	/** Water channel width as a fraction of the sandy bed */
	UPROPERTY(EditAnywhere, Category="Terrain", meta=(ClampMin=0.05, ClampMax=1)) float WaterWidthRatio = 0.35f;
	UPROPERTY(EditAnywhere, Category="Terrain") float RiverDepth = 400.f;
	UPROPERTY(EditAnywhere, Category="Terrain") float RiverMeander = 30000.f;
	/** River bed slope beyond the map (rise per cm toward +X, i.e. upstream) */
	UPROPERTY(EditAnywhere, Category="Terrain") float RiverGradeOutside = 0.012f;
	/** Thermal erosion: settles scree and softens noise artefacts */
	UPROPERTY(EditAnywhere, Category="Terrain", meta=(ClampMin=0)) int32 ErosionIterations = 24;
	UPROPERTY(EditAnywhere, Category="Terrain") float TalusAngleDeg = 40.f;
	/** Distant mountain ring around the playable map (render only): total width, grid spacing, extra height at the far edge (cm) */
	UPROPERTY(EditAnywhere, Category="Terrain|Backdrop") float BackdropSize = 2400000.f;
	UPROPERTY(EditAnywhere, Category="Terrain|Backdrop") float BackdropCell = 15000.f;
	UPROPERTY(EditAnywhere, Category="Terrain|Backdrop") float BackdropRise = 280000.f;
	/** Collision uses every Nth terrain vertex (roads have their own exact collision) */
	UPROPERTY(EditAnywhere, Category="Terrain", meta=(ClampMin=1, ClampMax=8)) int32 CollisionStep = 1;
	/** Render chunk size in cells (one mesh section each) */
	UPROPERTY(EditAnywhere, Category="Terrain", meta=(ClampMin=8)) int32 ChunkCells = 60;
};

USTRUCT(BlueprintType)
struct FCityRoadSettings
{
	GENERATED_BODY()

	/** Pathfinding grid spacing (cm) */
	UPROPERTY(EditAnywhere, Category="Roads") float PathCellSize = 1000.f;
	UPROPERTY(EditAnywhere, Category="Roads") float HighwayWidth = 600.f;
	UPROPERTY(EditAnywhere, Category="Roads") float StreetWidth = 500.f;
	UPROPERTY(EditAnywhere, Category="Roads") float ShortcutWidth = 420.f;
	/** Max road gradient (rise/run); forces hairpins on steep climbs */
	UPROPERTY(EditAnywhere, Category="Roads") float MaxGrade = 0.08f;
	UPROPERTY(EditAnywhere, Category="Roads") float ShortcutMaxGrade = 0.15f;
	/** The shortcut is kept only if at most this fraction of the highway distance it skips */
	UPROPERTY(EditAnywhere, Category="Roads") float ShortcutMaxLengthRatio = 0.6f;
	/** Bus must make every bend (centreline radius, cm) */
	UPROPERTY(EditAnywhere, Category="Roads") float MinTurnRadius = 1600.f;
	UPROPERTY(EditAnywhere, Category="Roads") float SampleSpacing = 400.f;
	/** Highway ends this fraction of the map size in from the edge */
	UPROPERTY(EditAnywhere, Category="Roads", meta=(ClampMin=0.02, ClampMax=0.3)) float EndInset = 0.1f;
	/** Deeper cuts/fills than this get a short steeper ramp (up to ShortcutMaxGrade) instead (cm) */
	UPROPERTY(EditAnywhere, Category="Roads") float MaxCutDepth = 600.f;
	/** Cost multipliers for pathfinding */
	UPROPERTY(EditAnywhere, Category="Roads") float SlopeCost = 60.f;
	UPROPERTY(EditAnywhere, Category="Roads") float TurnCost = 0.6f;
	UPROPERTY(EditAnywhere, Category="Roads") float WaterCost = 25.f;
	/** Penalty for ground steeper than MaxGrade (cut and fill); higher = more hairpins */
	UPROPERTY(EditAnywhere, Category="Roads") float OverGradeCost = 30.f;
	/** Penalty for running near the map border */
	UPROPERTY(EditAnywhere, Category="Roads") float EdgeCost = 8.f;
	/** Penalty for a street running alongside an existing road (keeps streets from braiding) */
	UPROPERTY(EditAnywhere, Category="Roads") float RoadOverlapCost = 40.f;
	/** Valley side: fill slope back down to natural ground over this distance */
	UPROPERTY(EditAnywhere, Category="Roads") float EmbankmentWidth = 1400.f;
	/** Hill side: near-vertical cut face rising to natural ground over this distance */
	UPROPERTY(EditAnywhere, Category="Roads") float CutWidth = 350.f;
	/** Steepest bank beside a road (rise/run); cut faces and fills are relaxed to this */
	UPROPERTY(EditAnywhere, Category="Roads") float MaxBankSlope = 1.2f;
	UPROPERTY(EditAnywhere, Category="Roads") float ShoulderWidth = 120.f;
	/** Parapet wall where the ground falls away more than this beside the road (cm) */
	UPROPERTY(EditAnywhere, Category="Roads") float ParapetDrop = 200.f;
	/** Painted parapet blocks along the drop (Himachal style), centre spacing and size (cm) */
	UPROPERTY(EditAnywhere, Category="Roads") float ParapetSpacing = 260.f;
	UPROPERTY(EditAnywhere, Category="Roads") FVector ParapetBlockSize = FVector(70.f, 35.f, 50.f);
};

USTRUCT(BlueprintType)
struct FCityTownSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Town") float Radius = 32000.f;
	/** Streets branching from the chowk / highway, then from each other */
	UPROPERTY(EditAnywhere, Category="Town") int32 PrimaryStreets = 4;
	UPROPERTY(EditAnywhere, Category="Town") int32 SecondaryStreets = 6;
	/** A street may only end on another road this far from where it started (cm) */
	UPROPERTY(EditAnywhere, Category="Town") float JunctionClearance = 1500.f;
	UPROPERTY(EditAnywhere, Category="Town") float MinStreetLength = 4000.f;
	UPROPERTY(EditAnywhere, Category="Town") float PlotSpacing = 1100.f;
	UPROPERTY(EditAnywhere, Category="Town") float PlotDepth = 900.f;
	UPROPERTY(EditAnywhere, Category="Town") float MaxPlotSlope = 0.8f;
	UPROPERTY(EditAnywhere, Category="Town") FVector2D FloorsAtEdge = FVector2D(1.f, 2.f);
	UPROPERTY(EditAnywhere, Category="Town") FVector2D FloorsAtCentre = FVector2D(2.f, 4.f);
	UPROPERTY(EditAnywhere, Category="Town") float FloorHeight = 320.f;
	/** Roadside plots outside town for dhabas etc., every this many cm of highway */
	UPROPERTY(EditAnywhere, Category="Town") float HighwayPlotInterval = 15000.f;
	UPROPERTY(EditAnywhere, Category="Nature") float TreeSpacing = 1000.f;
	UPROPERTY(EditAnywhere, Category="Nature") float TreeMaxSlope = 1.1f;
	UPROPERTY(EditAnywhere, Category="Nature") FVector2D TreeScale = FVector2D(0.8f, 1.5f);
};

/** How a landmark picks its plot; every candidate plot is scored and the best wins */
USTRUCT(BlueprintType)
struct FCityLandmarkRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Landmark") ECityLandmark Type = ECityLandmark::Market;
	UPROPERTY(EditAnywhere, Category="Landmark", meta=(ClampMin=1)) int32 Count = 1;
	/** Optional prefab spawned on the plot (bus stand, temple...). Placeholder block if empty */
	UPROPERTY(EditAnywhere, Category="Landmark") TSubclassOf<AActor> Prefab;
	UPROPERTY(EditAnywhere, Category="Landmark") FVector2D PlotSize = FVector2D(1200.f, 1200.f);
	/** Preferred centrality (1 chowk .. 0 town edge) and how much it matters */
	UPROPERTY(EditAnywhere, Category="Landmark") float TargetCentrality = 0.5f;
	UPROPERTY(EditAnywhere, Category="Landmark") float CentralityWeight = 1.f;
	/** Score for higher ground (temples on hilltops) */
	UPROPERTY(EditAnywhere, Category="Landmark") float HeightWeight = 0.f;
	UPROPERTY(EditAnywhere, Category="Landmark") float FlatnessWeight = 1.f;
	UPROPERTY(EditAnywhere, Category="Landmark") bool bRequireHighway = false;
	UPROPERTY(EditAnywhere, Category="Landmark") bool bOutsideTown = false;
	/** Must be at least this far from other landmarks of the same type (cm) */
	UPROPERTY(EditAnywhere, Category="Landmark") float MinSpacing = 6000.f;
	UPROPERTY(EditAnywhere, Category="Landmark") bool bHasBusStop = false;
	UPROPERTY(EditAnywhere, Category="Landmark") FText StopName;
};

USTRUCT(BlueprintType)
struct FCityArtKit
{
	GENERATED_BODY()

	/** Pivot at base centre, front facing +X. Engine cube if empty */
	UPROPERTY(EditAnywhere, Category="Art") TArray<TObjectPtr<UStaticMesh>> Houses;
	UPROPERTY(EditAnywhere, Category="Art") TArray<TObjectPtr<UStaticMesh>> Trees;
	/** Vertex colour: R slope (rock), G road proximity, B town, A river bed (sand); UV0 world-scaled (1 unit = 10 m) */
	UPROPERTY(EditAnywhere, Category="Art") TObjectPtr<UMaterialInterface> TerrainMaterial;
	/** UV0: U across (0..1), V along in road widths */
	UPROPERTY(EditAnywhere, Category="Art") TObjectPtr<UMaterialInterface> RoadMaterial;
	/** Parapet block (pivot at base centre, length along X). Engine cube if empty */
	UPROPERTY(EditAnywhere, Category="Art") TObjectPtr<UStaticMesh> ParapetMesh;
	UPROPERTY(EditAnywhere, Category="Art") TObjectPtr<UMaterialInterface> ParapetMaterial;
	UPROPERTY(EditAnywhere, Category="Art") TObjectPtr<UMaterialInterface> WaterMaterial;
	UPROPERTY(EditAnywhere, Category="Art") TObjectPtr<UMaterialInterface> BuildingMaterial;
	UPROPERTY(EditAnywhere, Category="Art") TObjectPtr<UMaterialInterface> LandmarkMaterial;
	/** Only used on placeholder trees (kit tree meshes keep their own materials) */
	UPROPERTY(EditAnywhere, Category="Art") TObjectPtr<UMaterialInterface> TreeMaterial;
	/** Distant scenic peaks (e.g. Fab "Mountain Tops"), placed around the horizon. Optional */
	UPROPERTY(EditAnywhere, Category="Art") TArray<TObjectPtr<UStaticMesh>> HorizonPeaks;
	UPROPERTY(EditAnywhere, Category="Art") int32 HorizonPeakCount = 14;
	/** Peak height range (cm) */
	UPROPERTY(EditAnywhere, Category="Art") FVector2D HorizonPeakHeight = FVector2D(180000.f, 320000.f);
	/** Fraction of each peak's height buried below the lowest ground under it */
	UPROPERTY(EditAnywhere, Category="Art", meta=(ClampMin=0, ClampMax=0.9)) float HorizonPeakSink = 0.3f;
	/** 0 = playable map edge .. 1 = backdrop edge */
	UPROPERTY(EditAnywhere, Category="Art", meta=(ClampMin=0, ClampMax=1)) float HorizonPeakRing = 0.7f;
	UPROPERTY(EditAnywhere, Category="Art") float TreeCullDistance = 60000.f;
	UPROPERTY(EditAnywhere, Category="Art") float HouseCullDistance = 120000.f;
};

/**
 *  One city's generation rules (Devgarh, Samundra Nagar...). All tuning lives here, not in code.
 */
UCLASS(BlueprintType)
class UCityDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="City") FText CityName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="City") int32 DefaultSeed = 1;
	UPROPERTY(EditAnywhere, Category="City") FCityTerrainSettings Terrain;
	UPROPERTY(EditAnywhere, Category="City") FCityRoadSettings Roads;
	UPROPERTY(EditAnywhere, Category="City") FCityTownSettings Town;
	UPROPERTY(EditAnywhere, Category="City") TArray<FCityLandmarkRule> Landmarks;
	UPROPERTY(EditAnywhere, Category="City") FCityArtKit Art;
	UPROPERTY(EditAnywhere, Category="City") FText StartStopName;
	UPROPERTY(EditAnywhere, Category="City") FText FinishStopName;
};
