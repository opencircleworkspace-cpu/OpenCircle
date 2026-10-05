// Copyright OpenCircle. All Rights Reserved.

#include "World/CityActor.h"
#include "World/CityDataAsset.h"
#include "World/CityGenerator.h"
#include "Gameplay/BusStop.h"
#include "Vehicles/BusVehicle.h"
#include "GameFramework/GameModeBase.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCity, Log, All);

ACityActor::ACityActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	CubeMesh = Cube.Object;
	ConeMesh = Cone.Object;
}

void ACityActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACityActor, Seed);
}

void ACityActor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		int32 NewSeed = bRandomSeedEachPlay ? FMath::RandRange(1, MAX_int32 - 1) : Seed;
		if (const AGameModeBase* GameMode = GetWorld()->GetAuthGameMode())
		{
			NewSeed = UGameplayStatics::GetIntOption(GameMode->OptionsString, TEXT("CitySeed"), NewSeed);
		}
		Seed = NewSeed;
	}
	Build();
}

void ACityActor::OnRep_Seed()
{
	if (HasActorBegunPlay())
	{
		Build();
	}
}

void ACityActor::SetSeed(int32 NewSeed)
{
	if (HasAuthority())
	{
		Seed = NewSeed;
		Build();
	}
}

void ACityActor::GeneratePreview()
{
	BuiltSeed = INDEX_NONE;
	Build();
}

FTransform ACityActor::GetStartTransform() const
{
	return Layout.Start * GetActorTransform();
}

FTransform ACityActor::GetFinishTransform() const
{
	return Layout.Finish * GetActorTransform();
}

void ACityActor::ClearCity()
{
	for (UActorComponent* Component : Generated)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	for (AActor* Actor : SpawnedActors)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	Generated.Reset();
	SpawnedActors.Reset();
	Layout = FCityLayout();
	BuiltSeed = INDEX_NONE;
}

void ACityActor::Build()
{
	if (!City)
	{
		UE_LOG(LogCity, Warning, TEXT("%s: no City data asset"), *GetName());
		return;
	}
	const int32 Wanted = Seed != 0 ? Seed : City->DefaultSeed;
	if (BuiltSeed == Wanted)
	{
		return;
	}
	ClearCity();

	// Deterministic retry: every machine skips the same unusable seeds
	TUniquePtr<FCityGenerator> Gen;
	for (int32 Try = 0; Try < 5 && !Gen; ++Try)
	{
		Gen = MakeUnique<FCityGenerator>(*City, Wanted + Try);
		if (!Gen->Run())
		{
			Gen.Reset();
		}
	}
	if (!Gen)
	{
		UE_LOG(LogCity, Error, TEXT("%s: could not generate a city near seed %d"), *GetName(), Wanted);
		return;
	}
	Layout = Gen->GetLayout();
	double T = FPlatformTime::Seconds();
	FString Timing;
	auto Stage = [&](const TCHAR* Name) { const double Now = FPlatformTime::Seconds(); Timing += FString::Printf(TEXT(" %s=%.0fms"), Name, (Now - T) * 1000.0); T = Now; };
	BuildTerrain(*Gen); Stage(TEXT("terrain"));
	BuildBackdrop(*Gen); Stage(TEXT("backdrop"));
	BuildRoads(*Gen); Stage(TEXT("roads"));
	BuildWater(); Stage(TEXT("water"));
	BuildPlots(*Gen); Stage(TEXT("plots"));
	BuildTrees(); Stage(TEXT("trees"));
	UE_LOG(LogCity, Display, TEXT("%s: meshes built:%s"), *GetName(), *Timing);
	if (bSpawnBusStops)
	{
		SpawnStops();
	}
	BuiltSeed = Wanted;

	if (GetWorld()->IsGameWorld())
	{
		if (bMovePawnsToStart)
		{
			MovePawnsToStart();
		}
		OnCityBuilt.Broadcast();
	}
}

// ---------------------------------------------------------------------------------------------------------------------

