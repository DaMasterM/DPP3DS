// Fill out your copyright notice in the Description page of Project Settings.


#include "CGALWrapperUtils.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

//Check if vertex 2 is colinear with vertices 1 and 3
bool vertexOnSegment(FVector2D vertex1, FVector2D vertex2, FVector2D vertex3) {
	return vertex2.X <= FMath::Max(vertex1.X, vertex3.X) && vertex2.X >= FMath::Min(vertex1.X, vertex3.X) &&
		vertex2.Y <= FMath::Max(vertex1.Y, vertex3.Y) && vertex2.Y >= FMath::Min(vertex1.Y, vertex3.Y);
}

//Check if vertices 1, 2 and 3 are clockwise, counterclockwise or colinear
int32 findOrientation(FVector2D vertex1, FVector2D vertex2, FVector2D vertex3) {
	int32 returnValue = (vertex2.Y - vertex1.Y) * (vertex3.X - vertex2.X) - (vertex2.X - vertex1.X) * (vertex3.Y - vertex2.Y);
	returnValue = FMath::Clamp(returnValue, -1, 1);
	return returnValue;
}

//Check if edge 12 intersects with edge 34
bool FSurfPolygon::intersects(FVector2D vertex1, FVector2D vertex2, FVector2D vertex3, FVector2D vertex4)
{
	int32 orientationA = findOrientation(vertex1, vertex2, vertex3);
	int32 orientationB = findOrientation(vertex1, vertex2, vertex4);
	int32 orientationC = findOrientation(vertex3, vertex4, vertex1);
	int32 orientationD = findOrientation(vertex3, vertex4, vertex2);

	//UE_LOG(LogTemp, Log, TEXT("Orientation A: %d"), orientationA);
	//UE_LOG(LogTemp, Log, TEXT("Orientation B: %d"), orientationB);
	//UE_LOG(LogTemp, Log, TEXT("Orientation C: %d"), orientationC);
	//UE_LOG(LogTemp, Log, TEXT("Orientation D: %d"), orientationD);

	//General case
	if (orientationA != orientationB && orientationC != orientationD) { return true; }
	
	//Special cases with colinear vertices
	if (orientationA == 0 && vertexOnSegment(vertex1, vertex3, vertex2)) { return true; }
	if (orientationB == 0 && vertexOnSegment(vertex1, vertex4, vertex2)) { return true; }
	if (orientationC == 0 && vertexOnSegment(vertex3, vertex1, vertex4)) { return true; }
	if (orientationD == 0 && vertexOnSegment(vertex3, vertex2, vertex4)) { return true; }

	return false;
}

FVector2D FSurfPolygon::intersection(FVector2D vertex1, FVector2D vertex2, FVector2D vertex3, FVector2D vertex4)
{
	FVector2D returnValue = FVector2D();
	if (intersects(vertex1, vertex2, vertex3, vertex4)) {
		float alpha = ((((vertex4.X - vertex3.X) * (vertex3.Y - vertex1.Y)) - ((vertex4.Y - vertex3.Y) * (vertex3.X - vertex1.X)))
			/ (((vertex4.Y - vertex3.Y) * (vertex2.X - vertex1.X)) - ((vertex4.X - vertex3.X) * (vertex2.Y - vertex1.Y))));
		returnValue = vertex1 + alpha * (vertex2 - vertex1);
	}

	return returnValue;
}

bool FSurfPolygon::hasEdge(FVector2D a, FVector2D b)
{
	for (int32 i = 0; i < vertices.Num(); i++) {
		int32 j = (i + 1) % vertices.Num();

		if ((a == vertices[i] && b == vertices[j]) || (a == vertices[j] && b == vertices[i])) { return true; }
	}

	return false;
}

FSurfPolygon::FSurfPolygon()
{
}

