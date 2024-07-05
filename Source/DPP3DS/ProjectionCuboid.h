// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "PRM.h"
#include "SurfaceArea.h"
#include "Vertex.h"
#include "ProjectionCuboid.generated.h"

UCLASS()
class DPP3DS_API AProjectionCuboid : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AProjectionCuboid();

	UPROPERTY(EditAnywhere, Category = "Surfaces")
		bool hasCeiling;

	UPROPERTY(EditAnywhere, Category = "Surfaces")
		bool hasFloor;

	UPROPERTY(EditAnywhere, Category = "Surfaces")
		bool hasNorthWall;

	UPROPERTY(EditAnywhere, Category = "Surfaces")
		bool hasSouthWall;

	UPROPERTY(EditAnywhere, Category = "Surfaces")
		bool hasEastWall;

	UPROPERTY(EditAnywhere, Category = "Surfaces")
		bool hasWestWall;

	UPROPERTY(EditAnywhere, Category = "Vertex Counts")
		int32 vertexCountX;

	UPROPERTY(EditAnywhere, Category = "Vertex Counts")
		int32 vertexCountY;

	UPROPERTY(EditAnywhere, Category = "Vertex Counts")
		int32 vertexCountZ;

	UPROPERTY(VisibleAnywhere, Category = "Vertex Counts")
		int32 verticesGeneratedCeiling;

	UPROPERTY(VisibleAnywhere, Category = "Vertex Counts")
		int32 verticesGeneratedFloor;

	UPROPERTY(VisibleAnywhere, Category = "Vertex Counts")
		int32 verticesGeneratedNorthWall;

	UPROPERTY(VisibleAnywhere, Category = "Vertex Counts")
		int32 verticesGeneratedSouthWall;

	UPROPERTY(VisibleAnywhere, Category = "Vertex Counts")
		int32 verticesGeneratedEastWall;

	UPROPERTY(VisibleAnywhere, Category = "Vertex Counts")
		int32 verticesGeneratedWestWall;

	UPROPERTY(EditAnywhere, Category = "Rendering")
		FVector size;

	UPROPERTY(EditAnywhere, Category = "Vertex Counts")
		float inverseVertexDensity;

	UPROPERTY(EditAnywhere, Category = "Vertex Counts")
		float agentSize;

	UPROPERTY(EditAnywhere, Category = "Rendering")
	FVector tne;

	UPROPERTY(EditAnywhere, Category = "Rendering")
	FVector bsw;
	
	UPROPERTY(EditAnywhere, Category = "Rendering")
	FVector spawnLocation;

	UPROPERTY(EditAnywhere, Category = "PRM")
		APRM* ceilingPRM;

	UPROPERTY(EditAnywhere, Category = "PRM")
		APRM* floorPRM;

	UPROPERTY(EditAnywhere, Category = "PRM")
		APRM* northPRM;

	UPROPERTY(EditAnywhere, Category = "PRM")
		APRM* southPRM;

	UPROPERTY(EditAnywhere, Category = "PRM")
		APRM* eastPRM;

	UPROPERTY(EditAnywhere, Category = "PRM")
		APRM* westPRM;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	//Box visualising the cuboid. Does not have collision
	UPROPERTY(VisibleAnywhere, Category = "Root")
	UBoxComponent* box;
	UPROPERTY(VisibleAnywhere, Category = "Root")
	USceneComponent* root;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(CallInEditor, Category = "Rendering")
		void updateSize();

	UFUNCTION(CallInEditor, Category = "Vertex Counts")
		void reset();

	UFUNCTION(CallInEditor, Category = "Rendering")
		void setSurfaces(bool c, bool f, bool n, bool s, bool e, bool w);

	UFUNCTION(CallInEditor, Category = "Vertex Counts")
		void setVertexCounts();

	void findPRMS(TArray<APRM*> prms);
	APRM* findPRM(FVector traceStart, FVector traceEnd, TArray<APRM*> prms);
	void generateRandomVertices(bool knn, bool knn3d, TArray<FPRMConnection> inConnections, TArray<FPRMConnection> &outConnections, int32 inVertexID, int32 inEdgeID, int32 &outVertexID, int32 &outEdgeID);
	void generateApproximateMedialVertices(bool knn, bool knn3d, TArray<FPRMConnection> inConnections, TArray<FPRMConnection> &outConnections, int32 inVertexID, int32 inEdgeID, int32 &outVertexID, int32 &outEdgeID);
	void generateExactMedialVertices(bool knn, bool knn3d, TArray<FPRMConnection> inConnections, TArray<FPRMConnection> &outConnections, int32 inVertexID, int32 inEdgeID, int32 &outVertexID, int32 &outEdgeID);
	AVertex* createVertex(FVector location, ESurfaceType surface, int32 vertexID, bool showVertex);
	AVertex* generateVertex(FVector location, int32 vertexID, APRM* prm, ESurfaceType surface);
	int32 connectVertex(AVertex* newVertex, APRM* prm, int32 edgeID, bool knn, bool knn3d, TArray<FPRMConnection> inConnections, TArray<FPRMConnection> &outConnections);
	TArray<AVertex*> findNeighbours(AVertex* newVertex, FVector extent);
	void projectToSurface(FVector point, APRM* prm, bool knn, bool knn3d, TArray<FPRMConnection> inConnections, TArray<FPRMConnection> &outConnections, int32 vertexID, int32 edgeID, int32 &nextVertexID, int32 &nextEdgeID);
	void projectToSurfaceMedial(FVector point, APRM* prm, bool knn, bool knn3d, TArray<FPRMConnection> inConnections, TArray<FPRMConnection> &outConnections, int32 vertexID, int32 edgeID, int32 &nextVertexID, int32 &nextEdgeID);
	void projectToSurfaceExactMedial(FVector point, APRM* prm, bool knn, bool knn3d, TArray<FPRMConnection> inConnections, TArray<FPRMConnection> &outConnections, int32 vertexID, int32 edgeID, int32 &nextVertexID, int32 &nextEdgeID);
	FVector getRandomPointInCuboid();
	bool isEdgeBlocked(AVertex* a, AVertex* b);
	bool isEdgeOverVoid(FVector traceStart, FVector traceEnd);
	APRMEdge* generateEdge(AVertex* a, AVertex* b, int32 ID, APRM* prm, ESurfaceType surfaceType, bool showEdge);

	//Subroutine for approximating the medial axis as given by Yan, Jouandeau and Cherif
	FVector approximateMedialAxisP(FVector inLocation, ASurfaceArea* surface, bool &canGenerate);

	//Find the closest obstacle. Done by tracing in all 4 directions for a 2D polygon and all vertices (in case a vertex is closer)
	FVector findClosestObstacleP(FVector location, ASurfaceArea* surface);

	//Checks if the base vertex and its neighbour are in neighbouring surfaces
	bool checkNeighbouringSurfaces(AVertex* base, AVertex* neighbour);

	//Checks if the base vertex and its neighbour are in neighbouring surfaces
	bool checkNeighbouringSurfacesTest(AVertex* base, AVertex* neighbour);

	// IDEA:
	// OPTION IN PRMCOLLECTOR TO PROJECT INSTEAD OF GENERATEVERTICES
	// THEN GO OVER EACH PROJECTIONCUBOID TO LET IT GENERATE VERTICES
	// PUT EACH NEW VERTEX IN THE ACCOMPANYING PRM. THIS NEEDS A LOOKUP FUNCTION TO FIND WHICH SURFACE AREA A VERTEX IS GENERATED IN AND WHICH PRM IT BELONGS TO!
	// DURING PROJECTION, GENERATE EDGES TO NEARBY VERTICES
	// AFTER PROJECTION, DO THE REGULAR CONNECTION STUFF TO CONNECT PARTIAL PRMS

	// CREATE ONE ENVIRONMENT WITH MERGED CUBOIDS AND ONE WITH SMALLER ONES, ESPECIALLY IN OBSTACLE BASED AREAS!
	// FOR EASE OF WORK, GENERATE A CUBOID IN EACH BASIC ROOM.

	// VARIABLES THAT ARE NEEDED:
	// BOOLEANS FOR WHICH SURFACES EXIST IN THE CUBOID
	// AMOUNT OF VERTICES TO GENERATE IN THE CUBOID (BASED ON SIZE, DIFFERENT FOR X, Y AND Z.
	// AMOUNT OF VERTICES GENERATED IN THE CUBOID (DIFFERENT FOR EACH SURFACE)
	// THE FIRST CAN BE SET UPON GENERATION BY THE BASIC ROOM
	// THE SECOND CAN BE SET UP BASED ON SIZE AFTER CREATION
	// THE THIRD IS KEPT TRACK OF DURING GENERATION

	// FUNCTIONS THAT ARE NEEDED
	// GENERATE ONE POINT
	// FIND A RANDOM POINT IN THE CUBOID
	// PROJECT TO ALL SURFACES
	// PROJECT TO SINGLE SURFACE
	// CONNECT NEW VERTEX TO PREVIOUS ONES
	// FIND PRMS THAT BELONG TO VERTICES IN THE CUBOID

};
