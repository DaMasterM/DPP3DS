// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Utils.h"
#include "CGALWrapperUtils.generated.h"

/**
 * Polygon without holes, determined by vertices in clockwise order. 
 */
USTRUCT()
struct FSurfPolygon
{
	GENERATED_BODY()
		
	FSurfPolygon();
	FSurfPolygon(TArray<FVector2D> inVertices, TArray<FVector> inVertices3D);
public:

	//All vertices denoting this polygon
	UPROPERTY(EditAnywhere, Category = "Polygon")
		TArray<FVector2D> vertices;

	//All 3D vertices denoting this polygon
	UPROPERTY(EditAnywhere, Category = "Polygon")
		TArray<FVector> vertices3D;

	//The vertex denoting the lower left of the bounding box: the minimum x and y values. We also add a padding of 5!
	UPROPERTY(EditAnywhere, Category = "Polygon")
		FVector2D minVertex;

	//The 3D vertex denoting the lower left of the bounding box: the minimum x and y values. We also add a padding of 5!
	UPROPERTY(EditAnywhere, Category = "Polygon")
		FVector minVertex3D;

	//The vertex denoting the upper right of the bounding box: the maximum x and y values. We also add a padding of 5!
	UPROPERTY(EditAnywhere, Category = "Polygon")
		FVector2D maxVertex;

	//The 3D vertex denoting the upper left of the bounding box: the maximum x and y values. We also add a padding of 5!
	UPROPERTY(EditAnywhere, Category = "Polygon")
		FVector maxVertex3D;

	//Checks if a given point is inside the polygon. The point should be given in world space, alongside the world location of the SurfaceArea the polygon belongs to.
	bool isInsidePolygon(FVector point3D, ESurfaceType surface);

	//Checks if a given point is on the polygon
	bool isOnPolygon(FVector2D point);

	//Checks if a given point is inside the polygon
	bool isInsidePolygon2D(FVector2D point);

	bool intersects(FVector2D vertex1, FVector2D vertex2, FVector2D vertex3, FVector2D vertex4);

	FVector2D intersection(FVector2D vertex1, FVector2D vertex2, FVector2D vertex3, FVector2D vertex4);

	FPolygonEdge intersectsEdge(FVector2D vertexA, FVector2D vertexB);

	bool hasEdge(FVector2D a, FVector2D b);
};

//Vertex on an advancing front
USTRUCT()
struct FFrontVertex {
	GENERATED_BODY()

		FFrontVertex();
	FFrontVertex(FVector2D inVertex, FPolygonEdge inEdgeA, FPolygonEdge inEdgeB);
public:
	UPROPERTY(EditAnywhere)
		FVector2D vertex;

	UPROPERTY(EditAnywhere)
		FPolygonEdge edgeA;

	UPROPERTY(EditAnywhere)
		FPolygonEdge edgeB;

	bool operator==(const FFrontVertex &other) const
	{
		return vertex == other.vertex && (edgeA == other.edgeA || edgeA == other.edgeB) && (edgeB == other.edgeA || edgeB == other.edgeB);
	}
};

//Delaunay triangle in 2D
USTRUCT()
struct FDTriangle {
	GENERATED_BODY()

	FDTriangle();
	FDTriangle(TArray<FVector2D> inVertices);
public:

	//All vertices denoting this triangle
	UPROPERTY(EditAnywhere, Category = "Polygon")
		TArray<FVector2D> vertices;

	//Center of the circumcircle
	FVector2D circumCenter;

	//Radius of the circumcircle
	float circumRadius;

	bool operator==(const FDTriangle &other) const {
		bool returnValue = true;
		for (FVector2D vertex : vertices) { if (!other.vertices.Contains(vertex)) { returnValue = false; } }
		return returnValue;
	}

	bool hasVertex(FVector2D vertex);

	bool hasEdge(FVector2D vertexA, FVector2D vertexB);

	float largestAngle();

	bool intersectsTriangle(FSurfPolygon polygon, FDTriangle other);

	//Check if A is closer to the edge of the triangle opposite of C than B is
	bool isNearer(FVector2D a, FVector2D b, FVector2D c);
};

USTRUCT()
struct FCDTriangle{
	GENERATED_BODY()

	FCDTriangle();
	FCDTriangle(FDTriangle inTriangle, EDTriangleType inType);
public:

	//Triangle
	UPROPERTY(EditAnywhere, Category = "Polygon")
	FDTriangle triangle;

	//Type of triangle
	UPROPERTY(EditAnywhere, Category = "Polygon")
	EDTriangleType type;

	bool fixed;

	bool operator==(const FCDTriangle &other) const {
		return (triangle == other.triangle) && (type == other.type);
	}
};

USTRUCT()
struct FBisectorPair {
	GENERATED_BODY()

		FBisectorPair();
	FBisectorPair(FPolygonEdge inBA, FPolygonEdge inBB);
public:

	FPolygonEdge bA;
	FPolygonEdge bB;
	float distA;
	float distB;
	FVector2D intersection;

	bool operator==(const FBisectorPair &other) const {
		return ((bA.vA == other.bA.vA && bB.vA == other.bB.vA) || (bB.vA == other.bA.vA && bA.vA == other.bB.vA)) && (intersection == other.intersection);
	}

	bool intersects(FVector2D vertex1, FVector2D vertex2, FVector2D vertex3, FVector2D vertex4);

	FVector2D findIntersection(FVector2D vertex1, FVector2D vertex2, FVector2D vertex3, FVector2D vertex4);
};

//Structure for a straight skeleton of a single cube
USTRUCT()
struct FCubeSkeleton {
	GENERATED_BODY()

	FCubeSkeleton();
	FCubeSkeleton(FVector neVertex, FVector nwVertex, FVector seVertex, FVector swVertex, FVector inMiddle);
	FCubeSkeleton(FVector neVertex, FVector nwVertex, FVector seVertex, FVector swVertex, FVector inMiddle, FVector inMiddleA, FVector inMiddleB, bool ns);
public:

	bool square;
	FPolygonEdge3D edgeNE;
	FPolygonEdge3D edgeNW;
	FPolygonEdge3D edgeSE;
	FPolygonEdge3D edgeSW;
	FVector middle;
	FVector middleA;
	FVector middleB;
	FPolygonEdge3D middleEdgeA;
	FPolygonEdge3D middleEdgeB;
	TArray<FCubeSkeleton> neighbours;
};