FSurfPolygon::FSurfPolygon(TArray<FVector2D> inVertices, TArray<FVector> inVertices3D)
{
	vertices = inVertices;
	if (vertices.Num() > 0) { minVertex = vertices[0]; }

	for (FVector2D vertex : vertices) {
		if (vertex.X < minVertex.X) { minVertex.X = vertex.X; }
		if (vertex.Y < minVertex.Y) { minVertex.Y = vertex.Y; }
	}

	minVertex = minVertex - FVector2D(5);
	
	vertices3D = inVertices3D;
	if (vertices3D.Num() > 0) { minVertex3D = vertices3D[0]; }

	for (FVector vertex3D : vertices3D) {
		if (vertex3D.X < minVertex3D.X) { minVertex3D.X = vertex3D.X; }
		if (vertex3D.Y < minVertex3D.Y) { minVertex3D.Y = vertex3D.Y; }
		if (vertex3D.Z < minVertex3D.Z) { minVertex3D.Z = vertex3D.Z; }
	}

	minVertex3D = minVertex3D - FVector(5); 

	if (vertices.Num() > 0) { maxVertex = vertices[0]; }
	for (FVector2D vertex : vertices) {
		if (vertex.X > maxVertex.X) { maxVertex.X = vertex.X; }
		if (vertex.Y > maxVertex.Y) { maxVertex.Y = vertex.Y; }
	}

	maxVertex = maxVertex + FVector2D(5);

	if (vertices3D.Num() > 0) { maxVertex3D = vertices3D[0]; }

	for (FVector vertex3D : vertices3D) {
		if (vertex3D.X < maxVertex3D.X) { maxVertex3D.X = vertex3D.X; }
		if (vertex3D.Y < maxVertex3D.Y) { maxVertex3D.Y = vertex3D.Y; }
		if (vertex3D.Z < maxVertex3D.Z) { maxVertex3D.Z = vertex3D.Z; }
	}

	maxVertex3D = maxVertex3D - FVector(5);
}

bool FSurfPolygon::isInsidePolygon(FVector point3D, ESurfaceType surface)
{
	bool returnValue = false;

	//Find the location of the 3D point relative to the surface
	FVector relativePoint3D = point3D;
	FVector2D relativePoint2D = FVector2D();

	//Create a 2D vector based on the surface type
	switch (surface) {
	case ESurfaceType::Ceiling:
		relativePoint2D = FVector2D(relativePoint3D.X, relativePoint3D.Y);
		break;
	case ESurfaceType::Floor:
		relativePoint2D = FVector2D(relativePoint3D.X, relativePoint3D.Y);
		break;
	case ESurfaceType::NorthWall:
		relativePoint2D = FVector2D(relativePoint3D.Z, relativePoint3D.Y);
		break;
	case ESurfaceType::SouthWall:
		relativePoint2D = FVector2D(relativePoint3D.Z, relativePoint3D.Y);
		break;
	case ESurfaceType::EastWall:
		relativePoint2D = FVector2D(relativePoint3D.X, relativePoint3D.Z);
		break;
	case ESurfaceType::WestWall:
		relativePoint2D = FVector2D(relativePoint3D.X, relativePoint3D.Z);
		break;
	case ESurfaceType::Stairs:
		relativePoint2D = FVector2D(relativePoint3D.X, relativePoint3D.Y);
		break;
	case ESurfaceType::StairsCeiling:
		relativePoint2D = FVector2D(relativePoint3D.X, relativePoint3D.Y);
		break;
	default:
		break;
	}

	//Alternative, use a ray cast from the relative point in 2D and check if an odd number of edges have been crossed
	if (vertices.Num() > 1) {
		int32 intersections = 0;
		for (int32 i = 0; i < vertices.Num(); i++) {

			//Find the two vertices of the supposed edge
			FVector2D vertex1 = vertices[i];
			FVector2D vertex2 = FVector2D();

			//Handle the case of the edge between the last and first vertex
			if (i == vertices.Num() - 1) { vertex2 = vertices[0]; }
			else { vertex2 = vertices[i + 1]; }

			if (intersects(vertex1, vertex2, relativePoint2D, FVector2D(relativePoint2D.X, minVertex.Y))) { intersections++; }
		}

		//If there is an odd number of intersections, the given point is in the polygon
		if (intersections % 2 == 1) { returnValue = true; }
	}
	
	return returnValue;
}

bool FSurfPolygon::isOnPolygon(FVector2D point)
{
	for (int32 i = 0; i < vertices.Num(); i++) {
		int32 j = (i + 1) % vertices.Num();
		FVector2D a = vertices[i];
		FVector2D b = vertices[j];
		if (point == a || point == b) { return true; }
		if (a.X == b.X && point.X == a.X) {
			if ((point.Y <= a.Y && point.Y >= b.Y) || (point.Y >= a.Y && point.Y <= b.Y)) {
				return true;
			}
		}
		if (a.Y == b.Y && point.Y == a.Y) {
			if ((point.X <= a.X && point.X >= b.X) || (point.X >= a.X && point.X <= b.X)) {
				return true;
			}
		}
	}

	return false;
}