UProceduralMeshComponent* ACityActor::NewMesh(const TCHAR* Name, bool bCollision)
{
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, MakeUniqueObjectName(this, UProceduralMeshComponent::StaticClass(), Name), RF_Transient);
	Mesh->bUseAsyncCooking = true;
	Mesh->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	Mesh->SetupAttachment(RootComponent);
	Mesh->RegisterComponent();
	Generated.Add(Mesh);
	return Mesh;
}

UHierarchicalInstancedStaticMeshComponent* ACityActor::NewInstances(UStaticMesh* Mesh, UMaterialInterface* Material, float CullDistance, bool bCollision)
{
	UHierarchicalInstancedStaticMeshComponent* Instances = NewObject<UHierarchicalInstancedStaticMeshComponent>(this,
		MakeUniqueObjectName(this, UHierarchicalInstancedStaticMeshComponent::StaticClass(), TEXT("Instances")), RF_Transient);
	Instances->SetStaticMesh(Mesh);
	if (Material)
	{
		Instances->SetMaterial(0, Material);
	}
	Instances->SetCullDistances(CullDistance * 0.8f, CullDistance);
	Instances->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	Instances->SetupAttachment(RootComponent);
	Instances->RegisterComponent();
	Generated.Add(Instances);
	return Instances;
}

FTransform ACityActor::FitToBox(const UStaticMesh* Mesh, const FVector& Size, const FTransform& Base)
{
	const FBox Bounds = Mesh ? Mesh->GetBoundingBox() : FBox(FVector(-50.f), FVector(50.f));
	const FVector Extent = Bounds.GetSize().ComponentMax(FVector(1.f));
	const FVector Scale = Size / Extent;
	const FVector Pivot(-Bounds.GetCenter().X * Scale.X, -Bounds.GetCenter().Y * Scale.Y, -Bounds.Min.Z * Scale.Z);
	return FTransform(Base.GetRotation(), Base.GetLocation() + Base.GetRotation().RotateVector(Pivot), Scale);
}

