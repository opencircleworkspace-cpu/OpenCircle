// Copyright OpenCircle. All Rights Reserved.

#include "World/CityGenerator.h"

DEFINE_LOG_CATEGORY_STATIC(LogCityGen, Log, All);

// ---------------------------------------------------------------------------------------------------------------------
// Heightfield

void FCityHeightfield::Init(int32 InNum, float InCell, const FVector2D& InOrigin)
{
	Num = InNum;
	Cell = InCell;
	Origin = InOrigin;
	Heights.Init(0.f, Num * Num);
}

FIntPoint FCityHeightfield::NearestVertex(const FVector2D& P) const
{
	const FVector2D G = (P - Origin) / Cell;
	return FIntPoint(FMath::Clamp(FMath::RoundToInt(G.X), 0, Num - 1), FMath::Clamp(FMath::RoundToInt(G.Y), 0, Num - 1));
}

bool FCityHeightfield::Contains(const FVector2D& P, float Margin) const
{
	const float Size = (Num - 1) * Cell;
	return P.X >= Origin.X + Margin && P.Y >= Origin.Y + Margin && P.X <= Origin.X + Size - Margin && P.Y <= Origin.Y + Size - Margin;
}

float FCityHeightfield::Sample(const FVector2D& P) const
{
	const FVector2D G = (P - Origin) / Cell;
	const float GX = FMath::Clamp(G.X, 0.f, float(Num - 1) - 0.001f);
	const float GY = FMath::Clamp(G.Y, 0.f, float(Num - 1) - 0.001f);
	const int32 X = FMath::FloorToInt(GX);
	const int32 Y = FMath::FloorToInt(GY);
	const float FX = GX - X;
	const float FY = GY - Y;
	return FMath::Lerp(FMath::Lerp(At(X, Y), At(X + 1, Y), FX), FMath::Lerp(At(X, Y + 1), At(X + 1, Y + 1), FX), FY);
}

FVector2D FCityHeightfield::Gradient(const FVector2D& P) const
{
	const float D = Cell;
	return FVector2D(Sample(P + FVector2D(D, 0)) - Sample(P - FVector2D(D, 0)), Sample(P + FVector2D(0, D)) - Sample(P - FVector2D(0, D))) / (2.f * D);
}

FVector FCityHeightfield::Normal(int32 X, int32 Y) const
{
	const float L = At(FMath::Max(X - 1, 0), Y);
	const float R = At(FMath::Min(X + 1, Num - 1), Y);
	const float D = At(X, FMath::Max(Y - 1, 0));
	const float U = At(X, FMath::Min(Y + 1, Num - 1));
	return FVector(L - R, D - U, 2.f * Cell).GetSafeNormal();
}

// ---------------------------------------------------------------------------------------------------------------------
// Helpers

namespace CityGen
{
	float SmoothStep01(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); }

	/** Ridged multifractal mixed with plain fBm; Ridged 0..1 */
	float Terrain(const FVector2D& Q, int32 Octaves, float Ridged, const FVector2D& Offset)
	{
		float Sum = 0.f, Norm = 0.f, Amp = 1.f, Freq = 1.f, Weight = 1.f;
		for (int32 O = 0; O < Octaves; ++O)
		{
			const float N = FMath::PerlinNoise2D(Q * Freq + Offset * (O + 1));
			float Ridge = 1.f - FMath::Abs(N);
			Ridge *= Ridge * Weight;
			Weight = FMath::Clamp(Ridge * 2.f, 0.f, 1.f);
			Sum += FMath::Lerp(N * 0.5f + 0.5f, Ridge, Ridged) * Amp;
			Norm += Amp;
			Amp *= 0.5f;
			Freq *= 2.03f;
		}
		return Sum / Norm;
	}

	float Fbm(const FVector2D& Q, int32 Octaves, const FVector2D& Offset)
	{
		float Sum = 0.f, Amp = 0.5f, Freq = 1.f;
		for (int32 O = 0; O < Octaves; ++O, Amp *= 0.5f, Freq *= 2.f)
		{
			Sum += FMath::PerlinNoise2D(Q * Freq + Offset) * Amp;
		}
		return Sum;
	}

	TArray<FVector2D> Resample(const TArray<FVector2D>& In, float Spacing)
	{
		TArray<FVector2D> Out;
		if (In.Num() < 2)
		{
			return In;
		}
		Out.Add(In[0]);
		float Carry = 0.f;
		for (int32 i = 1; i < In.Num(); ++i)
		{
			const FVector2D A = In[i - 1];
			const FVector2D Seg = In[i] - A;
			const float Len = Seg.Size();
			float T = Spacing - Carry;
			while (T <= Len)
			{
				Out.Add(A + Seg * (T / Len));
				T += Spacing;
			}
			Carry = Len - (T - Spacing);
		}
		if (FVector2D::Distance(Out.Last(), In.Last()) > Spacing * 0.3f)
		{
			Out.Add(In.Last());
		}
		else
		{
			Out.Last() = In.Last();
		}
		return Out;
	}

	TArray<FVector2D> Chaikin(TArray<FVector2D> P, int32 Iterations)
	{
		for (int32 It = 0; It < Iterations && P.Num() > 2; ++It)
		{
			TArray<FVector2D> Out;
			Out.Add(P[0]);
			for (int32 i = 0; i < P.Num() - 1; ++i)
			{
				Out.Add(FMath::Lerp(P[i], P[i + 1], 0.25f));
				Out.Add(FMath::Lerp(P[i], P[i + 1], 0.75f));
			}
			Out.Add(P.Last());
			P = MoveTemp(Out);
		}
		return P;
	}

	float PolylineLength(const TArray<FVector>& P, int32 From, int32 To)
	{
		float L = 0.f;
		for (int32 i = From + 1; i <= To; ++i)
		{
			L += FVector::Dist2D(P[i - 1], P[i]);
		}
		return L;
	}
}

// ---------------------------------------------------------------------------------------------------------------------

FCityGenerator::FCityGenerator(const UCityDataAsset& City, int32 InSeed)
	: TerrainCfg(City.Terrain), RoadCfg(City.Roads), TownCfg(City.Town), Rules(City.Landmarks)
	, StartName(City.StartStopName), FinishName(City.FinishStopName), Seed(InSeed)
{
	Layout.Seed = Seed;
}

FRandomStream FCityGenerator::Stream(const TCHAR* Stage) const
{
	// Independent stream per stage: tuning one stage never reshuffles the others
	return FRandomStream(int32(HashCombine(GetTypeHash(Seed), GetTypeHash(FString(Stage)))));
}