bool FSurfPolygon::isInsidePolygon2D(FVector2D point)
{
	bool returnValue = false;
	if (vertices.Num() > 1) {
		int32 intersections = 0;
		for (int32 i = 0; i < vertices.Num(); i++) {

			//Find the two vertices of the supposed edge
			FVector2D vertex1 = vertices[i];
			FVector2D vertex2 = FVector2D();

			//Handle the case of the edge between the last and first vertex
			if (i == vertices.Num() - 1) { vertex2 = vertices[0]; }
			else { vertex2 = vertices[i + 1]; }

			if (intersects(vertex1, vertex2, point, FVector2D(point.X, minVertex.Y))) { intersections++; }
		}

		//If there is an odd number of intersections, the given point is in the polygon
		if (intersections % 2 == 1) { returnValue = true; }
	}

	return returnValue;
}

FDTriangle::FDTriangle()
{
}

FDTriangle::FDTriangle(TArray<FVector2D> inVertices)
{
	vertices = inVertices;

	//Only proceed if three vertices are given.
	if (vertices.Num() == 3) {

		//Calculates the circumcircle
		FVector2D relativeB = vertices[1] - vertices[0];
		FVector2D relativeC = vertices[2] - vertices[0];
		float d = 2 * (relativeB.X * relativeC.Y - relativeB.Y * relativeC.X);

		if (d != 0) {
			float centerX = ((relativeC.Y * (FMath::Pow(relativeB.X, 2) + FMath::Pow(relativeB.Y, 2))) - (relativeB.Y * (FMath::Pow(relativeC.X, 2) + FMath::Pow(relativeC.Y, 2)))) / d;
			float centerY = ((relativeB.X * (FMath::Pow(relativeC.X, 2) + FMath::Pow(relativeC.Y, 2))) - (relativeC.X * (FMath::Pow(relativeB.X, 2) + FMath::Pow(relativeB.Y, 2)))) / d;
			circumRadius = FMath::Sqrt(FMath::Pow(centerX,2) + FMath::Pow(centerY,2));
			circumCenter = FVector2D(centerX, centerY) + vertices[0];
		}
		else { circumRadius = 1; circumCenter = FVector2D(0); }
	}
}

bool FDTriangle::hasVertex(FVector2D vertex)
{
	return vertices[0] == vertex || vertices[1] == vertex || vertices[2] == vertex;
}

bool FDTriangle::hasEdge(FVector2D vertexA, FVector2D vertexB) { return vertices.Contains(vertexA) && vertices.Contains(vertexB); }

float FDTriangle::largestAngle()
{
	float angleA = FMath::Acos(FVector2D::DotProduct(vertices[1] - vertices[0], vertices[2] - vertices[0]) / ((vertices[1] - vertices[0]).Size() * (vertices[2] - vertices[0]).Size())) * 180/PI;
	float angleB = FMath::Acos(FVector2D::DotProduct(vertices[0] - vertices[1], vertices[2] - vertices[1]) / ((vertices[0] - vertices[1]).Size() * (vertices[2] - vertices[1]).Size())) * 180/PI;
	float angleC = FMath::Acos(FVector2D::DotProduct(vertices[0] - vertices[2], vertices[1] - vertices[2]) / ((vertices[0] - vertices[2]).Size() * (vertices[1] - vertices[2]).Size())) * 180/PI;

	return FMath::Max3(angleA, angleB, angleC);
}