void ACityActor::BuildTerrain(const FCityGenerator& Gen)
{
	const FCityHeightfield& H = Gen.GetHeightfield();
	const FCityRoadField& Roads = Gen.GetRoadField();
	const float RoadFade = FMath::Max(City->Roads.EmbankmentWidth, 1.f);
	const float SandReach = City->Terrain.RiverWidth * 1.5f;
	const int32 Chunk = City->Terrain.ChunkCells;
	// Render: one component, many sections, no collision (proc mesh recooks ALL sections per collision section: O(n^2))
	UProceduralMeshComponent* Mesh = NewMesh(TEXT("Terrain"), false);

	int32 Section = 0;
	for (int32 CY = 0; CY < H.Num - 1; CY += Chunk)
	{
		for (int32 CX = 0; CX < H.Num - 1; CX += Chunk)
		{
			const int32 X1 = FMath::Min(CX + Chunk, H.Num - 1);
			const int32 Y1 = FMath::Min(CY + Chunk, H.Num - 1);
			const int32 W = X1 - CX + 1;
			TArray<FVector> Verts;
			TArray<int32> Tris;
			TArray<FVector> Normals;
			TArray<FVector2D> UVs;
			TArray<FLinearColor> Colors;
			TArray<FProcMeshTangent> Tangents;
			for (int32 Y = CY; Y <= Y1; ++Y)
			{
				for (int32 X = CX; X <= X1; ++X)
				{
					const FVector2D P = H.VertexXY(X, Y);
					const FVector N = H.Normal(X, Y);
					const int32 Idx = Y * H.Num + X;
					Verts.Add(FVector(P, H.At(X, Y)));
					Normals.Add(N);
					UVs.Add(P / 1000.f);
					Tangents.Add(FProcMeshTangent((FVector::ForwardVector - N * N.X).GetSafeNormal(), false));
					Colors.Add(FLinearColor(
						FMath::Clamp((1.f - N.Z) * 2.5f, 0.f, 1.f),
						1.f - FMath::Clamp(Roads.EdgeDistance[Idx] / RoadFade, 0.f, 1.f),
						Gen.TownMask(P),
						1.f - FMath::Clamp(Gen.RiverDistance(P) / SandReach, 0.f, 1.f)));
				}
			}
			for (int32 Y = 0; Y < Y1 - CY; ++Y)
			{
				for (int32 X = 0; X < W - 1; ++X)
				{
					const int32 I = Y * W + X;
					Tris.Append({ I, I + W, I + 1, I + 1, I + W, I + W + 1 });
				}
			}
			Mesh->CreateMeshSection_LinearColor(Section, Verts, Tris, Normals, UVs, Colors, Tangents, false);

			// Collision: own component per chunk at half resolution, cooked asynchronously; roads carry exact collision
			{
				const int32 Step = FMath::Max(City->Terrain.CollisionStep, 1);
				TArray<FVector> CV;
				TArray<int32> CT;
				int32 CW = 0;
				for (int32 Y = CY; Y <= Y1; Y = (Y == Y1 ? Y1 + 1 : FMath::Min(Y + Step, Y1)))
				{
					CW = 0;
					for (int32 X = CX; X <= X1; X = (X == X1 ? X1 + 1 : FMath::Min(X + Step, X1)))
					{
						CV.Add(FVector(H.VertexXY(X, Y), H.At(X, Y)));
						++CW;
					}
				}
				const int32 CH = CV.Num() / CW;
				for (int32 Y = 0; Y < CH - 1; ++Y)
				{
					for (int32 X = 0; X < CW - 1; ++X)
					{
						const int32 I = Y * CW + X;
						CT.Append({ I, I + CW, I + 1, I + 1, I + CW, I + CW + 1 });
					}
				}
				UProceduralMeshComponent* Col = NewMesh(TEXT("TerrainCollision"), true);
				Col->SetVisibility(false);
				Col->SetCastShadow(false);
				Col->CreateMeshSection(0, CV, CT, {}, {}, {}, {}, true);
			}
			if (City->Art.TerrainMaterial)
			{
				Mesh->SetMaterial(Section, City->Art.TerrainMaterial);
			}
			++Section;
		}
	}
}