bool FCityGenerator::Run()
{
	const double T0 = FPlatformTime::Seconds();
	double Last = T0;
	FString Timing;
	auto Stage = [&](const TCHAR* Name)
	{
		const double Now = FPlatformTime::Seconds();
		Timing += FString::Printf(TEXT(" %s=%.0fms"), Name, (Now - Last) * 1000.0);
		Last = Now;
	};
	GenerateTerrain(); Stage(TEXT("terrain"));
	Erode(); Stage(TEXT("erode"));
	CarveRiver(); Stage(TEXT("river"));
	if (!BuildHighway())
	{
		UE_LOG(LogCityGen, Warning, TEXT("City seed %d: no drivable highway found"), Seed);
		return false;
	}
	Stage(TEXT("highway"));
	PickTownSite(); Stage(TEXT("town"));
	BuildStreets(); Stage(TEXT("streets"));
	BuildShortcut(); Stage(TEXT("shortcut"));
	CarveRoads(); Stage(TEXT("carve"));
	MakePlots(); Stage(TEXT("plots"));
	AssignLandmarks(); Stage(TEXT("landmarks"));
	PlaceStops(); Stage(TEXT("stops"));
	ScatterTrees(); Stage(TEXT("trees"));
	GenerateBackdrop(); Stage(TEXT("backdrop"));
	ExtendRiver();
	UE_LOG(LogCityGen, Display, TEXT("City seed %d: %d roads, %d plots, %d stops, %d trees in %.2fs:%s"), Seed, Layout.Roads.Num(),
		Layout.Plots.Num(), Layout.Stops.Num(), Layout.Trees.Num(), FPlatformTime::Seconds() - T0, *Timing);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Terrain

float FCityGenerator::RiverCentreY(float X) const
{
	// Broad bends plus noise at three scales: irregular natural meanders, never a clean sine
	const float Broad = (FMath::Sin(X * RiverFreq + RiverPhase) + 0.5f * FMath::Sin(X * RiverFreq * 2.3f + RiverPhase * 1.7f)) / 1.5f;
	const float Wander = FMath::PerlinNoise1D(X / 40000.f + RiverPhase) * 0.6f + FMath::PerlinNoise1D(X / 12000.f + RiverPhase * 3.f) * 0.25f
		+ FMath::PerlinNoise1D(X / 3500.f + RiverPhase * 7.f) * 0.06f;
	return TerrainCfg.RiverMeander * (Broad + Wander);
}

float FCityGenerator::RiverHalfWidth(float X) const
{
	// Narrows through gorges, widens in pools
	const float N = FMath::PerlinNoise1D(X / 9000.f + RiverPhase * 5.f) * 0.5f + 0.5f;
	return TerrainCfg.RiverWidth * TerrainCfg.WaterWidthRatio * 0.5f * FMath::Lerp(0.55f, 1.6f, N);
}

float FCityGenerator::RiverDistance(const FVector2D& P) const
{
	return FMath::Abs(P.Y - RiverCentreY(P.X));
}

void FCityGenerator::GenerateTerrain()
{
	FRandomStream Rand = Stream(TEXT("terrain"));
	const int32 Num = FMath::RoundToInt(TerrainCfg.Size / TerrainCfg.CellSize) + 1;
	Height.Init(Num, TerrainCfg.CellSize, FVector2D(-TerrainCfg.Size * 0.5f));
	for (FVector2D& O : NoiseOffset)
	{
		O = FVector2D(Rand.FRandRange(-500.f, 500.f), Rand.FRandRange(-500.f, 500.f));
	}
	RiverPhase = Rand.FRandRange(0.f, 2.f * PI);
	RiverFreq = 2.f * PI / (TerrainCfg.Size * Rand.FRandRange(0.7f, 1.1f));

	ParallelFor(Num, [&](int32 Y)
	{
		for (int32 X = 0; X < Num; ++X)
		{
			Height.At(X, Y) = RawTerrain(Height.VertexXY(X, Y));
		}
	});
}

float FCityGenerator::RawTerrain(const FVector2D& P) const
{
	const float Warp = TerrainCfg.WarpStrength / TerrainCfg.FeatureSize;
	FVector2D Q = P / TerrainCfg.FeatureSize;
	Q += FVector2D(CityGen::Fbm(Q * 0.5f, 3, NoiseOffset[1]), CityGen::Fbm(Q * 0.5f, 3, NoiseOffset[2])) * Warp;
	const float H = CityGen::Terrain(Q, TerrainCfg.Octaves, TerrainCfg.Ridged, NoiseOffset[0]);
	// Valley: the river runs along X; mountains rise on both sides
	const float Valley = FMath::Pow(CityGen::SmoothStep01(RiverDistance(P) / TerrainCfg.ValleyWidth), 1.5f);
	const float Shape = FMath::Lerp(TerrainCfg.ValleyFloor, 1.f, Valley);
	return (H * Shape + Valley * 0.3f) * TerrainCfg.MountainHeight;
}

void FCityGenerator::GenerateBackdrop()
{
	// Distant ranges: the same landforms continue past the map edge and climb to snowy peaks on the horizon
	const float Half = TerrainCfg.Size * 0.5f;
	const float Outer = TerrainCfg.BackdropSize * 0.5f;
	const int32 Num = FMath::RoundToInt(TerrainCfg.BackdropSize / TerrainCfg.BackdropCell) + 1;
	Backdrop.Init(Num, TerrainCfg.BackdropCell, FVector2D(-Outer));
	ParallelFor(Num, [&](int32 Y)
	{
		for (int32 X = 0; X < Num; ++X)
		{
			const FVector2D P = Backdrop.VertexXY(X, Y);
			const float Out = FMath::Max(FMath::Abs(P.X), FMath::Abs(P.Y)) - Half;
			if (Out <= 0.f)
			{
				Backdrop.At(X, Y) = Height.Sample(P) - 800.f;      // tucked under the playable terrain
				continue;
			}
			// Ranges climb gradually past the border (eased, so the playable square never shows as a wall),
			// except along the river valley, which stays open all the way to the horizon
			const float Valley = CityGen::SmoothStep01((RiverDistance(P) - TerrainCfg.ValleyWidth * 0.3f) / (TerrainCfg.ValleyWidth * 1.5f));
			const float Ramp = FMath::Square(CityGen::SmoothStep01(Out / (Outer - Half)));
			const float Rise = Ramp * TerrainCfg.BackdropRise * Valley;
			const float Peaks = CityGen::Terrain(P / (TerrainCfg.FeatureSize * 3.f), 5, 0.85f, NoiseOffset[2]);
			// Blend from the playable edge height so there is no step at the border
			const float Edge = Height.Sample(P);
			float H = FMath::Lerp(Edge, RawTerrain(P), CityGen::SmoothStep01(Out / (Half * 0.5f))) + Rise * (0.35f + 0.9f * Peaks);
			// River channel continues: bed falls downstream (-X) and climbs upstream past the map
			const float Bed = RiverBedZ(P.X);
			H = FMath::Min(H, Bed + FMath::Max(RiverDistance(P) - TerrainCfg.RiverWidth * 0.5f, 0.f) * 0.25f);
			Backdrop.At(X, Y) = H;
		}
	});
}

float FCityGenerator::RiverBedZ(float X) const
{
	// Inside the map: the carved profile. Beyond it: continue at RiverGradeOutside (downhill toward -X)
	const float Half = TerrainCfg.Size * 0.5f;
	const float Clamped = FMath::Clamp(X, -Half, Half);
	const int32 Col = FMath::Clamp(Height.NearestVertex(FVector2D(Clamped, 0.f)).X, 0, RiverProfile.Num() - 1);
	return RiverProfile[Col] - TerrainCfg.RiverDepth + (X - Clamped) * TerrainCfg.RiverGradeOutside;
}

void FCityGenerator::ExtendRiver()
{
	// Water ribbon from horizon to horizon (inside points already exist; add the outside reaches)
	const float Half = TerrainCfg.Size * 0.5f;
	const float Outer = TerrainCfg.BackdropSize * 0.5f;
	const float Step = TerrainCfg.CellSize * 4.f;
	TArray<FVector> Before, After;
	for (float X = -Outer; X < -Half; X += Step)
	{
		Before.Add(FVector(X, RiverCentreY(X), RiverBedZ(X) + TerrainCfg.RiverDepth * 0.65f));
	}
	for (float X = Half + Step; X <= Outer; X += Step)
	{
		After.Add(FVector(X, RiverCentreY(X), RiverBedZ(X) + TerrainCfg.RiverDepth * 0.65f));
	}
	TArray<float> WB, WA;
	for (const FVector& P : Before) WB.Add(RiverHalfWidth(P.X));
	for (const FVector& P : After) WA.Add(RiverHalfWidth(P.X));
	Layout.River.Insert(Before, 0);
	Layout.River.Append(After);
	Layout.RiverHalfWidths.Insert(WB, 0);
	Layout.RiverHalfWidths.Append(WA);
}

void FCityGenerator::Erode()
{
	// Thermal erosion: material slides off anything steeper than the talus angle (settles scree, kills noise spikes)
	const float Talus = FMath::Tan(FMath::DegreesToRadians(TerrainCfg.TalusAngleDeg)) * Height.Cell;
	const int32 N = Height.Num;
	static const FIntPoint Neighbours[4] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
	for (int32 It = 0; It < TerrainCfg.ErosionIterations; ++It)
	{
		for (int32 Y = 1; Y < N - 1; ++Y)
		{
			for (int32 X = 1; X < N - 1; ++X)
			{
				float& H = Height.At(X, Y);
				int32 Best = -1;
				float BestDiff = Talus;
				for (int32 i = 0; i < 4; ++i)
				{
					const float Diff = H - Height.At(X + Neighbours[i].X, Y + Neighbours[i].Y);
					if (Diff > BestDiff)
					{
						BestDiff = Diff;
						Best = i;
					}
				}
				if (Best >= 0)
				{
					const float Move = (BestDiff - Talus) * 0.5f;
					H -= Move;
					Height.At(X + Neighbours[Best].X, Y + Neighbours[Best].Y) += Move;
				}
			}
		}
	}
}

void FCityGenerator::CarveRiver()
{
	// Bed profile: lowest ground across the channel, forced downhill toward -X so the water never climbs
	const int32 N = Height.Num;
	const float Reach = TerrainCfg.RiverWidth * 1.5f;
	RiverProfile.SetNum(N);
	for (int32 X = 0; X < N; ++X)
	{
		const float PX = Height.VertexXY(X, 0).X;
		const float C = RiverCentreY(PX);
		float Low = MAX_flt;
		for (float DY = -TerrainCfg.RiverWidth; DY <= TerrainCfg.RiverWidth; DY += Height.Cell)
		{
			Low = FMath::Min(Low, Height.Sample(FVector2D(PX, C + DY)));
		}
		RiverProfile[X] = Low;
	}
	for (int32 X = N - 2; X >= 0; --X)
	{
		RiverProfile[X] = FMath::Min(RiverProfile[X], RiverProfile[X + 1]);
	}
	// Channel: deepest mid-stream, shelving up past the water line (~1.5 half-widths) to a gravel bank.
	// Bank distance is perturbed by noise so the shoreline (where terrain crosses the water) is ragged
	for (int32 Y = 0; Y < N; ++Y)
	{
		for (int32 X = 0; X < N; ++X)
		{
			const FVector2D P = Height.VertexXY(X, Y);
			const float HW = RiverHalfWidth(P.X);
			const float Wobble = 1.f + 0.35f * FMath::PerlinNoise2D(P / 1800.f + NoiseOffset[1]) + 0.15f * FMath::PerlinNoise2D(P / 500.f);
			const float D = RiverDistance(P) / FMath::Max(Wobble, 0.3f);
			const float ChannelReach = HW * 2.5f;
			float& H = Height.At(X, Y);
			if (D < ChannelReach)
			{
				const float T = D / ChannelReach;
				const float Bed = RiverProfile[X] - TerrainCfg.RiverDepth * (1.f - T * T);
				H = FMath::Min(H, FMath::Lerp(Bed, H, T * T * T));
			}
			else if (D < Reach)
			{
				// Wide gravel flood plain beyond the channel
				const float T = (D - ChannelReach) / FMath::Max(Reach - ChannelReach, 1.f);
				H = FMath::Min(H, FMath::Lerp(RiverProfile[X] + 60.f, H, CityGen::SmoothStep01(T)));
			}
		}
	}
	for (int32 X = 0; X < N; X += 2)
	{
		const float PX = Height.VertexXY(X, 0).X;
		Layout.River.Add(FVector(PX, RiverCentreY(PX), RiverProfile[X] - TerrainCfg.RiverDepth * 0.35f));
		Layout.RiverHalfWidths.Add(RiverHalfWidth(PX));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Roads

TArray<FVector2D> FCityGenerator::FindPath(const FVector2D& From, const FVector2D& To, float MaxGrade, bool bAvoidRoads) const
{
	// A* over (cell, heading) with 16 headings so the turn cost is exact; grade above MaxGrade is impassable,
	// which is what makes the road zig-zag up a mountainside instead of going straight up it.
	const float C = RoadCfg.PathCellSize;
	const FVector2D O = Height.Origin;
	const int32 N = FMath::FloorToInt((Height.Num - 1) * Height.Cell / C) + 1;
	auto ToCell = [&](const FVector2D& P)
	{
		return FIntPoint(FMath::Clamp(FMath::RoundToInt((P.X - O.X) / C), 2, N - 3), FMath::Clamp(FMath::RoundToInt((P.Y - O.Y) / C), 2, N - 3));
	};
	static const FIntPoint Dirs[16] = { {1, 0}, {2, 1}, {1, 1}, {1, 2}, {0, 1}, {-1, 2}, {-1, 1}, {-2, 1},
		{-1, 0}, {-2, -1}, {-1, -1}, {-1, -2}, {0, -1}, {1, -2}, {1, -1}, {2, -1} };

	TArray<float> H;
	TBitArray<> Wet(false, N * N);
	TArray<float> EdgeCost;
	EdgeCost.SetNumZeroed(N * N);
	H.SetNumUninitialized(N * N);
	for (int32 Y = 0; Y < N; ++Y)
	{
		for (int32 X = 0; X < N; ++X)
		{
			// Plan on smoothed ground: the road is cut and filled through small bumps, so only the large-scale grade matters
			const FVector2D P = O + FVector2D(X, Y) * C;
			float Sum = 0.f;
			for (int32 DY = -2; DY <= 2; ++DY)
			{
				for (int32 DX = -2; DX <= 2; ++DX)
				{
					Sum += Height.Sample(P + FVector2D(DX, DY) * C);
				}
			}
			H[Y * N + X] = Sum / 25.f;
			Wet[Y * N + X] = RiverDistance(P) < TerrainCfg.RiverWidth * 1.2f;
			// Streets keep off existing roads except right at their own start/end junction
			if (bAvoidRoads && FVector2D::Distance(P, From) > RoadCfg.HighwayWidth * 2.f && FVector2D::Distance(P, To) > RoadCfg.HighwayWidth * 2.f
				&& NearRoad(P, RoadCfg.StreetWidth * 2.f))
			{
				EdgeCost[Y * N + X] += RoadCfg.RoadOverlapCost;
			}
			// Keep roads off the map border so the world never ends right beside the road
			const float Edge = FMath::Min(FMath::Min(X, N - 1 - X), FMath::Min(Y, N - 1 - Y)) * C;
			EdgeCost[Y * N + X] += RoadCfg.EdgeCost * FMath::Square(1.f - FMath::Clamp(Edge / (TerrainCfg.Size * 0.12f), 0.f, 1.f));
		}
	}

	const FIntPoint Start = ToCell(From);
	const FIntPoint Goal = ToCell(To);
	const int32 GoalIndex = Goal.Y * N + Goal.X;
	const int32 NumStates = N * N * 16;
	TArray<float> G;
	TArray<int32> Parent;
	G.Init(MAX_flt, NumStates);
	Parent.Init(INDEX_NONE, NumStates);
	TBitArray<> Closed(false, NumStates);

	struct FNode { float F; int32 State; };
	auto Less = [](const FNode& A, const FNode& B) { return A.F < B.F; };
	TArray<FNode> Open;
	for (int32 D = 0; D < 16; ++D)
	{
		const int32 S = (Start.Y * N + Start.X) * 16 + D;
		G[S] = 0.f;
		Open.HeapPush({ 0.f, S }, Less);
	}

	// Weighted A*: a slightly greedy heuristic explores far fewer states for near-identical roads
	constexpr float HeuristicWeight = 1.5f;
	int32 Found = INDEX_NONE;
	int32 Expanded = 0;
	while (Open.Num() > 0)
	{
		FNode Node;
		Open.HeapPop(Node, Less, EAllowShrinking::No);
		if (Closed[Node.State])
		{
			continue;
		}
		Closed[Node.State] = true;
		++Expanded;
		const int32 Cell = Node.State / 16;
		const int32 Dir = Node.State % 16;
		if (Cell == GoalIndex)
		{
			Found = Node.State;
			break;
		}
		const int32 CX = Cell % N;
		const int32 CY = Cell / N;
		for (int32 ND = 0; ND < 16; ++ND)
		{
			const int32 Turn = FMath::Min(FMath::Abs(ND - Dir), 16 - FMath::Abs(ND - Dir));
			if (Turn > 4)
			{
				continue;                                   // > 90 deg in one step
			}
			const int32 NX = CX + Dirs[ND].X;
			const int32 NY = CY + Dirs[ND].Y;
			if (NX < 1 || NY < 1 || NX >= N - 1 || NY >= N - 1)
			{
				continue;
			}
			const int32 NCell = NY * N + NX;
			const float Len = FVector2D(Dirs[ND]).Size() * C;
			const float Grade = FMath::Abs(H[NCell] - H[Cell]) / Len;
			if (Grade > MaxGrade * 3.f)
			{
				continue;
			}
			// Soft limit: going over the target grade gets steeply expensive, so hairpins win wherever they fit
			const float Over = FMath::Max(Grade - MaxGrade, 0.f) / MaxGrade;
			const float Cost = Len * (1.f + RoadCfg.SlopeCost * Grade * Grade + RoadCfg.OverGradeCost * Over * Over + EdgeCost[NCell] + (Wet[NCell] ? RoadCfg.WaterCost : 0.f))
				+ RoadCfg.TurnCost * C * Turn * Turn;
			const int32 NState = NCell * 16 + ND;
			const float NG = G[Node.State] + Cost;
			if (NG < G[NState])
			{
				G[NState] = NG;
				Parent[NState] = Node.State;
				Open.HeapPush({ float(NG + FVector2D::Distance(FVector2D(NX, NY), FVector2D(Goal)) * C * HeuristicWeight), NState }, Less);
			}
		}
	}

	UE_LOG(LogCityGen, Verbose, TEXT("Path %s -> %s grade %.3f: %s after %d states (start h %.0f goal h %.0f)"), *From.ToString(), *To.ToString(),
		MaxGrade, Found != INDEX_NONE ? TEXT("found") : TEXT("FAILED"), Expanded, H[Start.Y * N + Start.X], H[GoalIndex]);
	TArray<FVector2D> Path;
	for (int32 S = Found; S != INDEX_NONE; S = Parent[S])
	{
		const int32 Cell = S / 16;
		Path.Add(O + FVector2D(Cell % N, Cell / N) * C);
	}
	Algo::Reverse(Path);
	return Path;
}

FCityRoad FCityGenerator::MakeRoad(const TArray<FVector2D>& Path, ECityRoadType Type, float Width, float MaxGrade,
	TOptional<float> StartZ, TOptional<float> EndZ) const
{
	FCityRoad Road;
	Road.Type = Type;
	Road.Width = Width;
	if (Path.Num() < 2)
	{
		return Road;
	}

	// Plan: remove grid corners, then relax every bend until the bus can make it
	TArray<FVector2D> P = CityGen::Resample(CityGen::Chaikin(Path, 3), RoadCfg.SampleSpacing);
	for (int32 It = 0; It < 80; ++It)
	{
		bool bChanged = false;
		for (int32 i = 1; i < P.Num() - 1; ++i)
		{
			const FVector2D A = P[i - 1], B = P[i], C = P[i + 1];
			const float Cross = FMath::Abs(FVector2D::CrossProduct(B - A, C - A));
			const float Radius = Cross > KINDA_SMALL_NUMBER ? (B - A).Size() * (C - B).Size() * (C - A).Size() / (2.f * Cross) : MAX_flt;
			if (Radius < RoadCfg.MinTurnRadius)
			{
				P[i] = FMath::Lerp(B, (A + C) * 0.5f, 0.5f);
				bChanged = true;
			}
		}
		if (!bChanged)
		{
			break;
		}
	}
	P = CityGen::Resample(P, RoadCfg.SampleSpacing);

	// Profile: follow the ground, smooth it, keep clear of the river, then clamp the grade both ways
	TArray<float> Z;
	Z.SetNum(P.Num());
	for (int32 i = 0; i < P.Num(); ++i)
	{
		Z[i] = Height.Sample(P[i]);
	}
	for (int32 Pass = 0; Pass < 4; ++Pass)
	{
		TArray<float> S = Z;
		for (int32 i = 1; i < Z.Num() - 1; ++i)
		{
			S[i] = (Z[i - 1] + 2.f * Z[i] + Z[i + 1]) * 0.25f;
		}
		Z = MoveTemp(S);
	}
	for (int32 i = 0; i < P.Num(); ++i)
	{
		if (RiverDistance(P[i]) < TerrainCfg.RiverWidth * 1.5f)
		{
			const int32 Col = FMath::Clamp(Height.NearestVertex(P[i]).X, 0, RiverProfile.Num() - 1);
			Z[i] = FMath::Max(Z[i], RiverProfile[Col] + 350.f);    // embankment over the water (bridge later)
		}
	}
	if (StartZ.IsSet()) Z[0] = StartZ.GetValue();
	if (EndZ.IsSet()) Z.Last() = EndZ.GetValue();
	// Grade-limit both ways separately and average: each is feasible, so their mean is too, and it splits every
	// climb into half cut / half fill. Where that still leaves a deep trench or tall bank, let that stretch ramp
	// up to the steep limit and solve again.
	const float Step = MaxGrade * RoadCfg.SampleSpacing;
	const float SteepStep = FMath::Max(Step, RoadCfg.ShortcutMaxGrade * RoadCfg.SampleSpacing);
	const TArray<float> Ground = Z;
	TArray<float> Limit;
	Limit.Init(Step, Z.Num());
	for (int32 It = 0; It < 4; ++It)
	{
		TArray<float> Fwd = Ground, Bwd = Ground;
		for (int32 i = 1; i < Z.Num(); ++i)
		{
			Fwd[i] = FMath::Clamp(Fwd[i], Fwd[i - 1] - Limit[i], Fwd[i - 1] + Limit[i]);
		}
		for (int32 i = Z.Num() - 2; i >= 0; --i)
		{
			Bwd[i] = FMath::Clamp(Bwd[i], Bwd[i + 1] - Limit[i + 1], Bwd[i + 1] + Limit[i + 1]);
		}
		bool bDeep = false;
		for (int32 i = 0; i < Z.Num(); ++i)
		{
			Z[i] = (Fwd[i] + Bwd[i]) * 0.5f;
			if (FMath::Abs(Z[i] - Ground[i]) > RoadCfg.MaxCutDepth && Limit[i] < SteepStep)
			{
				for (int32 k = FMath::Max(i - 10, 0); k <= FMath::Min(i + 10, Z.Num() - 1); ++k)
				{
					Limit[k] = SteepStep;
				}
				bDeep = true;
			}
		}
		if (!bDeep)
		{
			break;
		}
	}
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (int32 i = 1; i < Z.Num(); ++i)
		{
			Z[i] = FMath::Clamp(Z[i], Z[i - 1] - Limit[i], Z[i - 1] + Limit[i]);
		}
		for (int32 i = Z.Num() - 2; i >= 0; --i)
		{
			Z[i] = FMath::Clamp(Z[i], Z[i + 1] - Limit[i + 1], Z[i + 1] + Limit[i + 1]);
		}
		if (StartZ.IsSet()) Z[0] = StartZ.GetValue();
		if (EndZ.IsSet()) Z.Last() = EndZ.GetValue();
	}
	// Junctions: hold the pinned height across the joined road's width so the surfaces meet flush,
	// then ease into the solved profile
	const float Pin = RoadCfg.HighwayWidth * 0.75f;
	const float Ease = RoadCfg.SampleSpacing * 6.f;
	auto PinEnd = [&](bool bStart, float Target)
	{
		float Dist = 0.f;
		for (int32 k = 0; k < Z.Num(); ++k)
		{
			const int32 i = bStart ? k : Z.Num() - 1 - k;
			if (k > 0) Dist += FVector2D::Distance(P[i], P[bStart ? i - 1 : i + 1]);
			if (Dist > Pin + Ease) break;
			Z[i] = FMath::Lerp(Target, Z[i], CityGen::SmoothStep01((Dist - Pin) / Ease));
		}
	};
	if (StartZ.IsSet()) PinEnd(true, StartZ.GetValue());
	if (EndZ.IsSet()) PinEnd(false, EndZ.GetValue());
	for (int32 i = 0; i < P.Num(); ++i)
	{
		Road.Points.Add(FVector(P[i], Z[i]));
	}
	Road.Walls.Init(0, Road.Points.Num());
	return Road;
}

void FCityGenerator::IndexRoad(int32 RoadIndex)
{
	const TArray<FVector>& Pts = Layout.Roads[RoadIndex].Points;
	for (int32 i = 0; i < Pts.Num(); ++i)
	{
		RoadHash.Add(FIntPoint(FMath::FloorToInt(Pts[i].X / RoadHashCell), FMath::FloorToInt(Pts[i].Y / RoadHashCell)), FIntPoint(RoadIndex, i));
	}
}

TOptional<float> FCityGenerator::NearestRoadZ(const FVector2D& P, float Radius) const
{
	const FIntPoint C(FMath::FloorToInt(P.X / RoadHashCell), FMath::FloorToInt(P.Y / RoadHashCell));
	const int32 R = FMath::CeilToInt(Radius / RoadHashCell);
	TOptional<float> Best;
	float BestD = Radius;
	TArray<FIntPoint> Found;
	for (int32 DY = -R; DY <= R; ++DY)
	{
		for (int32 DX = -R; DX <= R; ++DX)
		{
			Found.Reset();
			RoadHash.MultiFind(C + FIntPoint(DX, DY), Found);
			for (const FIntPoint& RP : Found)
			{
				const FVector& Q = Layout.Roads[RP.X].Points[RP.Y];
				const float D = FVector2D::Distance(P, FVector2D(Q));
				if (D < BestD) { BestD = D; Best = Q.Z; }
			}
		}
	}
	return Best;
}

bool FCityGenerator::NearRoad(const FVector2D& P, float Radius, int32 IgnoreRoad) const
{
	const FIntPoint C(FMath::FloorToInt(P.X / RoadHashCell), FMath::FloorToInt(P.Y / RoadHashCell));
	const int32 R = FMath::CeilToInt(Radius / RoadHashCell);
	TArray<FIntPoint> Found;
	for (int32 DY = -R; DY <= R; ++DY)
	{
		for (int32 DX = -R; DX <= R; ++DX)
		{
			Found.Reset();
			RoadHash.MultiFind(C + FIntPoint(DX, DY), Found);
			for (const FIntPoint& RP : Found)
			{
				if (RP.X != IgnoreRoad && FVector2D::Distance(P, FVector2D(Layout.Roads[RP.X].Points[RP.Y])) < Radius)
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool FCityGenerator::BuildHighway()
{
	// A on the west edge low in the valley, B on the east edge high up: the climb forces hairpins
	FRandomStream Rand = Stream(TEXT("highway"));
	const float Half = TerrainCfg.Size * 0.5f;
	const float Margin = TerrainCfg.Size * RoadCfg.EndInset;
	auto Pick = [&](float X, float LowPct, float HighPct)
	{
		TArray<FVector2D> Edge;
		for (float Y = -Half * 0.8f; Y <= Half * 0.8f; Y += RoadCfg.PathCellSize)
		{
			const FVector2D P(X, Y);
			if (RiverDistance(P) > TerrainCfg.RiverWidth * 3.f)
			{
				Edge.Add(P);
			}
		}
		Edge.Sort([this](const FVector2D& A, const FVector2D& B) { return Height.Sample(A) < Height.Sample(B); });
		const int32 Lo = FMath::FloorToInt(Edge.Num() * LowPct);
		const int32 Hi = FMath::Max(Lo, FMath::FloorToInt(Edge.Num() * HighPct) - 1);
		return Edge.Num() > 0 ? Edge[Rand.RandRange(Lo, Hi)] : FVector2D(X, 0.f);
	};
	const FVector2D A = Pick(-Half + Margin, 0.03f, 0.15f);      // valley floor
	const FVector2D B = Pick(Half - Margin, 0.85f, 0.97f);       // high on the ridge

	TArray<FVector2D> Path;
	for (float Grade = RoadCfg.MaxGrade; Path.Num() < 2 && Grade < RoadCfg.MaxGrade * 2.5f; Grade *= 1.25f)
	{
		Path = FindPath(A, B, Grade);
	}
	if (Path.Num() < 2)
	{
		return false;
	}
	Layout.Roads.Add(MakeRoad(Path, ECityRoadType::Highway, RoadCfg.HighwayWidth, RoadCfg.MaxGrade));
	IndexRoad(0);
	return true;
}

void FCityGenerator::PickTownSite()
{
	// Flattest stretch of highway away from both ends and off the river bank
	const TArray<FVector>& Hw = Layout.Roads[0].Points;
	float BestScore = -MAX_flt;
	for (int32 i = Hw.Num() / 4; i < Hw.Num() * 3 / 4; i += 5)
	{
		const FVector2D C(Hw[i]);
		float Slope = Height.Slope(C);
		for (int32 k = 0; k < 8; ++k)
		{
			Slope += Height.Slope(C + FVector2D(TownCfg.Radius * 0.5f, 0.f).GetRotated(k * 45.f));
		}
		const float Progress = float(i) / Hw.Num();
		const float Score = -Slope / 9.f - FMath::Abs(Progress - 0.5f) * 0.3f - C.Size() / TerrainCfg.Size
			- (RiverDistance(C) < TerrainCfg.RiverWidth * 3.f ? 1.f : 0.f);
		if (Score > BestScore)
		{
			BestScore = Score;
			TownHighwayIndex = i;
		}
	}
	Layout.TownCentre = Hw[TownHighwayIndex];
}

float FCityGenerator::SmoothGround(const FVector2D& P) const
{
	// Large-scale ground height: roads cut and fill through the small bumps
	float Sum = 0.f;
	for (int32 DY = -1; DY <= 1; ++DY)
	{
		for (int32 DX = -1; DX <= 1; ++DX)
		{
			Sum += Height.Sample(P + FVector2D(DX, DY) * RoadCfg.PathCellSize);
		}
	}
	return Sum / 9.f;
}

float FCityGenerator::SmoothSlope(const FVector2D& P) const
{
	const float D = RoadCfg.PathCellSize;
	return FVector2D(SmoothGround(P + FVector2D(D, 0)) - SmoothGround(P - FVector2D(D, 0)), SmoothGround(P + FVector2D(0, D)) - SmoothGround(P - FVector2D(0, D))).Size() / (2.f * D);
}

float FCityGenerator::TownMask(const FVector2D& P) const
{
	const float D = FVector2D::Distance(P, FVector2D(Layout.TownCentre));
	return 1.f - CityGen::SmoothStep01((D - TownCfg.Radius * 0.6f) / (TownCfg.Radius * 0.5f));
}

void FCityGenerator::BuildStreets()
{
	// Hill-town streets: pathfind from the chowk (and from existing streets) to points around town, so lanes
	// switch back between terraces exactly like the highway does; stop where they meet another road to form blocks
	FRandomStream Rand = Stream(TEXT("streets"));
	const FVector2D Centre(Layout.TownCentre);
	auto AddStreet = [&](const FVector2D& From, float FromZ, const FVector2D& To)
	{
		if (!Height.Contains(To, TownCfg.Radius * 0.1f) || RiverDistance(To) < TerrainCfg.RiverWidth * 2.f)
		{
			return;
		}
		TArray<FVector2D> Path = FindPath(From, To, RoadCfg.ShortcutMaxGrade, true);
		if (Path.Num() < 2)
		{
			return;
		}
		Path[0] = From;
		TArray<FVector2D> Kept;
		float Len = 0.f;
		for (int32 i = 0; i < Path.Num(); ++i)
		{
			Len += i > 0 ? FVector2D::Distance(Path[i - 1], Path[i]) : 0.f;
			Kept.Add(Path[i]);
			if (FVector2D::Distance(Path[i], From) > TownCfg.JunctionClearance && NearRoad(Path[i], RoadCfg.StreetWidth * 1.5f))
			{
				break;
			}
		}
		if (Len < TownCfg.MinStreetLength)
		{
			return;
		}
		// Ending on another road: meet it at its height
		const TOptional<float> EndZ = FVector2D::Distance(Kept.Last(), From) > TownCfg.JunctionClearance ? NearestRoadZ(Kept.Last(), RoadCfg.StreetWidth * 2.f) : TOptional<float>();
		FCityRoad Road = MakeRoad(Kept, ECityRoadType::Street, RoadCfg.StreetWidth, RoadCfg.ShortcutMaxGrade, FromZ, EndZ);
		if (Road.Points.Num() >= 4)
		{
			Layout.Roads.Add(MoveTemp(Road));
			IndexRoad(Layout.Roads.Num() - 1);
		}
	};

	const float Spread = 360.f / FMath::Max(TownCfg.PrimaryStreets, 1);
	for (int32 k = 0; k < TownCfg.PrimaryStreets; ++k)
	{
		const FVector2D Offset = FVector2D(TownCfg.Radius * Rand.FRandRange(0.7f, 1.f), 0.f).GetRotated(k * Spread + Rand.FRandRange(-20.f, 20.f));
		AddStreet(Centre, Layout.TownCentre.Z, Centre + Offset);
	}
	for (int32 k = 0; k < TownCfg.SecondaryStreets; ++k)
	{
		const FCityRoad& Parent = Layout.Roads[Rand.RandRange(0, Layout.Roads.Num() - 1)];
		const int32 i = Rand.RandRange(1, Parent.Points.Num() - 2);
		const FVector P = Parent.Points[i];
		const FVector2D To = FVector2D(P) + FVector2D(TownCfg.Radius * Rand.FRandRange(0.35f, 0.6f), 0.f).GetRotated(Rand.FRandRange(0.f, 360.f));
		if (TownMask(FVector2D(P)) > 0.4f && TownMask(To) > 0.2f)
		{
			AddStreet(FVector2D(P), P.Z, To);
		}
	}
}

void FCityGenerator::BuildShortcut()
{
	// Risky route: a steep narrow cut that skips the most winding part of the highway
	const TArray<FVector>& Hw = Layout.Roads[0].Points;
	const int32 Gap = FMath::Max(20, Hw.Num() / 6);
	int32 BestI = INDEX_NONE;
	float BestRatio = 1.f;
	for (int32 i = 0; i + Gap < Hw.Num(); i += 5)
	{
		for (int32 j = i + Gap; j < Hw.Num(); j += 5)
		{
			if (TownMask(FVector2D(Hw[i])) > 0.2f || TownMask(FVector2D(Hw[j])) > 0.2f)
			{
				continue;
			}
			const float Ratio = FVector::Dist(Hw[i], Hw[j]) / CityGen::PolylineLength(Hw, i, j);
			if (Ratio < BestRatio)
			{
				BestRatio = Ratio;
				BestI = i * 100000 + j;
			}
		}
	}
	if (BestI == INDEX_NONE)
	{
		return;
	}
	const int32 I = BestI / 100000;
	const int32 J = BestI % 100000;
	const TArray<FVector2D> Path = FindPath(FVector2D(Hw[I]), FVector2D(Hw[J]), RoadCfg.ShortcutMaxGrade);
	if (Path.Num() < 4)
	{
		return;
	}
	FCityRoad Road = MakeRoad(Path, ECityRoadType::Shortcut, RoadCfg.ShortcutWidth, RoadCfg.ShortcutMaxGrade, Hw[I].Z, Hw[J].Z);
	if (CityGen::PolylineLength(Road.Points, 0, Road.Points.Num() - 1) < CityGen::PolylineLength(Hw, I, J) * RoadCfg.ShortcutMaxLengthRatio)
	{
		Layout.Roads.Add(MoveTemp(Road));
		IndexRoad(Layout.Roads.Num() - 1);
	}
}

void FCityGenerator::CarveRoads()
{
	const int32 N = Height.Num;
	RoadField.EdgeDistance.Init(MAX_flt, N * N);
	RoadField.SurfaceZ.Init(0.f, N * N);
	const float Reach = RoadCfg.ShoulderWidth + FMath::Max(RoadCfg.EmbankmentWidth, RoadCfg.CutWidth);
	// Every segment proposes a carved height; cuts combine by MIN and fills by MAX, so where two roads (or two
	// hairpin legs) are close, one road's embankment can never bury the other
	const TArray<float> Natural = Height.Heights;
	TArray<float> MinCut = Natural, MaxFill = Natural;

	for (int32 RoadIndex = 0; RoadIndex < Layout.Roads.Num(); ++RoadIndex)
	{
		FCityRoad& Road = Layout.Roads[RoadIndex];
		const float HalfW = Road.Width * 0.5f;
		// Parapets where the natural ground falls away beside the road (checked before carving)
		for (int32 i = 0; i < Road.Points.Num(); ++i)
		{
			const FVector T = (Road.Points[FMath::Min(i + 1, Road.Points.Num() - 1)] - Road.Points[FMath::Max(i - 1, 0)]).GetSafeNormal2D();
			const FVector2D Right(-T.Y, T.X);
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const FVector2D Probe = FVector2D(Road.Points[i]) + Right * (Side == 0 ? -1.f : 1.f) * (HalfW + RoadCfg.ShoulderWidth + 400.f);
				// No parapets across junctions: skip where another road is right there
				if (Road.Points[i].Z - Height.Sample(Probe) > RoadCfg.ParapetDrop
					&& !NearRoad(FVector2D(Road.Points[i]), Road.Width * 0.5f + RoadCfg.StreetWidth * 1.5f, RoadIndex))
				{
					Road.Walls[i] |= 1 << Side;
				}
			}
		}
		// Nearest road edge for every vertex in reach
		for (int32 i = 0; i < Road.Points.Num() - 1; ++i)
		{
			const FVector A = Road.Points[i];
			const FVector B = Road.Points[i + 1];
			const FVector2D Lo = FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)) - FVector2D(HalfW + Reach);
			const FVector2D Hi = FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y)) + FVector2D(HalfW + Reach);
			const FIntPoint V0 = Height.NearestVertex(Lo);
			const FIntPoint V1 = Height.NearestVertex(Hi);
			const FVector2D AB = FVector2D(B - A);
			const float LenSq = FMath::Max(AB.SizeSquared(), 1.f);
			for (int32 Y = V0.Y; Y <= V1.Y; ++Y)
			{
				for (int32 X = V0.X; X <= V1.X; ++X)
				{
					const FVector2D P = Height.VertexXY(X, Y);
					const float T = FMath::Clamp(FVector2D::DotProduct(P - FVector2D(A), AB) / LenSq, 0.f, 1.f);
					const float D = FVector2D::Distance(P, FVector2D(A) + AB * T) - HalfW;
					const int32 Idx = Y * N + X;
					const float Z = FMath::Lerp(A.Z, B.Z, T);
					if (D < RoadField.EdgeDistance[Idx])
					{
						RoadField.EdgeDistance[Idx] = D;
						RoadField.SurfaceZ[Idx] = Z;
					}
					if (D < Reach)
					{
						// Ground sits well under the road so the 5 m terrain triangles never poke through it
						const float Target = Z - (D < 0.f ? 40.f : 25.f);
						const bool bCut = Natural[Idx] > Target;
						const float Blend = CityGen::SmoothStep01((D - RoadCfg.ShoulderWidth) / (bCut ? RoadCfg.CutWidth : RoadCfg.EmbankmentWidth));
						const float H = FMath::Lerp(Target, Natural[Idx], Blend);
						if (bCut) MinCut[Idx] = FMath::Min(MinCut[Idx], H);
						else MaxFill[Idx] = FMath::Max(MaxFill[Idx], H);
					}
				}
			}
		}
	}
	// Cut and fill: flat under the road and shoulder; a steep cut face where the hill is above the road,
	// a longer fill slope where the ground falls away
	for (int32 Idx = 0; Idx < N * N; ++Idx)
	{
		const float D = RoadField.EdgeDistance[Idx];
		if (D < RoadCfg.ShoulderWidth)
		{
			Height.Heights[Idx] = RoadField.SurfaceZ[Idx] - (D < 0.f ? 40.f : 25.f);     // under a road: that road wins
		}
		else if (MinCut[Idx] < Natural[Idx])
		{
			Height.Heights[Idx] = MinCut[Idx];                                           // never above any nearby road
		}
		else
		{
			Height.Heights[Idx] = MaxFill[Idx];
		}
	}

	// Guarantee: no terrain vertex within one grid cell of a road's shoulder may rise above that road
	// (otherwise a 5 m triangle spanning road edge -> cut slope slices across the carriageway)
	const float Band = RoadCfg.ShoulderWidth + Height.Cell * 1.5f;
	for (const FCityRoad& Road : Layout.Roads)
	{
		const float HalfW = Road.Width * 0.5f;
		for (int32 i = 0; i < Road.Points.Num() - 1; ++i)
		{
			const FVector A = Road.Points[i];
			const FVector B = Road.Points[i + 1];
			const FIntPoint V0 = Height.NearestVertex(FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)) - FVector2D(HalfW + Band));
			const FIntPoint V1 = Height.NearestVertex(FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y)) + FVector2D(HalfW + Band));
			const FVector2D AB = FVector2D(B - A);
			const float LenSq = FMath::Max(AB.SizeSquared(), 1.f);
			for (int32 Y = V0.Y; Y <= V1.Y; ++Y)
			{
				for (int32 X = V0.X; X <= V1.X; ++X)
				{
					const FVector2D P = Height.VertexXY(X, Y);
					const float T = FMath::Clamp(FVector2D::DotProduct(P - FVector2D(A), AB) / LenSq, 0.f, 1.f);
					const float D = FVector2D::Distance(P, FVector2D(A) + AB * T) - HalfW;
					if (D < Band)
					{
						float& H = Height.Heights[Y * N + X];
						H = FMath::Min(H, FMath::Lerp(A.Z, B.Z, T) - (D < 0.f ? 40.f : 25.f));
					}
				}
			}
		}
	}

	// Bank relaxation: next to roads, no vertex may stand more than MaxBankSlope above (cut) or below (fill)
	// its neighbours. Removes the grid-aliased teeth on steep cut faces and smooths embankments into slopes
	const float MaxRise = Height.Cell * RoadCfg.MaxBankSlope;
	const float Zone = Reach + Height.Cell * 4.f;
	for (int32 It = 0; It < 12; ++It)
	{
		for (int32 Y = 1; Y < N - 1; ++Y)
		{
			for (int32 X = 1; X < N - 1; ++X)
			{
				const int32 Idx = Y * N + X;
				const float D = RoadField.EdgeDistance[Idx];
				if (D < Band || D > Zone)
				{
					continue;
				}
				const float L = Height.Heights[Idx - 1], R = Height.Heights[Idx + 1], Dn = Height.Heights[Idx - N], Up = Height.Heights[Idx + N];
				float& H = Height.Heights[Idx];
				H = FMath::Clamp(H, FMath::Max(FMath::Max(L, R), FMath::Max(Dn, Up)) - MaxRise, FMath::Min(FMath::Min(L, R), FMath::Min(Dn, Up)) + MaxRise);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Plots, landmarks, stops, trees

bool FCityGenerator::FootprintClear(const FVector2D& Centre, float Yaw, const FVector2D& Size) const
{
	// 5x5 samples over the rotated footprint must all be off every road (+ shoulder margin)
	const FVector2D Fwd = FVector2D(1.f, 0.f).GetRotated(Yaw);
	const FVector2D Side(-Fwd.Y, Fwd.X);
	for (int32 a = -2; a <= 2; ++a)
	{
		for (int32 b = -2; b <= 2; ++b)
		{
			const FVector2D Q = Centre + Fwd * (Size.X * 0.25f * a) + Side * (Size.Y * 0.25f * b);
			if (!Height.Contains(Q))
			{
				return false;
			}
			const FIntPoint V = Height.NearestVertex(Q);
			if (RoadField.EdgeDistance[V.Y * Height.Num + V.X] < RoadCfg.ShoulderWidth + 150.f)
			{
				return false;
			}
		}
	}
	return true;
}

void FCityGenerator::MakePlots()
{
	TMultiMap<FIntPoint, FVector2D> Taken;
	const float HashCell = TownCfg.PlotSpacing;
	auto Free = [&](const FVector2D& P, float MinDist)
	{
		const FIntPoint C(FMath::FloorToInt(P.X / HashCell), FMath::FloorToInt(P.Y / HashCell));
		TArray<FVector2D> Found;
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				Found.Reset();
				Taken.MultiFind(C + FIntPoint(DX, DY), Found);
				for (const FVector2D& Q : Found)
				{
					if (FVector2D::Distance(P, Q) < MinDist) return false;
				}
			}
		}
		return true;
	};
	const FVector2D Centre(Layout.TownCentre);

	for (const FCityRoad& Road : Layout.Roads)
	{
		if (Road.Type == ECityRoadType::Shortcut)
		{
			continue;
		}
		const bool bHighway = Road.Type == ECityRoadType::Highway;
		for (int32 i = 1; i < Road.Points.Num() - 1; ++i)
		{
			const FVector2D P(Road.Points[i]);
			const float DistToTown = FVector2D::Distance(P, Centre);
			const bool bInTown = DistToTown < TownCfg.Radius * 1.1f;
			const float Interval = bInTown ? TownCfg.PlotSpacing : TownCfg.HighwayPlotInterval;
			if (!bInTown && !bHighway)
			{
				continue;
			}
			if (FMath::Fmod(i * RoadCfg.SampleSpacing, Interval) >= RoadCfg.SampleSpacing)
			{
				continue;
			}
			const FVector2D T = FVector2D(Road.Points[i + 1] - Road.Points[i - 1]).GetSafeNormal();
			const FVector2D Right(-T.Y, T.X);
			for (float Side : { -1.f, 1.f })
			{
				const FVector2D Out = Right * Side;
				const FVector2D C = P + Out * (Road.Width * 0.5f + RoadCfg.ShoulderWidth + 250.f + TownCfg.PlotDepth * 0.5f);
				const FIntPoint V = Height.NearestVertex(C);
				if (!Height.Contains(C, TownCfg.PlotDepth) || SmoothSlope(C) > TownCfg.MaxPlotSlope
					|| RoadField.EdgeDistance[V.Y * Height.Num + V.X] < TownCfg.PlotDepth * 0.5f
					|| RiverDistance(C) < TerrainCfg.RiverWidth * 2.f || !Free(C, TownCfg.PlotSpacing * 0.85f)
					|| !FootprintClear(C, FMath::RadiansToDegrees(FMath::Atan2(-Out.Y, -Out.X)), FVector2D(TownCfg.PlotDepth, TownCfg.PlotSpacing * 0.85f)))
				{
					continue;
				}
				FCityPlot Plot;
				Plot.Location = FVector(C, Height.Sample(C));
				Plot.Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Out.Y, -Out.X));   // front (+X) faces the road
				Plot.Size = FVector2D(TownCfg.PlotDepth, TownCfg.PlotSpacing * 0.85f);
				Plot.Centrality = FMath::Clamp(1.f - DistToTown / TownCfg.Radius, 0.f, 1.f);
				Plot.RoadPoint = Road.Points[i];
				Plot.bOnHighway = bHighway;
				Candidates.Add(Plot);
				Taken.Add(FIntPoint(FMath::FloorToInt(C.X / HashCell), FMath::FloorToInt(C.Y / HashCell)), C);
			}
		}
	}
}

