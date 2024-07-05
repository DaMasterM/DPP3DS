// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Utils.generated.h"

/**
 * Contains enumerations and structs for use in some classes
 */

//Enumeration for the type of surface
UENUM()
enum class ESurfaceType : uint8 {
	Floor,
	NorthWall,
	SouthWall,
	EastWall,
	WestWall,
	Ceiling,
	Stairs,
	StairsCeiling,
	TransitionStairs, //Used as the transition between two stair surfaces / floor
	TransitionStairsCeiling, //Used as the transition between two stair ceiling surfaces / ceiling
	TransitionEdge, //Transition between two helper vertices
	TransitionNE, //Transitions from one surface to another
	TransitionNW,
	TransitionNC,
	TransitionNF,
	TransitionSE,
	TransitionSW,
	TransitionSC,
	TransitionSF,
	TransitionEC,
	TransitionEF,
	TransitionWC,
	TransitionWF,
	
};

//Enumeration for the PRM generation methods
UENUM()
enum class EPRMMethod : uint8 {
	KNN,
};

//Enumeration for the PRM generation methods
UENUM()
enum class EDTriangleType : uint8 {
	A, //One edge in common with polygon
	B, //Two edges in common with polygon
	C //No edges in common with polygon
};

//Enumeration for how two surfaces (and by extent surfaces) are connected
UENUM()
enum class EConnectionMethod : uint8 {
	DCB, //Double connection box, so an overlapping connection box
	SCB, //Single connection box, only used if surfaces are split or with stairs
	Direct,
};

//Enumeration for which path planning method to use
UENUM()
enum class EPathPlanningMethod : uint8 {
	AStar, //Basic A* algorithm
	AStarStep, //Stepwise version of A*
	Dynamic, //Dynamic programming algorithm
	Combined //Combination of A* and Dynamic
};

//Structure for the neighbour of a surface
USTRUCT()
struct FSurfaceNeighbour
{
	GENERATED_BODY()

	FSurfaceNeighbour();
	FSurfaceNeighbour(int32 inNeighbourID, EConnectionMethod inCM, UStaticMeshComponent* inCube, UStaticMeshComponent* inConnectionCube, FVector inTNE, FVector inBSW);

public: 
	//ID of the neighbouring surface
	UPROPERTY(VisibleAnywhere, Category = "Neighbour")
	int32 neighbourID;

	//Connection method of the overlap box
	UPROPERTY(VisibleAnywhere, Category = "Neighbour")
	EConnectionMethod cm;

	//Cube that overlaps the neighbouring surface
	UPROPERTY(VisibleAnywhere, Category = "Neighbour")
		UStaticMeshComponent* cube;

	//Cube that overlaps the neighbouring surface
	UPROPERTY(VisibleAnywhere, Category = "Neighbour")
		UStaticMeshComponent* connectionCube;

	//Top north east point of the overlap box
	UPROPERTY(VisibleAnywhere, Category = "Neighbour")
	FVector tne;

	//Bottom south west point of the overlap box
	UPROPERTY(VisibleAnywhere, Category = "Neighbour")
	FVector bsw;
};

//Structure for the neighbour of a PRM
USTRUCT()
struct FPRMNeighbour
{
	GENERATED_BODY()

		FPRMNeighbour();
	FPRMNeighbour(int32 inNeighbourID, EConnectionMethod inCM, int32 inSurface, int32 inNeighbourSurface);

public:
	//ID of the neighbouring surface
	UPROPERTY(VisibleAnywhere, Category = "Neighbour")
		int32 neighbourID;

	//Connection method of the overlap box
	UPROPERTY(VisibleAnywhere, Category = "Neighbour")
		EConnectionMethod cm;

	//Surface that connects to the neighbour
	UPROPERTY(VisibleAnywhere, Category = "Neighbour")
		int32 surfaceID;

	//Surface of the neighbour that connects to the surface of this PRM that connects to the neighbour
	UPROPERTY(VisibleAnywhere, Category = "Neighbour")
		int32 neighbourSurfaceID;

	//Getter functions
	int32 getNeighbourID();
	EConnectionMethod getCM();
	int32 getSurfaceID();
	int32 getNeighbourSurfaceID();
};

//Structure for the overlap of two cover areas
USTRUCT()
struct FCartesianInPolar
{
	GENERATED_BODY()