void ACityActor::BuildBackdrop(const FCityGenerator& Gen)
{
	// Render-only distant ranges; quads wholly inside the playable square are skipped
	const FCityHeightfield& B = Gen.GetBackdrop();
	if (B.Num < 2)
	{
		return;
	}
	const float Inner = City->Terrain.Size * 0.5f - B.Cell;
	UProceduralMeshComponent* Mesh = NewMesh(TEXT("Backdrop"), false);
	Mesh->SetCastShadow(false);
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	for (int32 Y = 0; Y < B.Num; ++Y)
	{
		for (int32 X = 0; X < B.Num; ++X)
		{
			const FVector2D P = B.VertexXY(X, Y);
			const FVector N = B.Normal(X, Y);
			Verts.Add(FVector(P, B.At(X, Y)));
			Normals.Add(N);
			UVs.Add(P / 1000.f);
			Colors.Add(FLinearColor(FMath::Clamp((1.f - N.Z) * 2.5f, 0.f, 1.f), 0.f, 0.f, 0.f));
		}
	}
	for (int32 Y = 0; Y < B.Num - 1; ++Y)
	{
		for (int32 X = 0; X < B.Num - 1; ++X)
		{
			const FVector2D C = B.VertexXY(X, Y) + FVector2D(B.Cell * 0.5f);
			if (FMath::Abs(C.X) < Inner && FMath::Abs(C.Y) < Inner)
			{
				continue;
			}
			const int32 I = Y * B.Num + X;
			Tris.Append({ I, I + B.Num, I + 1, I + 1, I + B.Num, I + B.Num + 1 });
		}
	}
	Mesh->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UVs, Colors, {}, false);
	if (City->Art.TerrainMaterial)
	{
		Mesh->SetMaterial(0, City->Art.TerrainMaterial);
	}

	// Hero peaks (Fab "Mountain Tops" etc.) on the horizon ring
	TArray<UStaticMesh*> Peaks;
	for (UStaticMesh* M : City->Art.HorizonPeaks)
	{
		if (M) Peaks.Add(M);
	}
	if (Peaks.Num() == 0)
	{
		return;
	}
	FRandomStream Rand(int32(HashCombine(GetTypeHash(Layout.Seed), GetTypeHash(FString(TEXT("peaks"))))));
	const float Half = City->Terrain.Size * 0.5f;
	const float Ring = FMath::Lerp(Half, City->Terrain.BackdropSize * 0.5f, City->Art.HorizonPeakRing);
	for (int32 i = 0; i < City->Art.HorizonPeakCount; ++i)
	{
		const float Angle = (i + Rand.FRandRange(-0.3f, 0.3f)) * 360.f / City->Art.HorizonPeakCount;
		const FVector2D P = FVector2D(Ring * Rand.FRandRange(0.8f, 1.2f), 0.f).GetRotated(Angle);
		UStaticMesh* M = Peaks[Rand.RandRange(0, Peaks.Num() - 1)];
		UStaticMeshComponent* Peak = NewObject<UStaticMeshComponent>(this, MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), TEXT("Peak")), RF_Transient);
		Peak->SetStaticMesh(M);
		Peak->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Peak->SetCastShadow(false);
		Peak->SetupAttachment(RootComponent);
		// Scale to the wanted height, but never wider than the backdrop band so peaks stay off the playable map
		const FVector Size = M->GetBoundingBox().GetSize().ComponentMax(FVector(1.f));
		const float MaxWidth = (City->Terrain.BackdropSize - City->Terrain.Size) * 0.5f * 0.7f;
		const float Scale = FMath::Min(Rand.FRandRange(City->Art.HorizonPeakHeight.X, City->Art.HorizonPeakHeight.Y) / Size.Z,
			MaxWidth / FMath::Max(Size.X, Size.Y));
		// Open-bottomed shells: seat on the LOWEST ground under the footprint and sink, so no edge ever floats
		const float Radius = FMath::Max(Size.X, Size.Y) * Scale * 0.5f;
		float Ground = MAX_flt;
		for (int32 k = 0; k <= 24; ++k)
		{
			const FVector2D Q = k == 0 ? P : P + FVector2D(Radius * (k <= 8 ? 0.5f : k <= 16 ? 0.8f : 1.f), 0.f).GetRotated(k * 45.f);
			Ground = FMath::Min(Ground, B.Sample(Q));
		}
		const FVector Base(P, Ground - City->Art.HorizonPeakSink * Size.Z * Scale);
		Peak->SetRelativeTransform(FTransform(FRotator(0.f, Rand.FRandRange(0.f, 360.f), 0.f), Base - FVector(0, 0, M->GetBoundingBox().Min.Z * Scale), FVector(Scale)));
		Peak->RegisterComponent();
		Generated.Add(Peak);
	}
}

