// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Utils.h"
#include "Vertex.h"
#include "HelperVertex.h"
#include "CGALWrapperUtils.h"
#include "SurfaceArea.generated.h"

UCLASS()
class DPP3DS_API ASurfaceArea : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASurfaceArea();

	//ID of the surface area to prevent issues with connecting a lot of surface areas through pointers
	UPROPERTY(VisibleAnywhere, Category = "Surface Area")
		int32 id;

	//Type of surface this surface is
	UPROPERTY(EditAnywhere, Category = "Surface Area")
		ESurfaceType surface;

	//Neighbours of this surface area
	UPROPERTY(VisibleAnywhere, Category = "Surface Area")
		TArray<FSurfaceNeighbour> neighbours;

	//Array of all cubes that form this surface
	UPROPERTY(VisibleAnywhere, Category = "Surface Area")
		TArray<UStaticMeshComponent*> cubes;

	//Array of all cubes that are not part of the surface but help connecting to ridged surfaces
	UPROPERTY(VisibleAnywhere, Category = "Surface Area")
		TArray<UStaticMeshComponent*> connectionCubes;

	//Array of the weights of the cubes, which are the surface areas and determine the probability of generating a vertex there
	UPROPERTY(VisibleAnywhere, Category = "Surface Area")
		TArray<float> weights;

	//Total size of the surface area
	UPROPERTY(VisibleAnywhere, Category = "Surface Area")
		float totalSize;

	//Total size of the surface area covered by edges
	UPROPERTY(VisibleAnywhere, Category = "Build Stats")
		float totalCoverSize;

	//Polygon defining the surface
	UPROPERTY(VisibleAnywhere, Category = "Polygon")
		FSurfPolygon polygon;

	//Edges defining the straight skeleton
	UPROPERTY(VisibleAnywhere, Category = "Polygon")
		TArray<FPolygonEdge3D> straightSkeleton;

	UPROPERTY(EditAnywhere, Category = "Polygon")
		float skeletonSize;

	//Medial axis defining the surface
	UPROPERTY(VisibleAnywhere, Category = "Polygon")
		FMedialAxis medialAxis;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	//Rootcomponent
	UPROPERTY(VisibleAnywhere, Category = "Root")
		USceneComponent* root;

	//Storage for the cube mesh and materials so that they can be swapped after construction if needed
	UStaticMesh* cubeMesh; 
	UMaterialInstance* floorMat;
	UMaterialInstance* ceilingMat;
	UMaterialInstance* northWallMat;
	UMaterialInstance* southWallMat;
	UMaterialInstance* eastWallMat;
	UMaterialInstance* westWallMat;
	UMaterialInstance* stairsMat;
	UMaterialInstance* connectionMat;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	//Adds a box component to the surface
	UFUNCTION(CallInEditor, Category = "Surface Area")
		void addBox();

	//Removes the last box component added to the surface
	UFUNCTION(CallInEditor, Category = "Surface Area")
		void removeBox();

	//Adds a box component for connecting ridged surfaces
	UFUNCTION(CallInEditor, Category = "Surface Area")
		void addConnectionBox();

	//Removes the last connection box component added to the surface
	UFUNCTION(CallInEditor, Category = "Surface Area")
		void removeConnectionBox();

	//Calculates the weights of the cubes. Resets it beforehand
	UFUNCTION(CallInEditor, Category = "Surface Area")
		void calculateWeights();

	//Calculates the size of the cover in this surface
	UFUNCTION(CallInEditor, Category = "Build Stats")
		float calculateCoverSize(float agentSize);

	//Shows the surface area
	UFUNCTION(CallInEditor, Category = "Rendering")
		void showArea();

	//Hides the surface area
	UFUNCTION(CallInEditor, Category = "Rendering")
		void hideArea();

	//Shows the connection cubes of the surface area
	UFUNCTION(CallInEditor, Category = "Rendering")
		void showConnectionArea();

	//Hides the connection cubes of the surface area
	UFUNCTION(CallInEditor, Category = "Rendering")
		void hideConnectionArea();

	//Change the material of this surface, depending on its surfaceType
	UFUNCTION(CallInEditor, Category = "Rendering")
		void updateMaterials();

	//Finds all surfaces that are connected to this one
	UFUNCTION(CallInEditor, Category = "Surface Area")
		void findNeighbours();

	//Computes the medial axis of the surface
	UFUNCTION(CallInEditor, Category = "Surface Area")
		void computeMedialAxis();

	//Generates a connection cube between the current surface and a floor. Only works for the first cube component!
	UFUNCTION(CallInEditor, Category = "Connectors")
		void generateFloorConnector();

	//Generates a connection cube between the current surface and a ceiling. Only works for the first cube component!
	UFUNCTION(CallInEditor, Category = "Connectors")
		void generateCeilingConnector();

	//Generates a connection cube between the current surface and a northWall. Only works for the first cube component!
	UFUNCTION(CallInEditor, Category = "Connectors")
		void generateNorthWallConnector();

	//Generates a connection cube between the current surface and a southWall. Only works for the first cube component!
	UFUNCTION(CallInEditor, Category = "Connectors")
		void generateSouthWallConnector();

	//Generates a connection cube between the current surface and a eastWall. Only works for the first cube component!
	UFUNCTION(CallInEditor, Category = "Connectors")
		void generateEastWallConnector();

	//Generates a connection cube between the current surface and a westWall. Only works for the first cube component!
	UFUNCTION(CallInEditor, Category = "Connectors")
		void generateWestWallConnector();

	//Generates a connection cube between the current floor and a ceiling on the north side
	UFUNCTION(CallInEditor, Category = "Flat Connectors Floor")
		void generateFloorCeilingNorthConnector();

	//Generates a connection cube between the current floor and a ceiling on the south side
	UFUNCTION(CallInEditor, Category = "Flat Connectors Floor")
		void generateFloorCeilingSouthConnector();

	//Generates a connection cube between the current floor and a ceiling on the east side
	UFUNCTION(CallInEditor, Category = "Flat Connectors Floor")
		void generateFloorCeilingEastConnector();

	//Generates a connection cube between the current floor and a ceiling on the west side
	UFUNCTION(CallInEditor, Category = "Flat Connectors Floor")
		void generateFloorCeilingWestConnector();

	//Generates a connection cube between the current ceiling and a floor on the north side
	UFUNCTION(CallInEditor, Category = "Flat Connectors Ceiling")
		void generateCeilingFloorNorthConnector();

	//Generates a connection cube between the current ceiling and a floor on the south side
	UFUNCTION(CallInEditor, Category = "Flat Connectors Ceiling")
		void generateCeilingFloorSouthConnector();

	//Generates a connection cube between the current ceiling and a floor on the east side
	UFUNCTION(CallInEditor, Category = "Flat Connectors Ceiling")
		void generateCeilingFloorEastConnector();

	//Generates a connection cube between the current ceiling and a floor on the west side
	UFUNCTION(CallInEditor, Category = "Flat Connectors Ceiling")
		void generateCeilingFloorWestConnector();

	//Generates a connection cube between the current north wall and a south wall on the floor
	UFUNCTION(CallInEditor, Category = "Flat Connectors North")
		void generateNorthSouthFloorConnector();

	//Generates a connection cube between the current north wall and a south wall on the ceiling
	UFUNCTION(CallInEditor, Category = "Flat Connectors North")
		void generateNorthSouthCeilingConnector();

	//Generates a connection cube between the current north wall and a south wall on the east side
	UFUNCTION(CallInEditor, Category = "Flat Connectors North")
		void generateNorthSouthEastConnector();

	//Generates a connection cube between the current north wall and a south wall on the west side
	UFUNCTION(CallInEditor, Category = "Flat Connectors North")
		void generateNorthSouthWestConnector();

	//Generates a connection cube between the current south wall and a north wall on the floor
	UFUNCTION(CallInEditor, Category = "Flat Connectors South")
		void generateSouthNorthFloorConnector();

	//Generates a connection cube between the current south wall and a north wall on the ceiling
	UFUNCTION(CallInEditor, Category = "Flat Connectors South")
		void generateSouthNorthCeilingConnector();

	//Generates a connection cube between the current south wall and a north wall on the east side
	UFUNCTION(CallInEditor, Category = "Flat Connectors South")
		void generateSouthNorthEastConnector();

	//Generates a connection cube between the current south wall and a north wall on the west side
	UFUNCTION(CallInEditor, Category = "Flat Connectors South")
		void generateSouthNorthWestConnector();

	//Generates a connection cube between the current east wall and a west wall on the floor
	UFUNCTION(CallInEditor, Category = "Flat Connectors East")
		void generateEastWestFloorConnector();

	//Generates a connection cube between the current east wall and a west wall on the ceiling
	UFUNCTION(CallInEditor, Category = "Flat Connectors East")
		void generateEastWestCeilingConnector();

	//Generates a connection cube between the current east wall and a west wall on the east side
	UFUNCTION(CallInEditor, Category = "Flat Connectors East")
		void generateEastWestNorthConnector();

	//Generates a connection cube between the current east wall and a west wall on the west
	UFUNCTION(CallInEditor, Category = "Flat Connectors East")
		void generateEastWestSouthConnector();

	//Generates a connection cube between the current west wall and an east wall on the floor
	UFUNCTION(CallInEditor, Category = "Flat Connectors West")
		void generateWestEastFloorConnector();

	//Generates a connection cube between the current west wall and an eaest wall on the ceiling
	UFUNCTION(CallInEditor, Category = "Flat Connectors West")
		void generateWestEastCeilingConnector();

	//Generates a connection cube between the current west wall and an east wall on the north side
	UFUNCTION(CallInEditor, Category = "Flat Connectors West")
		void generateWestEastNorthConnector();

	//Generates a connection cube between the current west wall and an east wall on the south side
	UFUNCTION(CallInEditor, Category = "Flat Connectors West")
		void generateWestEastSouthConnector();

	//Generates a connection cube between the current stair and a step down to the north side
	UFUNCTION(CallInEditor, Category = "Stair Connectors")
		void generateStairNorthConnector();

	//Generates a connection cube between the current stair and a step down to the south side
	UFUNCTION(CallInEditor, Category = "Stair Connectors")
		void generateStairSouthConnector();

	//Generates a connection cube between the current stair and a step down to the east side
	UFUNCTION(CallInEditor, Category = "Stair Connectors")
		void generateStairEastConnector();

	//Generates a connection cube between the current stair and a step down to the west side
	UFUNCTION(CallInEditor, Category = "Stair Connectors")
		void generateStairWestConnector();

	//Generates a connection cube between the current north stair wall and a step down
	UFUNCTION(CallInEditor, Category = "Stair Connectors")
		void generateStairWallNorthConnector();

	//Generates a connection cube between the current south stair wall and a step down
	UFUNCTION(CallInEditor, Category = "Stair Connectors")
		void generateStairWallSouthConnector();

	//Generates a connection cube between the current east stair wall and a step down
	UFUNCTION(CallInEditor, Category = "Stair Connectors")
		void generateStairWallEastConnector();

	//Generates a connection cube between the current west stair wall and a step down
	UFUNCTION(CallInEditor, Category = "Stair Connectors")
		void generateStairWallWestConnector();

	//Draws the polygon that defines this surface
	UFUNCTION(CallInEditor, Category = "Polygon")
		void drawPolygon();

	//Draws the polygon that defines this surface
	UFUNCTION(CallInEditor, Category = "Polygon")
		void drawPolygonTriangulation();

	//Generates a vertex with a given ID
	AVertex* generateVertex(int32 vertexID, float agentSize, bool showVertex);

	//Generates a vertex with a given ID in a specific subsurface
	AVertex* generateVertexInSubsurface(int32 vertexID, float agentSize, int32 subsurfaceIndex, bool showVertex);

	//Generates a vertex with a given ID at a specific location
	AVertex* generateVertexAtLocation(int32 vertexID, float agentSize, FVector location, bool showVertex);

	//Generates a vertex with a given ID at a specific location
	AVertex* generateVertexInRange(int32 vertexID, float agentSize, FVector tne, FVector bsw, bool showVertex);

	//Generates a helper vertex with a given ID
	AHelperVertex* generateHelperVertex(int32 vertexID, FVector location, ESurfaceType surfaceType, float agentSize, bool showVertex, AVertex* inGoalA, AVertex* inGoalB);

	//Finds a random cube of this surface
	UStaticMeshComponent* getRandomCube();

	//Defines the spawn location for a vertex depending on the given cube and the surface type.
	FVector getRandomSpawnLocation(UStaticMeshComponent* cube);

	//Defines the spawn location on the exact medial axis.
	FVector getRandomSkeletonLocation();

	//Defines the spawn location for a vertex depending on the given area and the surface type.
	FVector getRandomSpawnLocationInBox(UStaticMeshComponent* cube, FVector tne, FVector bsw);

	//Checks if a location is within an obstacle or room
	bool isInObstacle(FVector location, float agentSize);

	//Checks if a location is within an obstacle. Rooms are not considered
	bool isInObstaclePure(FVector location, float agentSize);

	//Checks if a location is within another surface
	bool isInOtherSurface(FVector location);

	//Check if a certain surface is already a neighbour
	bool isNeighbour(int32 neighbourID);

	//Find the neighbour struct based on the ID of the neighbour
	FSurfaceNeighbour getNeighbourFromID(int32 neighbourID);

	//Finds the normal of the surface
	FVector getNormal();

	//Checks whether A -> B -> C is counterclockwise. 
	bool isCounterClockWise(FVector A, FVector B, FVector C);

	//Implementation of the Shoelace formula to calculate the area of a convex hull
	float calculateConvexArea(TArray<FVector2D> flatVertices);

	//Computes the polygon that defines this surface
	void computePolygon();
	
	//Ensures that two polygon edges do not overlap. Can return a variety of options
	void fixEdgeOverlap(FPolygonEdge oE, FPolygonEdge nE, TArray<FPolygonEdge> inEdges, TArray<FPolygonEdge> &outEdges, bool inHasSplit, bool &hasSplit);

	//Creates a Delaunay Triangulation of the polygon according to "Sweep-line algorithm for constrained Delaunay triangulation" by Domiter and Zalik
	TArray<FDTriangle> computeDelaunayTriangulation();

	//Checks if two triangles adjacent at a certain edge are illegal
	bool trianglesAreIllegal(FDTriangle a, FDTriangle b, FVector2D endA, FVector2D endB);

	//Finds the vertices of the advancing front
	TArray<FFrontVertex> findFrontVertices(TArray<FPolygonEdge> front);

	//Check if a vertex on the advancing front is visible
	bool checkIsVisible(FVector2D vertexA, FVector2D vertexB, TArray<FDTriangle> triangles);

	//Handle Steiner point insertion for obtuse triangles with three neighbours
	void addSteinerPoints();

	//Transforms a Delaunay triangulation for use in the medial axis algorithm by Smogavec and Zalik
	//TArray<FCDTriangle> medaxifyDelaunay();

	//Finds the straight skeleton of each cube and connects them to form a bigger skeleton
	TArray<FPolygonEdge3D> findSkeleton();

	//Find all pairs of bisectors, excluding pairs that include vertices that have a shorter pair
	TArray<FBisectorPair> findBisectorPairs(TArray<FPolygonEdge> bisectors);

	//Check if a vertex is on an edge
	bool vertexOnSegment(FVector2D vertex1, FVector2D vertex2, FVector2D vertex3);

	//Shift a point to the exact medial axis
	FVector shiftToMedialAxis(FVector point);
};