void FCityGenerator::AssignLandmarks()
{
	FRandomStream Rand = Stream(TEXT("landmarks"));
	const FVector2D Centre(Layout.TownCentre);
	float MinZ = MAX_flt, MaxZ = -MAX_flt;
	for (const FCityPlot& P : Candidates)
	{
		if (P.Centrality > 0.f)
		{
			MinZ = FMath::Min(MinZ, P.Location.Z);
			MaxZ = FMath::Max(MaxZ, P.Location.Z);
		}
	}
	const float ZRange = FMath::Max(MaxZ - MinZ, 100.f);

	for (const FCityLandmarkRule& Rule : Rules)
	{
		for (int32 n = 0; n < Rule.Count; ++n)
		{
			int32 Best = INDEX_NONE;
			float BestScore = -MAX_flt;
			for (int32 i = 0; i < Candidates.Num(); ++i)
			{
				const FCityPlot& P = Candidates[i];
				const bool bOutside = FVector2D::Distance(FVector2D(P.Location), Centre) > TownCfg.Radius;
				if (P.Use != ECityLandmark::House || (Rule.bRequireHighway && !P.bOnHighway) || Rule.bOutsideTown != bOutside)
				{
					continue;
				}
				bool bTooClose = false;
				for (const FCityPlot& Other : Candidates)
				{
					if (Other.Use == Rule.Type && FVector::Dist2D(Other.Location, P.Location) < Rule.MinSpacing)
					{
						bTooClose = true;
						break;
					}
				}
				const FVector2D Back = FVector2D(1.f, 0.f).GetRotated(P.Yaw + 180.f);
				if (bTooClose || !FootprintClear(FVector2D(P.Location) + Back * (Rule.PlotSize.X - P.Size.X) * 0.5f, P.Yaw, Rule.PlotSize))
				{
					continue;
				}
				const float Score = -Rule.CentralityWeight * FMath::Abs(P.Centrality - Rule.TargetCentrality)
					+ Rule.HeightWeight * (P.Location.Z - MinZ) / ZRange
					- Rule.FlatnessWeight * SmoothSlope(FVector2D(P.Location))
					+ Rand.FRandRange(0.f, 0.05f);
				if (Score > BestScore)
				{
					BestScore = Score;
					Best = i;
				}
			}
			if (Best == INDEX_NONE)
			{
				continue;
			}
			FCityPlot& Plot = Candidates[Best];
			Plot.Use = Rule.Type;
			// Grow the plot away from the road and clear the houses it now covers
			const FVector2D Back = FVector2D(1.f, 0.f).GetRotated(Plot.Yaw + 180.f);
			Plot.Location += FVector(Back * (Rule.PlotSize.X - Plot.Size.X) * 0.5f, 0.f);
			Plot.Location.Z = Height.Sample(FVector2D(Plot.Location));
			Plot.Size = Rule.PlotSize;
			const float Clear = Rule.PlotSize.GetMax() * 0.5f + TownCfg.PlotSpacing * 0.4f;
			for (FCityPlot& Other : Candidates)
			{
				if (&Other != &Plot && Other.Use == ECityLandmark::House && FVector::Dist2D(Other.Location, Plot.Location) < Clear)
				{
					Other.Centrality = -1.f;                      // marked for removal
				}
			}
		}
	}

	// Keep town houses and a few roadside huts
	for (const FCityPlot& P : Candidates)
	{
		if (P.Use != ECityLandmark::House || P.Centrality > 0.f || (P.Centrality == 0.f && Rand.FRand() < 0.3f))
		{
			Layout.Plots.Add(P);
		}
	}
}

