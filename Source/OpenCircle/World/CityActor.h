// Copyright OpenCircle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/CityTypes.h"
#include "CityActor.generated.h"

class UCityDataAsset;
class UProceduralMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class FCityGenerator;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCityBuilt);

/**
 *  Builds a procedural city (terrain, roads, river, buildings, landmarks, trees, bus stops) from a UCityDataAsset and a seed.
 *  Single player picks the seed locally. Multiplayer-ready: only the seed replicates and every machine builds the same city.
 *  Drop one into the level, assign the data asset, press Generate Preview to look at it in the editor.
 */
UCLASS()
class ACityActor : public AActor
{
	GENERATED_BODY()

public:

	ACityActor();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Choose a new seed and rebuild. Server / single player only */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="City")
	void SetSeed(int32 NewSeed);

	UFUNCTION(BlueprintPure, Category="City")
	int32 GetSeed() const { return BuiltSeed; }

	UFUNCTION(BlueprintPure, Category="City")
	const FCityLayout& GetLayout() const { return Layout; }

	/** Race start / finish in world space */
	UFUNCTION(BlueprintPure, Category="City")
	FTransform GetStartTransform() const;

	UFUNCTION(BlueprintPure, Category="City")
	FTransform GetFinishTransform() const;

	UPROPERTY(BlueprintAssignable, Category="City")
	FOnCityBuilt OnCityBuilt;

	UFUNCTION(CallInEditor, Category="City")
	void GeneratePreview();

	UFUNCTION(CallInEditor, Category="City")
	void ClearCity();

protected:

	UPROPERTY(EditAnywhere, Category="City")
	TObjectPtr<UCityDataAsset> City;

	/** 0 = the data asset's default. ?CitySeed=N on the map URL overrides it */
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Seed, Category="City")
	int32 Seed = 0;

	UPROPERTY(EditAnywhere, Category="City")
	bool bRandomSeedEachPlay = false;

	/** Put players' pawns at the route start once the city is built */
	UPROPERTY(EditAnywhere, Category="City")
	bool bMovePawnsToStart = true;

	/** Gap between pawns lined up at the start (cm) */
	UPROPERTY(EditAnywhere, Category="City")
	float StartSpacing = 1600.f;

	UPROPERTY(EditAnywhere, Category="City")
	bool bSpawnBusStops = true;

	/** If set, every player gets one of these at the route start and possesses it (replaces their default pawn) */
	UPROPERTY(EditAnywhere, Category="City")
	TSubclassOf<APawn> PlayerVehicleClass;

private:

	UFUNCTION()
	void OnRep_Seed();

	void Build();
	void BuildTerrain(const FCityGenerator& Gen);
	void BuildBackdrop(const FCityGenerator& Gen);
	void BuildRoads(const FCityGenerator& Gen);
	void BuildWater();
	void BuildPlots(const FCityGenerator& Gen);
	void BuildTrees();
	void SpawnStops();
	void MovePawnsToStart();
	void GivePlayerVehicle(APlayerController* PC, int32 Slot);
	void HandlePostLogin(class AGameModeBase* GameMode, APlayerController* PC);
	FDelegateHandle PostLoginHandle;
	int32 NextSlot = 0;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UProceduralMeshComponent* NewMesh(const TCHAR* Name, bool bCollision);
	UHierarchicalInstancedStaticMeshComponent* NewInstances(UStaticMesh* Mesh, UMaterialInterface* Material, float CullDistance, bool bCollision);
	/** Instance transform that fits a mesh's bounds to a box standing on Base (bottom centre) */
	static FTransform FitToBox(const UStaticMesh* Mesh, const FVector& Size, const FTransform& Base);

	UPROPERTY(Transient)
	FCityLayout Layout;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UActorComponent>> Generated;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnedActors;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	int32 BuiltSeed = INDEX_NONE;
};