bool FDTriangle::intersectsTriangle(FSurfPolygon polygon, FDTriangle other)
{
	for (int32 i = 0; i < vertices.Num(); i++) {
		int32 j = (i + 1) % vertices.Num();
		if (vertices[i] != other.vertices[0] && vertices[j] != other.vertices[0]) {
			if (vertices[i] != other.vertices[1] && vertices[j] != other.vertices[1]) {
				if (polygon.intersects(vertices[i], vertices[j], other.vertices[0], other.vertices[1])) {
					if (!vertexOnSegment(other.vertices[0], vertices[i], other.vertices[1]) && !vertexOnSegment(other.vertices[0], vertices[j], other.vertices[1]) && !vertexOnSegment(other.vertices[i], vertices[0], other.vertices[j]) && !vertexOnSegment(other.vertices[i], vertices[1], other.vertices[j])) {
						UE_LOG(LogTemp, Log, TEXT("Intersection: %s to %s and %s to %s"), *vertices[i].ToString(), *vertices[j].ToString(), *other.vertices[0].ToString(), *other.vertices[1].ToString());
						return true;
					}
				}
			}
			if (vertices[i] != other.vertices[2] && vertices[j] != other.vertices[2]) {
				if (polygon.intersects(vertices[i], vertices[j], other.vertices[0], other.vertices[2])) {
					if (!vertexOnSegment(other.vertices[0], vertices[i], other.vertices[2]) && !vertexOnSegment(other.vertices[0], vertices[j], other.vertices[2]) && !vertexOnSegment(other.vertices[i], vertices[0], other.vertices[j]) && !vertexOnSegment(other.vertices[i], vertices[2], other.vertices[j])) {
						UE_LOG(LogTemp, Log, TEXT("Intersection: %s to %s and %s to %s"), *vertices[i].ToString(), *vertices[j].ToString(), *other.vertices[0].ToString(), *other.vertices[2].ToString());
						return true;
					}
				}
			}
		}
		if (vertices[i] != other.vertices[1] && vertices[j] != other.vertices[1]) {
			if (vertices[i] != other.vertices[2] && vertices[j] != other.vertices[2]) {
				if (polygon.intersects(vertices[i], vertices[j], other.vertices[1], other.vertices[2])) {
					if (!vertexOnSegment(other.vertices[1], vertices[i], other.vertices[2]) && !vertexOnSegment(other.vertices[1], vertices[j], other.vertices[2]) && !vertexOnSegment(other.vertices[i], vertices[2], other.vertices[j]) && !vertexOnSegment(other.vertices[i], vertices[1], other.vertices[j])) {
						UE_LOG(LogTemp, Log, TEXT("Intersection: %s to %s and %s to %s"), *vertices[i].ToString(), *vertices[j].ToString(), *other.vertices[1].ToString(), *other.vertices[1].ToString());
						return true;
					}
				}
			}
		}
	}

	return false;
}

bool FDTriangle::isNearer(FVector2D a, FVector2D b, FVector2D c)
{
	FVector2D oppositeA = vertices[0];
	FVector2D oppositeB = vertices[1];
	if (oppositeA == c) { oppositeA = vertices[2]; }
	if (oppositeB == c) { oppositeB = vertices[2]; }

	float d = FMath::Abs(((oppositeB.X - oppositeA.X) * (a.Y - oppositeA.Y)) - ((a.X - oppositeA.X) * (oppositeB.Y - oppositeA.Y))) / FMath::Sqrt(FMath::Pow(oppositeB.X - oppositeA.X, 2) + FMath::Pow(oppositeB.Y - oppositeA.Y, 2));
	float e = FMath::Abs(((oppositeB.X - oppositeA.X) * (b.Y - oppositeA.Y)) - ((b.X - oppositeA.X) * (oppositeB.Y - oppositeA.Y))) / FMath::Sqrt(FMath::Pow(oppositeB.X - oppositeA.X, 2) + FMath::Pow(oppositeB.Y - oppositeA.Y, 2));

	return d < e;
}

FPolygonEdge FSurfPolygon::intersectsEdge(FVector2D vertexA, FVector2D vertexB)
{
	FPolygonEdge returnValue;
	if (vertices.Num() > 1) {
		for (int32 i = 0; i < vertices.Num(); i++) {

			//Find the two vertices of the supposed edge
			FVector2D vertex1 = vertices[i];
			FVector2D vertex2 = vertices[(i + 1) % vertices.Num()];

			if (intersects(vertex1, vertex2, vertexA, vertexB)) { returnValue = FPolygonEdge(vertex1, vertex2, true); }
		}
	}

	return returnValue;
}

FFrontVertex::FFrontVertex()
{
}

FFrontVertex::FFrontVertex(FVector2D inVertex, FPolygonEdge inEdgeA, FPolygonEdge inEdgeB)
{
	vertex = inVertex;
	edgeA = inEdgeA;
	edgeB = inEdgeB;
}

