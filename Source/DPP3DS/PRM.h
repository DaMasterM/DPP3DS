// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurfaceArea.h"
#include "PRMEdge.h"
#include "HelperVertex.h"
#include "PRM.generated.h"

//Structure for indicating that two vertices in two different PRMS should be connected
USTRUCT()
struct FPRMConnection {

	GENERATED_BODY()

		FPRMConnection();
	FPRMConnection(AVertex* inVertexA, AVertex* inVertexB);

public:
	//Vertex to connect
	UPROPERTY(EditAnywhere)
		AVertex* vertexA;

	//Vertex to connect to
	UPROPERTY(EditAnywhere)
		AVertex* vertexB; 
	
	bool operator==(const FPRMConnection &other) const
	{
		return ((other.vertexA == vertexA && other.vertexB == vertexB) || (other.vertexA == vertexB && other.vertexB == vertexA));
	}
};

UCLASS()
class DPP3DS_API APRM : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APRM();

	//ID of the PRM
	UPROPERTY(VisibleAnywhere, Category = "PRM")
		int32 id;

	//The surfaces that this PRM will be generated in.
	UPROPERTY(EditAnywhere, Category = "PRM")
		TArray<ASurfaceArea*> surfaces;

	/*The relative weights of the surfaces in this PRM. 
	* The weight is equal to the surface area of the surface and determines the probability of generating a vertex on the surface
	*/
	UPROPERTY(VisibleAnywhere, Category = "PRM")
		TArray<float> weights;

	//Neighbouring PRMs that should be connected to.
	UPROPERTY(VisibleAnywhere, Category = "PRM")
		TArray<FPRMNeighbour> neighbours;

	//Amount of vertices that should be created for this PRM.
	UPROPERTY(EditAnywhere, Category = "PRM")
		int32 vertexCount;

	//Amount of vertices a vertex should be connected with based on the vertexCount (half with a minimum of 2)
	UPROPERTY(EditAnywhere, Category = "PRM")
		int32 edgeCount;

	//Amount of vertices a vertex should be connected with based on the vertexCount (in 3D, so half with a minimum of 2 + the same for neighbouring PRMs)
	UPROPERTY(EditAnywhere, Category = "PRM")
		int32 edgeCount3D;

	//Vertices created by this PRM
	UPROPERTY(EditAnywhere, Category = "PRM")
		TArray<AVertex*> vertices;

	//Edges created by this PRM
	UPROPERTY(EditAnywhere, Category = "PRM")
		TArray<APRMEdge*> edges;

	//Total size of all surfaces combined
	UPROPERTY(VisibleAnywhere, Category = "PRM")
		float totalSize;

	//Cover size of all surfaces combined
	UPROPERTY(VisibleAnywhere, Category = "Build Stats")
		float coverSize;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Calculates the weight of the surfaces this PRM uses
	UFUNCTION(CallInEditor, Category = "PRM")
		void calculateWeights();

	// Destroys this PRM. N.B. other PRMs are not affected by this removal! Use with caution!
	UFUNCTION(CallInEditor, Category = "PRM")
		void removePRM();

	//Hides the PRM vertices and edges from view.
	UFUNCTION(CallInEditor, Category = "Rendering")
		void hidePRM();

	//Shows the PRM vertices and edges from view.
	UFUNCTION(CallInEditor, Category = "Rendering")
		void showPRM();

	//Calculate the size of the cover
	UFUNCTION(CallInEditor, Category = "PRM")
		float calculateCoverSize(float agentSize);

	//Sets the vertex count depending on the total size and the vertex density of the collector
	void setVertexCount(float inverseVertexDensity);

	// Generate random vertices on the surfaces. Returns the ID of the last generated vertex + 1
	int32 generateRandomVertices(int32 startID, float agentSize, bool apsmo, bool assmo);

	// Generate vertices approximately on the medial axis of the surfaces. Returns the ID of the last generated vertex + 1
	int32 generateApproximateMedialVertices(int32 startID, float agentSize);

	// Generate vertices approximately on the medial axis of the surfaces. Returns the ID of the last generated vertex + 1
	int32 generateExactMedialVertices(int32 startID, float agentSize);

	//Subroutine for approximating the medial axis as given by Yan, Jouandeau and Cherif
	FVector approximateMedialAxis(FVector inLocation, ASurfaceArea* surface, float agentSize, bool &canGenerate);

	//Find the closest obstacle. Done by tracing in all 4 directions for a 2D polygon and all vertices (in case a vertex is closer)
	FVector findClosestObstacle(FVector location, ASurfaceArea* surface);

	//Checks if the base vertex and its neighbour are in neighbouring surfaces
	bool checkNeighbouringSurfaces(AVertex* base, AVertex* neighbour);

	//Checks if the base vertex and its neighbour are in neighbouring surfaces
	bool checkIfInSurface(AVertex* vertex, ASurfaceArea* surface);

	// Choose a surface area to generate a vertex in
	ASurfaceArea* selectRandomSurface();

	// Generate a single vertex
	int32 generateVertex(int32 startID, ASurfaceArea* surface, float agentSize);

	// Generate a single vertex at the given location
	int32 generateVertexAtLocation(int32 startID, ASurfaceArea* surface, FVector location, float agentSize);

	// Generate a single vertex in a subsurface with a given index
	int32 generateVertexInSubsurface(int32 startID, ASurfaceArea* surface, int32 index, float agentSize);

	// Generate edges between vertices. Returns the ID of the last generated edge + 1
	int32 generateEdges(int32 startID, float agentSize, TArray<FPRMConnection> inConnections, TArray<FPRMConnection> &outConnections, bool knn, bool knn3d);

	// Find neighbouring vertices in a specific range
	TArray<AVertex*> findAllNearbyNeighbours(AVertex* baseVertex, FVector extent, TArray<AVertex*> checkedVertices);

	// Check if the edge from a to b is blocked. Returns true if so or if no collision happens
	bool isEdgeBlocked(AVertex* a, AVertex* b, float agentSize);

	// Check if the edge from a to b is blocked. Returns true if so or if no collision happens
	bool isFutureEdgeBlocked(FVector a, FVector b, ESurfaceType surfaceA, ESurfaceType surfaceB, float agentSize);

	// Check if the edge from a to b is blocked. Returns true if so or if no collision happens
	bool isEdgeBlockedHelper(AVertex* a, AVertex* b, float agentSize);

	// Check if the edge from a to b is blocked. Returns true if so or if no collision happens
	bool isEdgeDoubleBlocked(AVertex* a, AVertex* b, float agentSize);

	// Check if the edge from a to b is blocked. Returns true if so or if no collision happens
	bool isEdgeDoubleBlockedHelper(AVertex* a, AVertex* b, float agentSize);

	// Check if an edge already exists between vertices a and b
	bool doesEdgeExist(AVertex* a, AVertex* b);

	// Creates a single edge between vertices a and b
	APRMEdge* generateEdge(AVertex* a, AVertex* b, int32 ID, ESurfaceType surfaceType, bool showEdge);

	// Checks if an edge from a to b is over a hole, so an area between two surfaces
	bool isEdgeOverVoid(FVector traceStart, FVector traceEnd);

	//Check if a certain surface is already a neighbour
	bool isNeighbour(int32 neighbourID);

	FPRMNeighbour getNeighbourFromID(int32 neighbourID);

	//Check if a certain surface is already a neighbour
	ASurfaceArea* getSurfaceFromStruct(FPRMNeighbour neighbour);

	//Check if a certain surface is already a neighbour
	ASurfaceArea* getNeighbourSurfaceFromStruct(APRM* b, FPRMNeighbour neighbour);
};