void ACityActor::BuildRoads(const FCityGenerator& Gen)
{
	const FCityRoadSettings& Cfg = Gen.GetRoadSettings();
	UStaticMesh* BlockMesh = City->Art.ParapetMesh ? City->Art.ParapetMesh.Get() : CubeMesh.Get();
	UHierarchicalInstancedStaticMeshComponent* Blocks = NewInstances(BlockMesh, City->Art.ParapetMaterial, City->Art.HouseCullDistance, true);

	for (int32 R = 0; R < Layout.Roads.Num(); ++R)
	{
		const FCityRoad& Road = Layout.Roads[R];
		const TArray<FVector>& Pts = Road.Points;
		if (Pts.Num() < 2)
		{
			continue;
		}
		// Junction overlaps: highway on top, then shortcut, then streets
		const float Lift = Road.Type == ECityRoadType::Highway ? 4.f : Road.Type == ECityRoadType::Shortcut ? 3.f : 2.f;
		const float HalfW = Road.Width * 0.5f;
		const float Sh = Cfg.ShoulderWidth;
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		float Along = 0.f;
		float NextBlock = 0.f;
		for (int32 i = 0; i < Pts.Num(); ++i)
		{
			if (i > 0)
			{
				Along += FVector::Dist(Pts[i - 1], Pts[i]);
			}
			const FVector T = (Pts[FMath::Min(i + 1, Pts.Num() - 1)] - Pts[FMath::Max(i - 1, 0)]).GetSafeNormal2D();
			const FVector Right(-T.Y, T.X, 0.f);
			const FVector C = Pts[i] + FVector(0.f, 0.f, Lift);
			const float V = Along / Road.Width;
			// Shoulder slopes down 12 cm, then a skirt drops 1 m into the ground: no gaps on any slope
			const FVector Down(0.f, 0.f, 100.f);
			const FVector L = C - Right * (HalfW + Sh) - FVector(0, 0, 12.f);
			const FVector R2 = C + Right * (HalfW + Sh) - FVector(0, 0, 12.f);
			Verts.Append({ L - Right * 30.f - Down, L, C - Right * HalfW, C + Right * HalfW, R2, R2 + Right * 30.f - Down });
			const float US = Sh / Road.Width;
			UVs.Append({ FVector2D(-US - 0.05f, V), FVector2D(-US, V), FVector2D(0.f, V), FVector2D(1.f, V), FVector2D(1.f + US, V), FVector2D(1.f + US + 0.05f, V) });
			Normals.Append({ -Right, FVector::UpVector, FVector::UpVector, FVector::UpVector, FVector::UpVector, Right });
			if (i > 0)
			{
				const int32 B = (i - 1) * 6;
				for (int32 k = 0; k < 5; ++k)
				{
					Tris.Append({ B + k, B + k + 1, B + k + 6, B + k + 6, B + k + 1, B + k + 7 });
				}
			}
			// Painted parapet blocks along the drop
			if (Road.Walls.IsValidIndex(i) && Road.Walls[i] != 0 && Along >= NextBlock)
			{
				NextBlock = Along + Cfg.ParapetSpacing;
				for (int32 Side = 0; Side < 2; ++Side)
				{
					if (Road.Walls[i] & (1 << Side))
					{
						const FVector At = Pts[i] + Right * (Side == 0 ? -1.f : 1.f) * (HalfW + Sh * 0.6f) - FVector(0, 0, 4.f);
						Blocks->AddInstance(FitToBox(BlockMesh, Cfg.ParapetBlockSize, FTransform(T.Rotation(), At)));
					}
				}
			}
		}
		// One component per road: each cooks only its own collision
		UProceduralMeshComponent* Mesh = NewMesh(TEXT("Road"), true);
		Mesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, {}, {}, true);
		if (City->Art.RoadMaterial)
		{
			Mesh->SetMaterial(0, City->Art.RoadMaterial);
		}
	}
}

void ACityActor::BuildWater()
{
	// Flat water sheet wider than the channel: the carved banks rise through it, so the terrain draws the
	// shoreline (ragged, natural) instead of the mesh edge. UV: U across, V = metres downstream / 10
	const TArray<FVector>& River = Layout.River;
	if (River.Num() < 2 || Layout.RiverHalfWidths.Num() != River.Num())
	{
		return;
	}
	UProceduralMeshComponent* Mesh = NewMesh(TEXT("Water"), false);
	Mesh->SetCastShadow(false);
	constexpr int32 Across = 6;
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	float Along = 0.f;
	for (int32 i = River.Num() - 1; i > 0; --i)
	{
		Along += FVector::Dist2D(River[i], River[i - 1]);
	}
	for (int32 i = 0; i < River.Num(); ++i)
	{
		if (i > 0)
		{
			Along -= FVector::Dist2D(River[i], River[i - 1]);
		}
		const FVector T = (River[FMath::Min(i + 1, River.Num() - 1)] - River[FMath::Max(i - 1, 0)]).GetSafeNormal2D();
		const FVector Right(-T.Y, T.X, 0.f);
		const float HalfW = Layout.RiverHalfWidths[i] * 2.2f;
		for (int32 k = 0; k <= Across; ++k)
		{
			const float U = float(k) / Across;
			Verts.Add(River[i] + Right * HalfW * (U * 2.f - 1.f));
			UVs.Add(FVector2D(U, Along / 1000.f));
			Normals.Add(FVector::UpVector);
		}
		if (i > 0)
		{
			const int32 B = (i - 1) * (Across + 1);
			for (int32 k = 0; k < Across; ++k)
			{
				Tris.Append({ B + k, B + k + 1, B + k + Across + 1, B + k + Across + 1, B + k + 1, B + k + Across + 2 });
			}
		}
	}
	Mesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, {}, {}, false);
	if (City->Art.WaterMaterial)
	{
		Mesh->SetMaterial(0, City->Art.WaterMaterial);
	}
}