void FCityGenerator::PlaceStops()
{
	auto StopAt = [&](const FVector& RoadPoint, const FVector2D& Tangent, const FVector2D& Kerb, float Width, const FText& Name)
	{
		// Bus pulls up in the kerb-side lane with its door (+Y) facing the kerb
		FVector2D Fwd = Tangent;
		if (FVector2D::DotProduct(FVector2D(-Fwd.Y, Fwd.X), Kerb) < 0.f)
		{
			Fwd = -Fwd;
		}
		FCityStop Stop;
		Stop.Transform = FTransform(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Fwd.Y, Fwd.X)), 0.f), RoadPoint + FVector(Kerb * Width * 0.25f, 0.f));
		Stop.Name = Name;
		Layout.Stops.Add(Stop);
	};

	const FCityRoad& Hw = Layout.Roads[0];
	auto HighwayFrame = [&](int32 i)
	{
		const FVector T = (Hw.Points[FMath::Min(i + 1, Hw.Points.Num() - 1)] - Hw.Points[FMath::Max(i - 1, 0)]).GetSafeNormal2D();
		return FTransform(T.Rotation(), Hw.Points[i] + FVector(0.f, 0.f, 50.f));
	};
	const int32 Lead = FMath::Min(5, Hw.Points.Num() / 4);
	Layout.Start = HighwayFrame(Lead);
	Layout.Finish = HighwayFrame(Hw.Points.Num() - 1 - Lead);
	const FVector2D StartT(Layout.Start.GetRotation().GetForwardVector());
	const FVector2D EndT(Layout.Finish.GetRotation().GetForwardVector());
	StopAt(Hw.Points[Lead + 3], StartT, FVector2D(-StartT.Y, StartT.X), Hw.Width, StartName);
	StopAt(Hw.Points[Hw.Points.Num() - 1 - Lead], EndT, FVector2D(-EndT.Y, EndT.X), Hw.Width, FinishName);

	for (const FCityPlot& Plot : Layout.Plots)
	{
		const FCityLandmarkRule* Rule = Rules.FindByPredicate([&Plot](const FCityLandmarkRule& R) { return R.Type == Plot.Use; });
		if (Plot.Use == ECityLandmark::House || !Rule || !Rule->bHasBusStop)
		{
			continue;
		}
		const FVector2D Kerb = FVector2D(Plot.Location - Plot.RoadPoint).GetSafeNormal();
		const FVector2D Tangent(-Kerb.Y, Kerb.X);
		StopAt(Plot.RoadPoint, Tangent, Kerb, Plot.bOnHighway ? RoadCfg.HighwayWidth : RoadCfg.StreetWidth, Rule->StopName);
	}
}