FCDTriangle::FCDTriangle()
{
}

FCDTriangle::FCDTriangle(FDTriangle inTriangle, EDTriangleType inType)
{
	triangle = inTriangle;
	type = inType;
	fixed = false;
}

FBisectorPair::FBisectorPair()
{
}

FBisectorPair::FBisectorPair(FPolygonEdge inBA, FPolygonEdge inBB)
{
	bA = inBA;
	bB = inBB;

	if (intersects(bA.vA, bA.vB, bB.vA, bB.vB)) { intersection = findIntersection(bA.vA, bA.vB, bB.vA, bB.vB); }

	distA = (intersection - bA.vA).Size();
	distB = (intersection - bB.vA).Size();
}

bool FBisectorPair::intersects(FVector2D vertex1, FVector2D vertex2, FVector2D vertex3, FVector2D vertex4)
{
	int32 orientationA = findOrientation(vertex1, vertex2, vertex3);
	int32 orientationB = findOrientation(vertex1, vertex2, vertex4);
	int32 orientationC = findOrientation(vertex3, vertex4, vertex1);
	int32 orientationD = findOrientation(vertex3, vertex4, vertex2);

	//General case
	if (orientationA != orientationB && orientationC != orientationD) { return true; }

	//Special cases with colinear vertices
	if (orientationA == 0 && vertexOnSegment(vertex1, vertex3, vertex2)) { return true; }
	if (orientationB == 0 && vertexOnSegment(vertex1, vertex4, vertex2)) { return true; }
	if (orientationC == 0 && vertexOnSegment(vertex3, vertex1, vertex4)) { return true; }
	if (orientationD == 0 && vertexOnSegment(vertex3, vertex2, vertex4)) { return true; }

	return false;
}

FVector2D FBisectorPair::findIntersection(FVector2D vertex1, FVector2D vertex2, FVector2D vertex3, FVector2D vertex4)
{
	FVector2D returnValue = FVector2D();
	if (intersects(vertex1, vertex2, vertex3, vertex4)) {
		float alpha = ((((vertex1.X - vertex3.X) * (vertex3.Y - vertex4.Y)) - ((vertex1.Y - vertex3.Y) * (vertex3.X - vertex4.X)))
			/ (((vertex1.X - vertex2.X) * (vertex3.Y - vertex4.Y)) - ((vertex1.Y - vertex2.Y) * (vertex3.X - vertex4.X))));
		//UE_LOG(LogTemp, Log, TEXT("alpha: %f"), alpha);
		returnValue = vertex1 + alpha * (vertex2 - vertex1);
	}

	return returnValue;
}

FCubeSkeleton::FCubeSkeleton()
{
}

FCubeSkeleton::FCubeSkeleton(FVector neVertex, FVector nwVertex, FVector seVertex, FVector swVertex, FVector inMiddle)
{
	middle = inMiddle;
	edgeNE = FPolygonEdge3D(neVertex, middle, true);
	edgeNW = FPolygonEdge3D(nwVertex, middle, true);
	edgeSE = FPolygonEdge3D(seVertex, middle, true);
	edgeSW = FPolygonEdge3D(swVertex, middle, true);
	square = true;
}

FCubeSkeleton::FCubeSkeleton(FVector neVertex, FVector nwVertex, FVector seVertex, FVector swVertex, FVector inMiddle, FVector inMiddleA, FVector inMiddleB, bool ns)
{
	middle = inMiddle;
	middleA = inMiddleA;
	middleB = inMiddleB;
	square = false;	

	if (ns) {
		edgeNE = FPolygonEdge3D(neVertex, middleA, true);
		edgeNW = FPolygonEdge3D(nwVertex, middleB, true);
		edgeSE = FPolygonEdge3D(seVertex, middleA, true);
		edgeSW = FPolygonEdge3D(swVertex, middleB, true);
	}
	else {
		edgeNE = FPolygonEdge3D(neVertex, middleA, true);
		edgeNW = FPolygonEdge3D(nwVertex, middleA, true);
		edgeSE = FPolygonEdge3D(seVertex, middleB, true);
		edgeSW = FPolygonEdge3D(swVertex, middleB, true);
	}

	middleEdgeA = FPolygonEdge3D(middle, middleA, true);
	middleEdgeB = FPolygonEdge3D(middle, middleB, true);
}
