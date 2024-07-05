// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PRMCollector.h"
#include "PRMGenerator.generated.h"

UCLASS()
class DPP3DS_API APRMGenerator : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APRMGenerator();

	UPROPERTY(EditAnywhere, Category = "PRM")
		APRMCollector* PRMCollector;

	//Name of the save file to read
	UPROPERTY(EditAnywhere, Category = "PRM")
		FString saveName;

	//If true, vertices are projected onto surfaces instead of generated there
	UPROPERTY(EditAnywhere, Category = "Options")
		bool projectOntoSurfaces;

	//If true, the PRM is generated along the medial axis
	UPROPERTY(EditAnywhere, Category = "Options")
		bool useMedialAxis;

	//If true, the PRM is generated along the medial axis
	UPROPERTY(EditAnywhere, Category = "Options")
		bool approximateMedialAxis;

	//If true, the kNearestNeighbours connection method is used instead of all nearby vertices
	UPROPERTY(EditAnywhere, Category = "Options")
		bool kNearestNeighbours;

	//If true, the kNearestNeighbours connection method is used instead of all nearby vertices, and it takes 3D space into account
	UPROPERTY(EditAnywhere, Category = "Options")
		bool kNearestNeighbours3D;

	//If true, all the partial surfaces get a minimum of 1 vertex. The rest of the vertices are randomly placed as usual
	UPROPERTY(EditAnywhere, Category = "Options")
		bool allPartialSurfacesMinimumOne;

	//If true, all the subsurfaces get a minimum of 1 vertex. The rest of the vertices are randomly placed as usual
	UPROPERTY(EditAnywhere, Category = "Options")
		bool allSubSurfacesMinimumOne;

	//If true, at least 1 vertex is generated near another surface before the connection process to increase the probability of having a connection
	UPROPERTY(EditAnywhere, Category = "Options")
		bool guaranteeNearbyVertices;

	//If true, any missing connections between neighbouring surfaces are forced to exist after the connection process
	UPROPERTY(EditAnywhere, Category = "Options")
		bool guaranteeConnections;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(CallInEditor, Category = "PRM")
		void generatePRM();

	UFUNCTION(CallInEditor, Category = "PRM")
		void resetPRM();

	UFUNCTION(CallInEditor, Category = "PRM")
		void reconnectPRM();

	UFUNCTION(CallInEditor, Category = "PRM")
		void showPRM();

	UFUNCTION(CallInEditor, Category = "PRM")
		void hidePRM();

	UFUNCTION(CallInEditor, Category = "PRM")
		void showStats();

	UFUNCTION(CallInEditor, Category = "PRM")
		void showStatsCSV();

	UFUNCTION(CallInEditor, Category = "Options")
		void applyOptions();

};