void ACityActor::BuildPlots(const FCityGenerator& Gen)
{
	const FCityHeightfield& H = Gen.GetHeightfield();
	const FCityTownSettings& Town = City->Town;
	FRandomStream Rand(int32(HashCombine(GetTypeHash(Layout.Seed), GetTypeHash(FString(TEXT("buildings"))))));

	TArray<UStaticMesh*> HouseMeshes;
	for (UStaticMesh* M : City->Art.Houses)
	{
		if (M) HouseMeshes.Add(M);
	}
	if (HouseMeshes.Num() == 0)
	{
		HouseMeshes.Add(CubeMesh);
	}
	TArray<UHierarchicalInstancedStaticMeshComponent*> Houses;
	for (UStaticMesh* M : HouseMeshes)
	{
		Houses.Add(NewInstances(M, City->Art.BuildingMaterial, City->Art.HouseCullDistance, true));
	}
	UHierarchicalInstancedStaticMeshComponent* Placeholders = nullptr;

	for (const FCityPlot& Plot : Layout.Plots)
	{
		// Foundation reaches the lowest corner so nothing floats on the slope
		const FRotator Rot(0.f, Plot.Yaw, 0.f);
		float MinZ = Plot.Location.Z;
		for (const FVector2D Corner : { FVector2D(1, 1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(-1, -1) })
		{
			MinZ = FMath::Min(MinZ, H.Sample(FVector2D(Plot.Location + Rot.RotateVector(FVector(Corner * Plot.Size * 0.5f, 0.f)))));
		}
		const float Rise = Plot.Location.Z - MinZ + 50.f;
		const FTransform Base(Rot, FVector(FVector2D(Plot.Location), MinZ - 50.f));

		if (Plot.Use == ECityLandmark::House)
		{
			const FVector2D Floors = FMath::Lerp(Town.FloorsAtEdge, Town.FloorsAtCentre, Plot.Centrality);
			const float Height = FMath::RoundToFloat(Rand.FRandRange(Floors.X, Floors.Y)) * Town.FloorHeight + Rise;
			const int32 Kind = Rand.RandRange(0, Houses.Num() - 1);
			const FVector Size(Plot.Size.X * Rand.FRandRange(0.7f, 0.9f), Plot.Size.Y * Rand.FRandRange(0.75f, 0.95f), Height);
			Houses[Kind]->AddInstance(FitToBox(HouseMeshes[Kind], Size, Base));
			continue;
		}

		const FCityLandmarkRule* Rule = City->Landmarks.FindByPredicate([&Plot](const FCityLandmarkRule& R) { return R.Type == Plot.Use; });
		if (Rule && Rule->Prefab)
		{
			FActorSpawnParameters Params;
			Params.ObjectFlags |= RF_Transient;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FTransform At = FTransform(Rot, Plot.Location) * GetActorTransform();
			if (AActor* Actor = GetWorld()->SpawnActor<AActor>(Rule->Prefab, At, Params))
			{
				SpawnedActors.Add(Actor);
			}
			continue;
		}
		if (!Placeholders)
		{
			Placeholders = NewInstances(CubeMesh, City->Art.LandmarkMaterial, City->Art.HouseCullDistance, true);
		}
		Placeholders->AddInstance(FitToBox(CubeMesh, FVector(Plot.Size * 0.8f, 2.f * Town.FloorHeight + Rise), Base));
	}
}

void ACityActor::BuildTrees()
{
	TArray<UStaticMesh*> Meshes;
	for (UStaticMesh* M : City->Art.Trees)
	{
		if (M) Meshes.Add(M);
	}
	const bool bPlaceholder = Meshes.Num() == 0;
	if (bPlaceholder)
	{
		Meshes.Add(ConeMesh);
	}
	TArray<UHierarchicalInstancedStaticMeshComponent*> Instances;
	for (UStaticMesh* M : Meshes)
	{
		Instances.Add(NewInstances(M, bPlaceholder ? City->Art.TreeMaterial.Get() : nullptr, City->Art.TreeCullDistance, true));
	}
	for (int32 i = 0; i < Layout.Trees.Num(); ++i)
	{
		const FTransform& T = Layout.Trees[i];
		const int32 Kind = i % Meshes.Num();
		Instances[Kind]->AddInstance(bPlaceholder ? FitToBox(ConeMesh, FVector(350.f, 350.f, 900.f) * T.GetScale3D().X, FTransform(T.GetRotation(), T.GetLocation())) : T);
	}
}

void ACityActor::SpawnStops()
{
	for (const FCityStop& Stop : Layout.Stops)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ABusStop* Actor = GetWorld()->SpawnActor<ABusStop>(ABusStop::StaticClass(), Stop.Transform * GetActorTransform(), Params))
		{
			Actor->SetStopName(Stop.Name);
			SpawnedActors.Add(Actor);
		}
	}
}

