// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils.h"

FSurfaceNeighbour::FSurfaceNeighbour() {
	neighbourID = 0;
	cm = EConnectionMethod::Direct;
	cube = nullptr;
	connectionCube = nullptr;
	tne = FVector(0);
	bsw = FVector(0);
}

FSurfaceNeighbour::FSurfaceNeighbour(int32 inNeighbourID, EConnectionMethod inCM, UStaticMeshComponent* inCube, UStaticMeshComponent* inConnectionCube, FVector inTNE, FVector inBSW)
{
	neighbourID = inNeighbourID;
	cm = inCM;
	cube = inCube;
	connectionCube = inConnectionCube;
	tne = inTNE;
	bsw = inBSW;
}

FPRMNeighbour::FPRMNeighbour() {
	neighbourID = 0;
	cm = EConnectionMethod::Direct;
	surfaceID = 0;
	neighbourSurfaceID = 0;
}

FPRMNeighbour::FPRMNeighbour(int32 inNeighbourID, EConnectionMethod inCM, int32 inSurface, int32 inNeighbourSurface)
{
	neighbourID = inNeighbourID;
	cm = inCM;
	surfaceID = inSurface;
	neighbourSurfaceID = inNeighbourSurface;
}

int32 FPRMNeighbour::getNeighbourID()
{
	return neighbourID;
}

EConnectionMethod FPRMNeighbour::getCM()
{
	return cm;
}

int32 FPRMNeighbour::getSurfaceID()
{
	return surfaceID;
}

int32 FPRMNeighbour::getNeighbourSurfaceID()
{
	return neighbourSurfaceID;
}

FCartesianInPolar::FCartesianInPolar() { cartesian = FVector(0); relative = FVector(0); relativeMin = FVector(0); angle = 0; distance = 0; }

FCartesianInPolar::FCartesianInPolar(FVector inCartesian, FVector minPoint, FVector normal, FVector minimum)
{
	cartesian = inCartesian;
	relative = inCartesian - minPoint;
	relativeMin = inCartesian - minimum;

	FVector base = FVector(0);

	if (normal.X == 1) { base = FVector(0, 1, 0); }
	if (normal.X == -1) { base = FVector(0, -1, 0); }
	if (normal.Y == 1) { base = FVector(-1, 0, 0); }
	if (normal.Y == -1) { base = FVector(1, 0, 0); }
	if (normal.Z == 1) { base = FVector(1, 0, 0); }
	if (normal.Z == -1) { base = FVector(-1, 0, 0); }

	angle = (FVector::DotProduct(relative.GetSafeNormal(), base));
	distance = relative.Size();
}

FPolygonEdge::FPolygonEdge()
{
	vA = FVector2D(0);
	vB = FVector2D(0);
	border = false;
}

FPolygonEdge::FPolygonEdge(FVector2D inVA, FVector2D inVB, bool polygonBound)
{
	vA = inVA;
	vB = inVB;
	border = polygonBound;
}

bool FPolygonEdge::hasVertex(FVector2D vertex) { return vA == vertex || vB == vertex; }

FPolygonEdge3D::FPolygonEdge3D()
{
	vA = FVector(0);
	vB = FVector(0);
	border = false;
}

FPolygonEdge3D::FPolygonEdge3D(FVector inVA, FVector inVB, bool polygonBound)
{
	vA = inVA;
	vB = inVB;
	border = polygonBound;
}

FSurfacePolygon::FSurfacePolygon()
{
}

FSurfacePolygon::FSurfacePolygon(TArray<FVector> inVertices, TArray<FPolygonEdge> inEdges, TArray<FPolygonHole> inHoles)
{
	vertices = inVertices;
	edges = inEdges;
	holes = inHoles;
}

FPolygonHole::FPolygonHole()
{
}

FPolygonHole::FPolygonHole(TArray<FVector> inVertices, TArray<FPolygonEdge> inEdges)
{
	vertices = inVertices;
	edges = inEdges;
}

FMedialAxis::FMedialAxis()
{
}

FMedialAxis::FMedialAxis(TArray<FVector> inVertices, TArray<FPolygonEdge> inEdges)
{
	vertices = inVertices;
	edges = inEdges;
}
