// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PRM.h"
#include "Vertex.h"
#include "PRMEdge.h"
#include "ProjectionCuboid.h"
#include "Utils.h"
#include "ConnectivityGraph.h"
#include "PRMBuildSave.h"
#include "PRMCollector.generated.h"

UCLASS()
class DPP3DS_API APRMCollector : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APRMCollector();

	UPROPERTY(EditAnywhere, Category = "PRM")
		TArray<APRM*> PRMS;

	UPROPERTY(EditAnywhere, Category = "PRM")
		AConnectivityGraph* connectivityGraph;

	UPROPERTY(EditAnywhere, Category = "PRM")
		TArray<AProjectionCuboid*> projectionCuboids;

	UPROPERTY(EditAnywhere, Category = "PRM")
		TArray<AVertex*> vertices;

	UPROPERTY(EditAnywhere, Category = "PRM")
		TArray<APRMEdge*> edges;

		TArray<FPRMConnection> interPRMConnections;

	//Inverse of amount of vertices per cm^2
	UPROPERTY(EditAnywhere, Category = "PRM")
		float inverseVertexDensity;

	//Size of the agent in the environment
	UPROPERTY(EditAnywhere, Category = "PRM")
		float agentSize;

	//Starting IDs of the vertices and edges
	int32 startVertexID;
	int32 startEdgeID;

	//Name of the save file that saves the build data
	UPROPERTY(EditAnywhere, Category = "PRM")
		FString saveName;

	//If true, uses 3D projection to generate vertices
	UPROPERTY(EditAnywhere, Category = "Options")
		bool projectOntoSurfaces;

	//If true, the PRM is generated along the medial axis
	UPROPERTY(EditAnywhere, Category = "Options")
		bool useMedialAxis;

	//If true, the approximation of the Medial Axis is used instead of the exact one
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

	//Save object and the values to use
	UPRMBuildSave* saveFile;
	FDateTime startTimeMoment;
	FDateTime endTimeMoment;
	int32 totalConnections;
	int32 madeConnections;
	float totalSurfaceSize;
	float coverSize;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	//Find all PRM objects in the environment
	UFUNCTION(CallInEditor, Category = "PRM")
		void collectPRMS();

	//Generate all PRMs in the environment
	UFUNCTION(CallInEditor, Category = "PRM")
		void generatePRMS();

	//Pass onto each PRM what PRMS it should be connected to
	UFUNCTION(CallInEditor, Category = "PRM")
		void setNeighboursPRMS();

	//Connect the neighbouring PRMs together
	UFUNCTION(CallInEditor, Category = "PRM")
		void connectPRMS();

	//Remove all vertices and edges. Reset all PRMs too.
	UFUNCTION(CallInEditor, Category = "PRM")
		void reset();

	//Hides all PRM vertices and edges from view.
	UFUNCTION(CallInEditor, Category = "Rendering")
		void hidePRMS();

	//Shows all PRM vertices and edges from view.
	UFUNCTION(CallInEditor, Category = "Rendering")
		void showPRMS();

	//Shows all PRM vertices and edges from view.
	UFUNCTION(CallInEditor, Category = "Build Stats")
		void calculateCoverSize();

	//Finds an edge with given end vertices, if it exists
	UFUNCTION(CallInEditor, Category = "PRM")
		APRMEdge* findEdge(int32 a, int32 b);

	//Finds an edge with given end vertices, if it exists
	UFUNCTION(CallInEditor, Category = "Options")
		void applyOptions(FString fileName, bool project, bool maprm, bool approx, bool knn, bool knn3d, bool apsmo, bool assmo, bool guaneavert, bool guacon);

	//Generate a PRM with pure random sampling
	void generateRandomPRM();

	//Approximate the medial axis PRM
	void generateApproximateMedialPRM();

	//Calculate the exact medial axis PRM
	void generateExactMedialPRM();

	//Calculate the exact medial axis PRM
	void reconnectPRM();

	//Helper function for projecting a point onto a plane
	FVector getPointProjectionOntoPlane(FVector planePos, FVector planeNormal, FVector point);

	//Helper function for projectiong a point onto the edge of a surface with a normal and edges given by TNE and BSW alongside a given direction
	FVector getPointProjectionOnPlaneAlongDirection(FVector planeNormal, FVector point, FVector direction);

	//Multiplies two vectors pairwise by coordinate
	FVector pairwiseMult(FVector a, FVector b);

	//Connects two vertices
	bool connectTwoVertices(bool checkDistance, FPRMNeighbour neighbourStruct, APRM* prmA, APRM* prmB, ASurfaceArea* surfaceA, ASurfaceArea* surfaceB, AVertex* vertexA, AVertex* vertexB);

	//Check which direction the helper vertex should be placed based on an orthogonal vector and two vertex locations
	float checkDirection(FVector a, FVector b, FVector orthogonal);

	//Find the TNE's and BSW's of two cubes
	void findTNEBSW(FVector overlapTNE, FVector overlapBSW, FVector inATNE, FVector inABSW, FVector inBTNE, FVector inBBSW, FVector &outATNE, FVector &outABSW, FVector &outBTNE, FVector &outBBSW);

	//Find the nearby areas of two cubes
	void findNearbyAreas(FVector oTNE, FVector oBSW, FVector aTNE, FVector aBSW, FVector bTNE, FVector bBSW, FVector &oCenter, FVector &oExtent, FVector &aCenter, FVector &aExtent, FVector &bCenter, FVector &bExtent);

	//Find vertices in nearby areas
	void findVertices(FVector aCenter, FVector aExtent, FVector bCenter, FVector bExtent, TArray<AVertex*> &outA, TArray<AVertex*> &outB);

	//Find the normals and projections of two surfaces
	void findNormals(EConnectionMethod cm, ASurfaceArea* surfaceA, ASurfaceArea* surfaceB, FVector oCenter, AVertex* vertexA, AVertex* vertexB, FVector &normalA, FVector &normalB);

	//Project two vertices to the sides of their two surfaces at their connection area
	void projectVerticesDCB(AVertex* vertexA, AVertex* vertexB, FVector normalA, FVector normalB, FVector normalC, FVector aExtent, FVector bExtent, FVector aTNE, FVector aBSW, FVector bTNE, FVector bBSW, FVector &projectionA, FVector &projectionB);
	
	//Project two vertices to the sides of their two surfaces at their connection area
	void projectVerticesSCB(AVertex* vertexA, AVertex* vertexB, FVector normalA, FVector normalB, FVector normalC, FVector aExtent, FVector bExtent, FVector aTNE, FVector aBSW, FVector bTNE, FVector bBSW, FVector &projectionA, FVector &projectionB);

	//Find the normal around which two parallel stair surfaces are connected
	FVector findNormalC(FVector normalA, FVector aCenter, FVector bCenter);

	//Find the normals and projections of two surfaces
	void findNormalsDCB(EConnectionMethod cm, ASurfaceArea* surfaceA, ASurfaceArea* surfaceB, FVector oCenter, FVector oExtent, AVertex* vertexA, AVertex* vertexB, FVector &normalA, FVector &normalB, FVector &oAB, FVector &normalC);

	//Find direct projections between two directly connected surfaces
	void findDirectProjection(AVertex* a, AVertex* b, FVector normalA, FVector normalB, FVector overlapTNE, FVector overlapBSW, FVector &projectionA, FVector &projectionB);

	//Find the second projection for DBC and SBC connections
	void findProjections(ASurfaceArea* surfaceA, ASurfaceArea* surfaceB, EConnectionMethod cm, AVertex* vertexA, AVertex* vertexB, FVector projectionA, FVector projectionB, FVector &projectionA2, FVector &projectionB2);

	//Find the helper vertex locations based on the projections and distance ratios
	void findHelperLocations(EConnectionMethod cm, AVertex* vertexA, AVertex* vertexB, FVector projectionA, FVector projectionB, FVector normalA, FVector normalB, FVector oAB, FVector cNormal, FVector &helperLocationA, FVector &helperLocationB);
	void findHelperLocation(AVertex* vertexA, AVertex* vertexB, FVector projectionA, FVector projectionB, FVector &helperLocation);

	//Connect helper vertices with two helper vertices
	bool connectHelperVertices(APRM* a, APRM* b, AVertex* vertexA, AVertex* vertexB, AVertex* newVertexA, AVertex* newVertexB, APRMEdge* edgeA, APRMEdge* edgeB, APRMEdge* edgeC, int32  inEdgeID, int32 &newEdgeID);
	
	//Connect helper vertices with a single helper vertex
	bool connectHelperVerticesSingle(APRM* a, APRM* b, AVertex* vertexA, AVertex* vertexB, AVertex* newVertexA, AVertex* newVertexB, APRMEdge* edgeA, APRMEdge* edgeB, int32 inEdgeID, int32 &newEdgeID);

	//Create a helper vertex
	AVertex* createHelperVertex(ASurfaceArea* surface, FVector location, FVector extent, ESurfaceType surfaceType, int32 inID, int32 &outID, AVertex* goalA, AVertex* goalB);

	//Checks if a prospect helper vertex is near a surface
	bool checkNearSurface(FVector location);

	//Checks if a prospect edge has two vertices on roughly the same surface. Used for SCB connections to prevent extra stair connections
	bool isOnSurface(AVertex* vertexA, AVertex* vertexB);

	//Finds a vertex in the vertices array with a certain id
	AVertex* findVertex(int32 id);

	//Gets a vertex from the vertices array with a specific ID
	AVertex* getVertex(int32 id);

	//Finds the transition surface type based on two surfaces
	ESurfaceType findTransitionType(ESurfaceType a, ESurfaceType b);

	//Finds the transition surface type based on two surfaces
	ESurfaceType findStairsTransition(ESurfaceType a, ESurfaceType b);

	//Find the neighbours of a certain vertex that is added later on
	void findExtraNeighbours(AVertex* vertex, APRM* prm, FVector location, FVector extent);

	//Generate an extra vertex. Return the new edge ID
	int32 generateExtraVertex(AVertex* &extraVertex, APRM* prm, ASurfaceArea* surface, UStaticMeshComponent* cube, int32 newID, int32 newEdgeID, FVector tne, FVector bsw, bool addToExtraArray, TArray<AVertex*> extraArray);

	//Generate an edge between two vertices if possible. Return the new edge ID
	int32 generateExtraEdge(AVertex* a, AVertex* b, APRM* prm, int32 newID, ESurfaceType surfaceType, bool showEdge);
};