void ACityActor::MovePawnsToStart()
{
	if (!HasAuthority())
	{
		return;
	}
	NextSlot = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			GivePlayerVehicle(PC, NextSlot++);
		}
	}
	// Players who join later (multiplayer) get a bus too
	if (!PostLoginHandle.IsValid())
	{
		PostLoginHandle = FGameModeEvents::GameModePostLoginEvent.AddUObject(this, &ACityActor::HandlePostLogin);
	}
}

void ACityActor::HandlePostLogin(AGameModeBase* GameMode, APlayerController* PC)
{
	if (PC && PC->GetWorld() == GetWorld())
	{
		GivePlayerVehicle(PC, NextSlot++);
	}
}

void ACityActor::EndPlay(const EEndPlayReason::Type Reason)
{
	FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
	Super::EndPlay(Reason);
}

void ACityActor::GivePlayerVehicle(APlayerController* PC, int32 Slot)
{
	// Line up behind the start, one bus length + gap apart
	const FTransform Start = GetStartTransform();
	const FVector At = Start.GetLocation() - Start.GetRotation().GetForwardVector() * StartSpacing * Slot + FVector(0.f, 0.f, 150.f);
	const FTransform Spawn(Start.GetRotation(), At);
	APawn* Old = PC->GetPawn();
	if (!PlayerVehicleClass)
	{
		if (Old)
		{
			Old->SetActorLocationAndRotation(At, Start.GetRotation(), false, nullptr, ETeleportType::ResetPhysics);
			PC->SetControlRotation(Start.Rotator());
		}
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APawn* Vehicle = GetWorld()->SpawnActor<APawn>(PlayerVehicleClass, Spawn, Params);
	if (!Vehicle)
	{
		return;
	}
	if (ABusVehicle* Bus = Cast<ABusVehicle>(Vehicle))
	{
		Bus->SetRespawnPoint(FTransform(Start.GetRotation(), At - FVector(0.f, 0.f, 150.f)));
	}
	PC->Possess(Vehicle);
	PC->SetControlRotation(Start.Rotator());
	if (Old && Old != Vehicle)
	{
		Old->Destroy();
	}
}