void FCityGenerator::ScatterTrees()
{
	// Bridson Poisson-disk sampling: natural, evenly spaced forest without clumps or grid patterns
	FRandomStream Rand = Stream(TEXT("trees"));
	const float R = TownCfg.TreeSpacing;
	const float CellSize = R / UE_SQRT_2;
	const float Size = TerrainCfg.Size;
	const int32 G = FMath::CeilToInt(Size / CellSize);
	TArray<int32> Grid;
	Grid.Init(INDEX_NONE, G * G);
	TArray<FVector2D> Samples;
	TArray<int32> Active;
	auto GridOf = [&](const FVector2D& P) { return FIntPoint(FMath::Clamp(int32((P.X - Height.Origin.X) / CellSize), 0, G - 1), FMath::Clamp(int32((P.Y - Height.Origin.Y) / CellSize), 0, G - 1)); };
	auto Add = [&](const FVector2D& P) { const FIntPoint C = GridOf(P); Grid[C.Y * G + C.X] = Samples.Add(P); Active.Add(Samples.Num() - 1); };
	Add(Height.Origin + FVector2D(Rand.FRand(), Rand.FRand()) * Size);
	while (Active.Num() > 0)
	{
		const int32 AI = Rand.RandRange(0, Active.Num() - 1);
		const FVector2D Base = Samples[Active[AI]];
		bool bPlaced = false;
		for (int32 k = 0; k < 12 && !bPlaced; ++k)
		{
			const FVector2D P = Base + FVector2D(Rand.FRandRange(R, 2.f * R), 0.f).GetRotated(Rand.FRandRange(0.f, 360.f));
			if (!Height.Contains(P, 100.f))
			{
				continue;
			}
			const FIntPoint C = GridOf(P);
			bool bOk = true;
			for (int32 DY = -2; DY <= 2 && bOk; ++DY)
			{
				for (int32 DX = -2; DX <= 2 && bOk; ++DX)
				{
					const int32 X = C.X + DX, Y = C.Y + DY;
					if (X >= 0 && Y >= 0 && X < G && Y < G && Grid[Y * G + X] != INDEX_NONE && FVector2D::Distance(Samples[Grid[Y * G + X]], P) < R)
					{
						bOk = false;
					}
				}
			}
			if (bOk)
			{
				Add(P);
				bPlaced = true;
			}
		}
		if (!bPlaced)
		{
			Active.RemoveAtSwap(AI);
		}
	}

	float MaxZ = -MAX_flt;
	for (float H : Height.Heights) MaxZ = FMath::Max(MaxZ, H);
	const float TreeLine = MaxZ * 0.88f;
	for (const FVector2D& P : Samples)
	{
		const FIntPoint V = Height.NearestVertex(P);
		const float Z = Height.Sample(P);
		if (Height.Slope(P) > TownCfg.TreeMaxSlope || RoadField.EdgeDistance[V.Y * Height.Num + V.X] < 900.f
			|| RiverDistance(P) < TerrainCfg.RiverWidth * 1.3f || Z > TreeLine || Rand.FRand() < TownMask(P) * 0.9f)
		{
			continue;
		}
		bool bOnPlot = false;
		for (const FCityPlot& Plot : Layout.Plots)
		{
			if (FVector2D::Distance(FVector2D(Plot.Location), P) < Plot.Size.GetMax() * 0.75f)
			{
				bOnPlot = true;
				break;
			}
		}
		if (!bOnPlot)
		{
			const float S = Rand.FRandRange(TownCfg.TreeScale.X, TownCfg.TreeScale.Y);
			Layout.Trees.Add(FTransform(FRotator(0.f, Rand.FRandRange(0.f, 360.f), 0.f), FVector(P, Z - 20.f), FVector(S)));
		}
	}
}
