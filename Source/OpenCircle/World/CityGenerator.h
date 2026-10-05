// Copyright OpenCircle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "World/CityDataAsset.h"

/** Square height grid in the city's local space (cm) */
struct FCityHeightfield
{
	int32 Num = 0;
	float Cell = 500.f;
	FVector2D Origin = FVector2D::ZeroVector;
	TArray<float> Heights;

	void Init(int32 InNum, float InCell, const FVector2D& InOrigin);
	float& At(int32 X, int32 Y) { return Heights[Y * Num + X]; }
	float At(int32 X, int32 Y) const { return Heights[Y * Num + X]; }
	FVector2D VertexXY(int32 X, int32 Y) const { return Origin + FVector2D(X, Y) * Cell; }
	FIntPoint NearestVertex(const FVector2D& P) const;
	bool Contains(const FVector2D& P, float Margin = 0.f) const;
	/** Bilinear, clamped to the edge */
	float Sample(const FVector2D& P) const;
	/** dz/dx, dz/dy */
	FVector2D Gradient(const FVector2D& P) const;
	float Slope(const FVector2D& P) const { return Gradient(P).Size(); }
	FVector Normal(int32 X, int32 Y) const;
};

/** Per heightfield vertex: distance past the nearest road edge (<= 0 on the road) and that road's surface height */
struct FCityRoadField
{
	TArray<float> EdgeDistance;
	TArray<float> SurfaceZ;
};

/**
 *  Deterministic city generator: same data asset + seed gives the same city on every machine,
 *  so multiplayer only needs to share the seed. Pure data, no UObjects: can run on a worker thread or server.
 *
 *  Stages: domain-warped ridged terrain + river valley -> thermal erosion -> river channel ->
 *  grade-limited A* highway (hairpins emerge on climbs) -> town site scoring -> contour-following streets ->
 *  risky shortcut -> cut/fill road carving + parapets -> plots -> scored landmark placement -> stops -> Poisson trees.
 */
class FCityGenerator
{
public:

	FCityGenerator(const UCityDataAsset& City, int32 InSeed);

	/** Returns false if no drivable highway could be found (try another seed) */
	bool Run();

	const FCityLayout& GetLayout() const { return Layout; }
	const FCityHeightfield& GetHeightfield() const { return Height; }
	/** Low-res distant mountains; vertices inside the playable square sit just under it */
	const FCityHeightfield& GetBackdrop() const { return Backdrop; }
	const FCityRoadField& GetRoadField() const { return RoadField; }
	const FCityRoadSettings& GetRoadSettings() const { return RoadCfg; }

	/** 1 at the chowk fading to 0 past the town radius */
	float TownMask(const FVector2D& P) const;
	float RiverDistance(const FVector2D& P) const;
	float RiverHalfWidth(float X) const;
	float SmoothGround(const FVector2D& P) const;
	float SmoothSlope(const FVector2D& P) const;

private:

	void GenerateTerrain();
	float RawTerrain(const FVector2D& P) const;
	void GenerateBackdrop();
	float RiverBedZ(float X) const;
	void ExtendRiver();
	void Erode();
	void CarveRiver();
	bool BuildHighway();
	void PickTownSite();
	void BuildStreets();
	void BuildShortcut();
	void CarveRoads();
	void MakePlots();
	bool FootprintClear(const FVector2D& Centre, float Yaw, const FVector2D& Size) const;
	void AssignLandmarks();
	void PlaceStops();
	void ScatterTrees();

	TArray<FVector2D> FindPath(const FVector2D& From, const FVector2D& To, float MaxGrade, bool bAvoidRoads = false) const;
	/** Smooths a raw path into a bus-drivable road: Chaikin, turn-radius relaxation, grade-limited profile */
	FCityRoad MakeRoad(const TArray<FVector2D>& Path, ECityRoadType Type, float Width, float MaxGrade,
		TOptional<float> StartZ = {}, TOptional<float> EndZ = {}) const;
	TOptional<float> NearestRoadZ(const FVector2D& P, float Radius) const;
	bool NearRoad(const FVector2D& P, float Radius, int32 IgnoreRoad = INDEX_NONE) const;
	void IndexRoad(int32 RoadIndex);
	FRandomStream Stream(const TCHAR* Stage) const;
	float RiverCentreY(float X) const;

	FCityTerrainSettings TerrainCfg;
	FCityRoadSettings RoadCfg;
	FCityTownSettings TownCfg;
	TArray<FCityLandmarkRule> Rules;
	FText StartName;
	FText FinishName;
	int32 Seed = 0;

	FCityHeightfield Height;
	FCityHeightfield Backdrop;
	FCityRoadField RoadField;
	FCityLayout Layout;
	TArray<FCityPlot> Candidates;
	TArray<float> RiverProfile;          // water bed height per heightfield column
	FVector2D NoiseOffset[3];
	float RiverPhase = 0.f;
	float RiverFreq = 0.f;
	int32 TownHighwayIndex = 0;

	/** Coarse spatial hash of road centreline points: cell -> (road, point) */
	TMultiMap<FIntPoint, FIntPoint> RoadHash;
	float RoadHashCell = 2000.f;
};