		FCartesianInPolar();
	FCartesianInPolar(FVector inCartesian, FVector minPoint, FVector normal, FVector minimum);

public:
	//Position in world space
	UPROPERTY(VisibleAnywhere, Category = "Cartesian")
		FVector cartesian;

	//Position relative to the minPoint
	UPROPERTY(VisibleAnywhere, Category = "Polar")
		FVector relative;

	//Position relative to the absolute minimum of the surface
	UPROPERTY(VisibleAnywhere, Category = "Polar")
		FVector relativeMin;

	//Slope of the relative vertex to the minPoint
	UPROPERTY(VisibleAnywhere, Category = "Polar")
		float angle;

	//Distance from the minPoint to the given cartesian
	UPROPERTY(VisibleAnywhere, Category = "Polar")
		float distance;
};

//Structure representing an edge between two points in space
USTRUCT()
struct FPolygonEdge
{
	GENERATED_BODY()

		FPolygonEdge();
	FPolygonEdge(FVector2D inVA, FVector2D inVB, bool polygonBound);

public:
	//Position in world space of vertex A of this edge
	UPROPERTY(VisibleAnywhere, Category = "FEdge")
		FVector2D vA;

	//Position in world space of vertex B of this edge
	UPROPERTY(VisibleAnywhere, Category = "FEdge")
		FVector2D vB;

	//Whether the edge is on the border of the polygon or inside the polygon
	UPROPERTY(VisibleAnywhere, Category = "Polar")
		bool border;

	bool operator==(const FPolygonEdge &other) const
	{
		return ((other.vA == vA && other.vB == vB) || (other.vA == vB && other.vB == vA));
	}

	//Checks if the edge contains a vertex
	bool hasVertex(FVector2D vertex);
};

//Structure representing an edge between two points in space
USTRUCT()
struct FPolygonEdge3D
{
	GENERATED_BODY()

		FPolygonEdge3D();
	FPolygonEdge3D(FVector inVA, FVector inVB, bool polygonBound);

public:
	//Position in world space of vertex A of this edge
	UPROPERTY(VisibleAnywhere, Category = "FEdge")
		FVector vA;

	//Position in world space of vertex B of this edge
	UPROPERTY(VisibleAnywhere, Category = "FEdge")
		FVector vB;

	//Whether the edge is on the border of the polygon or inside the polygon
	UPROPERTY(VisibleAnywhere, Category = "Polar")
		bool border;

	bool operator==(const FPolygonEdge3D &other) const
	{
		return ((other.vA == vA && other.vB == vB) || (other.vA == vB && other.vB == vA));
	}
};

//Structure for a polygon hole
USTRUCT()
struct FPolygonHole
{
	GENERATED_BODY()

		FPolygonHole();
	FPolygonHole(TArray<FVector> inVertices, TArray<FPolygonEdge> inEdges);

public:
	//Vertices of the medial axis
	UPROPERTY(VisibleAnywhere, Category = "Polygon")
		TArray<FVector> vertices;

	//Edges of the medial axis
	UPROPERTY(VisibleAnywhere, Category = "Polygon")
		TArray<FPolygonEdge> edges;
};

//Structure for a polygon
USTRUCT()
struct FSurfacePolygon
{
	GENERATED_BODY()

		FSurfacePolygon();
	FSurfacePolygon(TArray<FVector> inVertices, TArray<FPolygonEdge> inEdges, TArray<FPolygonHole> inHoles);

public:
	//Vertices of the polygon
	UPROPERTY(VisibleAnywhere, Category = "Polygon")
		TArray<FVector> vertices;

	//Edges of the polygon
	UPROPERTY(VisibleAnywhere, Category = "Polygon")
		TArray<FPolygonEdge> edges;

	//Holes of the polygon
	UPROPERTY(VisibleAnywhere, Category = "Polygon")
		TArray<FPolygonHole> holes;
};

//Structure for the medial axis
USTRUCT()
struct FMedialAxis
{
	GENERATED_BODY()

		FMedialAxis();
	FMedialAxis(TArray<FVector> inVertices, TArray<FPolygonEdge> inEdges);

public:
	//Vertices of the medial axis
	UPROPERTY(VisibleAnywhere, Category = "Medial Axis")
		TArray<FVector> vertices;

	//Edges of the medial axis
	UPROPERTY(VisibleAnywhere, Category = "Medial Axis")
		TArray<FPolygonEdge> edges;
};