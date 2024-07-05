// Fill out Xour copyright notice in the Description page of Project Settings.


#include "SurfaceArea.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Obstacle.h"
#include "BasicRoom.h"
#include "HelperVertex.h"
#include "PRMEdge.h"
#include "CoverArea.h"
#include "Utils.h"
#include "DrawDebugHelpers.h"

//Sorts an array of polar representations of locations based on their angle with the minPoint
struct FSortByAngle
{
	FSortByAngle(){}

	bool operator()(FCartesianInPolar A, FCartesianInPolar B) const
	{
		if (A.angle == B.angle) {
			return A.distance < B.distance;
		}

		return A.angle < B.angle;
	}
};

//Sorts an array of 2D points by their y-value (or x if y is the same)
struct FSortByYX
{
	FSortByYX() {}

	bool operator()(FVector2D A, FVector2D B) const
	{
		if (A.Y == B.Y) {
			return A.X < B.X;
		}

		return A.Y < B.Y;
	}
};

//Sorts an array of 2D points by their x-value (or distance if x is the same) where being closer to a given point is better
struct FSortByXDist
{
	FSortByXDist(FVector2D inC) { C = inC; }

	FVector2D C;

	bool operator()(FFrontVertex A, FFrontVertex B) const
	{
		if (A.vertex.X == B.vertex.X) {
			return (A.vertex - C).Size() < (B.vertex - C).Size();
		}

		return FMath::Abs(A.vertex.X - C.X) < FMath::Abs(B.vertex.X - C.X);
	}
};

//Sorts an array of triangles by the angle at a specific point
struct FSortByTriangleAngle
{
	FSortByTriangleAngle(FVector2D inRightVertex, FVector2D inBaseVertex) { rightVertex = inRightVertex; baseVertex = inBaseVertex; }

	FVector2D baseVertex;
	FVector2D rightVertex;

	bool operator()(FCDTriangle CA, FCDTriangle CB) const
	{
		FDTriangle A = CA.triangle;
		FDTriangle B = CB.triangle;

		float angleA = 0;
		float angleB = 0;

		if (A.vertices[0] == baseVertex) { angleA = FMath::Acos(FVector2D::DotProduct(A.vertices[1] - A.vertices[0], A.vertices[2] - A.vertices[0]) / ((A.vertices[1] - A.vertices[0]).Size() * (A.vertices[2] - A.vertices[0]).Size())) * 180 / PI; }
		else if (A.vertices[1] == baseVertex) { angleA = FMath::Acos(FVector2D::DotProduct(A.vertices[0] - A.vertices[1], A.vertices[2] - A.vertices[1]) / ((A.vertices[0] - A.vertices[1]).Size() * (A.vertices[2] - A.vertices[1]).Size())) * 180 / PI; }
		else { angleA = FMath::Acos(FVector2D::DotProduct(A.vertices[1] - A.vertices[2], A.vertices[0] - A.vertices[2]) / ((A.vertices[1] - A.vertices[2]).Size() * (A.vertices[0] - A.vertices[2]).Size())) * 180 / PI; }
		if (B.vertices[0] == baseVertex) { angleB = FMath::Acos(FVector2D::DotProduct(B.vertices[1] - B.vertices[0], B.vertices[2] - B.vertices[0]) / ((B.vertices[1] - B.vertices[0]).Size() * (B.vertices[2] - B.vertices[0]).Size())) * 180 / PI; }
		else if (B.vertices[1] == baseVertex) { angleB = FMath::Acos(FVector2D::DotProduct(B.vertices[0] - B.vertices[1], B.vertices[2] - B.vertices[1]) / ((B.vertices[0] - B.vertices[1]).Size() * (B.vertices[2] - B.vertices[1]).Size())) * 180 / PI; }
		else { angleB = FMath::Acos(FVector2D::DotProduct(B.vertices[1] - B.vertices[2], B.vertices[0] - B.vertices[2]) / ((B.vertices[1] - B.vertices[2]).Size() * (B.vertices[0] - B.vertices[2]).Size())) * 180 / PI; }
		return angleA < angleB;
	}
};

//Sorts an array of bisector pairs by the smallest distance to the intersection point
struct FSortByBisectorDist
{
	FSortByBisectorDist() {}

	bool operator()(FBisectorPair A, FBisectorPair B) const
	{
		if (A.distA == B.distA) {
			return A.distB < B.distB;
		}

		return A.distA < B.distA;
	}
};

//Check if vertices 1, 2 and 3 are clockwise, counterclockwise or colinear
int32 findHelperOrientation(FVector2D vertex1, FVector2D vertex2, FVector2D vertex3) {
	int32 returnValue = (vertex2.Y - vertex1.Y) * (vertex3.X - vertex2.X) - (vertex2.X - vertex1.X) * (vertex3.Y - vertex2.Y);
	returnValue = FMath::Clamp(returnValue, -1, 1);
	return returnValue;
}

// Sets default values
ASurfaceArea::ASurfaceArea()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//Find the cube mesh and vertex material
	cubeMesh = ConstructorHelpers::FObjectFinder<UStaticMesh>(TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'")).Object;
	floorMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/SurfaceFloorMI.SurfaceFloorMI'")).Object;
	ceilingMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/SurfaceCeilingMI.SurfaceCeilingMI'")).Object;
	northWallMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/SurfaceNorthWallMI.SurfaceNorthWallMI'")).Object;
	southWallMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/SurfaceSouthWallMI.SurfaceSouthWallMI'")).Object;
	eastWallMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/SurfaceEastWallMI.SurfaceEastWallMI'")).Object;
	westWallMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/SurfaceWestWallMI.SurfaceWestWallMI'")).Object;
	stairsMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/SurfaceStairsMI.SurfaceStairsMI'")).Object;
	connectionMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/SurfaceConnectionMI.SurfaceConnectionMI'")).Object;

	//Create the root of this vertex
	root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = root;
	root->SetMobility(EComponentMobility::Static);

	//Show the area in the environment the first time it is created
	showArea(); 
}

// Called when the game starts or when spawned
void ASurfaceArea::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ASurfaceArea::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ASurfaceArea::addBox()
{
	//Find the cube mesh
	UMaterialInstance* mat;

	//Find the correct material
	switch (surface) {
	case ESurfaceType::Floor:
		mat = floorMat;
		break;
	case ESurfaceType::Ceiling:
		mat = ceilingMat;
		break;
	case ESurfaceType::NorthWall:
		mat = northWallMat;
		break;
	case ESurfaceType::SouthWall:
		mat = southWallMat;
		break;
	case ESurfaceType::EastWall:
		mat = eastWallMat;
		break;
	case ESurfaceType::WestWall:
		mat = westWallMat;
		break;
	case ESurfaceType::Stairs:
		mat = stairsMat;
		break;
	case ESurfaceType::StairsCeiling:
		mat = stairsMat;
		break;
	default:
		mat = floorMat;
		break;
	}

	//Add the new box. Scale and position should be done manually
	FString name = "Box " + FString::FromInt(cubes.Num());
	FName displayName = FName(name);		
	UStaticMeshComponent* newCube = NewObject<UStaticMeshComponent>(this, displayName); 
	newCube->CreationMethod = EComponentCreationMethod::Instance;
	newCube->SetStaticMesh(cubeMesh);
	newCube->SetMaterial(0, mat);
	newCube->SetWorldScale3D(FVector(1));
	newCube->RegisterComponent();
	newCube->AttachTo(root);
	this->AddInstanceComponent(newCube);

	//Set up detection of overlaps
	newCube->SetMobility(EComponentMobility::Static);
	newCube->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	newCube->SetGenerateOverlapEvents(true);
	newCube->SetCollisionProfileName(FName("OverlapAll"));

	//Add this cube to the array of all cubes
	cubes.AddUnique(newCube); 
}

void ASurfaceArea::removeBox()
{
	if (cubes.Num() > 0) {
		UStaticMeshComponent* oldBox = cubes[cubes.Num() - 1];
		cubes.Remove(oldBox);
		oldBox->DestroyComponent();
	}
}

void ASurfaceArea::addConnectionBox()
{
	//Add the new box. Scale and position should be done manually
	FString name = "Connection Box " + FString::FromInt(connectionCubes.Num());
	FName displayName = FName(name);
	UStaticMeshComponent* newCube = NewObject<UStaticMeshComponent>(this, displayName);
	newCube->CreationMethod = EComponentCreationMethod::Instance;
	newCube->SetStaticMesh(cubeMesh);
	newCube->SetMaterial(0, connectionMat);
	newCube->SetWorldScale3D(FVector(1));
	newCube->RegisterComponent();
	newCube->AttachTo(root);
	this->AddInstanceComponent(newCube);

	//Set up detection of overlaps
	newCube->SetMobility(EComponentMobility::Static);
	newCube->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	newCube->SetGenerateOverlapEvents(true);
	newCube->SetCollisionProfileName(FName("OverlapAll"));

	//Add this cube to the array of all cubes
	connectionCubes.AddUnique(newCube);
}

void ASurfaceArea::removeConnectionBox()
{
	if (connectionCubes.Num() > 0) {
		UStaticMeshComponent* oldBox = connectionCubes[connectionCubes.Num() - 1];
		connectionCubes.Remove(oldBox);
		oldBox->DestroyComponent();
	}
}

void ASurfaceArea::calculateWeights()
{
	//Combined weight of all the cubes. Used for calculating the relative size of each cube
	totalSize = 0;

	//Reset the weights array
	weights.Empty();

	//Create a separate array for the weights such that calculations can happen later
	TArray<float> tempWeights;

	for (UStaticMeshComponent* cube : cubes) {
		float area = 0;

		//Calculate the weight of the cube
		switch (surface) {
		case ESurfaceType::Floor:
			area = cube->GetComponentScale().X * cube->GetComponentScale().Y;
			break;
		case ESurfaceType::Ceiling:
			area = cube->GetComponentScale().X * cube->GetComponentScale().Y;
			break;
		case ESurfaceType::NorthWall:
			area = cube->GetComponentScale().Z * cube->GetComponentScale().Y;
			break;
		case ESurfaceType::SouthWall:
			area = cube->GetComponentScale().Z * cube->GetComponentScale().Y;
			break;
		case ESurfaceType::EastWall:
			area = cube->GetComponentScale().X * cube->GetComponentScale().Z;
			break;
		case ESurfaceType::WestWall:
			area = cube->GetComponentScale().X * cube->GetComponentScale().Z;
			break;
		case ESurfaceType::Stairs:
			area = cube->GetComponentScale().X * cube->GetComponentScale().Y;
			break;
		case ESurfaceType::StairsCeiling:
			area = cube->GetComponentScale().X * cube->GetComponentScale().Y;
			break;
		default:
			break;
		}

		// Add the weight of the cube to the array and to the total
		tempWeights.Add(area);
		totalSize += area;
	}

	// Go over the weights array and divide the weights by the total area size
	for (float weight : tempWeights) {
		weight = weight / totalSize;
		weights.Add(weight);
	}
}

float ASurfaceArea::calculateCoverSize(float agentSize) {
	totalCoverSize = 0;

	//Output array
	TArray<AActor*> overlaps;
	TArray<APRMEdge*> overlappingEdges = {};

	//Actors to ignore
	TArray<AActor*> ignores;

	//Object types to trace
	TArray<TEnumAsByte<EObjectTypeQuery>> traceObjectTypes;
	traceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldDynamic));

	//Find the area covering the edges on the surface using convex hull (Graham scan)
	for (int c = 0; c < cubes.Num(); c++) {
		UStaticMeshComponent* cube = cubes[c];
		//Find the extent of the cube
		FVector extent = FVector(50 * cube->GetComponentScale().X, 50 * cube->GetComponentScale().Y, 50 * cube->GetComponentScale().Z);
		FVector absoluteMin = cube->GetComponentLocation() - FVector(50 * cube->GetComponentScale().X, 50 * cube->GetComponentScale().Y, 50 * cube->GetComponentScale().Z);
		if (getNormal().X != 0) { absoluteMin.X = cube->GetComponentLocation().X; }
		else if (getNormal().Y != 0) { absoluteMin.Y = cube->GetComponentLocation().Y; }
		else if (getNormal().Z != 0) { absoluteMin.Z = cube->GetComponentLocation().Z; }

		//All vertices of the cover areas
		TArray<FVector> coverVertices;

		//Find all edges overlapping the cube
		UKismetSystemLibrary::BoxOverlapActors(GetWorld(), cube->GetComponentLocation(), extent, traceObjectTypes, false, ignores, overlaps);
		
		//Convert the overlaps to edges
		for (AActor* actor : overlaps) {
			if (actor->GetClass() == APRMEdge::StaticClass()) { 
				APRMEdge* edge = (APRMEdge*)actor;
				if (edge->surface == this->surface) { overlappingEdges.AddUnique((APRMEdge*)actor); }
			}
		}

		//Create a cover area for each edge
		for (APRMEdge* edge : overlappingEdges) {
			//Rotation such that the area is in line with the edge. X-axis is now forward, Y is now side, Z is now height
			FRotator spawnRotation = UKismetMathLibrary::MakeRotFromXZ(edge->spline->GetEndPosition().GetSafeNormal(), getNormal());

			//Find the corners of the cover area
			FVector backLeft = edge->GetActorLocation() - spawnRotation.RotateVector(FVector(agentSize, agentSize, 0));;
			FVector backRight = edge->GetActorLocation() - spawnRotation.RotateVector(FVector(agentSize, -agentSize, 0));;
			FVector endLeft = edge->GetActorLocation() + edge->spline->GetEndPosition() + spawnRotation.RotateVector(FVector(agentSize, -agentSize, 0));
			FVector endRight = edge->GetActorLocation() + edge->spline->GetEndPosition() + spawnRotation.RotateVector(FVector(agentSize, agentSize, 0));
			FVector spawnLocation = (backLeft + endRight) / 2;

			//Limit the corners to the surface area
			if (surface == ESurfaceType::Floor || surface == ESurfaceType::Ceiling || surface == ESurfaceType::Stairs || surface == ESurfaceType::StairsCeiling) {
				//X and Y limitation
				backLeft.X = FMath::Min(backLeft.X, cube->GetComponentLocation().X + extent.X); backLeft.X = FMath::Max(backLeft.X, cube->GetComponentLocation().X - extent.X);
				backLeft.Y = FMath::Min(backLeft.Y, cube->GetComponentLocation().Y + extent.Y); backLeft.Y = FMath::Max(backLeft.Y, cube->GetComponentLocation().Y - extent.Y);
				backLeft.Z = GetActorLocation().Z;
				endLeft.X = FMath::Min(endLeft.X, cube->GetComponentLocation().X + extent.X); endLeft.X = FMath::Max(endLeft.X, cube->GetComponentLocation().X - extent.X);
				endLeft.Y = FMath::Min(endLeft.Y, cube->GetComponentLocation().Y + extent.Y); endLeft.Y = FMath::Max(endLeft.Y, cube->GetComponentLocation().Y - extent.Y);
				endLeft.Z = GetActorLocation().Z;
				backRight.X = FMath::Min(backRight.X, cube->GetComponentLocation().X + extent.X); backRight.X = FMath::Max(backRight.X, cube->GetComponentLocation().X - extent.X);
				backRight.Y = FMath::Min(backRight.Y, cube->GetComponentLocation().Y + extent.Y); backRight.Y = FMath::Max(backRight.Y, cube->GetComponentLocation().Y - extent.Y);
				backRight.Z = GetActorLocation().Z;
				endRight.X = FMath::Min(endRight.X, cube->GetComponentLocation().X + extent.X); endRight.X = FMath::Max(endRight.X, cube->GetComponentLocation().X - extent.X);
				endRight.Y = FMath::Min(endRight.Y, cube->GetComponentLocation().Y + extent.Y); endRight.Y = FMath::Max(endRight.Y, cube->GetComponentLocation().Y - extent.Y);
				endRight.Z = GetActorLocation().Z;
			}
			else if (surface == ESurfaceType::EastWall || surface == ESurfaceType::WestWall) {
				//X and Z limitation
				backLeft.X = FMath::Min(backLeft.X, cube->GetComponentLocation().X + extent.X); backLeft.X = FMath::Max(backLeft.X, cube->GetComponentLocation().X - extent.X);
				backLeft.Y = GetActorLocation().Y;
				backLeft.Z = FMath::Min(backLeft.Z, cube->GetComponentLocation().Z + extent.Z); backLeft.Z = FMath::Max(backLeft.Z, cube->GetComponentLocation().Z - extent.Z);
				endLeft.X = FMath::Min(endLeft.X, cube->GetComponentLocation().X + extent.X); endLeft.X = FMath::Max(endLeft.X, cube->GetComponentLocation().X - extent.X);
				endLeft.Y = GetActorLocation().Y;
				endLeft.Z = FMath::Min(endLeft.Z, cube->GetComponentLocation().Z + extent.Z); endLeft.Z = FMath::Max(endLeft.Z, cube->GetComponentLocation().Z - extent.Z);
				backRight.X = FMath::Min(backRight.X, cube->GetComponentLocation().X + extent.X); backRight.X = FMath::Max(backRight.X, cube->GetComponentLocation().X - extent.X);
				backRight.Y = GetActorLocation().Y;
				backRight.Z = FMath::Min(backRight.Z, cube->GetComponentLocation().Z + extent.Z); backRight.Z = FMath::Max(backRight.Z, cube->GetComponentLocation().Z - extent.Z);
				endRight.X = FMath::Min(endRight.X, cube->GetComponentLocation().X + extent.X); endRight.X = FMath::Max(endRight.X, cube->GetComponentLocation().X - extent.X);
				endRight.Y = GetActorLocation().Y;
				endRight.Z = FMath::Min(endRight.Z, cube->GetComponentLocation().Z + extent.Z); endRight.Z = FMath::Max(endRight.Z, cube->GetComponentLocation().Z - extent.Z);
			}
			else if (surface == ESurfaceType::NorthWall || surface == ESurfaceType::SouthWall) {
				//Y and Z limitation
				backLeft.X = GetActorLocation().X;
				backLeft.Y = FMath::Min(backLeft.Y, cube->GetComponentLocation().Y + extent.Y); backLeft.Y = FMath::Max(backLeft.Y, cube->GetComponentLocation().Y - extent.Y);
				backLeft.Z = FMath::Min(backLeft.Z, cube->GetComponentLocation().Z + extent.Z); backLeft.Z = FMath::Max(backLeft.Z, cube->GetComponentLocation().Z - extent.Z);
				endLeft.X = GetActorLocation().X;
				endLeft.Y = FMath::Min(endLeft.Y, cube->GetComponentLocation().Y + extent.Y); endLeft.Y = FMath::Max(endLeft.Y, cube->GetComponentLocation().Y - extent.Y);
				endLeft.Z = FMath::Min(endLeft.Z, cube->GetComponentLocation().Z + extent.Z); endLeft.Z = FMath::Max(endLeft.Z, cube->GetComponentLocation().Z - extent.Z);
				backRight.X = GetActorLocation().X;
				backRight.Y = FMath::Min(backRight.Y, cube->GetComponentLocation().Y + extent.Y); backRight.Y = FMath::Max(backRight.Y, cube->GetComponentLocation().Y - extent.Y);
				backRight.Z = FMath::Min(backRight.Z, cube->GetComponentLocation().Z + extent.Z); backRight.Z = FMath::Max(backRight.Z, cube->GetComponentLocation().Z - extent.Z);
				endRight.X = GetActorLocation().X;
				endRight.Y = FMath::Min(endRight.Y, cube->GetComponentLocation().Y + extent.Y); endRight.Y = FMath::Max(endRight.Y, cube->GetComponentLocation().Y - extent.Y);
				endRight.Z = FMath::Min(endRight.Z, cube->GetComponentLocation().Z + extent.Z); endRight.Z = FMath::Max(endRight.Z, cube->GetComponentLocation().Z - extent.Z);
			}

			//Add the corners to the vertices
			coverVertices.Add(backLeft); coverVertices.Add(backRight); coverVertices.Add(endLeft); coverVertices.Add(endRight);
		}

		//Create a convex hull from the cover vertices
		TArray<FCartesianInPolar> stack;
		if (coverVertices.Num() > 2) {
			FVector minPoint = coverVertices[0];

			//Find the lowest point (in order Z, Y, X)
			for (FVector vertex : coverVertices) {
				if (vertex.Z < minPoint.Z) { minPoint = vertex; }
				else if (vertex.Z == minPoint.Z && vertex.Y < minPoint.Y) { minPoint = vertex; }
				else if (vertex.Z == minPoint.Z && vertex.Y == minPoint.Y && vertex.X < minPoint.X) { minPoint = vertex; }
			}

			FCartesianInPolar minPolarVertex;

			//Find the polar coordinates from this vertex
			TArray<FCartesianInPolar> polarVertices;
			for (FVector vertex : coverVertices) {
				FCartesianInPolar polarVertex = FCartesianInPolar(vertex, minPoint, getNormal(), absoluteMin);
				if (vertex == minPoint) { minPolarVertex = polarVertex; }
				else { polarVertices.Add(polarVertex); }
			}

			//Sort the polar coordinates
			polarVertices.Sort(FSortByAngle());
			stack.Push(minPolarVertex);

			//Find the convex hull, which will be in the stack
			for (FCartesianInPolar polarVertex : polarVertices) {
				while (stack.Num() > 1 && isCounterClockWise(stack[stack.Num() - 2].relative, stack[stack.Num() - 1].relative, polarVertex.relative)) { stack.Pop(); }
				stack.Push(polarVertex);
			}

			//Turns the 3D vertices into 2D based on the normal of the surface
			TArray<FVector2D> flatVertices;

			//Convert the 3D cartesian coordinates to 2D
			for (FCartesianInPolar polarVertex : stack) {
				if (getNormal().X != 0) { flatVertices.Add(FVector2D(polarVertex.relativeMin.Y, polarVertex.relativeMin.Z)); }
				else if (getNormal().Y != 0) { flatVertices.Add(FVector2D(polarVertex.relativeMin.Z, polarVertex.relativeMin.X)); }
				else if (getNormal().Z != 0) { flatVertices.Add(FVector2D(polarVertex.relativeMin.X, polarVertex.relativeMin.Y)); }
			}

			//If the convex area is somehow larger, then this is a bug. Ensure that the maximum is the size of the area
			float area = calculateConvexArea(flatVertices);
			if (weights.IsValidIndex(c)) {
				if (area - weights[c] * totalSize > 0.5f) {
					UE_LOG(LogTemp, Log, TEXT("Convex area is larger than the total area: cube %s of surface %s. Convex Area: %f; totalArea: %f"), *cube->GetName(), *this->GetName(), area, weights[c] * totalSize);
					area = weights[c] * totalSize;
				}
			}

			if (getNormal().X != 0) {
				for (int32 i = 0; i < flatVertices.Num(); i++) {
					FVector v1Loc = FVector(GetActorLocation().X, flatVertices[i].X, flatVertices[i].Y);
					FVector v2Loc = FVector(GetActorLocation().X, flatVertices[(i+1)%flatVertices.Num()].X, flatVertices[(i + 1) % flatVertices.Num()].Y);

					DrawDebugLine(GetWorld(), v1Loc, v2Loc, FColor::Purple, false, 60, 0, 5);
				}
			}
			else if (getNormal().Y != 0) {
				for (int32 i = 0; i < flatVertices.Num(); i++) {
					FVector v1Loc = FVector(flatVertices[i].Y, GetActorLocation().Y, flatVertices[i].X);
					FVector v2Loc = FVector(flatVertices[(i + 1) % flatVertices.Num()].Y, GetActorLocation().Y, flatVertices[(i + 1) % flatVertices.Num()].X);

					DrawDebugLine(GetWorld(), v1Loc, v2Loc, FColor::Purple, false, 60, 0, 5);
				}
			}
			else if (getNormal().Z != 0) {
				for (int32 i = 0; i < flatVertices.Num(); i++) {
					FVector v1Loc = FVector(flatVertices[i].X, flatVertices[i].Y, GetActorLocation().Z);
					FVector v2Loc = FVector(flatVertices[(i + 1) % flatVertices.Num()].X, flatVertices[(i + 1) % flatVertices.Num()].Y, GetActorLocation().Z);

					DrawDebugLine(GetWorld(), v1Loc, v2Loc, FColor::Purple, false, 60, 0, 5);
				}
			}

			//Add the convex area to the total cover size
			totalCoverSize += area;
		}
	}
	return totalCoverSize;
}

void ASurfaceArea::showArea()
{
	for (UStaticMeshComponent* cube : cubes) { cube->SetVisibility(true); }
	for (UStaticMeshComponent* cube : connectionCubes) { cube->SetVisibility(true); }
}

void ASurfaceArea::hideArea()
{
	for (UStaticMeshComponent* cube : cubes) { cube->SetVisibility(false); }
	for (UStaticMeshComponent* cube : connectionCubes) { cube->SetVisibility(false); }
}

void ASurfaceArea::showConnectionArea()
{
	for (UStaticMeshComponent* cube : connectionCubes) { cube->SetVisibility(true); }
}

void ASurfaceArea::hideConnectionArea()
{
	for (UStaticMeshComponent* cube : connectionCubes) { cube->SetVisibility(false); }
}

void ASurfaceArea::updateMaterials() {
	//Find the right material
	UMaterialInstance* mat = floorMat;
	switch (surface) {
	case ESurfaceType::Floor:
		mat = floorMat;
		break;
	case ESurfaceType::Ceiling:
		mat = ceilingMat;
		break;
	case ESurfaceType::NorthWall:
		mat = northWallMat;
		break;
	case ESurfaceType::SouthWall:
		mat = southWallMat;
		break;
	case ESurfaceType::EastWall:
		mat = eastWallMat;
		break;
	case ESurfaceType::WestWall:
		mat = westWallMat;
		break;
	case ESurfaceType::Stairs:
		mat = stairsMat;
		break;
	case ESurfaceType::StairsCeiling:
		mat = stairsMat;
		break;
	default:
		mat = floorMat;
		break;
	}

	//Apply the material
	for (UStaticMeshComponent* cube : cubes) { cube->SetMaterial(0, mat); }
}

void ASurfaceArea::findNeighbours() {
	//Restart from a clean slate
	neighbours.Empty();

	//Combine the cubes and connection cubes into one array
	TArray<UStaticMeshComponent*> allCubes;
	for (UStaticMeshComponent* cube : cubes) { allCubes.AddUnique(cube); }
	for (UStaticMeshComponent* cube : connectionCubes) { allCubes.AddUnique(cube); }

	//Output array
	TArray<FHitResult> overlaps;
	TArray<FHitResult> overlapsLarger;

	//Actors to ignore, so the surface itself
	TArray<AActor*> ignores;
	ignores.Add(this);

	//Object types to trace
	TArray<TEnumAsByte<EObjectTypeQuery>> traceObjectTypes;
	traceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));

	//Do a box trace in every cube and connection cube
	for (UStaticMeshComponent* cube : allCubes) {
		//Find the extent of the cube. First do a smaller version to disregard finding wrong components of nearby surfaces
		FVector extent = FVector(49 * cube->GetComponentScale().X, 49 * cube->GetComponentScale().Y, 49 * cube->GetComponentScale().Z);
		FVector extentLarger = FVector(49.9f * cube->GetComponentScale().X, 49.9f * cube->GetComponentScale().Y, 49.95f * cube->GetComponentScale().Z);

		//Find all surface areas overlapping the cube
		UKismetSystemLibrary::BoxTraceMultiForObjects(GetWorld(), cube->GetComponentLocation(), cube->GetComponentLocation(), extent, FRotator(0), traceObjectTypes, false, ignores, EDrawDebugTrace::None, overlaps, true, FLinearColor::Green, FLinearColor::Red, 60);
		UKismetSystemLibrary::BoxTraceMultiForObjects(GetWorld(), cube->GetComponentLocation(), cube->GetComponentLocation(), extentLarger, FRotator(0), traceObjectTypes, false, ignores, EDrawDebugTrace::None, overlapsLarger, true, FLinearColor::Green, FLinearColor::Red, 60);

		//Merge the two hit result arrays: the hit of the smaller overlap takes priority
		for (FHitResult overlapLarger : overlapsLarger) {
			bool alreadyExists = false;
			for (FHitResult overlap : overlaps) {
				if (overlap.GetActor() == overlapLarger.GetActor()) { alreadyExists = true; break; }
			}
			if (!alreadyExists) { overlaps.Add(overlapLarger); }
		}

		//Add the overlapping surfaces to the neighbours array
		for (FHitResult overlap : overlaps) {
			if (overlap.GetActor()->GetClass() == ASurfaceArea::StaticClass()) {
				ASurfaceArea* overSurf = (ASurfaceArea*)overlap.GetActor();

				//Only add surfaces that have not been handled by this surface yet, or the same surface with a different connection method if applicable
				if (!isNeighbour(overSurf->id)) {
					int32 neighbourID = overSurf->id;
					//Check how the two surfaces are connected. First assume SBC, then check if it is not

					//List of all variables use in the distinction between cubes and connection cubes
					EConnectionMethod cm = EConnectionMethod::SCB;
					FVector inTNE = FVector(0);
					FVector inBSW = FVector(0);
					UStaticMeshComponent* inCube = nullptr;
					UStaticMeshComponent* inConnectionCube = nullptr;
					FVector tneA = FVector(0);
					FVector tneB = FVector(0);
					FVector bswA = FVector(0);
					FVector bswB = FVector(0);
					TArray<AActor*> ignoresConnection;

					//Find the top north east point of the overlap box
					tneA = cube->GetComponentLocation() + 50 * cube->GetComponentScale();
					tneB = overlap.GetComponent()->GetComponentLocation() + 50 * overlap.GetComponent()->GetComponentScale();
					inTNE = FVector(FMath::Min(tneA.X, tneB.X), FMath::Min(tneA.Y, tneB.Y), FMath::Min(tneA.Z, tneB.Z));

					//Find the bottom south west point of the overlap box
					bswA = cube->GetComponentLocation() - 50 * cube->GetComponentScale();
					bswB = overlap.GetComponent()->GetComponentLocation() - 50 * overlap.GetComponent()->GetComponentScale();
					inBSW = FVector(FMath::Max(bswA.X, bswB.X), FMath::Max(bswA.Y, bswB.Y), FMath::Max(bswA.Z, bswB.Z));

					//In this case, the cube of this Surface Area is a connection cube
					if (connectionCubes.Contains(cube)) {
						//DCB: The cube of the other Surface Area is a connection cube too
						if (overSurf->connectionCubes.Contains(overlap.GetComponent())) {
							cm = EConnectionMethod::DCB;
							inConnectionCube = cube;

							FVector connectionExtent = 1.1 * extent;
							TArray<FHitResult> connectionOverlaps;
							UKismetSystemLibrary::BoxTraceMultiForObjects(GetWorld(), cube->GetComponentLocation(), cube->GetComponentLocation(), connectionExtent, FRotator(0), traceObjectTypes, false, ignoresConnection, EDrawDebugTrace::None, connectionOverlaps, true, FLinearColor::Green, FLinearColor::Red, 60);

							for (FHitResult connectionOverlap : connectionOverlaps) {
								if (connectionOverlap.GetActor() == this) {
									if (cubes.Contains(connectionOverlap.GetComponent())) { inCube = (UStaticMeshComponent*)connectionOverlap.GetComponent(); }
								}
							}
						}

						//SCB: The cube of the other surface is a regular cube
						else if (overSurf->cubes.Contains(overlap.GetComponent())) {
							inConnectionCube = cube;

							FVector connectionExtent = 1.1 * extent;
							TArray<FHitResult> connectionOverlaps;
							UKismetSystemLibrary::BoxTraceMultiForObjects(GetWorld(), cube->GetComponentLocation(), cube->GetComponentLocation(), connectionExtent, FRotator(0), traceObjectTypes, false, ignoresConnection, EDrawDebugTrace::None, connectionOverlaps, true, FLinearColor::Green, FLinearColor::Red, 60);

							for (FHitResult connectionOverlap : connectionOverlaps) {
								if (connectionOverlap.GetActor() == this) {
									if (cubes.Contains(connectionOverlap.GetComponent())) { inCube = (UStaticMeshComponent*)connectionOverlap.GetComponent(); }
								}
							}
						}
					}

					//In this case, the cube of this Surface Area is not a connection cube
					if (cubes.Contains(cube)) {
						//Direct: The cube of the other surface area is a regular cube
						if (overSurf->cubes.Contains(overlap.GetComponent())) {
							//The surfaces are directly connected
							cm = EConnectionMethod::Direct;
							inCube = cube;
						}

						//SCB: The cube of the other surface is a connection cube
						else if (overSurf->connectionCubes.Contains(overlap.GetComponent())) {
							inCube = cube;
							inConnectionCube = (UStaticMeshComponent*)overlap.GetComponent();
						}
					}

					//Create a neighbour struct
					FSurfaceNeighbour neighbour = FSurfaceNeighbour(neighbourID, cm, inCube, inConnectionCube, inTNE, inBSW);
					neighbours.Add(neighbour);
				}
			}
		}
	}
}

void ASurfaceArea::computeMedialAxis()
{
	computePolygon();
	straightSkeleton = findSkeleton();
	//baseDelaunay = computeDelaunayTriangulation();
	//addSteinerPoints();
	//finalTriangulation = medaxifyDelaunay();
}

void ASurfaceArea::generateFloorConnector() {
	//A connector to a floor won't be generated if the current surface is also a floor or ceiling
	//A connector to any wall has 1 possibility, so generate it there
	switch (surface) {
	case ESurfaceType::Floor:
		break;

	case ESurfaceType::Ceiling:
		break;

	case ESurfaceType::NorthWall:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(30, 0, (50 * cubes[0]->GetComponentScale().Z + 25)));
			newCube->SetWorldScale3D(FVector(0.5f, 0.75f * cubes[0]->GetComponentScale().Y, 0.5f));
		}
		break;

	case ESurfaceType::SouthWall:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(-30, 0, (50 * cubes[0]->GetComponentScale().Z + 25)));
			newCube->SetWorldScale3D(FVector(0.5f, 0.75f * cubes[0]->GetComponentScale().Y, 0.5f));
		}
		break;

	case ESurfaceType::EastWall:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(0, 30, (50 * cubes[0]->GetComponentScale().Z + 25)));
			newCube->SetWorldScale3D(FVector(0.75f * cubes[0]->GetComponentScale().X, 0.5f, 0.5f));
		}
		break;

	case ESurfaceType::WestWall:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(0, -30, (50 * cubes[0]->GetComponentScale().Z + 25)));
			newCube->SetWorldScale3D(FVector(0.75f * cubes[0]->GetComponentScale().X, 0.5f, 0.5f));
		}
		break;

	default:
		break;
	}
}

void ASurfaceArea::generateCeilingConnector() {
	//A connector to a ceiling won't be generated if the current surface is also a ceiling or floor
	//A connector to any wall has 1 possibility, so generate it there
	switch (surface) {
	case ESurfaceType::Floor:
		break;

	case ESurfaceType::Ceiling:
		break;

	case ESurfaceType::NorthWall:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(30, 0, -(50 * cubes[0]->GetComponentScale().Z + 25)));
			newCube->SetWorldScale3D(FVector(0.5f, 0.75f * cubes[0]->GetComponentScale().Y, 0.5f));
		}
		break;

	case ESurfaceType::SouthWall:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(-30, 0, -(50 * cubes[0]->GetComponentScale().Z + 25)));
			newCube->SetWorldScale3D(FVector(0.5f, 0.75f * cubes[0]->GetComponentScale().Y, 0.5f));
		}
		break;

	case ESurfaceType::EastWall:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(0, 30, -(50 * cubes[0]->GetComponentScale().Z + 25)));
			newCube->SetWorldScale3D(FVector(0.75f * cubes[0]->GetComponentScale().X, 0.5f, 0.5f));
		}
		break;

	case ESurfaceType::WestWall:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(0, -30, -(50 * cubes[0]->GetComponentScale().Z + 25)));
			newCube->SetWorldScale3D(FVector(0.75f * cubes[0]->GetComponentScale().X, 0.5f, 0.5f));
		}
		break;

	default:
		break;
	}
}

void ASurfaceArea::generateNorthWallConnector() {
	//A connector to a northWall won't be generated if the current surface is also a northWall or southWall
	//A connector to any yWall, floor or ceiling has 1 possibility, so generate it there
	switch (surface) {
	case ESurfaceType::Floor:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(-(50 * cubes[0]->GetComponentScale().X + 25), 0, -30));
			newCube->SetWorldScale3D(FVector(0.5f, 0.75f * cubes[0]->GetComponentScale().Y, 0.5f));
		}
		break;

	case ESurfaceType::Ceiling:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(-(50 * cubes[0]->GetComponentScale().X + 25), 0, 30));
			newCube->SetWorldScale3D(FVector(0.5f, 0.75f * cubes[0]->GetComponentScale().Y, 0.5f));
		}
		break;

	case ESurfaceType::NorthWall:
		break;

	case ESurfaceType::SouthWall:
		break;

	case ESurfaceType::EastWall:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(-(50 * cubes[0]->GetComponentScale().X + 25), 30, 0));
			newCube->SetWorldScale3D(FVector(0.5, 0.5, 0.75f * cubes[0]->GetComponentScale().Z));
		}
		break;

	case ESurfaceType::WestWall:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(-(50 * cubes[0]->GetComponentScale().X + 25), -30, 0));
			newCube->SetWorldScale3D(FVector(0.5, 0.5, 0.75f * cubes[0]->GetComponentScale().Z));
		}
		break;

	default:
		break;
	}
}

void ASurfaceArea::generateSouthWallConnector() {
	//A connector to a southhWall won't be generated if the current surface is also a southWall or northWall
	//A connector to any yWall, floor or ceiling has 1 possibility, so generate it there
	switch (surface) {
	case ESurfaceType::Floor:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector((50 * cubes[0]->GetComponentScale().X + 25), 0, -30));
			newCube->SetWorldScale3D(FVector(0.5f, 0.75f * cubes[0]->GetComponentScale().Y, 0.5f));
		}
		break;

	case ESurfaceType::Ceiling:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector((50 * cubes[0]->GetComponentScale().X + 25), 0, 30));
			newCube->SetWorldScale3D(FVector(0.5f, 0.75f * cubes[0]->GetComponentScale().Y, 0.5f));
		}
		break;

	case ESurfaceType::NorthWall:
		break;

	case ESurfaceType::SouthWall:
		break;

	case ESurfaceType::EastWall:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector((50 * cubes[0]->GetComponentScale().X + 25), 30, 0));
			newCube->SetWorldScale3D(FVector(0.5, 0.5, 0.75f * cubes[0]->GetComponentScale().Z));
		}
		break;

	case ESurfaceType::WestWall:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector((50 * cubes[0]->GetComponentScale().X + 25), -30, 0));
			newCube->SetWorldScale3D(FVector(0.5, 0.5, 0.75f * cubes[0]->GetComponentScale().Z));
		}
		break;

	default:
		break;
	}
}

void ASurfaceArea::generateEastWallConnector() {
	//A connector to a easthWall won't be generated if the current surface is also a eastWall or westWall
	//A connector to any xWall, floor or ceiling has 1 possibility, so generate it there
	switch (surface) {
	case ESurfaceType::Floor:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(0, -(50 * cubes[0]->GetComponentScale().Y + 25), -30));
			newCube->SetWorldScale3D(FVector(0.75f * cubes[0]->GetComponentScale().X, 0.5f, 0.5f));
		}
		break;

	case ESurfaceType::Ceiling:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(0, -(50 * cubes[0]->GetComponentScale().Y + 25), 30));
			newCube->SetWorldScale3D(FVector(0.75f * cubes[0]->GetComponentScale().X, 0.5f, 0.5f));
		}
		break;

	case ESurfaceType::NorthWall:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(30, -(50 * cubes[0]->GetComponentScale().Y + 25), 0));
			newCube->SetWorldScale3D(FVector(0.5, 0.5, 0.75f * cubes[0]->GetComponentScale().Z));
		}
		break;

	case ESurfaceType::SouthWall:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(-30, -(50 * cubes[0]->GetComponentScale().Y + 25), 0));
			newCube->SetWorldScale3D(FVector(0.5, 0.5, 0.75f * cubes[0]->GetComponentScale().Z));
		}
		break;

	case ESurfaceType::EastWall:
		break;

	case ESurfaceType::WestWall:
		break;

	default:
		break;
	}
}

void ASurfaceArea::generateWestWallConnector() {
	//A connector to a easthWall won't be generated if the current surface is also an eastWall or westWall
	//A connector to any xWall, floor or ceiling has 1 possibility, so generate it there
	switch (surface) {
	case ESurfaceType::Floor:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(0, (50 * cubes[0]->GetComponentScale().Y + 25), -30));
			newCube->SetWorldScale3D(FVector(0.75f * cubes[0]->GetComponentScale().X, 0.5f, 0.5f));
		}
		break;

	case ESurfaceType::Ceiling:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(0, (50 * cubes[0]->GetComponentScale().Y + 25), 30));
			newCube->SetWorldScale3D(FVector(0.75f * cubes[0]->GetComponentScale().X, 0.5f, 0.5f));
		}
		break;

	case ESurfaceType::NorthWall:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(30, (50 * cubes[0]->GetComponentScale().Y + 25), 0));
			newCube->SetWorldScale3D(FVector(0.5, 0.5, 0.75f * cubes[0]->GetComponentScale().Z));
		}
		break;

	case ESurfaceType::SouthWall:
		addConnectionBox();
		if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(-30, (50 * cubes[0]->GetComponentScale().Y + 25), 0));
			newCube->SetWorldScale3D(FVector(0.5, 0.5, 0.75f * cubes[0]->GetComponentScale().Z));
		}
		break;

	case ESurfaceType::EastWall:
		addConnectionBox();
		if (connectionCubes.Num() > 0) {
			UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
			newCube->SetRelativeLocation(FVector(0, -30, 0));
		}
		break;

	case ESurfaceType::WestWall:
		break;

	default:
		break;
	}
}

void ASurfaceArea::generateFloorCeilingNorthConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector((50 * cubes[0]->GetComponentScale().X + 5), 0, -55));
		newCube->SetWorldScale3D(FVector(0.1f, 0.75f * cubes[0]->GetComponentScale().Y, 1));
	}
}

void ASurfaceArea::generateFloorCeilingSouthConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(-(50 * cubes[0]->GetComponentScale().X + 5), 0, -55));
		newCube->SetWorldScale3D(FVector(0.1f, 0.75f * cubes[0]->GetComponentScale().Y, 1));
	}
}

void ASurfaceArea::generateFloorCeilingEastConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(0, (50 * cubes[0]->GetComponentScale().Y + 5), -55));
		newCube->SetWorldScale3D(FVector(0.75f * cubes[0]->GetComponentScale().X, 0.1f, 1));
	}
}

void ASurfaceArea::generateFloorCeilingWestConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(0, -(50 * cubes[0]->GetComponentScale().Y + 5), -55));
		newCube->SetWorldScale3D(FVector(0.75f * cubes[0]->GetComponentScale().X, 0.1f, 1));
	}
}

void ASurfaceArea::generateCeilingFloorNorthConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector((50 * cubes[0]->GetComponentScale().X + 5), 0, 55));
		newCube->SetWorldScale3D(FVector(0.1f, 0.75f * cubes[0]->GetComponentScale().Y, 1));
	}
}

void ASurfaceArea::generateCeilingFloorSouthConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(-(50 * cubes[0]->GetComponentScale().X + 5), 0, 55));
		newCube->SetWorldScale3D(FVector(0.1f, 0.75f * cubes[0]->GetComponentScale().Y, 1));
	}
}

void ASurfaceArea::generateCeilingFloorEastConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(0, (50 * cubes[0]->GetComponentScale().Y + 5), 55));
		newCube->SetWorldScale3D(FVector(0.75f * cubes[0]->GetComponentScale().X, 0.1f, 1));
	}
}

void ASurfaceArea::generateCeilingFloorWestConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(0, -(50 * cubes[0]->GetComponentScale().Y + 5), 55));
		newCube->SetWorldScale3D(FVector(0.75f * cubes[0]->GetComponentScale().X, 0.1f, 1));
	}
}

void ASurfaceArea::generateNorthSouthFloorConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(55, 0, -(50 * cubes[0]->GetComponentScale().Z + 5)));
		newCube->SetWorldScale3D(FVector(1, 0.75f * cubes[0]->GetComponentScale().Y, 0.1f));
	}
}

void ASurfaceArea::generateNorthSouthCeilingConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(55, 0, (50 * cubes[0]->GetComponentScale().Z + 5)));
		newCube->SetWorldScale3D(FVector(1, 0.75f * cubes[0]->GetComponentScale().Y, 0.1f));
	}
}

void ASurfaceArea::generateNorthSouthEastConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(55, (50 * cubes[0]->GetComponentScale().Y + 5), 0));
		newCube->SetWorldScale3D(FVector(1, 0.1f, 0.75f * cubes[0]->GetComponentScale().Z));
	}
}

void ASurfaceArea::generateNorthSouthWestConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(55, -(50 * cubes[0]->GetComponentScale().Y + 5), 0));
		newCube->SetWorldScale3D(FVector(1, 0.1f, 0.75f * cubes[0]->GetComponentScale().Z));
	}
}

void ASurfaceArea::generateSouthNorthFloorConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(-55, 0, -(50 * cubes[0]->GetComponentScale().Z + 5)));
		newCube->SetWorldScale3D(FVector(1, 0.75f * cubes[0]->GetComponentScale().Y, 0.1f));
	}
}

void ASurfaceArea::generateSouthNorthCeilingConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(-55, 0, (50 * cubes[0]->GetComponentScale().Z + 5)));
		newCube->SetWorldScale3D(FVector(1, 0.75f * cubes[0]->GetComponentScale().Y, 0.1f));
	}
}

void ASurfaceArea::generateSouthNorthEastConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(-55, (50 * cubes[0]->GetComponentScale().Y + 5), 0));
		newCube->SetWorldScale3D(FVector(1, 0.1f, 0.75f * cubes[0]->GetComponentScale().Z));
	}
}

void ASurfaceArea::generateSouthNorthWestConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(-55, -(50 * cubes[0]->GetComponentScale().Y + 5), 0));
		newCube->SetWorldScale3D(FVector(1, 0.1f, 0.75f * cubes[0]->GetComponentScale().Z));
	}
}

void ASurfaceArea::generateEastWestFloorConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(0, 55, -(50 * cubes[0]->GetComponentScale().Z + 5)));
		newCube->SetWorldScale3D(FVector(0.75f * cubes[0]->GetComponentScale().X, 1, 0.1f));
	}
}

void ASurfaceArea::generateEastWestCeilingConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(0, 55, (50 * cubes[0]->GetComponentScale().Z + 5)));
		newCube->SetWorldScale3D(FVector(0.75f * cubes[0]->GetComponentScale().X, 1, 0.1f));
	}
}

void ASurfaceArea::generateEastWestNorthConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector((50 * cubes[0]->GetComponentScale().X + 5), 55, 0));
		newCube->SetWorldScale3D(FVector(0.1f, 1, 0.75f * cubes[0]->GetComponentScale().Z));
	}
}

void ASurfaceArea::generateEastWestSouthConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(-(50 * cubes[0]->GetComponentScale().X + 5), 55, 0));
		newCube->SetWorldScale3D(FVector(0.1f, 1, 0.75f * cubes[0]->GetComponentScale().Z));
	}
}

void ASurfaceArea::generateWestEastFloorConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(0, -55, -(50 * cubes[0]->GetComponentScale().Z + 5)));
		newCube->SetWorldScale3D(FVector(0.75f * cubes[0]->GetComponentScale().X, 1, 0.1f));
	}
}

void ASurfaceArea::generateWestEastCeilingConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(0, -55, (50 * cubes[0]->GetComponentScale().Z + 5)));
		newCube->SetWorldScale3D(FVector(0.75f * cubes[0]->GetComponentScale().X, 1, 0.1f));
	}
}

void ASurfaceArea::generateWestEastNorthConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector((50 * cubes[0]->GetComponentScale().X + 5), -55, 0));
		newCube->SetWorldScale3D(FVector(0.1f, 1, 0.75f * cubes[0]->GetComponentScale().Z));
	}
}

void ASurfaceArea::generateWestEastSouthConnector() {
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(-(50 * cubes[0]->GetComponentScale().X + 5), -55, 0));
		newCube->SetWorldScale3D(FVector(0.1f, 1, 0.75f * cubes[0]->GetComponentScale().Z));
	}
}

void ASurfaceArea::generateStairNorthConnector()
{
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector((50* cubes[0]->GetComponentScale().X + 12.5), 0, -25));
		newCube->SetWorldScale3D(FVector(0.25f, 0.75f * cubes[0]->GetComponentScale().Y, 0.5f));
	}
}

void ASurfaceArea::generateStairSouthConnector()
{
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(-(50 * cubes[0]->GetComponentScale().X + 12.5), 0, -25));
		newCube->SetWorldScale3D(FVector(0.25f, 0.75f * cubes[0]->GetComponentScale().Y, 0.5f));
	}
}

void ASurfaceArea::generateStairEastConnector()
{
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(0, (50 * cubes[0]->GetComponentScale().Y + 12.5), -25));
		newCube->SetWorldScale3D(FVector(0.75f * cubes[0]->GetComponentScale().X, 0.25f, 0.5f));
	}
}

void ASurfaceArea::generateStairWestConnector()
{
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(0, -(50 * cubes[0]->GetComponentScale().Y + 12.5), -25));
		newCube->SetWorldScale3D(FVector(0.75f * cubes[0]->GetComponentScale().X, 0.25f, 0.5f));
	}
}

void ASurfaceArea::generateStairWallNorthConnector()
{
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(50 * cubes[0]->GetComponentScale().X, 0, -25));
		newCube->SetWorldScale3D(FVector(0.25f, 0.1f, 0.5f * cubes[0]->GetComponentScale().Z));
	}
}

void ASurfaceArea::generateStairWallSouthConnector()
{
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(-50 * cubes[0]->GetComponentScale().X, 0, -25));
		newCube->SetWorldScale3D(FVector(0.25f, 0.1f, 0.5f * cubes[0]->GetComponentScale().Z));
	}
}

void ASurfaceArea::generateStairWallEastConnector()
{
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(0, 50 * cubes[0]->GetComponentScale().Y, -25));
		newCube->SetWorldScale3D(FVector(0.1f, 0.25f, 0.5f * cubes[0]->GetComponentScale().Z));
	}
}

void ASurfaceArea::generateStairWallWestConnector()
{
	addConnectionBox();
	if (connectionCubes.Num() > 0 && cubes.Num() > 0) {
		UStaticMeshComponent* newCube = connectionCubes[connectionCubes.Num() - 1];
		newCube->SetRelativeLocation(FVector(0, -50 * cubes[0]->GetComponentScale().Y, -25));
		newCube->SetWorldScale3D(FVector(0.1f, 0.25f, 0.5f * cubes[0]->GetComponentScale().Z));
	}
}

void ASurfaceArea::drawPolygon()
{
	for (int32 i = 0; i < polygon.vertices3D.Num(); i++) {
		FVector traceStart = polygon.vertices3D[i];
		FVector traceEnd = traceStart;

		if (i == polygon.vertices3D.Num() - 1) { traceEnd = polygon.vertices3D[0]; }
		else { traceEnd = polygon.vertices3D[i + 1]; }

		DrawDebugLine(GetWorld(), traceStart, traceEnd, FColor::Blue, false, 60, 0, 6);
	}

	/*for (FDTriangle triangle : baseDelaunay) {
		FVector a = GetActorLocation();
		FVector b = GetActorLocation();
		FVector c = GetActorLocation();
		switch (surface) {
		case ESurfaceType::Ceiling:
			a.X = triangle.vertices[0].X; a.Y = triangle.vertices[0].Y;
			b.X = triangle.vertices[1].X; b.Y = triangle.vertices[1].Y;
			c.X = triangle.vertices[2].X; c.Y = triangle.vertices[2].Y;
			break;
		case ESurfaceType::Floor:
			a.X = triangle.vertices[0].X; a.Y = triangle.vertices[0].Y;
			b.X = triangle.vertices[1].X; b.Y = triangle.vertices[1].Y;
			c.X = triangle.vertices[2].X; c.Y = triangle.vertices[2].Y;
			break;
		case ESurfaceType::Stairs:
			a.X = triangle.vertices[0].X; a.Y = triangle.vertices[0].Y;
			b.X = triangle.vertices[1].X; b.Y = triangle.vertices[1].Y;
			c.X = triangle.vertices[2].X; c.Y = triangle.vertices[2].Y;
			break;
		case ESurfaceType::StairsCeiling:
			a.X = triangle.vertices[0].X; a.Y = triangle.vertices[0].Y;
			b.X = triangle.vertices[1].X; b.Y = triangle.vertices[1].Y;
			c.X = triangle.vertices[2].X; c.Y = triangle.vertices[2].Y;
			break;
		case ESurfaceType::NorthWall:
			a.Z = triangle.vertices[0].X; a.Y = triangle.vertices[0].Y;
			b.Z = triangle.vertices[1].X; b.Y = triangle.vertices[1].Y;
			c.Z = triangle.vertices[2].X; c.Y = triangle.vertices[2].Y;
			break;
		case ESurfaceType::SouthWall:
			a.Z = triangle.vertices[0].X; a.Y = triangle.vertices[0].Y;
			b.Z = triangle.vertices[1].X; b.Y = triangle.vertices[1].Y;
			c.Z = triangle.vertices[2].X; c.Y = triangle.vertices[2].Y;
			break;
		case ESurfaceType::EastWall:
			a.X = triangle.vertices[0].X; a.Z = triangle.vertices[0].Y;
			b.X = triangle.vertices[1].X; b.Z = triangle.vertices[1].Y;
			c.X = triangle.vertices[2].X; c.Z = triangle.vertices[2].Y;
			break;
		case ESurfaceType::WestWall :
			a.X = triangle.vertices[0].X; a.Z = triangle.vertices[0].Y;
			b.X = triangle.vertices[1].X; b.Z = triangle.vertices[1].Y;
			c.X = triangle.vertices[2].X; c.Z = triangle.vertices[2].Y;
			break;
		}
		DrawDebugLine(GetWorld(), a, b, FColor::Red, false, 60, 0, 6);
		DrawDebugLine(GetWorld(), b, c, FColor::Red, false, 60, 0, 6);
		DrawDebugLine(GetWorld(), c, a, FColor::Red, false, 60, 0, 6);
	}*/
}

void ASurfaceArea::drawPolygonTriangulation()
{
	for (FPolygonEdge3D edge : straightSkeleton) { DrawDebugLine(GetWorld(), edge.vA, edge.vB, FColor::Red, false, 15, 0, 1); }
}

AVertex* ASurfaceArea::generateVertex(int32 vertexID, float agentSize, bool showVertex) {
	//Vertex that will inevitably be placed
	AVertex* returnValue;

	//Random cube to place a vertex in
	UStaticMeshComponent* selectedCube = getRandomCube();

	//If a cube has been selected, continue
	if (selectedCube) {
		//Find the location to spawn a vertex
		FVector spawnLocation = getRandomSpawnLocation(selectedCube);

		//Handle obstacles
		if (!isInObstacle(spawnLocation, agentSize) && !isInOtherSurface(spawnLocation)) {
			//Now create the vertex. Handle the rest of the vertex creation later
			returnValue = GetWorld()->SpawnActor<AVertex>(spawnLocation, FRotator(0), FActorSpawnParameters());
			returnValue->id = vertexID;
			returnValue->surface = this->surface;

			if (showVertex) { returnValue->showVertex(); }
			else { returnValue->hideVertex(); }

			return returnValue;
		}

		return nullptr;
	}

	// No cube was selected. This should not happen. Return null pointer instead.
	return nullptr;
}

AVertex * ASurfaceArea::generateVertexInSubsurface(int32 vertexID, float agentSize, int32 subsurfaceIndex, bool showVertex)
{
	//Vertex that will inevitably be placed
	AVertex* returnValue = nullptr;

	if (cubes.IsValidIndex(subsurfaceIndex)) {
		//The cube that a vertex will be generating in
		UStaticMeshComponent* selectedCube = cubes[subsurfaceIndex];

		//Find the location to spawn a vertex
		FVector spawnLocation = getRandomSpawnLocation(selectedCube);

		//Handle obstacles
		if (!isInObstacle(spawnLocation, agentSize) && !isInOtherSurface(spawnLocation)) {
			//Now create the vertex. Handle the rest of the vertex creation later
			returnValue = GetWorld()->SpawnActor<AVertex>(spawnLocation, FRotator(0), FActorSpawnParameters());
			returnValue->id = vertexID;
			returnValue->surface = this->surface;

			if (showVertex) { returnValue->showVertex(); }
			else { returnValue->hideVertex(); }
		}
	}

	return returnValue;
}

AVertex* ASurfaceArea::generateVertexAtLocation(int32 vertexID, float agentSize, FVector location, bool showVertex) {
	//Vertex that will inevitably be placed
	AVertex* returnValue;

	//Handle obstacles
	if (!isInObstaclePure(location, agentSize)) {
		//Now create the vertex. Handle the rest of the vertex creation later
		returnValue = GetWorld()->SpawnActor<AVertex>(location, FRotator(0), FActorSpawnParameters());
		returnValue->id = vertexID;
		returnValue->surface = this->surface;

		if (showVertex) { returnValue->showVertex(); }
		else { returnValue->hideVertex(); }

		return returnValue;
	}

	// No cube was selected. This should not happen. Return null pointer instead.
	return nullptr;
}

AVertex * ASurfaceArea::generateVertexInRange(int32 vertexID, float agentSize, FVector tne, FVector bsw, bool showVertex)
{
	//Vertex that will inevitably be placed
	AVertex* returnValue;

	//Get a random location between tne and bsw
	FVector location = FVector(FMath::FRandRange(bsw.X, tne.X), FMath::FRandRange(bsw.Y, tne.Y), FMath::FRandRange(bsw.Z, tne.Z));
	switch (surface) {
	case ESurfaceType::Floor:
		location.Z = (tne.Z + bsw.Z) / 2;
		break;
	case ESurfaceType::Ceiling:
		location.Z = (tne.Z + bsw.Z) / 2;
		break;
	case ESurfaceType::NorthWall:
		location.X = (tne.X + bsw.X) / 2;
		break;
	case ESurfaceType::SouthWall:
		location.X = (tne.X + bsw.X) / 2;
		break;
	case ESurfaceType::EastWall:
		location.Y = (tne.Y + bsw.Y) / 2;
		break;
	case ESurfaceType::WestWall:
		location.Y = (tne.Y + bsw.Y) / 2;
		break;
	case ESurfaceType::Stairs:
		location.Z = (tne.Z + bsw.Z) / 2;
		break;
	case ESurfaceType::StairsCeiling:
		location.Z = (tne.Z + bsw.Z) / 2;
		break;
	default:
		break;
	}

	//Handle obstacles
	if (!isInObstaclePure(location, agentSize)) {
		//Now create the vertex. Handle the rest of the vertex creation later
		returnValue = GetWorld()->SpawnActor<AVertex>(location, FRotator(0), FActorSpawnParameters());
		returnValue->id = vertexID;
		returnValue->surface = this->surface;

		if (showVertex) { returnValue->showVertex(); }
		else { returnValue->hideVertex(); }

		return returnValue;
	}

	// No cube was selected. This should not happen. Return null pointer instead.
	return nullptr;
}

AHelperVertex* ASurfaceArea::generateHelperVertex(int32 vertexID, FVector location, ESurfaceType surfaceType, float agentSize, bool showVertex, AVertex* inGoalA, AVertex* inGoalB) {
	//Vertex that will inevitably be placed
	AHelperVertex* returnValue;

	//Handle obstacles
	if (!isInObstaclePure(location, agentSize)) {
		//Now create the vertex. Handle the rest of the vertex creation later
		returnValue = GetWorld()->SpawnActor<AHelperVertex>(location, FRotator(0), FActorSpawnParameters());
		if (returnValue == nullptr) { return nullptr; }

		//A vertex was generated, so continue
		returnValue->id = vertexID;
		returnValue->surface = surfaceType;
		returnValue->goalA = inGoalA;
		returnValue->goalB = inGoalB;

		if (showVertex) { returnValue->showVertex(); }
		else { returnValue->hideVertex(); }

		return returnValue;
	}

	// No cube was selected. This should not happen. Return null pointer instead.
	return nullptr;
}

UStaticMeshComponent * ASurfaceArea::getRandomCube()
{
	//The cube that a vertex will be generating in
	UStaticMeshComponent* returnValue = nullptr;

	//Find a random number between 0 and 1
	float random = FMath::FRandRange(0, 1);

	//Create an array with the sum of the weights per cube, so cube A has weight a, B has a + b, etc. Last cube has a + b + ... = 1.
	TArray<float> sumOfWeights;

	for (int i = 0; i < weights.Num(); i++) {
		float sumWeight = weights[i];

		//Except for the first item, add the weight of the previous sum
		if (i > 0) { sumWeight += sumOfWeights[i - 1]; }
		sumOfWeights.Add(sumWeight);
	}

	//Then select the lowest index j such that random <= weight at j
	for (int j = 0; j < sumOfWeights.Num(); j++) {
		if (random <= sumOfWeights[j]) {
			returnValue = cubes[j];
			break;
		}
	}

	return returnValue;
}

FVector ASurfaceArea::getRandomSpawnLocation(UStaticMeshComponent* cube) {
	FVector returnValue = cube->GetComponentLocation();
	float rx;
	float ry;
	float rz;

	switch (surface) {
	case ESurfaceType::Floor: //Find random X and Y values in range of the cube to add to the component location
		rx = FMath::FRandRange(-50 * cube->GetComponentScale().X + 9, 50 * cube->GetComponentScale().X - 9);
		ry = FMath::FRandRange(-50 * cube->GetComponentScale().Y + 9, 50 * cube->GetComponentScale().Y - 9);
		returnValue.X += rx;
		returnValue.Y += ry;
		return returnValue;
		
	case ESurfaceType::Ceiling://Find random X and Y values in range of the cube to add to the component location
		rx = FMath::FRandRange(-50 * cube->GetComponentScale().X + 9, 50 * cube->GetComponentScale().X - 9);
		ry = FMath::FRandRange(-50 * cube->GetComponentScale().Y + 9, 50 * cube->GetComponentScale().Y - 9);
		returnValue.X += rx;
		returnValue.Y += ry;
		return returnValue;
		
	case ESurfaceType::NorthWall://Find random Z and Y values in range of the cube to add to the component location
		rz = FMath::FRandRange(-50 * cube->GetComponentScale().Z + 9, 50 * cube->GetComponentScale().Z - 9);
		ry = FMath::FRandRange(-50 * cube->GetComponentScale().Y + 9, 50 * cube->GetComponentScale().Y - 9);
		returnValue.Z += rz;
		returnValue.Y += ry;
		return returnValue;
		
	case ESurfaceType::SouthWall://Find random Z and Y values in range of the cube to add to the component location
		rz = FMath::FRandRange(-50 * cube->GetComponentScale().Z + 9, 50 * cube->GetComponentScale().Z - 9);
		ry = FMath::FRandRange(-50 * cube->GetComponentScale().Y + 9, 50 * cube->GetComponentScale().Y - 9);
		returnValue.Z += rz;
		returnValue.Y += ry;
		return returnValue;
		
	case ESurfaceType::EastWall://Find random X and Z values in range of the cube to add to the component location
		rx = FMath::FRandRange(-50 * cube->GetComponentScale().X + 9, 50 * cube->GetComponentScale().X - 9);
		rz = FMath::FRandRange(-50 * cube->GetComponentScale().Z + 9, 50 * cube->GetComponentScale().Z - 9);
		returnValue.X += rx;
		returnValue.Z += rz;
		return returnValue;
		
	case ESurfaceType::WestWall://Find random X and Z values in range of the cube to add to the component location
		rx = FMath::FRandRange(-50 * cube->GetComponentScale().X + 9, 50 * cube->GetComponentScale().X - 9);
		rz = FMath::FRandRange(-50 * cube->GetComponentScale().Z + 9, 50 * cube->GetComponentScale().Z - 9);
		returnValue.X += rx;
		returnValue.Z += rz;
		return returnValue;
		
	case ESurfaceType::Stairs://Find random X and Y values in range of the cube to add to the component location
		rx = FMath::FRandRange(-50 * cube->GetComponentScale().X + 9, 50 * cube->GetComponentScale().X - 9);
		ry = FMath::FRandRange(-50 * cube->GetComponentScale().Y + 9, 50 * cube->GetComponentScale().Y - 9);
		returnValue.X += rx;
		returnValue.Y += ry;
		return returnValue;
		
	case ESurfaceType::StairsCeiling://Find random X and Y values in range of the cube to add to the component location
		rx = FMath::FRandRange(-50 * cube->GetComponentScale().X + 9, 50 * cube->GetComponentScale().X - 9);
		ry = FMath::FRandRange(-50 * cube->GetComponentScale().Y + 9, 50 * cube->GetComponentScale().Y - 9);
		returnValue.X += rx;
		returnValue.Y += ry;
		return returnValue;
		
	default:
		return returnValue;
	}
}

FVector ASurfaceArea::getRandomSkeletonLocation()
{
	//The cube that a vertex will be generating in
	FVector returnValue = FVector();

	//Find a random number between 0 and 1
	float random = FMath::FRandRange(0, 1);
	float random2 = FMath::FRandRange(0, 1);

	//Create an array with the sum of the weights per cube, so cube A has weight a, B has a + b, etc. Last cube has a + b + ... = 1.
	TArray<float> sumOfWeights;

	for (int i = 0; i < straightSkeleton.Num(); i++) {
		float sumWeight = (straightSkeleton[i].vA - straightSkeleton[i].vB).Size() / skeletonSize;

		//Except for the first item, add the weight of the previous sum
		if (i > 0) { sumWeight += sumOfWeights[i - 1]; }
		sumOfWeights.Add(sumWeight);
	}

	//Then select the lowest index j such that random <= weight at j
	for (int j = 0; j < sumOfWeights.Num(); j++) {
		if (random <= sumOfWeights[j]) {
			returnValue = (1 - random2) * straightSkeleton[j].vA + random2 * straightSkeleton[j].vB;
			break;
		}
	}

	return returnValue;
}

FVector ASurfaceArea::getRandomSpawnLocationInBox(UStaticMeshComponent* cube, FVector tne, FVector bsw) {
	FVector returnValue = (tne + bsw) / 2;
	float rx;
	float ry;
	float rz;

	switch (surface) {
	case ESurfaceType::Floor: //Find random X and Y values in range of the cube to add to the component location
		rx = FMath::FRandRange(-49 * cube->GetComponentScale().X, 49 * cube->GetComponentScale().X);
		ry = FMath::FRandRange(-49 * cube->GetComponentScale().Y, 49 * cube->GetComponentScale().Y);
		returnValue.X += rx;
		returnValue.Y += ry;
		return returnValue;

	case ESurfaceType::Ceiling://Find random X and Y values in range of the cube to add to the component location
		rx = FMath::FRandRange(-49 * cube->GetComponentScale().X, 49 * cube->GetComponentScale().X);
		ry = FMath::FRandRange(-49 * cube->GetComponentScale().Y, 49 * cube->GetComponentScale().Y);
		returnValue.X += rx;
		returnValue.Y += ry;
		return returnValue;

	case ESurfaceType::NorthWall://Find random Z and Y values in range of the cube to add to the component location
		rz = FMath::FRandRange(-49 * cube->GetComponentScale().Z, 49 * cube->GetComponentScale().Z);
		ry = FMath::FRandRange(-49 * cube->GetComponentScale().Y, 49 * cube->GetComponentScale().Y);
		returnValue.Z += rz;
		returnValue.Y += ry;
		return returnValue;

	case ESurfaceType::SouthWall://Find random Z and Y values in range of the cube to add to the component location
		rz = FMath::FRandRange(-49 * cube->GetComponentScale().Z, 49 * cube->GetComponentScale().Z);
		ry = FMath::FRandRange(-49 * cube->GetComponentScale().Y, 49 * cube->GetComponentScale().Y);
		returnValue.Z += rz;
		returnValue.Y += ry;
		return returnValue;

	case ESurfaceType::EastWall://Find random X and Z values in range of the cube to add to the component location
		rx = FMath::FRandRange(-49 * cube->GetComponentScale().X, 49 * cube->GetComponentScale().X);
		rz = FMath::FRandRange(-49 * cube->GetComponentScale().Z, 49 * cube->GetComponentScale().Z);
		returnValue.X += rx;
		returnValue.Z += rz;
		return returnValue;

	case ESurfaceType::WestWall://Find random X and Z values in range of the cube to add to the component location
		rx = FMath::FRandRange(-49 * cube->GetComponentScale().X, 49 * cube->GetComponentScale().X);
		rz = FMath::FRandRange(-49 * cube->GetComponentScale().Z, 49 * cube->GetComponentScale().Z);
		returnValue.X += rx;
		returnValue.Z += rz;
		return returnValue;

	case ESurfaceType::Stairs://Find random X and Y values in range of the cube to add to the component location
		rx = FMath::FRandRange(-49 * cube->GetComponentScale().X, 49 * cube->GetComponentScale().X);
		ry = FMath::FRandRange(-49 * cube->GetComponentScale().Y, 49 * cube->GetComponentScale().Y);
		returnValue.X += rx;
		returnValue.Y += ry;
		return returnValue;

	case ESurfaceType::StairsCeiling://Find random X and Y values in range of the cube to add to the component location
		rx = FMath::FRandRange(-49 * cube->GetComponentScale().X, 49 * cube->GetComponentScale().X);
		ry = FMath::FRandRange(-49 * cube->GetComponentScale().Y, 49 * cube->GetComponentScale().Y);
		returnValue.X += rx;
		returnValue.Y += ry;
		return returnValue;

	default:
		return returnValue;
	}
}

bool ASurfaceArea::isInObstacle(FVector location, float agentSize) {
	//Overlapped obstacles
	TArray<AActor*> overlaps;
	FHitResult hit;

	//Actors to ignore, so the surface itself
	TArray<AActor*> ignores;
	ignores.Add(this);

	//Object types to trace
	TArray<TEnumAsByte<EObjectTypeQuery>> traceObjectTypes;
	traceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));
	
	//Set the extent of the vertex in the directions parallel to the surface
	FVector extent = FVector(agentSize); 
	switch (surface) {
	case ESurfaceType::Floor:
		extent.Z = 1;
		break;
	case ESurfaceType::Ceiling:
		extent.Z = 1;
		break;
	case ESurfaceType::NorthWall:
		extent.X = 1;
		break;
	case ESurfaceType::SouthWall:
		extent.X = 1;
		break;
	case ESurfaceType::EastWall:
		extent.Y = 1;
		break;
	case ESurfaceType::WestWall:
		extent.Y = 1;
		break;
	case ESurfaceType::Stairs:
		extent.Z = 1;
		break;
	case ESurfaceType::StairsCeiling:
		extent.Z = 1;
		break;
	default:
		break;
	}

	//Check if an obstacle or room is in the location to generate a vertex
	bool obstacles = UKismetSystemLibrary::BoxOverlapActors(GetWorld(), location, extent, traceObjectTypes, AObstacle::StaticClass(), ignores, overlaps);
	bool rooms = UKismetSystemLibrary::BoxTraceSingleByProfile(GetWorld(), location, location, extent, FRotator(0), "EdgeTraceBlocker", false, ignores, EDrawDebugTrace::None, hit, true, FLinearColor::Yellow, FLinearColor::White, 60);

	//Return whether any rooms or obstacles were found
	return obstacles || rooms;
}

bool ASurfaceArea::isInObstaclePure(FVector location, float agentSize) {
	//Overlapped obstacles
	TArray<AActor*> overlaps;
	FHitResult hit;

	//Actors to ignore, so the surface itself
	TArray<AActor*> ignores;
	ignores.Add(this);

	//Object types to trace
	TArray<TEnumAsByte<EObjectTypeQuery>> traceObjectTypes;
	traceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));

	//Set the extent of the vertex in the directions parallel to the surface
	FVector extent = FVector(agentSize);
	switch (surface) {
	case ESurfaceType::Floor:
		extent.Z = 1;
		break;
	case ESurfaceType::Ceiling:
		extent.Z = 1;
		break;
	case ESurfaceType::NorthWall:
		extent.X = 1;
		break;
	case ESurfaceType::SouthWall:
		extent.X = 1;
		break;
	case ESurfaceType::EastWall:
		extent.Y = 1;
		break;
	case ESurfaceType::WestWall:
		extent.Y = 1;
		break;
	case ESurfaceType::Stairs:
		extent.Z = 1;
		break;
	case ESurfaceType::StairsCeiling:
		extent.Z = 1;
		break;
	default:
		break;
	}

	//Check if an obstacle is in the location to generate a vertex
	return UKismetSystemLibrary::BoxOverlapActors(GetWorld(), location, extent, traceObjectTypes, AObstacle::StaticClass(), ignores, overlaps);
}

bool ASurfaceArea::isInOtherSurface(FVector location) {
	//Overlapped obstacles
	TArray<AActor*> overlaps;

	//Actors to ignore, so the surface itself
	TArray<AActor*> ignores;
	ignores.Add(this);

	//Object types to trace
	TArray<TEnumAsByte<EObjectTypeQuery>> traceObjectTypes;
	traceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));

	//Set the extent of the vertex
	FVector extent = FVector(5);

	//Find all surface areas overlapping the cube. Return whether any were found
	return UKismetSystemLibrary::BoxOverlapActors(GetWorld(), location, extent, traceObjectTypes, ASurfaceArea::StaticClass(), ignores, overlaps);
}

bool ASurfaceArea::isNeighbour(int32 neighbourID) {
	for (FSurfaceNeighbour neighbour : neighbours) {
		if (neighbour.neighbourID == neighbourID) { return true; }
	}
	return false;
}

FSurfaceNeighbour ASurfaceArea::getNeighbourFromID(int32 neighbourID) {
	for (FSurfaceNeighbour neighbour : neighbours) {
		if (neighbour.neighbourID == neighbourID) { return neighbour; }
	}
	return FSurfaceNeighbour(-1, EConnectionMethod::Direct, nullptr, nullptr, FVector(0), FVector(0));
}

FVector ASurfaceArea::getNormal() {
	switch (surface) {
	case ESurfaceType::Floor:
		return FVector(0, 0, 1);
		break;
	case ESurfaceType::Ceiling:
		return FVector(0, 0, -1);
		break;
	case ESurfaceType::NorthWall:
		return FVector(-1, 0, 0);
		break;
	case ESurfaceType::SouthWall:
		return FVector(1, 0, 0);
		break;
	case ESurfaceType::EastWall:
		return FVector(0, -1, 0);
		break;
	case ESurfaceType::WestWall:
		return FVector(0, 1, 0);
		break;
	case ESurfaceType::Stairs:
		return FVector(0, 0, 1);
		break;
	case ESurfaceType::StairsCeiling:
		return FVector(0, 0, -1);
		break;
	default:
		return FVector(0, 0, 0);
		break;
	}
}

bool ASurfaceArea::isCounterClockWise(FVector A, FVector B, FVector C)
{
	//Find the normal vector based on the directions of A to B and A to C
	FVector normal = FVector::CrossProduct((B - A), (C - A));
	if (normal == FVector(0)) { return true; }

	//If the calculated normal is perpendicular to the normal of the surface, then the the points cannot be counter clockwise on a single surface
	if (FVector::DotProduct(getNormal(), normal) >= 0) { return true; }

	return false;
}

float ASurfaceArea::calculateConvexArea(TArray<FVector2D> flatVertices)
{
	float returnValue = 0;
	for (int i = 0; i < flatVertices.Num(); i++) {
		//Get the correct indices
		int j = i - 1; if (j == -1) { j = flatVertices.Num() - 1; }
		int k = i + 1; if (k == flatVertices.Num()) { k = 0; }
		
		//Shoelace formula: Area = 1/2 * sum_{i=1}^n (x_i * (y_{i+1} - y_{i-1})). Divide by 2500 to account for the size in the world (50)
		float XDiff = (flatVertices[i].X - flatVertices[k].X) / 100.0f;
		float YDiff = (flatVertices[i].Y + flatVertices[k].Y) / 100.0f;

		//If the normal is negative in a direction, the area must be removed instead of added!
		if (getNormal().X < 0 || getNormal().Y < 0 || getNormal().Z < 0) { returnValue -= XDiff * YDiff / -2; }
		else { returnValue += XDiff * YDiff / -2; }
	}

	return returnValue;
}

void ASurfaceArea::computePolygon() {
	//Arrays to keep track of all edges and the candidates for the polygon. Vertices are added at the end based on the edges.
	TArray<FVector> polygonVertices;

	TArray<FPolygonEdge> allEdges;
	TArray<FPolygonEdge> candidateEdges;

	//Incrementally add cubes of the surface to find the polygon's edges
	for (UStaticMeshComponent* cube : cubes) {
		//Find the 4 vertices of the subsurface
		FVector v1 = cube->GetComponentLocation() + 50 * cube->GetComponentScale();
		FVector v2 = cube->GetComponentLocation() - 50 * cube->GetComponentScale();

		FVector2D fV1 = FVector2D(0);
		FVector2D fV2 = FVector2D(0);
		FVector2D fV3 = FVector2D(0);
		FVector2D fV4 = FVector2D(0);

		switch (surface) {
		case ESurfaceType::Ceiling:
			fV1 = FVector2D(v1.X, v1.Y);
			fV2 = FVector2D(v1.X, v2.Y);
			fV3 = FVector2D(v2.X, v1.Y);
			fV4 = FVector2D(v2.X, v2.Y);
			break;
		case ESurfaceType::Floor:
			fV1 = FVector2D(v1.X, v1.Y);
			fV2 = FVector2D(v1.X, v2.Y);
			fV3 = FVector2D(v2.X, v1.Y);
			fV4 = FVector2D(v2.X, v2.Y);
			break;
		case ESurfaceType::Stairs:
			fV1 = FVector2D(v1.X, v1.Y);
			fV2 = FVector2D(v1.X, v2.Y);
			fV3 = FVector2D(v2.X, v1.Y);
			fV4 = FVector2D(v2.X, v2.Y);
			break;
		case ESurfaceType::StairsCeiling:
			fV1 = FVector2D(v1.X, v1.Y);
			fV2 = FVector2D(v1.X, v2.Y);
			fV3 = FVector2D(v2.X, v1.Y);
			fV4 = FVector2D(v2.X, v2.Y);
			break;
		case ESurfaceType::NorthWall:
			fV1 = FVector2D(v1.Z, v1.Y);
			fV2 = FVector2D(v1.Z, v2.Y);
			fV3 = FVector2D(v2.Z, v1.Y);
			fV4 = FVector2D(v2.Z, v2.Y);
			break;
		case ESurfaceType::SouthWall:
			fV1 = FVector2D(v1.Z, v1.Y);
			fV2 = FVector2D(v1.Z, v2.Y);
			fV3 = FVector2D(v2.Z, v1.Y);
			fV4 = FVector2D(v2.Z, v2.Y);
			break;
		case ESurfaceType::EastWall:
			fV1 = FVector2D(v1.X, v1.Z);
			fV2 = FVector2D(v1.X, v2.Z);
			fV3 = FVector2D(v2.X, v1.Z);
			fV4 = FVector2D(v2.X, v2.Z);
			break;
		case ESurfaceType::WestWall:
			fV1 = FVector2D(v1.X, v1.Z);
			fV2 = FVector2D(v1.X, v2.Z);
			fV3 = FVector2D(v2.X, v1.Z);
			fV4 = FVector2D(v2.X, v2.Z);
			break;
		default:
			break;
		}

		//Find the corresponding edges
		FPolygonEdge e13 = FPolygonEdge(fV1, fV3, true);
		FPolygonEdge e23 = FPolygonEdge(fV3, fV4, true);
		FPolygonEdge e14 = FPolygonEdge(fV4, fV2, true);
		FPolygonEdge e24 = FPolygonEdge(fV2, fV1, true); 
		
		allEdges.Add(e13); allEdges.Add(e23); allEdges.Add(e14); allEdges.Add(e24);

		//Extra array to allow removal and addition to candidateEdges throughout the loop
		TArray<FPolygonEdge> tempEdges = candidateEdges;

		//If there are no candidate edges, the edges can safely be added
		if (candidateEdges.Num() == 0) { candidateEdges.Add(e13); candidateEdges.Add(e14); candidateEdges.Add(e23); candidateEdges.Add(e24); }

		//If there are candidate edges already present, we should handle overlaps
		else {
			//Now check if the edges are candidates for the polygon or whether they need to be adjusted
			bool hasSplit = false;
			//UE_LOG(LogTemp, Log, TEXT("Handle edge: %s to %s"), *fV1.ToString(), *fV3.ToString());
			for (FPolygonEdge edge : tempEdges) { fixEdgeOverlap(edge, e13, candidateEdges, candidateEdges, hasSplit, hasSplit); }
			if (!hasSplit) { candidateEdges.Add(e13); }
			hasSplit = false;
			//UE_LOG(LogTemp, Log, TEXT("Handle edge: %s to %s"), *fV1.ToString(), *fV4.ToString());
			for (FPolygonEdge edge : tempEdges) { fixEdgeOverlap(edge, e14, candidateEdges, candidateEdges, hasSplit, hasSplit); }
			if (!hasSplit) { candidateEdges.Add(e14); }
			hasSplit = false;
			//UE_LOG(LogTemp, Log, TEXT("Handle edge: %s to %s"), *fV2.ToString(), *fV3.ToString());
			for (FPolygonEdge edge : tempEdges) { fixEdgeOverlap(edge, e23, candidateEdges, candidateEdges, hasSplit, hasSplit); }
			if (!hasSplit) { candidateEdges.Add(e23); }
			hasSplit = false;
			//UE_LOG(LogTemp, Log, TEXT("Handle edge: %s to %s"), *fV2.ToString(), *fV4.ToString());
			for (FPolygonEdge edge : tempEdges) { fixEdgeOverlap(edge, e24, candidateEdges, candidateEdges, hasSplit, hasSplit); }
			if (!hasSplit) { candidateEdges.Add(e24); }
		}
	}

	//Now create the polygon based on its vertices and edges, ordered from the tne and clockwise
	//First find the tne (in 2D)
	FVector2D tne2D = FVector2D(0);
	if (candidateEdges.Num() > 0) { tne2D = candidateEdges[0].vA; }

	for (FPolygonEdge edge : candidateEdges) {
		if (edge.vA.Y > tne2D.Y) { tne2D = edge.vA; }
		else if (edge.vA.X == tne2D.X && tne2D.X > edge.vA.X) { tne2D = edge.vA; }
		if (edge.vB.Y > tne2D.Y) { tne2D = edge.vB; }
		else if (edge.vB.X == tne2D.X && tne2D.X > edge.vB.X) { tne2D = edge.vB; }
	}

	//Now create a list of vertices starting at tne2D and ending there too by following the candidate edges
	TArray<FVector2D> orderedVertices2D;
	orderedVertices2D.AddUnique(tne2D);
	
	//To prevent UE4 from thinking we'll have an infinite loop
	int32 tries = 0;

	//The last added vertex
	FVector2D currentVertex = tne2D;

	//Remove any edges with vertices that only have one end.
	//Continue this until no dead ends exist, or until an arbitrary value to prevent infinite loops
	bool removedEdge = true;

	while (removedEdge && tries < 100) {
		TArray<FPolygonEdge> tempEdges = candidateEdges;
		removedEdge = false;
		for (FPolygonEdge edge : tempEdges) {
			int32 edgeCountA = 0;
			int32 edgeCountB = 0;

			for (FPolygonEdge edge2 : tempEdges) { if (edge2.hasVertex(edge.vA)) { edgeCountA++; } }
			for (FPolygonEdge edge2 : tempEdges) { if (edge2.hasVertex(edge.vB)) { edgeCountB++; } }

			if (edgeCountA < 2 || edgeCountB < 2) { candidateEdges.Remove(edge);  removedEdge = true; }
		}
		tries++;
	}

	//Reset tries for another infinite loop prevention
	tries = 0;

	//Go over all edges until a complete polygon is formed
	while (tries < 100) {
		for (FPolygonEdge edge : candidateEdges) {
			//Case a: vA is the current vertex
			if (edge.vA == currentVertex) {
				//Case a1: The current vertex is also tne
				if (currentVertex == tne2D) {
					//Case a1I: The current vertex is the first vertex in the polygon
					if (orderedVertices2D.Num() == 1) { orderedVertices2D.AddUnique(edge.vB); currentVertex = edge.vB; }
					//Case a1II: The current vertex is not the first vertex, but then current != tne, so this case doesn't exist
				}
				//Case a2: The current vertex is not the tne
				else {
					//Case a2I: The other vertex is not in the orderedVertices set
					if (!orderedVertices2D.Contains(edge.vB)) {
						//Add vB to the ordered vertex set and move over to vB
						orderedVertices2D.AddUnique(edge.vB);
						currentVertex = edge.vB;
					}
					//Case a2II: The other vertex is in the orderedVertices set.
					else {
						//Case a2IIt: The other vertex is the tne, so we are done now
						if (edge.vB == tne2D) { tries = 100; }
						//Case a2IIu: The other vertex is not the tne, so we can ignore this edge
					}
				}
			}
			//Case b: vB is the current vertex
			else if (edge.vB == currentVertex) {
				//Case b1: The current vertex is also tne
				if (currentVertex == tne2D) {
					//Case b1I: The current vertex is the first vertex in the polygon
					if (orderedVertices2D.Num() == 1) { orderedVertices2D.AddUnique(edge.vA); currentVertex = edge.vA; }
					//Case b1II: The current vertex is not the first vertex, but then current != tne, so this case doesn't exist
				}
				//Case b2: The current vertex is not the tne
				else {
					//Case b2I: The other vertex is not in the orderedVertices set
					if (!orderedVertices2D.Contains(edge.vA)) {
						//Add vA to the ordered vertex set and move over to vA
						orderedVertices2D.AddUnique(edge.vA);
						currentVertex = edge.vA;
					}
					//Case b2II: The other vertex is in the orderedVertices set.
					else {
						//Case b2IIt: The other vertex is the tne, so we are done now
						if (edge.vA == tne2D) { tries = 200; }
						//Case b2IIu: The other vertex is not the tne, so we can ignore this edge
					}
				}
			}
			//Case c: The current vertex does not belong to this edge, so ignore it for now
		}
		tries++;
	}

	//Remove vertices with angles of 180 degrees
	TArray<FVector2D> removals;
	for (int32 i = 0; i < orderedVertices2D.Num(); i++) {
		int32 j = (i + 1) % orderedVertices2D.Num();
		int32 k = (j + 1) % orderedVertices2D.Num();

		FVector2D edgeA = orderedVertices2D[i] - orderedVertices2D[j];
		FVector2D edgeB = orderedVertices2D[k] - orderedVertices2D[j];
		float angle = FMath::Acos((FVector2D::DotProduct(edgeA, edgeB)) / (edgeA.Size() * edgeB.Size())) * 180 / PI;

		if (angle == 180) {	removals.Add(orderedVertices2D[j]); }
	}

	for (FVector2D edge : removals) { orderedVertices2D.Remove(edge); }

	//Make the vertices 3D again
	TArray<FVector> orderedVertices;
	for (FVector2D vertex : orderedVertices2D) {
		FVector vertex3D = FVector();
		switch (surface) {
		case ESurfaceType::Ceiling:
			vertex3D = FVector(vertex.X, vertex.Y, GetActorLocation().Z);
			break;
		case ESurfaceType::Floor:
			vertex3D = FVector(vertex.X, vertex.Y, GetActorLocation().Z);
			break;
		case ESurfaceType::Stairs:
			vertex3D = FVector(vertex.X, vertex.Y, GetActorLocation().Z);
			break;
		case ESurfaceType::StairsCeiling:
			vertex3D = FVector(vertex.X, vertex.Y, GetActorLocation().Z);
			break;
		case ESurfaceType::NorthWall:
			vertex3D = FVector(GetActorLocation().X, vertex.Y, vertex.X);
			break;
		case ESurfaceType::SouthWall:
			vertex3D = FVector(GetActorLocation().X, vertex.Y, vertex.X);
			break;
		case ESurfaceType::EastWall:
			vertex3D = FVector(vertex.X, GetActorLocation().Y, vertex.Y);
			break;
		case ESurfaceType::WestWall:
			vertex3D = FVector(vertex.X, GetActorLocation().Y, vertex.Y);
			break;
		default:
			break;
		}
		orderedVertices.AddUnique(vertex3D);
	}

	polygon = FSurfPolygon(orderedVertices2D, orderedVertices);
}

void ASurfaceArea::fixEdgeOverlap(FPolygonEdge oE, FPolygonEdge nE, TArray<FPolygonEdge> inEdges, TArray<FPolygonEdge>& outEdges, bool inHasSplit, bool &outHasSplit)
{
	outEdges = inEdges;
	outHasSplit = inHasSplit;
	//Depending on the overlap case, remove the oE and add new edges.

	//Case 1: old and new edge are the same. This is an interior edge and should be removed entirely
	if ((oE.vA == nE.vA && oE.vB == nE.vB) || (oE.vA == nE.vB && oE.vB == nE.vA)) { outEdges.Remove(oE); outHasSplit = true; }

	//Case 2: new edge and old edge are parallel at the same X
	else if (oE.vA.X == oE.vB.X && nE.vA.X == nE.vB.X && oE.vA.X == nE.vA.X) {
		//UE_LOG(LogTemp, Log, TEXT("Case 2: Parallel X"));
		outEdges.Remove(oE);

		//Case A: oA & nA | oB & nB \/ oB & nB | oA & nA
		if ((oE.vA.Y < oE.vB.Y && oE.vA.Y < nE.vB.Y && nE.vA.Y < oE.vB.Y && nE.vA.Y < nE.vB.Y) || (oE.vA.Y > oE.vB.Y && oE.vA.Y > nE.vB.Y && nE.vA.Y > oE.vB.Y && nE.vA.Y > nE.vB.Y))
		{
			//UE_LOG(LogTemp, Log, TEXT("X overlap case A: try to connect oA and nA"));
			if (oE.vA.Y != nE.vA.Y) { outEdges.Add(FPolygonEdge(oE.vA, nE.vA, true)); outHasSplit = true; }
			//else { UE_LOG(LogTemp, Log, TEXT("OAY and NAY are equal")); }
			if (oE.vB.Y != nE.vB.Y) { outEdges.Add(FPolygonEdge(oE.vB, nE.vB, true)); outHasSplit = true; }
			//else { UE_LOG(LogTemp, Log, TEXT("OBY and NBY are equal")); }
		}
		//Case B: oA & nB | oB & nA \/ oB & nA | oA & nB
		else if ((oE.vA.Y < oE.vB.Y && oE.vA.Y < nE.vA.Y && nE.vB.Y < oE.vB.Y && nE.vB.Y < nE.vA.Y) || (oE.vA.Y > oE.vB.Y && oE.vA.Y > nE.vA.Y && nE.vB.Y > oE.vB.Y && nE.vB.Y > nE.vA.Y))
		{
			//UE_LOG(LogTemp, Log, TEXT("X overlap case B: try to connect oA and nB"));
			if (oE.vA.Y != nE.vB.Y) { outEdges.Add(FPolygonEdge(oE.vA, nE.vB, true)); outHasSplit = true; }
			//else { UE_LOG(LogTemp, Log, TEXT("OAY and NBY are equal")); }
			if (oE.vB.Y != nE.vA.Y) { outEdges.Add(FPolygonEdge(oE.vB, nE.vA, true)); outHasSplit = true; }
			//else { UE_LOG(LogTemp, Log, TEXT("OBY and NAY are equal")); }
		}
		//Case C: oA & oB | nA & nB \/ nA & nB | oA & oB
		else if ((oE.vA.Y < nE.vA.Y && oE.vA.Y < nE.vB.Y && oE.vB.Y < nE.vA.Y && oE.vB.Y < nE.vB.Y) || (oE.vA.Y > nE.vA.Y && oE.vA.Y > nE.vB.Y && oE.vB.Y > nE.vA.Y && oE.vB.Y > nE.vB.Y))
		{
			//UE_LOG(LogTemp, Log, TEXT("X overlap case C: try to connect oA and oB"));
			if (oE.vA.Y != oE.vB.Y) { outEdges.Add(FPolygonEdge(oE.vA, oE.vB, true)); outHasSplit = true; }
			//else { UE_LOG(LogTemp, Log, TEXT("OAY and OBY are equal")); }
			if (nE.vB.Y != nE.vA.Y) { outEdges.Add(FPolygonEdge(nE.vB, nE.vA, true)); outHasSplit = true; }
			//else { UE_LOG(LogTemp, Log, TEXT("NAY and NBY are equal")); }
		}

		//Case D: Two of the vertices overlap, so the two edges either touch or are equal (handled by Case 1). If edges touch, they can be safely added
		else {
			//UE_LOG(LogTemp, Log, TEXT("X overlap case D: two overlapping vertices"));
			if (oE.vA.Y != oE.vB.Y) { outEdges.Add(FPolygonEdge(oE.vA, oE.vB, true)); outHasSplit = true; }
			//else { UE_LOG(LogTemp, Log, TEXT("OAY and OBY are equal")); }
			if (nE.vB.Y != nE.vA.Y) { outEdges.Add(FPolygonEdge(nE.vB, nE.vA, true)); outHasSplit = true; }
			//else { UE_LOG(LogTemp, Log, TEXT("NAY and NBY are equal")); }
		}
	}
	
	//Case 3: new edge and old edge are parallel at the same Y
	else if (oE.vA.Y == oE.vB.Y && nE.vA.Y == nE.vB.Y && oE.vA.Y == nE.vA.Y) {
		//UE_LOG(LogTemp, Log, TEXT("Case 3: Parallel Y"));
		outEdges.Remove(oE);

		//Case A: oA & nA | oB & nB \/ oB & nB | oA & nA
		if ((oE.vA.X < oE.vB.X && oE.vA.X < nE.vB.X && nE.vA.X < oE.vB.X && nE.vA.X < nE.vB.X) || (oE.vA.X > oE.vB.X && oE.vA.X > nE.vB.X && nE.vA.X > oE.vB.X && nE.vA.X > nE.vB.X))
		{
			//UE_LOG(LogTemp, Log, TEXT("Y overlap case A: try to connect oA and nA"));
			if (oE.vA.X != nE.vA.X) { outEdges.Add(FPolygonEdge(oE.vA, nE.vA, true)); outHasSplit = true; }
			//else { UE_LOG(LogTemp, Log, TEXT("OAX and NAX are equal")); }
			if (oE.vB.X != nE.vB.X) { outEdges.Add(FPolygonEdge(oE.vB, nE.vB, true)); outHasSplit = true; }
			//else { UE_LOG(LogTemp, Log, TEXT("OBX and NBX are equal")); }
		}
		//Case B: oA & nB | oB & nA \/ oB & nA | oA & nB
		else if ((oE.vA.X < oE.vB.X && oE.vA.X < nE.vA.X && nE.vB.X < oE.vB.X && nE.vB.X < nE.vA.X) || (oE.vA.X > oE.vB.X && oE.vA.X > nE.vA.X && nE.vB.X > oE.vB.X && nE.vB.X > nE.vA.X))
		{
			//UE_LOG(LogTemp, Log, TEXT("Y overlap case B: try to connect oA and nB"));
			if (oE.vA.X != nE.vB.X) { outEdges.Add(FPolygonEdge(oE.vA, nE.vB, true)); outHasSplit = true; }
			//else { UE_LOG(LogTemp, Log, TEXT("OAX and NBX are equal")); }
			if (oE.vB.X != nE.vA.X) { outEdges.Add(FPolygonEdge(oE.vB, nE.vA, true)); outHasSplit = true; }
			//else { UE_LOG(LogTemp, Log, TEXT("NAX and OBX are equal")); }
		}
		//Case C: oA & oB | nA & nB \/ nA & nB | oA & oB
		else if ((oE.vA.X < nE.vA.X && oE.vA.X < nE.vB.X && oE.vB.X < nE.vA.X && oE.vB.X < nE.vB.X) || (oE.vA.X > nE.vA.X && oE.vA.X > nE.vB.X && oE.vB.X > nE.vA.X && oE.vB.X > nE.vB.X))
		{
			//UE_LOG(LogTemp, Log, TEXT("Y overlap case C: try to connect oA and oB"));
			if (oE.vA.X != oE.vB.X) { outEdges.Add(FPolygonEdge(oE.vA, oE.vB, true)); outHasSplit = true; }
			//else { UE_LOG(LogTemp, Log, TEXT("OAX and OBX are equal")); }
			if (nE.vB.X != nE.vA.X) { outEdges.Add(FPolygonEdge(nE.vB, nE.vA, true)); outHasSplit = true; }
			//else { UE_LOG(LogTemp, Log, TEXT("NAX and NBX are equal")); }
		}

		//Case D: Two of the vertices overlap, so the two edges either touch or are equal (handled by Case 1). If edges touch, they can be safely added
		else {
			//UE_LOG(LogTemp, Log, TEXT("Y overlap case D: two overlapping vertices"));
			if (oE.vA.X != oE.vB.X) { outEdges.Add(FPolygonEdge(oE.vA, oE.vB, true)); outHasSplit = true; }
			//else { UE_LOG(LogTemp, Log, TEXT("OAX and OBX are equal")); }
			if (nE.vB.X != nE.vA.X) { outEdges.Add(FPolygonEdge(nE.vB, nE.vA, true)); outHasSplit = true; }
			//else { UE_LOG(LogTemp, Log, TEXT("NAX and NBX are equal")); }
		}
	}

	//Case 4: new edge and old edge are not parallel and the new one can thus safely be added
	//else {	UE_LOG(LogTemp, Log, TEXT("Case 4: Non-parallel edges")); }
}

TArray<FDTriangle> ASurfaceArea::computeDelaunayTriangulation()
{
	//Sort the vertices of the polygon by their y (or x) values
	TArray<FVector2D> vertices2D = polygon.vertices;
	TArray<FVector2D> allVertices2D;
	vertices2D.Sort(FSortByYX());

	//UE_LOG(LogTemp, Log, TEXT("Amount of vertices: %d"), vertices2D.Num());
		
	allVertices2D.Append(vertices2D);
	allVertices2D.Add(polygon.minVertex - FVector2D(1));
	allVertices2D.Add(FVector2D(polygon.maxVertex.X, polygon.minVertex.Y) + FVector2D(1, -1));
	//UE_LOG(LogTemp, Log, TEXT("minVertex: %s"), *(polygon.minVertex - FVector2D(1)).ToString());
	//UE_LOG(LogTemp, Log, TEXT("maxVertex: %s"), *(FVector2D(polygon.maxVertex.X, polygon.minVertex.Y) + FVector2D(1, -1)).ToString());

	//DrawDebugPoint(GetWorld(), FVector(polygon.minVertex - FVector2D(1,1), GetActorLocation().Z), 20, FColor::Red, false, 15, 0);
	//DrawDebugPoint(GetWorld(), FVector(polygon.maxVertex.X + 1, polygon.minVertex.Y - 1, GetActorLocation().Z), 20, FColor::Green, false, 15, 0);

	//UE_LOG(LogTemp, Log, TEXT("Amount of vertices including artificial points: %d"), allVertices2D.Num());

	TArray<FPolygonEdge> advancingFront;
	FPolygonEdge leftArtificial = FPolygonEdge(polygon.minVertex - FVector2D(1,1), vertices2D[0], true); advancingFront.Add(leftArtificial);
	FPolygonEdge rightArtificial = FPolygonEdge(FVector2D(polygon.maxVertex.X + 1, polygon.minVertex.Y - 1), vertices2D[0], true); advancingFront.Add(rightArtificial);
	
	//DrawDebugLine(GetWorld(), FVector(leftArtificial.vA, GetActorLocation().Z), FVector(leftArtificial.vB, GetActorLocation().Z), FColor::Green, false, 60, 0, 6);
	//DrawDebugLine(GetWorld(), FVector(rightArtificial.vA, GetActorLocation().Z), FVector(rightArtificial.vB, GetActorLocation().Z), FColor::Green, false, 60, 0, 6);
	//DrawDebugLine(GetWorld(), FVector(leftArtificial.vA, GetActorLocation().Z), FVector(rightArtificial.vA, GetActorLocation().Z), FColor::Green, false, 60, 0, 6);

	//UE_LOG(LogTemp, Log, TEXT("Amount of edges in the advancing front: %d"), advancingFront.Num());

	//for (FPolygonEdge front : advancingFront) { UE_LOG(LogTemp, Log, TEXT("Front edge: %s to %s"), *front.vA.ToString(), *front.vB.ToString()); }

	TArray<FDTriangle> triangles;
	FDTriangle initialTriangle = FDTriangle({ polygon.minVertex - FVector2D(1,1), FVector2D(polygon.maxVertex.X + 1, polygon.minVertex.Y - 1), vertices2D[0] }); triangles.AddUnique(initialTriangle);

	//Handle the addition of new vertices
	for (FVector2D vC : vertices2D) { if (vC != vertices2D[0]) {
		//UE_LOG(LogTemp, Log, TEXT("vC: %s"), *vC.ToString());
		//UE_LOG(LogTemp, Log, TEXT("Amount of edges in the advancing front (for loop): %d"), advancingFront.Num());

		//for (FPolygonEdge front : advancingFront) { UE_LOG(LogTemp, Log, TEXT("Front edge: %s to %s"), *front.vA.ToString(), *front.vB.ToString()); }

		//Find the projection of vC onto the advancing front. Remove that edge from the front and add edges from the two end points to the new vertex
		TArray<FPolygonEdge> overlappingFronts;
		FDTriangle newTriangleA;
		FDTriangle newTriangleB;

		for (FPolygonEdge front : advancingFront) {
			//UE_LOG(LogTemp, Log, TEXT("Front edge to check: %s to %s"), *front.vA.ToString(), *front.vB.ToString());
			//Projection of vC is on the advancing front
			if ((vC.X < front.vA.X && vC.X > front.vB.X) || (vC.X > front.vA.X && vC.X < front.vB.X)) {
				//UE_LOG(LogTemp, Log, TEXT("vC is on a front"));
				overlappingFronts.AddUnique(front);
				newTriangleA = FDTriangle({ front.vA, front.vB, vC });
				//UE_LOG(LogTemp, Log, TEXT("newTriangleA: %s, %s, %s"), *front.vA.ToString(), *front.vB.ToString(), *vC.ToString());
				triangles.AddUnique(newTriangleA);
				break;
			}

			//Projection of vC is exactly on a vertex vA
			else if ((vC.X == front.vA.X)) {
				//UE_LOG(LogTemp, Log, TEXT("vC is on a vertex vA"));
				overlappingFronts.Add(front);

				//Find the other front with this vertex
				FPolygonEdge otherOverlap;
				for (FPolygonEdge other : advancingFront) {
					if (!(other == front)) {
						if (other.vA == front.vA) {
							//UE_LOG(LogTemp, Log, TEXT("vC is on other vertex vA"));
							overlappingFronts.Add(other);
							newTriangleB = FDTriangle({ front.vA, other.vB, vC });
							//UE_LOG(LogTemp, Log, TEXT("newTriangleB: %s, %s, %s"), *front.vA.ToString(), *other.vB.ToString(), *vC.ToString());
							triangles.AddUnique(newTriangleB);
						}
						else if (other.vB == front.vA) {
							//UE_LOG(LogTemp, Log, TEXT("vC is on other vertex vB"));
							overlappingFronts.Add(other);
							newTriangleB = FDTriangle({ front.vA, other.vA, vC });
							//UE_LOG(LogTemp, Log, TEXT("newTriangleB: %s, %s, %s"), *front.vA.ToString(), *other.vA.ToString(), *vC.ToString());
							triangles.AddUnique(newTriangleB);
						}
					}
				}

				newTriangleA = FDTriangle({ front.vA, front.vB, vC });
				//UE_LOG(LogTemp, Log, TEXT("newTriangleA: %s, %s, %s"), *front.vA.ToString(), *front.vB.ToString(), *vC.ToString());
				triangles.AddUnique(newTriangleA);
				break;
			}

			//Projection of vC is exactly on a vertex vB
			else if ((vC.X == front.vB.X)) {
				//UE_LOG(LogTemp, Log, TEXT("vC is on vB"));
				overlappingFronts.Add(front);

				//Find the other front with this vertex
				for (FPolygonEdge other : advancingFront) {
					if (!(other == front)) {
						if (other.vA == front.vB) {
							//UE_LOG(LogTemp, Log, TEXT("vC is on other vertex vA"));
							overlappingFronts.Add(other);
							newTriangleB = FDTriangle({ front.vB, other.vB, vC });
							//UE_LOG(LogTemp, Log, TEXT("newTriangleB: %s, %s, %s"), *front.vB.ToString(), *other.vB.ToString(), *vC.ToString());
							triangles.AddUnique(newTriangleB);
						}
						else if (other.vB == front.vB) {
							//UE_LOG(LogTemp, Log, TEXT("vC is on other vertex vB"));
							overlappingFronts.Add(other);
							newTriangleB = FDTriangle({ front.vB, other.vA, vC });
							//UE_LOG(LogTemp, Log, TEXT("newTriangleB: %s, %s, %s"), *front.vB.ToString(), *other.vA.ToString(), *vC.ToString());
							triangles.AddUnique(newTriangleB);
						}
					}
				}

				newTriangleA = FDTriangle({ front.vA, front.vB, vC });
				//UE_LOG(LogTemp, Log, TEXT("newTriangleA: %s, %s, %s"), *front.vA.ToString(), *front.vB.ToString(), *vC.ToString());
				triangles.AddUnique(newTriangleA);
				break;
			}

			//else { UE_LOG(LogTemp, Log, TEXT("vC is outside of the front")); }
		}

		//UE_LOG(LogTemp, Log, TEXT("Amount of overlapping fronts: %d"), overlappingFronts.Num());

		for (FPolygonEdge front : overlappingFronts) {
			//UE_LOG(LogTemp, Log, TEXT("Overlapping front being handled"));
			if ((vC.X < front.vA.X && vC.X > front.vB.X) || (vC.X > front.vA.X && vC.X < front.vB.X)) {
				//UE_LOG(LogTemp, Log, TEXT("Overlapping front on front"));
				advancingFront.Remove(front);
				advancingFront.AddUnique(FPolygonEdge(front.vA, vC, true));
				advancingFront.AddUnique(FPolygonEdge(front.vB, vC, true));
			}
			else if ((vC.X == front.vA.X)) {
				//UE_LOG(LogTemp, Log, TEXT("Overlapping front on vA"));
				advancingFront.Remove(front);
				advancingFront.AddUnique(FPolygonEdge(front.vB, vC, true)); 
				
				FPolygonEdge otherOverlap;
				for (FPolygonEdge other : overlappingFronts) { if (!(other == front)) {
					if (other.vA == front.vA || other.vB == front.vA) { otherOverlap = other; break; }
				} }

				if (otherOverlap.vA == front.vA){
					advancingFront.Remove(otherOverlap);
					advancingFront.AddUnique(FPolygonEdge(otherOverlap.vB, vC, true));
				}
				else if (otherOverlap.vB == front.vA){
					advancingFront.Remove(otherOverlap);
					advancingFront.AddUnique(FPolygonEdge(otherOverlap.vA, vC, true));
				}

			}
			else if ((vC.X == front.vB.X)) {
				//UE_LOG(LogTemp, Log, TEXT("Overlapping front on vB"));
				advancingFront.Remove(front);
				advancingFront.Add(FPolygonEdge(front.vA, vC, true));

				FPolygonEdge otherOverlap;
				for (FPolygonEdge other : overlappingFronts) { if (!(other == front)) {
					if (other.vA == front.vA || other.vB == front.vA) { otherOverlap = other; break; }
				} }

				if (otherOverlap.vA == front.vB) {
					advancingFront.Remove(otherOverlap);
					advancingFront.AddUnique(FPolygonEdge(otherOverlap.vB, vC, true));
				}
				else if (otherOverlap.vB == front.vB) {
					advancingFront.Remove(otherOverlap);
					advancingFront.AddUnique(FPolygonEdge(otherOverlap.vA, vC, true));
				}
			}
		}

		//Add extra triangles if necessary
		//Find all vertices on the front to try to connect. Sort them by X-distance from vC
		//UE_LOG(LogTemp, Log, TEXT("Find front vertices"));
		TArray<FFrontVertex> frontVertices = findFrontVertices(advancingFront);
		frontVertices.Sort(FSortByXDist(vC));
		//for (FFrontVertex frontVertex : frontVertices) { UE_LOG(LogTemp, Log, TEXT("Front vertex: %s"), *frontVertex.vertex.ToString()); }
		FFrontVertex currentVertex;

		//Find the front vertex that is vC, so that we can use the edges
		for (FFrontVertex frontVertex : frontVertices) { if (frontVertex.vertex == vC) { currentVertex = frontVertex; break; } }

		//Check if a front vertex is visible from vC. If so, create a new triangle and advance the front
		for (FFrontVertex frontVertex : frontVertices) {
			if (frontVertex.vertex != vC) {
				if (checkIsVisible(vC, frontVertex.vertex, triangles)) {
					if (frontVertex.edgeA.vA != vC && frontVertex.edgeA.vB != vC && frontVertex.edgeB.vA != vC && frontVertex.edgeB.vB != vC) {

						//Find the third vertex of the triangle based on the edges of the current and visible front vertices
						FVector2D thirdVertex = frontVertex.edgeA.vA;
						if (frontVertex.edgeA.vA == currentVertex.edgeA.vA || frontVertex.edgeA.vA == currentVertex.edgeA.vB || frontVertex.edgeA.vA == currentVertex.edgeB.vA || frontVertex.edgeA.vA == currentVertex.edgeB.vB) { thirdVertex = frontVertex.edgeA.vA; }
						if (frontVertex.edgeA.vB == currentVertex.edgeA.vA || frontVertex.edgeA.vB == currentVertex.edgeA.vB || frontVertex.edgeA.vB == currentVertex.edgeB.vA || frontVertex.edgeA.vB == currentVertex.edgeB.vB) { thirdVertex = frontVertex.edgeA.vB; }
						if (frontVertex.edgeB.vA == currentVertex.edgeA.vA || frontVertex.edgeB.vA == currentVertex.edgeA.vB || frontVertex.edgeB.vA == currentVertex.edgeB.vA || frontVertex.edgeB.vA == currentVertex.edgeB.vB) { thirdVertex = frontVertex.edgeB.vA; }
						if (frontVertex.edgeB.vB == currentVertex.edgeA.vA || frontVertex.edgeB.vB == currentVertex.edgeA.vB || frontVertex.edgeB.vB == currentVertex.edgeB.vA || frontVertex.edgeB.vB == currentVertex.edgeB.vB) { thirdVertex = frontVertex.edgeB.vB; }

						advancingFront.AddUnique(FPolygonEdge(frontVertex.vertex, vC, true));

						//Find the edges belonging to the third vertex
						for (FFrontVertex thirdFrontVertex : frontVertices) {
							if (thirdFrontVertex.vertex == thirdVertex) {
								advancingFront.Remove(thirdFrontVertex.edgeA); 
								advancingFront.Remove(thirdFrontVertex.edgeB); 
								break;
							}
						}
						triangles.AddUnique(FDTriangle({ frontVertex.vertex, vC, thirdVertex }));
					}
				}
			}
		}
		
		//Change the triangulation if it is no longer legal. Again, add something to prevent infinite loops
		bool illegal = true;
		int32 changes = 0;

		while (illegal && changes < 100) {
			illegal = false;

			//Extra array to prevent removing things from an array that is being iterated over
			TArray<FDTriangle> tempTriangles;
			for (FDTriangle triangle : triangles) { tempTriangles.AddUnique(triangle); }

			//UE_LOG(LogTemp, Log, TEXT("Amount of triangles: %d"), triangles.Num());

			int32 checkIndex = 7;
			TArray<FDTriangle> emptyTriangles;

			//Go over all pairs of triangles
			for (FDTriangle triangleA : tempTriangles) {
				for (FDTriangle triangleB : tempTriangles) {
					if (!(triangleA == triangleB)) {
						//We do not need to adjust triangles with artificial vertices, and we don't want to create triangles that lie outside the polygon
						if (!triangleA.hasVertex(leftArtificial.vA) && !triangleA.hasVertex(rightArtificial.vA) 
							&& !triangleB.hasVertex(leftArtificial.vA) && !triangleB.hasVertex(rightArtificial.vA)) {
							//Check if the two triangles have an edge in common
							if (triangleB.hasEdge(triangleA.vertices[0], triangleA.vertices[1])) {
								if (trianglesAreIllegal(triangleA, triangleB, triangleA.vertices[0], triangleA.vertices[1])) {

									/*if (vertices2D.IsValidIndex(checkIndex)) {
										if (vC == vertices2D[checkIndex]) {
											UE_LOG(LogTemp, Log, TEXT("Illegal triangles found!"));
											UE_LOG(LogTemp, Log, TEXT("TriangleA: %s, %s, %s"), *triangleA.vertices[0].ToString(), *triangleA.vertices[1].ToString(), *triangleA.vertices[2].ToString());
											UE_LOG(LogTemp, Log, TEXT("TriangleB: %s, %s, %s"), *triangleB.vertices[0].ToString(), *triangleB.vertices[1].ToString(), *triangleB.vertices[2].ToString());
											FVector a = GetActorLocation();
											FVector b = GetActorLocation();
											FVector c = GetActorLocation();
											a.X = triangleA.vertices[0].X; a.Y = triangleA.vertices[0].Y;
											b.X = triangleA.vertices[1].X; b.Y = triangleA.vertices[1].Y;
											c.X = triangleA.vertices[2].X; c.Y = triangleA.vertices[2].Y;
											DrawDebugLine(GetWorld(), a, b, FColor::Magenta, false, 60, 0, 12);
											DrawDebugLine(GetWorld(), b, c, FColor::Magenta, false, 60, 0, 12);
											DrawDebugLine(GetWorld(), c, a, FColor::Magenta, false, 60, 0, 12);
											a.X = triangleB.vertices[0].X; a.Y = triangleB.vertices[0].Y;
											b.X = triangleB.vertices[1].X; b.Y = triangleB.vertices[1].Y;
											c.X = triangleB.vertices[2].X; c.Y = triangleB.vertices[2].Y;
											DrawDebugLine(GetWorld(), a, b, FColor::Magenta, false, 60, 0, 12);
											DrawDebugLine(GetWorld(), b, c, FColor::Magenta, false, 60, 0, 12);
											DrawDebugLine(GetWorld(), c, a, FColor::Magenta, false, 60, 0, 12);
										}
									}*/
									//UE_LOG(LogTemp, Log, TEXT("Illegal triangle found!"));

									//Check whether B0, B1 or B2 is the other vertex that is not on the edge 01
									if (triangleB.vertices[0] != triangleA.vertices[0] && triangleB.vertices[0] != triangleA.vertices[1]) {
										if (checkIsVisible(triangleB.vertices[0], triangleA.vertices[2], emptyTriangles)) {
											triangles.Remove(triangleA);
											triangles.Remove(triangleB);
											FDTriangle newExtraTriangleA = FDTriangle({ triangleB.vertices[0], triangleA.vertices[2], triangleA.vertices[0] });
											FDTriangle newExtraTriangleB = FDTriangle({ triangleB.vertices[0], triangleA.vertices[2], triangleA.vertices[1] });
											triangles.AddUnique(newExtraTriangleA); triangles.AddUnique(newExtraTriangleB);
										}
									}
									else if (triangleB.vertices[1] != triangleA.vertices[0] && triangleB.vertices[1] != triangleA.vertices[1]) {
										if (checkIsVisible(triangleB.vertices[1], triangleA.vertices[2], emptyTriangles)) {
											triangles.Remove(triangleA);
											triangles.Remove(triangleB);
											FDTriangle newExtraTriangleA = FDTriangle({ triangleB.vertices[1], triangleA.vertices[2], triangleA.vertices[0] });
											FDTriangle newExtraTriangleB = FDTriangle({ triangleB.vertices[1], triangleA.vertices[2], triangleA.vertices[1] });
											triangles.AddUnique(newExtraTriangleA); triangles.AddUnique(newExtraTriangleB);
										}
									}
									else if (triangleB.vertices[2] != triangleA.vertices[0] && triangleB.vertices[2] != triangleA.vertices[1]) {
										if (checkIsVisible(triangleB.vertices[2], triangleA.vertices[2], emptyTriangles)) {
											triangles.Remove(triangleA);
											triangles.Remove(triangleB);
											FDTriangle newExtraTriangleA = FDTriangle({ triangleB.vertices[2], triangleA.vertices[2], triangleA.vertices[0] });
											FDTriangle newExtraTriangleB = FDTriangle({ triangleB.vertices[2], triangleA.vertices[2], triangleA.vertices[1] });
											triangles.AddUnique(newExtraTriangleA); triangles.AddUnique(newExtraTriangleB);
										}
									}
									//else { UE_LOG(LogTemp, Log, TEXT("No triangles added. Something is off... (A)")); }
								}
							}
							else if (triangleB.hasEdge(triangleA.vertices[0], triangleA.vertices[2])) {
								if (trianglesAreIllegal(triangleA, triangleB, triangleA.vertices[0], triangleA.vertices[2])) {
									//UE_LOG(LogTemp, Log, TEXT("Illegal triangles found!"));

									/*if (vertices2D.IsValidIndex(checkIndex)) {
										if (vC == vertices2D[checkIndex]) {
											UE_LOG(LogTemp, Log, TEXT("Illegal triangles found!"));
											UE_LOG(LogTemp, Log, TEXT("TriangleA: %s, %s, %s"), *triangleA.vertices[0].ToString(), *triangleA.vertices[1].ToString(), *triangleA.vertices[2].ToString());
											UE_LOG(LogTemp, Log, TEXT("TriangleB: %s, %s, %s"), *triangleB.vertices[0].ToString(), *triangleB.vertices[1].ToString(), *triangleB.vertices[2].ToString());
											FVector a = GetActorLocation();
											FVector b = GetActorLocation();
											FVector c = GetActorLocation();
											a.X = triangleA.vertices[0].X; a.Y = triangleA.vertices[0].Y;
											b.X = triangleA.vertices[1].X; b.Y = triangleA.vertices[1].Y;
											c.X = triangleA.vertices[2].X; c.Y = triangleA.vertices[2].Y;
											DrawDebugLine(GetWorld(), a, b, FColor::Green, false, 60, 0, 12);
											DrawDebugLine(GetWorld(), b, c, FColor::Green, false, 60, 0, 12);
											DrawDebugLine(GetWorld(), c, a, FColor::Green, false, 60, 0, 12);
											a.X = triangleB.vertices[0].X; a.Y = triangleB.vertices[0].Y;
											b.X = triangleB.vertices[1].X; b.Y = triangleB.vertices[1].Y;
											c.X = triangleB.vertices[2].X; c.Y = triangleB.vertices[2].Y;
											DrawDebugLine(GetWorld(), a, b, FColor::Green, false, 60, 0, 12);
											DrawDebugLine(GetWorld(), b, c, FColor::Green, false, 60, 0, 12);
											DrawDebugLine(GetWorld(), c, a, FColor::Green, false, 60, 0, 12);
										}
									}*/

									//Check whether B0, B1 or B2 is the other vertex that is not on the edge 01
									if (triangleB.vertices[0] != triangleA.vertices[0] && triangleB.vertices[0] != triangleA.vertices[2]) {
										if (checkIsVisible(triangleB.vertices[0], triangleA.vertices[1], emptyTriangles)) {
											triangles.Remove(triangleA);
											triangles.Remove(triangleB);
											FDTriangle newExtraTriangleA = FDTriangle({ triangleB.vertices[0], triangleA.vertices[1], triangleA.vertices[0] });
											FDTriangle newExtraTriangleB = FDTriangle({ triangleB.vertices[0], triangleA.vertices[1], triangleA.vertices[2] });
											triangles.AddUnique(newExtraTriangleA); triangles.AddUnique(newExtraTriangleB);
										}
									}
									else if (triangleB.vertices[1] != triangleA.vertices[0] && triangleB.vertices[1] != triangleA.vertices[2]) {
										if (checkIsVisible(triangleB.vertices[1], triangleA.vertices[1], emptyTriangles)) {
											triangles.Remove(triangleA);
											triangles.Remove(triangleB);
											FDTriangle newExtraTriangleA = FDTriangle({ triangleB.vertices[1], triangleA.vertices[1], triangleA.vertices[0] });
											FDTriangle newExtraTriangleB = FDTriangle({ triangleB.vertices[1], triangleA.vertices[1], triangleA.vertices[2] });
											triangles.AddUnique(newExtraTriangleA); triangles.AddUnique(newExtraTriangleB);
										}
									}
									else if (triangleB.vertices[2] != triangleA.vertices[0] && triangleB.vertices[2] != triangleA.vertices[2]) {
										if (checkIsVisible(triangleB.vertices[2], triangleA.vertices[1], emptyTriangles)) {
											triangles.Remove(triangleA);
											triangles.Remove(triangleB);
											FDTriangle newExtraTriangleA = FDTriangle({ triangleB.vertices[2], triangleA.vertices[1], triangleA.vertices[0] });
											FDTriangle newExtraTriangleB = FDTriangle({ triangleB.vertices[2], triangleA.vertices[1], triangleA.vertices[2] });
											triangles.AddUnique(newExtraTriangleA); triangles.AddUnique(newExtraTriangleB);
										}
									}
									//else { UE_LOG(LogTemp, Log, TEXT("No triangles added. Something is off... (B)")); }
								}
							}
							else if (triangleB.hasEdge(triangleA.vertices[1], triangleA.vertices[2])) {
								if (trianglesAreIllegal(triangleA, triangleB, triangleA.vertices[1], triangleA.vertices[2])) {

									/*if (vertices2D.IsValidIndex(checkIndex)) {
										if (vC == vertices2D[checkIndex]) {
											UE_LOG(LogTemp, Log, TEXT("Illegal triangles found!"));
											UE_LOG(LogTemp, Log, TEXT("TriangleA: %s, %s, %s"), *triangleA.vertices[0].ToString(), *triangleA.vertices[1].ToString(), *triangleA.vertices[2].ToString());
											UE_LOG(LogTemp, Log, TEXT("TriangleB: %s, %s, %s"), *triangleB.vertices[0].ToString(), *triangleB.vertices[1].ToString(), *triangleB.vertices[2].ToString());
											FVector a = GetActorLocation();
											FVector b = GetActorLocation();
											FVector c = GetActorLocation();
											a.X = triangleA.vertices[0].X; a.Y = triangleA.vertices[0].Y;
											b.X = triangleA.vertices[1].X; b.Y = triangleA.vertices[1].Y;
											c.X = triangleA.vertices[2].X; c.Y = triangleA.vertices[2].Y;
											DrawDebugLine(GetWorld(), a, b, FColor::Orange, false, 60, 0, 12);
											DrawDebugLine(GetWorld(), b, c, FColor::Orange, false, 60, 0, 12);
											DrawDebugLine(GetWorld(), c, a, FColor::Orange, false, 60, 0, 12);
											a.X = triangleB.vertices[0].X; a.Y = triangleB.vertices[0].Y;
											b.X = triangleB.vertices[1].X; b.Y = triangleB.vertices[1].Y;
											c.X = triangleB.vertices[2].X; c.Y = triangleB.vertices[2].Y;
											DrawDebugLine(GetWorld(), a, b, FColor::Orange, false, 60, 0, 12);
											DrawDebugLine(GetWorld(), b, c, FColor::Orange, false, 60, 0, 12);
											DrawDebugLine(GetWorld(), c, a, FColor::Orange, false, 60, 0, 12);
										}
									}*/
									//UE_LOG(LogTemp, Log, TEXT("Illegal triangles found!"));

									//Check whether B0, B1 or B2 is the other vertex that is not on the edge 01
									if (triangleB.vertices[0] != triangleA.vertices[1] && triangleB.vertices[0] != triangleA.vertices[2]) {
										if (checkIsVisible(triangleB.vertices[0], triangleA.vertices[0], emptyTriangles)) {
											triangles.Remove(triangleA);
											triangles.Remove(triangleB);
											FDTriangle newExtraTriangleA = FDTriangle({ triangleB.vertices[0], triangleA.vertices[0], triangleA.vertices[1] });
											FDTriangle newExtraTriangleB = FDTriangle({ triangleB.vertices[0], triangleA.vertices[0], triangleA.vertices[2] });
											triangles.AddUnique(newExtraTriangleA); triangles.AddUnique(newExtraTriangleB);
										}
									}
									else if (triangleB.vertices[1] != triangleA.vertices[1] && triangleB.vertices[1] != triangleA.vertices[2]) {
										if (checkIsVisible(triangleB.vertices[1], triangleA.vertices[0], emptyTriangles)) {
											triangles.Remove(triangleA);
											triangles.Remove(triangleB);
											FDTriangle newExtraTriangleA = FDTriangle({ triangleB.vertices[1], triangleA.vertices[0], triangleA.vertices[1] });
											FDTriangle newExtraTriangleB = FDTriangle({ triangleB.vertices[1], triangleA.vertices[0], triangleA.vertices[2] });
											triangles.AddUnique(newExtraTriangleA); triangles.AddUnique(newExtraTriangleB);
										}
									}
									else if (triangleB.vertices[2] != triangleA.vertices[1] && triangleB.vertices[2] != triangleA.vertices[2]) {
										if (checkIsVisible(triangleB.vertices[2], triangleA.vertices[0], emptyTriangles)) {
											triangles.Remove(triangleA);
											triangles.Remove(triangleB);
											FDTriangle newExtraTriangleA = FDTriangle({ triangleB.vertices[2], triangleA.vertices[0], triangleA.vertices[1] });
											FDTriangle newExtraTriangleB = FDTriangle({ triangleB.vertices[2], triangleA.vertices[0], triangleA.vertices[2] });
											triangles.AddUnique(newExtraTriangleA); triangles.AddUnique(newExtraTriangleB);
										}
									}
									//else { UE_LOG(LogTemp, Log, TEXT("No triangles added. Something is off... (C)")); }
								}
							}
						}
					}
				}
			}
		}

		/*if (vertices2D.IsValidIndex(5)) {
			if (vC == vertices2D[5]) {
				UE_LOG(LogTemp, Log, TEXT("Amount of front vertices: %d"), advancingFront.Num() - 1);
				for (FPolygonEdge front : advancingFront) {
					DrawDebugLine(GetWorld(), FVector(front.vA, GetActorLocation().Z), FVector(front.vB, GetActorLocation().Z), FColor::Green, false, 60, 0, 6); 
				}
				for (FDTriangle triangle : triangles) {
					FVector a = GetActorLocation();
					FVector b = GetActorLocation();
					FVector c = GetActorLocation();
					switch (surface) {
					case ESurfaceType::Ceiling:
						a.X = triangle.vertices[0].X; a.Y = triangle.vertices[0].Y;
						b.X = triangle.vertices[1].X; b.Y = triangle.vertices[1].Y;
						c.X = triangle.vertices[2].X; c.Y = triangle.vertices[2].Y;
						break;
					case ESurfaceType::Floor:
						a.X = triangle.vertices[0].X; a.Y = triangle.vertices[0].Y;
						b.X = triangle.vertices[1].X; b.Y = triangle.vertices[1].Y;
						c.X = triangle.vertices[2].X; c.Y = triangle.vertices[2].Y;
						break;
					case ESurfaceType::Stairs:
						a.X = triangle.vertices[0].X; a.Y = triangle.vertices[0].Y;
						b.X = triangle.vertices[1].X; b.Y = triangle.vertices[1].Y;
						c.X = triangle.vertices[2].X; c.Y = triangle.vertices[2].Y;
						break;
					case ESurfaceType::StairsCeiling:
						a.X = triangle.vertices[0].X; a.Y = triangle.vertices[0].Y;
						b.X = triangle.vertices[1].X; b.Y = triangle.vertices[1].Y;
						c.X = triangle.vertices[2].X; c.Y = triangle.vertices[2].Y;
						break;
					case ESurfaceType::NorthWall:
						a.Z = triangle.vertices[0].X; a.Y = triangle.vertices[0].Y;
						b.Z = triangle.vertices[1].X; b.Y = triangle.vertices[1].Y;
						c.Z = triangle.vertices[2].X; c.Y = triangle.vertices[2].Y;
						break;
					case ESurfaceType::SouthWall:
						a.Z = triangle.vertices[0].X; a.Y = triangle.vertices[0].Y;
						b.Z = triangle.vertices[1].X; b.Y = triangle.vertices[1].Y;
						c.Z = triangle.vertices[2].X; c.Y = triangle.vertices[2].Y;
						break;
					case ESurfaceType::EastWall:
						a.X = triangle.vertices[0].X; a.Z = triangle.vertices[0].Y;
						b.X = triangle.vertices[1].X; b.Z = triangle.vertices[1].Y;
						c.X = triangle.vertices[2].X; c.Z = triangle.vertices[2].Y;
						break;
					case ESurfaceType::WestWall:
						a.X = triangle.vertices[0].X; a.Z = triangle.vertices[0].Y;
						b.X = triangle.vertices[1].X; b.Z = triangle.vertices[1].Y;
						c.X = triangle.vertices[2].X; c.Z = triangle.vertices[2].Y;
						break;
					}
					DrawDebugLine(GetWorld(), a, b, FColor::Red, false, 60, 0, 6);
					DrawDebugLine(GetWorld(), b, c, FColor::Red, false, 60, 0, 6);
					DrawDebugLine(GetWorld(), c, a, FColor::Red, false, 60, 0, 6);
				}
			}
		}*/
	} }

	//UE_LOG(LogTemp, Log, TEXT("Amount of triangles: %d"), triangles.Num());

	//Array for the final triangle set
	TArray<FDTriangle> returnValue;

	//Create the full triangulation (without the Steiner points just yet)
	for (FDTriangle triangle : triangles) {
		//Check A: triangle does not use an artificial point
		if (!triangle.hasVertex(polygon.minVertex - FVector2D(1)) && !triangle.hasVertex(FVector2D(polygon.maxVertex.X, polygon.minVertex.Y) + FVector2D(1, -1))) {
			//Check B: Midpoints of all triangles are inside the polygon
			FVector2D midA = (triangle.vertices[0] + triangle.vertices[1]) / 2;
			FVector2D midB = (triangle.vertices[0] + triangle.vertices[2]) / 2;
			FVector2D midC = (triangle.vertices[1] + triangle.vertices[2]) / 2;
			if ((polygon.isInsidePolygon2D(midA) || polygon.isOnPolygon(midA))	
				&& (polygon.isInsidePolygon2D(midB) || polygon.isOnPolygon(midB))
				&& (polygon.isInsidePolygon2D(midC) || polygon.isOnPolygon(midC))) {
				returnValue.AddUnique(triangle);
			}
			else {
				//UE_LOG(LogTemp, Log, TEXT("Something is wrong with this triangle"));
				//if (!polygon.isOnPolygon(midA) && !polygon.isInsidePolygon2D(midA)) { UE_LOG(LogTemp, Log, TEXT("This midpoint (A) is neither in nor on the polygon: %s"), *midA.ToString()); }
				//if (!polygon.isOnPolygon(midB) && !polygon.isInsidePolygon2D(midB)) { UE_LOG(LogTemp, Log, TEXT("This midpoint (B) is neither in nor on the polygon: %s"), *midB.ToString()); }
				//if (!polygon.isOnPolygon(midC) && !polygon.isInsidePolygon2D(midC)) { UE_LOG(LogTemp, Log, TEXT("This midpoint (C) is neither in nor on the polygon: %s"), *midC.ToString()); }
			}
		}
	}

	for (FPolygonEdge front : advancingFront) {
		//DrawDebugLine(GetWorld(), FVector(front.vA, GetActorLocation().Z), FVector(front.vB, GetActorLocation().Z), FColor::Yellow, false, 60, 0, 2);
	}

	return returnValue;
}

bool ASurfaceArea::trianglesAreIllegal(FDTriangle a, FDTriangle b, FVector2D endA, FVector2D endB)
{
	//Find the two third vertices of the triangles
	FVector2D aC = FVector2D(0);
	FVector2D bC = FVector2D(0);
	if (a.vertices[0] != endA && a.vertices[0] != endB) { aC = a.vertices[0]; }
	else if (a.vertices[1] != endA && a.vertices[1] != endB) { aC = a.vertices[1]; }
	else if (a.vertices[2] != endA && a.vertices[2] != endB) { aC = a.vertices[2]; }
	if (b.vertices[0] != endA && b.vertices[0] != endB) { bC = b.vertices[0]; }
	else if (b.vertices[1] != endA && b.vertices[1] != endB) { bC = b.vertices[1]; }
	else if (b.vertices[2] != endA && b.vertices[2] != endB) { bC = b.vertices[2]; }

	//Calculate the angles of aC and bC
	FVector2D aC1 = endA - aC;
	FVector2D aC2 = endB - aC;
	FVector2D bC1 = endA - bC;
	FVector2D bC2 = endB - bC;
	float angleA = FMath::Acos((FVector2D::DotProduct(aC1, aC2)) / (aC1.Size() * aC2.Size())) * 180/PI;
	float angleB = FMath::Acos((FVector2D::DotProduct(bC1, bC2)) / (bC1.Size() * bC2.Size())) * 180/PI;

	//UE_LOG(LogTemp, Log, TEXT("Angle A: %f"), angleA);
	//UE_LOG(LogTemp, Log, TEXT("Angle B: %f"), angleB);

	return angleA + angleB > 180;
}

TArray<FFrontVertex> ASurfaceArea::findFrontVertices(TArray<FPolygonEdge> front)
{
	TArray<FFrontVertex> returnValue;
	for (FPolygonEdge edgeA : front) { for (FPolygonEdge edgeB : front) { if (!(edgeA == edgeB)) {
		if (edgeA.vA == edgeB.vA || edgeA.vA == edgeB.vB) {
			if (edgeA.vA == edgeB.vA) {
				FFrontVertex vertex = FFrontVertex(edgeA.vA, edgeA, edgeB);
				returnValue.AddUnique(vertex);
			}
			if (edgeA.vA == edgeB.vB) {
				FFrontVertex vertex = FFrontVertex(edgeA.vA, edgeA, edgeB);
				returnValue.AddUnique(vertex);
			}
		}
		else if (edgeA.vB == edgeB.vA || edgeB.vB == edgeB.vB) {
			if (edgeA.vB == edgeB.vA) {
				FFrontVertex vertex = FFrontVertex(edgeA.vB, edgeA, edgeB);
				returnValue.AddUnique(vertex);
			}
			if (edgeA.vB == edgeB.vB) {
				FFrontVertex vertex = FFrontVertex(edgeA.vB, edgeA, edgeB);
				returnValue.AddUnique(vertex);
			}
		}
		//else { UE_LOG(LogTemp, Log, TEXT("Edges %s to %s and %s to %s have no vertices in common"), *edgeA.vA.ToString(), *edgeA.vB.ToString(), *edgeB.vA.ToString(), *edgeB.vB.ToString()); }
	} } }
	return returnValue;
}

bool ASurfaceArea::checkIsVisible(FVector2D vertexA, FVector2D vertexB, TArray<FDTriangle> triangles)
{
	bool returnValue = true;

	//Since polygons are well-defined, if vertices A and B are on the same edge of the polygon, they are visible
	//if (polygon.hasEdge(vertexA, vertexB)) { return true; }

	//Ensure that the edges of the polygon are not intersected by the sight line
	for (int32 i = 0; i < polygon.vertices.Num(); i++) {
		FVector2D pVertexA = polygon.vertices[i];
		FVector2D pVertexB = polygon.vertices[0];
		if (i != polygon.vertices.Num() - 1) { pVertexB = polygon.vertices[i + 1]; }

		if (vertexA != pVertexA && vertexA != pVertexB && vertexB != pVertexA && vertexB != pVertexB) {
			if (polygon.intersects(vertexA, vertexB, pVertexA, pVertexB)) { 
				//UE_LOG(LogTemp, Log, TEXT("Intersection with polygon found: %s to %s and %s to %s"), *pVertexA.ToString(), *pVertexB.ToString(), *vertexA.ToString(), *vertexB.ToString()); 
				returnValue = false; break; }
		}
	}

	if (returnValue == false) { return false; }

	//Ensure that the triangles are not intersected by the sightline
	for (FDTriangle triangle : triangles) {
		if (vertexA != triangle.vertices[0] && vertexA != triangle.vertices[1] && vertexB != triangle.vertices[0] && vertexB != triangle.vertices[1]) {
			if (polygon.intersects(vertexA, vertexB, triangle.vertices[0], triangle.vertices[1])) { returnValue = false; break; }
		}
		if (vertexA != triangle.vertices[0] && vertexA != triangle.vertices[2] && vertexB != triangle.vertices[0] && vertexB != triangle.vertices[2]) {
			if (polygon.intersects(vertexA, vertexB, triangle.vertices[0], triangle.vertices[2])) { returnValue = false; break; }
		}
		if (vertexA != triangle.vertices[1] && vertexA != triangle.vertices[2] && vertexB != triangle.vertices[1] && vertexB != triangle.vertices[2]) {
			if (polygon.intersects(vertexA, vertexB, triangle.vertices[1], triangle.vertices[2])) { returnValue = false; break; }
		}
	}

	if (returnValue == false) {  return false; }

	//Ensure that the edge is not on a triangle already
	/*for (FDTriangle triangle : triangles) {
		if ((vertexA == triangle.vertices[0] && vertexB == triangle.vertices[1]) || (vertexB == triangle.vertices[0] && vertexA == triangle.vertices[1])
			|| (vertexA == triangle.vertices[0] && vertexB == triangle.vertices[2]) || (vertexB == triangle.vertices[0] && vertexA == triangle.vertices[2])
			|| (vertexA == triangle.vertices[1] && vertexB == triangle.vertices[2]) || (vertexB == triangle.vertices[1] && vertexA == triangle.vertices[2])) {
			returnValue = false; 
			break;
		}
	}*/

	//if (returnValue == false) {
		//DrawDebugLine(GetWorld(), FVector(vertexA, GetActorLocation().Z), FVector(vertexB, GetActorLocation().Z), FColor::Blue, false, 60, 0, 12); return false; }
	return returnValue;
}

void ASurfaceArea::addSteinerPoints()
{
	/*Go over the triangulation until there are no more obtuse triangles with three neighbours
	bool hasChanged = true;
	int32 tries = 0;
	while (hasChanged && tries < 100) {
		UE_LOG(LogTemp, Log, TEXT("Try to find new steiner points"));
		hasChanged = false;
		TArray<FDTriangle> triangles = baseDelaunay;

		//Check every triangle. Proceed if there is an obtuse angle
		for (FDTriangle triangle : triangles) { if (triangle.largestAngle() > 90.0f) {
			UE_LOG(LogTemp, Log, TEXT("There is a triangle with an obtuse angle: %s, %s, %s"), *triangle.vertices[0].ToString(), *triangle.vertices[1].ToString(), *triangle.vertices[2].ToString());
			FDTriangle neighbour01 = FDTriangle();
			FDTriangle neighbour02 = FDTriangle();
			FDTriangle neighbour12 = FDTriangle();

			//Find the neighbouring triangles
			for (FDTriangle neighbour : triangles) { if (!(neighbour == triangle)) {
				if (neighbour.hasEdge(triangle.vertices[0], triangle.vertices[1])) { neighbour01 = neighbour; }
				if (neighbour.hasEdge(triangle.vertices[0], triangle.vertices[2])) { neighbour02 = neighbour; }
				if (neighbour.hasEdge(triangle.vertices[1], triangle.vertices[2])) { neighbour12 = neighbour; }
			} }

			//Proceed if there are three neighbours
			if (!(neighbour01 == FDTriangle()) && !(neighbour02 == FDTriangle()) && !(neighbour12 == FDTriangle())) {
				UE_LOG(LogTemp, Log, TEXT("Three neighbours found"));
				//Check if the circumcenter is outside the polygon. If this is the case, proceed
				if (!polygon.isInsidePolygon2D(triangle.circumCenter)) {
					UE_LOG(LogTemp, Log, TEXT("Circumcenter is outside polygon: %s"), *triangle.circumCenter.ToString());
					//Find the polygon edge that is intersected by the ray from any edge of the triangle to the circumcenter
					FPolygonEdge intersectedEdge = polygon.intersectsEdge(triangle.circumCenter, (triangle.vertices[0] + triangle.vertices[1]) / 2);
					if (!(intersectedEdge == FPolygonEdge())) {
						UE_LOG(LogTemp, Log, TEXT("An intersected polygon edge has been found: %s to %s"), *intersectedEdge.vA.ToString(), *intersectedEdge.vB.ToString());
						//Find the vertex of this triangle that is also on the intersected edge. Proceed with the other two vertices
						FVector2D oppositeA = FVector2D();
						FVector2D oppositeB = FVector2D();
						if (intersectedEdge.hasVertex(triangle.vertices[0])) { oppositeA = triangle.vertices[1]; oppositeB = triangle.vertices[2]; }
						else if (intersectedEdge.hasVertex(triangle.vertices[1])) { oppositeA = triangle.vertices[0]; oppositeB = triangle.vertices[2]; }
						else if (intersectedEdge.hasVertex(triangle.vertices[2])) { oppositeA = triangle.vertices[1]; oppositeB = triangle.vertices[0]; }

						//Find the middle of this edge opposite of the common vertex and find the intersection between (that and the circumcenter) and the polygon
						FVector2D oppositeMid = (oppositeA + oppositeB) / 2;
						UE_LOG(LogTemp, Log, TEXT("Opposite mid: %s"), *oppositeMid.ToString());
						FVector2D steinerPoint = polygon.intersection(intersectedEdge.vA, intersectedEdge.vB, oppositeMid, triangle.circumCenter);
						UE_LOG(LogTemp, Log, TEXT("Steiner point: %s"), *steinerPoint.ToString());

						//Find the other triangle that needs to be fixed, which will be the one containing both vertices of the intersected edge and an opposite vertex
						FDTriangle otherTriangle = FDTriangle();
						for (FDTriangle other : triangles) {
							if (other.hasEdge(intersectedEdge.vA, intersectedEdge.vB) && (other.hasVertex(oppositeA) || other.hasVertex(oppositeB))) {
								otherTriangle = other;
								break;
							}
						}

						if (!(otherTriangle == FDTriangle())) {
							UE_LOG(LogTemp, Log, TEXT("The neighbouring triangle has been located"));
							baseDelaunay.Remove(triangle);
							baseDelaunay.Remove(otherTriangle);
							baseDelaunay.AddUnique(FDTriangle({ steinerPoint, oppositeA, oppositeB }));

							if ((intersectedEdge.vA - oppositeA).Size() < (intersectedEdge.vB - oppositeA).Size()) {
								baseDelaunay.AddUnique(FDTriangle({ steinerPoint, intersectedEdge.vA, oppositeA }));
								baseDelaunay.AddUnique(FDTriangle({ steinerPoint, intersectedEdge.vB, oppositeB }));
							}
							else {
								baseDelaunay.AddUnique(FDTriangle({ steinerPoint, intersectedEdge.vB, oppositeA }));
								baseDelaunay.AddUnique(FDTriangle({ steinerPoint, intersectedEdge.vA, oppositeB }));
							}
							hasChanged = true;
						}
						else { UE_LOG(LogTemp, Log, TEXT("No other triangle found?")); }
					}
				}
				else { UE_LOG(LogTemp, Log, TEXT("Circumcenter is inside polygon: %s"), *triangle.circumCenter.ToString()); }
			}
		} }

		tries++;
	}*/
}

/*TArray<FCDTriangle> ASurfaceArea::medaxifyDelaunay()
{
	TArray<FCDTriangle> returnValue;
	for (FDTriangle triangle : baseDelaunay) { 
		EDTriangleType type;
		int32 polygonEdges = 0;
		for (int32 i = 0; i < triangle.vertices.Num(); i++) {
			int32 j = (i + 1) % triangle.vertices.Num();

			if (polygon.isOnPolygon(triangle.vertices[i]) && polygon.isOnPolygon(triangle.vertices[j]) && polygon.isOnPolygon((triangle.vertices[i] + triangle.vertices[j]) / 2)) { UE_LOG(LogTemp, Log, TEXT("Edge detected")); polygonEdges++; }
		}

		if (polygonEdges == 0) { type = EDTriangleType::C; }
		if (polygonEdges == 1) { type = EDTriangleType::A; }
		if (polygonEdges == 2) { type = EDTriangleType::B; }

		returnValue.AddUnique(FCDTriangle(triangle, type)); 
	}

	//To redirect triangles
	FVector2D previousHelperA = FVector2D();
	FVector2D previousHelperB = FVector2D();

	int32 changes = 0;
	//Find each triplet of vertices, where we do operations on the vertex with index j
	for (int32 i = 0; i < polygon.vertices.Num(); i++) {
		UE_LOG(LogTemp, Log, TEXT("Checking new vertex"));
		int32 j = (i + 1) % polygon.vertices.Num();
		int32 k = (j + 1) % polygon.vertices.Num();

		FVector2D leftVertex = polygon.vertices[i];
		FVector2D convexVertex = polygon.vertices[j];
		FVector2D rightVertex = polygon.vertices[k];

		//if (changes == changesDT) {
		//	DrawDebugLine(GetWorld(), FVector(leftVertex, GetActorLocation().Z), FVector(convexVertex, GetActorLocation().Z), FColor::Blue, false, 15, 0, 6);
		//	DrawDebugLine(GetWorld(), FVector(rightVertex, GetActorLocation().Z), FVector(convexVertex, GetActorLocation().Z), FColor::Orange, false, 15, 0, 6);
		//}

		if (findHelperOrientation(leftVertex, convexVertex, rightVertex) > 0) { UE_LOG(LogTemp, Log, TEXT("Orientation is clockwise")); }
		if (findHelperOrientation(leftVertex, convexVertex, rightVertex) > 0) { FVector2D temp = leftVertex; leftVertex = rightVertex; rightVertex = temp; }

		//UE_LOG(LogTemp, Log, TEXT("leftVertex: %s"), *leftVertex.ToString());
		//UE_LOG(LogTemp, Log, TEXT("convexVertex: %s"), *convexVertex.ToString());
		//UE_LOG(LogTemp, Log, TEXT("rightVertex: %s"), *rightVertex.ToString());

		//Find out if the convex vertex is actually convex. Do so by finding a point near the convex vertex on the line towards the midpoint of the opposite side
		FVector2D midHypothenuse = (leftVertex + rightVertex) / 2;
		FVector2D nearConvexVertex = 0.01f * midHypothenuse + 0.99f * convexVertex;

		if (polygon.isInsidePolygon2D(nearConvexVertex)) {
			UE_LOG(LogTemp, Log, TEXT("Vertex is convex"));
			//Find the distance towards the nearest point. This may or may not be one of the adjacent vertices
			float shortestDist = (leftVertex - convexVertex).Size();
			if ((rightVertex - convexVertex).Size() < shortestDist) {
				UE_LOG(LogTemp, Log, TEXT("Right is shorter"));
				shortestDist = (rightVertex - convexVertex).Size();
				FVector2D temp = rightVertex;
				rightVertex = leftVertex;
				leftVertex = temp;
			}
			TArray<FCDTriangle> relevantTriangles;
			for (FCDTriangle triangle : returnValue) { if (triangle.triangle.hasVertex(convexVertex)) { relevantTriangles.AddUnique(triangle); } }
			relevantTriangles.Sort(FSortByTriangleAngle(rightVertex, convexVertex));
			UE_LOG(LogTemp, Log, TEXT("Amount of relevant triangles: %d"), relevantTriangles.Num());
			/*
			if (changes == changesDT) {
				int32 index = 0;
				FColor color = FColor::Orange;
				for (FCDTriangle cTriangle : relevantTriangles) {
					FDTriangle triangle = cTriangle.triangle;
					FVector a = GetActorLocation();
					FVector b = GetActorLocation();
					FVector c = GetActorLocation();
					switch (surface) {
					case ESurfaceType::Ceiling:
						a.X = triangle.vertices[0].X; a.Y = triangle.vertices[0].Y;
						b.X = triangle.vertices[1].X; b.Y = triangle.vertices[1].Y;
						c.X = triangle.vertices[2].X; c.Y = triangle.vertices[2].Y;
						break;
					case ESurfaceType::Floor:
						a.X = triangle.vertices[0].X; a.Y = triangle.vertices[0].Y;
						b.X = triangle.vertices[1].X; b.Y = triangle.vertices[1].Y;
						c.X = triangle.vertices[2].X; c.Y = triangle.vertices[2].Y;
						break;
					case ESurfaceType::Stairs:
						a.X = triangle.vertices[0].X; a.Y = triangle.vertices[0].Y;
						b.X = triangle.vertices[1].X; b.Y = triangle.vertices[1].Y;
						c.X = triangle.vertices[2].X; c.Y = triangle.vertices[2].Y;
						break;
					case ESurfaceType::StairsCeiling:
						a.X = triangle.vertices[0].X; a.Y = triangle.vertices[0].Y;
						b.X = triangle.vertices[1].X; b.Y = triangle.vertices[1].Y;
						c.X = triangle.vertices[2].X; c.Y = triangle.vertices[2].Y;
						break;
					case ESurfaceType::NorthWall:
						a.Z = triangle.vertices[0].X; a.Y = triangle.vertices[0].Y;
						b.Z = triangle.vertices[1].X; b.Y = triangle.vertices[1].Y;
						c.Z = triangle.vertices[2].X; c.Y = triangle.vertices[2].Y;
						break;
					case ESurfaceType::SouthWall:
						a.Z = triangle.vertices[0].X; a.Y = triangle.vertices[0].Y;
						b.Z = triangle.vertices[1].X; b.Y = triangle.vertices[1].Y;
						c.Z = triangle.vertices[2].X; c.Y = triangle.vertices[2].Y;
						break;
					case ESurfaceType::EastWall:
						a.X = triangle.vertices[0].X; a.Z = triangle.vertices[0].Y;
						b.X = triangle.vertices[1].X; b.Z = triangle.vertices[1].Y;
						c.X = triangle.vertices[2].X; c.Z = triangle.vertices[2].Y;
						break;
					case ESurfaceType::WestWall:
						a.X = triangle.vertices[0].X; a.Z = triangle.vertices[0].Y;
						b.X = triangle.vertices[1].X; b.Z = triangle.vertices[1].Y;
						c.X = triangle.vertices[2].X; c.Z = triangle.vertices[2].Y;
						break;
					}
					DrawDebugLine(GetWorld(), a, b, color, false, 15, 0, 6);
					DrawDebugLine(GetWorld(), b, c, color, false, 15, 0, 6);
					DrawDebugLine(GetWorld(), c, a, color, false, 15, 0, 6);
					if (index == 0) { color = FColor::Magenta; }
					if (index == 1) { color = FColor::Blue; }
					if (index == 2) { color = FColor::Yellow; }
					if (index == 3) { color = FColor::Red; }
					if (index == 4) { color = FColor::Green; }
					if (index == 4) { color = FColor::Purple; }
					if (index == 4) { color = FColor::Cyan; }
					index++;
				}
			}
			
			//Divide the distance by two to find the halfdistance of the segment from the vertex to the nearest vertex
			shortestDist /= 2;

			//Find the points on the edges that have the convex vertex that are the shortest distance away from it
			float alphaA = shortestDist / (leftVertex - convexVertex).Size();
			float alphaB = shortestDist / (rightVertex - convexVertex).Size();

			FVector2D helperA = (1 - alphaA) * convexVertex + alphaA * leftVertex;
			FVector2D helperB = (1 - alphaB) * convexVertex + alphaB * rightVertex;

			//DrawDebugPoint(GetWorld(), FVector(helperA, GetActorLocation().Z), 20, FColor::Blue, false, 15, 0);
			//DrawDebugPoint(GetWorld(), FVector(helperB, GetActorLocation().Z), 20, FColor::Orange, false, 15, 0);

			float cumulativeAngle = 0;
			//All triangles that are below the half bisector (cumulatively) are redirected towards helperA. The others to helperB

			/*for (FCDTriangle cTriangle : relevantTriangles) {
				FDTriangle triangle = cTriangle.triangle;
				EDTriangleType type = EDTriangleType::C;
				if (triangle.vertices[0] == convexVertex) {
					float angle = FMath::Acos(FVector2D::DotProduct(triangle.vertices[1] - triangle.vertices[0], triangle.vertices[2] - triangle.vertices[0]) / ((triangle.vertices[1] - triangle.vertices[0]).Size() * (triangle.vertices[2] - triangle.vertices[0]).Size())) * 180 / PI;
						if (triangle.isNearer(helperA, helperB, convexVertex)) {
							returnValue.Remove(cTriangle);
							if (triangle.vertices[1] != helperA && triangle.vertices[2] != helperA) {
								int32 polygonEdges = 0;
								for (int32 p = 0; p < polygon.vertices.Num(); p++) {
									int32 q = (p + 1) % polygon.vertices.Num();
									if (triangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
								}

								if (polygonEdges == 0) { type = EDTriangleType::C; }
								if (polygonEdges == 1) { type = EDTriangleType::A; }
								if (polygonEdges == 2) { type = EDTriangleType::B; }
								returnValue.AddUnique(FCDTriangle(FDTriangle({ helperA, triangle.vertices[1], triangle.vertices[2] }), type));
							}
						}
						else {
							returnValue.Remove(cTriangle);
							if (triangle.vertices[1] != helperB && triangle.vertices[2] != helperB) {
								int32 polygonEdges = 0;
								for (int32 p = 0; p < polygon.vertices.Num(); p++) {
									int32 q = (p + 1) % polygon.vertices.Num();
									if (triangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
								}

								if (polygonEdges == 0) { type = EDTriangleType::C; }
								if (polygonEdges == 1) { type = EDTriangleType::A; }
								if (polygonEdges == 2) { type = EDTriangleType::B; }
								returnValue.AddUnique(FCDTriangle(FDTriangle({ helperB, triangle.vertices[1], triangle.vertices[2] }), type));
							}
						}

				}
				else if (triangle.vertices[1] == convexVertex) {
					float angle = FMath::Acos(FVector2D::DotProduct(triangle.vertices[0] - triangle.vertices[1], triangle.vertices[2] - triangle.vertices[1]) / ((triangle.vertices[0] - triangle.vertices[1]).Size() * (triangle.vertices[2] - triangle.vertices[1]).Size())) * 180 / PI;
					if (triangle.isNearer(helperA, helperB, convexVertex)) {
							returnValue.Remove(cTriangle);
							if (triangle.vertices[0] != helperA && triangle.vertices[2] != helperA) {
								int32 polygonEdges = 0;
								for (int32 p = 0; p < polygon.vertices.Num(); p++) {
									int32 q = (p + 1) % polygon.vertices.Num();
									if (triangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
								}

								if (polygonEdges == 0) { type = EDTriangleType::C; }
								if (polygonEdges == 1) { type = EDTriangleType::A; }
								if (polygonEdges == 2) { type = EDTriangleType::B; }
								returnValue.AddUnique(FCDTriangle(FDTriangle({ helperA, triangle.vertices[0], triangle.vertices[2] }), type));
							}
						}
					else {
							returnValue.Remove(cTriangle);
							if (triangle.vertices[0] != helperB && triangle.vertices[2] != helperB) {
								int32 polygonEdges = 0;
								for (int32 p = 0; p < polygon.vertices.Num(); p++) {
									int32 q = (p + 1) % polygon.vertices.Num();
									if (triangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
								}

								if (polygonEdges == 0) { type = EDTriangleType::C; }
								if (polygonEdges == 1) { type = EDTriangleType::A; }
								if (polygonEdges == 2) { type = EDTriangleType::B; }
								returnValue.AddUnique(FCDTriangle(FDTriangle({ helperB, triangle.vertices[0], triangle.vertices[2] }), type));
							}
						}
				}
				else if (triangle.vertices[2] == convexVertex) {
					float angle = FMath::Acos(FVector2D::DotProduct(triangle.vertices[1] - triangle.vertices[2], triangle.vertices[0] - triangle.vertices[2]) / ((triangle.vertices[1] - triangle.vertices[2]).Size() * (triangle.vertices[0] - triangle.vertices[2]).Size())) * 180 / PI;
					if (triangle.isNearer(helperA, helperB, convexVertex)) {
							returnValue.Remove(cTriangle);
							if (triangle.vertices[0] != helperA && triangle.vertices[1] != helperA) {
								int32 polygonEdges = 0;
								for (int32 p = 0; p < polygon.vertices.Num(); p++) {
									int32 q = (p + 1) % polygon.vertices.Num();
									if (triangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
								}

								if (polygonEdges == 0) { type = EDTriangleType::C; }
								if (polygonEdges == 1) { type = EDTriangleType::A; }
								if (polygonEdges == 2) { type = EDTriangleType::B; }
								returnValue.AddUnique(FCDTriangle(FDTriangle({ helperA, triangle.vertices[0], triangle.vertices[1] }), type));
							}
						}
					else {
							returnValue.Remove(cTriangle);
							if (triangle.vertices[0] != helperB && triangle.vertices[1] != helperB) {
								int32 polygonEdges = 0;
								for (int32 p = 0; p < polygon.vertices.Num(); p++) {
									int32 q = (p + 1) % polygon.vertices.Num();
									if (triangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
								}

								if (polygonEdges == 0) { type = EDTriangleType::C; }
								if (polygonEdges == 1) { type = EDTriangleType::A; }
								if (polygonEdges == 2) { type = EDTriangleType::B; }
								returnValue.AddUnique(FCDTriangle(FDTriangle({ helperB, triangle.vertices[0], triangle.vertices[1] }), type));
							}
						}
				}
			}
			
			for (int32 t = 0; t < relevantTriangles.Num(); t++) {
				FCDTriangle cTriangle = relevantTriangles[t];
				FDTriangle triangle = cTriangle.triangle;
				EDTriangleType type = EDTriangleType::C;
				if (triangle.vertices[0] == convexVertex) {
					UE_LOG(LogTemp, Log, TEXT("Handle 0"));
					float angle = FMath::Acos(FVector2D::DotProduct(triangle.vertices[1] - triangle.vertices[0], triangle.vertices[2] - triangle.vertices[0]) / ((triangle.vertices[1] - triangle.vertices[0]).Size() * (triangle.vertices[2] - triangle.vertices[0]).Size())) * 180 / PI;
					cumulativeAngle += angle;
					if (cumulativeAngle < 45) {
						returnValue.Remove(cTriangle);
						int32 polygonEdges = 0;
						FDTriangle newTriangle = FDTriangle({ triangle.vertices[1], helperB, triangle.vertices[2] });
						for (int32 p = 0; p < polygon.vertices.Num(); p++) {
							int32 q = (p + 1) % polygon.vertices.Num();
							if (newTriangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
						}

						if (polygonEdges == 0) { type = EDTriangleType::C; }
						if (polygonEdges == 1) { type = EDTriangleType::A; }
						if (polygonEdges == 2) { type = EDTriangleType::B; }
						returnValue.AddUnique(FCDTriangle(newTriangle, type));
					}
					else {
						returnValue.Remove(cTriangle);
						int32 polygonEdges = 0;
						FDTriangle newTriangle = FDTriangle({ triangle.vertices[1], helperA, triangle.vertices[2] });
						for (int32 p = 0; p < polygon.vertices.Num(); p++) {
							int32 q = (p + 1) % polygon.vertices.Num();
							if (newTriangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
						}

						if (polygonEdges == 0) { type = EDTriangleType::C; }
						if (polygonEdges == 1) { type = EDTriangleType::A; }
						if (polygonEdges == 2) { type = EDTriangleType::B; }
						returnValue.AddUnique(FCDTriangle(newTriangle, type));
					}
				}
				else if (triangle.vertices[1] == convexVertex) {
					UE_LOG(LogTemp, Log, TEXT("Handle 1"));
					float angle = FMath::Acos(FVector2D::DotProduct(triangle.vertices[0] - triangle.vertices[1], triangle.vertices[2] - triangle.vertices[1]) / ((triangle.vertices[0] - triangle.vertices[1]).Size() * (triangle.vertices[2] - triangle.vertices[1]).Size())) * 180 / PI;
					cumulativeAngle += angle;
					if (cumulativeAngle < 45) {
						returnValue.Remove(cTriangle);
						int32 polygonEdges = 0;
						FDTriangle newTriangle = FDTriangle({ triangle.vertices[0], helperB, triangle.vertices[2] });
						for (int32 p = 0; p < polygon.vertices.Num(); p++) {
							int32 q = (p + 1) % polygon.vertices.Num();
							if (newTriangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
						}

						if (polygonEdges == 0) { type = EDTriangleType::C; }
						if (polygonEdges == 1) { type = EDTriangleType::A; }
						if (polygonEdges == 2) { type = EDTriangleType::B; }
						returnValue.AddUnique(FCDTriangle(newTriangle, type));
					}
					else {
						returnValue.Remove(cTriangle);
						int32 polygonEdges = 0;
						FDTriangle newTriangle = FDTriangle({ triangle.vertices[0], helperA, triangle.vertices[2] });
						for (int32 p = 0; p < polygon.vertices.Num(); p++) {
							int32 q = (p + 1) % polygon.vertices.Num();
							if (newTriangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
						}

						if (polygonEdges == 0) { type = EDTriangleType::C; }
						if (polygonEdges == 1) { type = EDTriangleType::A; }
						if (polygonEdges == 2) { type = EDTriangleType::B; }
						returnValue.AddUnique(FCDTriangle(newTriangle, type));
					}
				}
				else if (triangle.vertices[2] == convexVertex) {
					UE_LOG(LogTemp, Log, TEXT("Handle 2"));
					float angle = FMath::Acos(FVector2D::DotProduct(triangle.vertices[0] - triangle.vertices[2], triangle.vertices[1] - triangle.vertices[2]) / ((triangle.vertices[0] - triangle.vertices[2]).Size() * (triangle.vertices[1] - triangle.vertices[2]).Size())) * 180 / PI;
					cumulativeAngle += angle;
					if (cumulativeAngle < 45) {
						returnValue.Remove(cTriangle);
						int32 polygonEdges = 0;
						FDTriangle newTriangle = FDTriangle({ triangle.vertices[0], helperB, triangle.vertices[1] });
						for (int32 p = 0; p < polygon.vertices.Num(); p++) {
							int32 q = (p + 1) % polygon.vertices.Num();
							if (newTriangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
						}

						if (polygonEdges == 0) { type = EDTriangleType::C; }
						if (polygonEdges == 1) { type = EDTriangleType::A; }
						if (polygonEdges == 2) { type = EDTriangleType::B; }
						returnValue.AddUnique(FCDTriangle(newTriangle, type));
					}
					else {
						returnValue.Remove(cTriangle);
						int32 polygonEdges = 0;
						FDTriangle newTriangle = FDTriangle({ triangle.vertices[0], helperA, triangle.vertices[1] });
						for (int32 p = 0; p < polygon.vertices.Num(); p++) {
							int32 q = (p + 1) % polygon.vertices.Num();
							if (newTriangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
						}

						if (polygonEdges == 0) { type = EDTriangleType::C; }
						if (polygonEdges == 1) { type = EDTriangleType::A; }
						if (polygonEdges == 2) { type = EDTriangleType::B; }
						returnValue.AddUnique(FCDTriangle(newTriangle, type));
					}
				}
			}

			/*for (FCDTriangle cTriangle : relevantTriangles) {
				FDTriangle triangle = cTriangle.triangle;
				EDTriangleType type = EDTriangleType::C;
				if (triangle.vertices[0] == convexVertex) {
					UE_LOG(LogTemp, Log, TEXT("Handle 0"));
					float angle = FMath::Acos(FVector2D::DotProduct(triangle.vertices[1] - triangle.vertices[0], triangle.vertices[2] - triangle.vertices[0]) / ((triangle.vertices[1] - triangle.vertices[0]).Size() * (triangle.vertices[2] - triangle.vertices[0]).Size())) * 180 / PI;
					cumulativeAngle += angle;
					if (cumulativeAngle < 45) {
						UE_LOG(LogTemp, Log, TEXT("Angle sufficiently small"));
						returnValue.Remove(cTriangle);
						if (triangle.vertices[1] != helperA && triangle.vertices[2] != helperA) {
							if (!triangle.intersectsTriangle(polygon, FDTriangle({ helperA, triangle.vertices[1], triangle.vertices[2] }))) {
								//if (triangle.vertices[1] != helperB && triangle.vertices[2] != helperB) {
									int32 polygonEdges = 0;
									for (int32 p = 0; p < polygon.vertices.Num(); p++) {
										int32 q = (p + 1) % polygon.vertices.Num();
										if (triangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
									}

									if (polygonEdges == 0) { type = EDTriangleType::C; }
									if (polygonEdges == 1) { type = EDTriangleType::A; }
									if (polygonEdges == 2) { type = EDTriangleType::B; }
									returnValue.AddUnique(FCDTriangle(FDTriangle({ helperA, triangle.vertices[1], triangle.vertices[2] }), type));
								//}
							}
							else {
								//if (triangle.vertices[1] != helperB && triangle.vertices[2] != helperB) {
									int32 polygonEdges = 0;
									for (int32 p = 0; p < polygon.vertices.Num(); p++) {
										int32 q = (p + 1) % polygon.vertices.Num();
										if (triangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
									}

									if (polygonEdges == 0) { type = EDTriangleType::C; }
									if (polygonEdges == 1) { type = EDTriangleType::A; }
									if (polygonEdges == 2) { type = EDTriangleType::B; }
									returnValue.AddUnique(FCDTriangle(FDTriangle({ helperB, triangle.vertices[1], triangle.vertices[2] }), type));
								//}
							}
						}
					}
					else {
						UE_LOG(LogTemp, Log, TEXT("Angle too large"));
						returnValue.Remove(cTriangle);
						if (triangle.vertices[1] != helperB && triangle.vertices[2] != helperB) {
							if (!triangle.intersectsTriangle(polygon, FDTriangle({ helperB, triangle.vertices[1], triangle.vertices[2] }))) {
								//if (triangle.vertices[1] != helperA && triangle.vertices[2] != helperA) {
									int32 polygonEdges = 0;
									for (int32 p = 0; p < polygon.vertices.Num(); p++) {
										int32 q = (p + 1) % polygon.vertices.Num();
										if (triangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
									}

									if (polygonEdges == 0) { type = EDTriangleType::C; }
									if (polygonEdges == 1) { type = EDTriangleType::A; }
									if (polygonEdges == 2) { type = EDTriangleType::B; }
									returnValue.AddUnique(FCDTriangle(FDTriangle({ helperB, triangle.vertices[1], triangle.vertices[2] }), type));
								//}
							}
							else {
								//if (triangle.vertices[1] != helperA && triangle.vertices[2] != helperA) {
									int32 polygonEdges = 0;
									for (int32 p = 0; p < polygon.vertices.Num(); p++) {
										int32 q = (p + 1) % polygon.vertices.Num();
										if (triangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
									}

									if (polygonEdges == 0) { type = EDTriangleType::C; }
									if (polygonEdges == 1) { type = EDTriangleType::A; }
									if (polygonEdges == 2) { type = EDTriangleType::B; }
									returnValue.AddUnique(FCDTriangle(FDTriangle({ helperA, triangle.vertices[1], triangle.vertices[2] }), type));
								//}
							}
						}
					}
				}
				else if (triangle.vertices[1] == convexVertex) {
					UE_LOG(LogTemp, Log, TEXT("Handle 1"));
					float angle = FMath::Acos(FVector2D::DotProduct(triangle.vertices[0] - triangle.vertices[1], triangle.vertices[2] - triangle.vertices[1]) / ((triangle.vertices[0] - triangle.vertices[1]).Size() * (triangle.vertices[2] - triangle.vertices[1]).Size())) * 180 / PI;
					cumulativeAngle += angle;
					if (cumulativeAngle < 45) {
						UE_LOG(LogTemp, Log, TEXT("Angle sufficiently small"));
						returnValue.Remove(cTriangle);
						if (triangle.vertices[0] != helperB && triangle.vertices[2] != helperB) {
							if (!triangle.intersectsTriangle(polygon, FDTriangle({ triangle.vertices[0], helperB, triangle.vertices[2] }))) {
								//if (triangle.vertices[0] != helperA && triangle.vertices[*2] != helperA) {
									int32 polygonEdges = 0;
									for (int32 p = 0; p < polygon.vertices.Num(); p++) {
										int32 q = (p + 1) % polygon.vertices.Num();
										if (triangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
									}

									if (polygonEdges == 0) { type = EDTriangleType::C; }
									if (polygonEdges == 1) { type = EDTriangleType::A; }
									if (polygonEdges == 2) { type = EDTriangleType::B; }
									returnValue.AddUnique(FCDTriangle(FDTriangle({ triangle.vertices[0], helperB, triangle.vertices[2] }), type));
								//}
							}
							else {
								//if (triangle.vertices[0] != helperA && triangle.vertices[2] != helperA) {
									int32 polygonEdges = 0;
									for (int32 p = 0; p < polygon.vertices.Num(); p++) {
										int32 q = (p + 1) % polygon.vertices.Num();
										if (triangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
									}

									if (polygonEdges == 0) { type = EDTriangleType::C; }
									if (polygonEdges == 1) { type = EDTriangleType::A; }
									if (polygonEdges == 2) { type = EDTriangleType::B; }
									returnValue.AddUnique(FCDTriangle(FDTriangle({ triangle.vertices[0], helperA, triangle.vertices[2] }), type));
								//}
							}
						}
					}
					else {
						UE_LOG(LogTemp, Log, TEXT("Angle too large"));
						returnValue.Remove(cTriangle);
						if (triangle.vertices[0] != helperA && triangle.vertices[2] != helperA) {
							if (!triangle.intersectsTriangle(polygon, FDTriangle({ triangle.vertices[0], helperA, triangle.vertices[2] }))) {
								//if (triangle.vertices[0] != helperB && triangle.vertices[2] != helperB) {
									int32 polygonEdges = 0;
									for (int32 p = 0; p < polygon.vertices.Num(); p++) {
										int32 q = (p + 1) % polygon.vertices.Num();
										if (triangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
									}

									if (polygonEdges == 0) { type = EDTriangleType::C; }
									if (polygonEdges == 1) { type = EDTriangleType::A; }
									if (polygonEdges == 2) { type = EDTriangleType::B; }
									returnValue.AddUnique(FCDTriangle(FDTriangle({ triangle.vertices[0], helperA, triangle.vertices[2] }), type));
								//}
							}
							else {
								//if (triangle.vertices[0] != helperB && triangle.vertices[2] != helperB) {
									int32 polygonEdges = 0;
									for (int32 p = 0; p < polygon.vertices.Num(); p++) {
										int32 q = (p + 1) % polygon.vertices.Num();
										if (triangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
									}

									if (polygonEdges == 0) { type = EDTriangleType::C; }
									if (polygonEdges == 1) { type = EDTriangleType::A; }
									if (polygonEdges == 2) { type = EDTriangleType::B; }
									returnValue.AddUnique(FCDTriangle(FDTriangle({ triangle.vertices[0], helperB, triangle.vertices[2] }), type));
								//}
							}
						}
					}
				}
				else if (triangle.vertices[2] == convexVertex) {
					UE_LOG(LogTemp, Log, TEXT("Handle 2"));
					float angle = FMath::Acos(FVector2D::DotProduct(triangle.vertices[1] - triangle.vertices[2], triangle.vertices[0] - triangle.vertices[2]) / ((triangle.vertices[1] - triangle.vertices[2]).Size() * (triangle.vertices[0] - triangle.vertices[2]).Size())) * 180 / PI;
					cumulativeAngle += angle;
					if (cumulativeAngle < 45) {
						UE_LOG(LogTemp, Log, TEXT("Angle sufficiently small"));
						returnValue.Remove(cTriangle);
						if (triangle.vertices[0] != helperB && triangle.vertices[1] != helperB) {
							if (!triangle.intersectsTriangle(polygon, FDTriangle({ triangle.vertices[0], triangle.vertices[1], helperB }))) {
								//if (triangle.vertices[0] != helperA && triangle.vertices[1] != helperA) {
									int32 polygonEdges = 0;
									for (int32 p = 0; p < polygon.vertices.Num(); p++) {
										int32 q = (p + 1) % polygon.vertices.Num();
										if (triangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
									}

									if (polygonEdges == 0) { type = EDTriangleType::C; }
									if (polygonEdges == 1) { type = EDTriangleType::A; }
									if (polygonEdges == 2) { type = EDTriangleType::B; }
									returnValue.AddUnique(FCDTriangle(FDTriangle({ triangle.vertices[0], triangle.vertices[1], helperB }), type));
								//}
							}
							else {
								//if (triangle.vertices[0] != helperA && triangle.vertices[1] != helperA) {
									int32 polygonEdges = 0;
									for (int32 p = 0; p < polygon.vertices.Num(); p++) {
										int32 q = (p + 1) % polygon.vertices.Num();
										if (triangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
									}

									if (polygonEdges == 0) { type = EDTriangleType::C; }
									if (polygonEdges == 1) { type = EDTriangleType::A; }
									if (polygonEdges == 2) { type = EDTriangleType::B; }
									returnValue.AddUnique(FCDTriangle(FDTriangle({ triangle.vertices[0], triangle.vertices[1], helperA }), type));
								//}
							}
						}
					}
					else {
						UE_LOG(LogTemp, Log, TEXT("Angle too large"));
						returnValue.Remove(cTriangle);
						if (triangle.vertices[0] != helperA && triangle.vertices[1] != helperA) {
							if (!triangle.intersectsTriangle(polygon, FDTriangle({ triangle.vertices[0], triangle.vertices[1], helperA }))) {
								//if (triangle.vertices[0] != helperB && triangle.vertices[1] != helperB) {
									int32 polygonEdges = 0;
									for (int32 p = 0; p < polygon.vertices.Num(); p++) {
										int32 q = (p + 1) % polygon.vertices.Num();
										if (triangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
									}

									if (polygonEdges == 0) { type = EDTriangleType::C; }
									if (polygonEdges == 1) { type = EDTriangleType::A; }
									if (polygonEdges == 2) { type = EDTriangleType::B; }
									returnValue.AddUnique(FCDTriangle(FDTriangle({ triangle.vertices[0], triangle.vertices[1], helperA }), type));
								//}
							}
							else {
								//if (triangle.vertices[0] != helperB && triangle.vertices[1] != helperB) {
									int32 polygonEdges = 0;
									for (int32 p = 0; p < polygon.vertices.Num(); p++) {
										int32 q = (p + 1) % polygon.vertices.Num();
										if (triangle.hasEdge(polygon.vertices[p], polygon.vertices[q])) { polygonEdges++; }
									}

									if (polygonEdges == 0) { type = EDTriangleType::C; }
									if (polygonEdges == 1) { type = EDTriangleType::A; }
									if (polygonEdges == 2) { type = EDTriangleType::B; }
									returnValue.AddUnique(FCDTriangle(FDTriangle({ triangle.vertices[0], triangle.vertices[1], helperB }), type));
								//}
							}
						}
					}
				}
			}
			
			//Add the final two triangles that use both Steiner points. First find the third vertex of the triangle opposite of the one with the convex vertex
			FVector2D thirdVertex = FVector2D();

			for (FCDTriangle cTriangleA : returnValue) {
				for (FCDTriangle cTriangleB : returnValue) {
					if (!(cTriangleA == cTriangleB)) {
						FDTriangle triangleA = cTriangleA.triangle;
						FDTriangle triangleB = cTriangleB.triangle;
						if (triangleA.hasVertex(helperA) && triangleB.hasVertex(helperB)) {
							if (triangleA.vertices[0] != helperA) {
								if (triangleA.vertices[0] == triangleB.vertices[0] || triangleA.vertices[0] == triangleB.vertices[1] || triangleA.vertices[0] == triangleB.vertices[2]) {
									thirdVertex = triangleA.vertices[0];
								}
							}
							if (triangleA.vertices[1] != helperA) {
								if (triangleA.vertices[1] == triangleB.vertices[0] || triangleA.vertices[1] == triangleB.vertices[1] || triangleA.vertices[1] == triangleB.vertices[2]) {
									thirdVertex = triangleA.vertices[1];
								}
							}
							if (triangleA.vertices[2] != helperA) {
								if (triangleA.vertices[2] == triangleB.vertices[0] || triangleA.vertices[2] == triangleB.vertices[1] || triangleA.vertices[2] == triangleB.vertices[2]) {
									thirdVertex = triangleA.vertices[2];
								}
							}
						}
					}
				}
			}

			returnValue.AddUnique(FCDTriangle(FDTriangle({ convexVertex, helperA, helperB }), EDTriangleType::B));
			bool edge1OnPolygon = polygon.isOnPolygon(helperA) && polygon.isOnPolygon(helperB) && polygon.isOnPolygon((helperA + helperB) / 2);
			bool edge2OnPolygon = polygon.isOnPolygon(helperA) && polygon.isOnPolygon(thirdVertex) && polygon.isOnPolygon((helperA + thirdVertex) / 2);
			bool edge3OnPolygon = polygon.isOnPolygon(helperB) && polygon.isOnPolygon(thirdVertex) && polygon.isOnPolygon((helperB + thirdVertex) / 2);
			if ((edge1OnPolygon && edge2OnPolygon) || (edge1OnPolygon && edge3OnPolygon) || (edge2OnPolygon && edge3OnPolygon)){
				returnValue.AddUnique(FCDTriangle(FDTriangle({ helperA, helperB, thirdVertex }), EDTriangleType::B));
			}
			else if (edge1OnPolygon || edge2OnPolygon || edge3OnPolygon) {
				returnValue.AddUnique(FCDTriangle(FDTriangle({ helperA, helperB, thirdVertex }), EDTriangleType::A));
			}
			else { returnValue.AddUnique(FCDTriangle(FDTriangle({ helperA, helperB, thirdVertex }), EDTriangleType::C)); }

			//If two B type triangles are directly touching, ensure that a C type triangle is put beside it!
			/*for (FCDTriangle cTriangleA : returnValue) {
				if (cTriangleA.type == EDTriangleType::B) {
					for (FCDTriangle cTriangleB : returnValue) {
						if (cTriangleB.type == EDTriangleType::B) {
							if (!(cTriangleA == cTriangleB)) {
								if (cTriangleA.triangle.hasVertex(cTriangleB.triangle.vertices[0]) ||
									cTriangleA.triangle.hasVertex(cTriangleB.triangle.vertices[1]) ||
									cTriangleA.triangle.hasVertex(cTriangleB.triangle.vertices[0])) {

									//Now that two touching B type triangles have been found, find two other triangles at this point. If there are 2, they must flip their common edge
									for (FCDTriangle cTriangleC : returnValue) {
										if (!(cTriangleC == cTriangleA) && !(cTriangleC == cTriangleB)) {
											for (FCDTriangle cTriangleD : returnValue) {
												if (!(cTriangleD == cTriangleA) && !(cTriangleD == cTriangleB) && !(cTriangleD == cTriangleC)) {

													//Find the common edge of the two triangles c and d and flip it
													FDTriangle triangleC = cTriangleC.triangle;
													FDTriangle triangleD = cTriangleD.triangle;
													if (!triangleC.hasVertex(triangleD.vertices[0])) {
														if (triangleD.hasEdge(triangleC.vertices[0], triangleC.vertices[1])) {
															returnValue.Remove(cTriangleC);
															returnValue.Remove(cTriangleD);
															FDTriangle newExtraTriangleA = FDTriangle({ triangleD.vertices[0], triangleC.vertices[2], triangleC.vertices[0] });
															FDTriangle newExtraTriangleB = FDTriangle({ triangleD.vertices[0], triangleC.vertices[2], triangleC.vertices[1] });
															returnValue.AddUnique(FCDTriangle(newExtraTriangleA, EDTriangleType::C));
															returnValue.AddUnique(FCDTriangle(newExtraTriangleB, EDTriangleType::C));
														}
														else if (triangleD.hasEdge(triangleC.vertices[0], triangleC.vertices[2])) {
															returnValue.Remove(cTriangleC);
															returnValue.Remove(cTriangleD);
															FDTriangle newExtraTriangleA = FDTriangle({ triangleD.vertices[0], triangleC.vertices[1], triangleC.vertices[0] });
															FDTriangle newExtraTriangleB = FDTriangle({ triangleD.vertices[0], triangleC.vertices[1], triangleC.vertices[2] });
															returnValue.AddUnique(FCDTriangle(newExtraTriangleA, EDTriangleType::C));
															returnValue.AddUnique(FCDTriangle(newExtraTriangleB, EDTriangleType::C));
														}
														else if (triangleD.hasEdge(triangleC.vertices[1], triangleC.vertices[2])) {
															returnValue.Remove(cTriangleC);
															returnValue.Remove(cTriangleD);
															FDTriangle newExtraTriangleA = FDTriangle({ triangleD.vertices[0], triangleC.vertices[0], triangleC.vertices[1] });
															FDTriangle newExtraTriangleB = FDTriangle({ triangleD.vertices[0], triangleC.vertices[0], triangleC.vertices[2] });
															returnValue.AddUnique(FCDTriangle(newExtraTriangleA, EDTriangleType::C));
															returnValue.AddUnique(FCDTriangle(newExtraTriangleB, EDTriangleType::C));
														}
													}
													else if (!triangleC.hasVertex(triangleD.vertices[1])) {
														if (triangleD.hasEdge(triangleC.vertices[0], triangleC.vertices[1])) {
															returnValue.Remove(cTriangleC);
															returnValue.Remove(cTriangleD);
															FDTriangle newExtraTriangleA = FDTriangle({ triangleD.vertices[1], triangleC.vertices[2], triangleC.vertices[0] });
															FDTriangle newExtraTriangleB = FDTriangle({ triangleD.vertices[1], triangleC.vertices[2], triangleC.vertices[1] });
															returnValue.AddUnique(FCDTriangle(newExtraTriangleA, EDTriangleType::C));
															returnValue.AddUnique(FCDTriangle(newExtraTriangleB, EDTriangleType::C));
														}
														else if (triangleD.hasEdge(triangleC.vertices[0], triangleC.vertices[2])) {
															returnValue.Remove(cTriangleC);
															returnValue.Remove(cTriangleD);
															FDTriangle newExtraTriangleA = FDTriangle({ triangleD.vertices[1], triangleC.vertices[1], triangleC.vertices[0] });
															FDTriangle newExtraTriangleB = FDTriangle({ triangleD.vertices[1], triangleC.vertices[1], triangleC.vertices[2] });
															returnValue.AddUnique(FCDTriangle(newExtraTriangleA, EDTriangleType::C));
															returnValue.AddUnique(FCDTriangle(newExtraTriangleB, EDTriangleType::C));
														}
														else if (triangleD.hasEdge(triangleC.vertices[1], triangleC.vertices[2])) {
															returnValue.Remove(cTriangleC);
															returnValue.Remove(cTriangleD);
															FDTriangle newExtraTriangleA = FDTriangle({ triangleD.vertices[1], triangleC.vertices[0], triangleC.vertices[1] });
															FDTriangle newExtraTriangleB = FDTriangle({ triangleD.vertices[1], triangleC.vertices[0], triangleC.vertices[2] });
															returnValue.AddUnique(FCDTriangle(newExtraTriangleA, EDTriangleType::C));
															returnValue.AddUnique(FCDTriangle(newExtraTriangleB, EDTriangleType::C));
														}
													}
													else if (!triangleC.hasVertex(triangleD.vertices[2])) {
														if (triangleD.hasEdge(triangleC.vertices[0], triangleC.vertices[1])) {
															returnValue.Remove(cTriangleC);
															returnValue.Remove(cTriangleD);
															FDTriangle newExtraTriangleA = FDTriangle({ triangleD.vertices[2], triangleC.vertices[2], triangleC.vertices[0] });
															FDTriangle newExtraTriangleB = FDTriangle({ triangleD.vertices[2], triangleC.vertices[2], triangleC.vertices[1] });
															returnValue.AddUnique(FCDTriangle(newExtraTriangleA, EDTriangleType::C));
															returnValue.AddUnique(FCDTriangle(newExtraTriangleB, EDTriangleType::C));
														}
														else if (triangleD.hasEdge(triangleC.vertices[0], triangleC.vertices[2])) {
															returnValue.Remove(cTriangleC);
															returnValue.Remove(cTriangleD);
															FDTriangle newExtraTriangleA = FDTriangle({ triangleD.vertices[2], triangleC.vertices[1], triangleC.vertices[0] });
															FDTriangle newExtraTriangleB = FDTriangle({ triangleD.vertices[2], triangleC.vertices[1], triangleC.vertices[2] });
															returnValue.AddUnique(FCDTriangle(newExtraTriangleA, EDTriangleType::C));
															returnValue.AddUnique(FCDTriangle(newExtraTriangleB, EDTriangleType::C));
														}
														else if (triangleD.hasEdge(triangleC.vertices[1], triangleC.vertices[2])) {
															returnValue.Remove(cTriangleC);
															returnValue.Remove(cTriangleD);
															FDTriangle newExtraTriangleA = FDTriangle({ triangleD.vertices[2], triangleC.vertices[0], triangleC.vertices[1] });
															FDTriangle newExtraTriangleB = FDTriangle({ triangleD.vertices[2], triangleC.vertices[0], triangleC.vertices[2] });
															returnValue.AddUnique(FCDTriangle(newExtraTriangleA, EDTriangleType::C));
															returnValue.AddUnique(FCDTriangle(newExtraTriangleB, EDTriangleType::C));
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}

			changes++;
			if (changes > changesDT) { break; }

			previousHelperA = helperA;
			previousHelperB = helperB;
		}
		else { UE_LOG(LogTemp, Log, TEXT("Vertex is not convex")); }
	}
	
	return returnValue;
}
*/

TArray<FPolygonEdge3D> ASurfaceArea::findSkeleton()
{
	TArray<FCubeSkeleton> cubeSkeletons;
	TArray<FPolygonEdge3D> skeleton;
	skeletonSize = 0;

	/*
	for (int32 i = 0; i < polygon.vertices.Num(); i++) {
		int32 j = (i + 1) % polygon.vertices.Num();
		int32 k = (j + 1) % polygon.vertices.Num();

		//Find a triplet of a convex vertex and the two neighbours
		FVector2D leftVertex = polygon.vertices[i];
		FVector2D convexVertex = polygon.vertices[j];
		FVector2D rightVertex = polygon.vertices[k];

		FVector2D midHypothenuse = (leftVertex + rightVertex) / 2;
		FVector2D nearConvexVertex = 0.01f * midHypothenuse + 0.99f * convexVertex;
		FVector2D direction = convexVertex;

		float xSize = 0;
		float ySize = 0;

		//Compute the bisector based on whether the vertex is convex or not
		if (polygon.isInsidePolygon2D(nearConvexVertex)) {
			if (convexVertex.X == leftVertex.X && convexVertex.Y == rightVertex.Y) {
				xSize = FMath::Abs(convexVertex.X - rightVertex.X);
				ySize = FMath::Abs(convexVertex.Y - leftVertex.Y);
				if (leftVertex.Y < convexVertex.Y) { direction.Y -= FMath::Min(xSize, ySize); }
				else { direction.Y += FMath::Min(xSize, ySize); }
				if (rightVertex.X < convexVertex.X) { direction.X -= FMath::Min(xSize, ySize); }
				else { direction.X += FMath::Min(xSize, ySize); }
			}
			else if (convexVertex.Y == leftVertex.Y && convexVertex.X == rightVertex.X) {
				xSize = FMath::Abs(convexVertex.X - leftVertex.X);
				ySize = FMath::Abs(convexVertex.Y - rightVertex.Y);
				if (rightVertex.Y < convexVertex.Y) { direction.Y -= FMath::Min(xSize, ySize); }
				else { direction.Y += FMath::Min(xSize, ySize); }
				if (leftVertex.X < convexVertex.X) { direction.X -= FMath::Min(xSize, ySize); }
				else { direction.X += FMath::Min(xSize, ySize); }
				bisectors.AddUnique(FPolygonEdge(convexVertex, direction, false));
			}
		}
		/*else {
			if (convexVertex.X == leftVertex.X && convexVertex.Y == rightVertex.Y) {
				if (rightVertex.X < convexVertex.X) { direction.X += 10000; }
				else { direction.X -= 10000; }
				if (leftVertex.Y < convexVertex.Y) { direction.Y += 10000; }
				else { direction.Y -= 10000; }
			}
			else if (convexVertex.Y == leftVertex.Y && convexVertex.X == rightVertex.X) {
				if (leftVertex.X < convexVertex.X) { direction.X += 10000; }
				else { direction.X -= 10000; }
				if (rightVertex.Y < convexVertex.Y) { direction.Y += 10000; }
				else { direction.Y -= 10000; }
			}
		}

		//DrawDebugLine(GetWorld(), FVector(convexVertex, GetActorLocation().Z), FVector(direction, GetActorLocation().Z), FColor::Red, false, 15, 0, 6); 
		
	}

	//Now combine bisectors until there are no loose bisectors anymore
	bool combined = true;
	int32 changes = 0;

	while (combined && changes < 100) {
		combined = false;

		//Find all bisector pairs and ensure that the shortest distances are used
		TArray<FBisectorPair> bisectorPairs = findBisectorPairs(bisectors);
		
		
		changes++;
	}*/

	for (UStaticMeshComponent* cube : cubes) {
		if (surface == ESurfaceType::NorthWall || surface == ESurfaceType::SouthWall) {
			FVector vertexNE = cube->GetComponentLocation() + FVector(0, 50 * cube->GetComponentScale().Y, 50 * cube->GetComponentScale().Z);
			FVector vertexNW = cube->GetComponentLocation() + FVector(0, -50 * cube->GetComponentScale().Y, 50 * cube->GetComponentScale().Z);
			FVector vertexSE = cube->GetComponentLocation() + FVector(0, 50 * cube->GetComponentScale().Y, -50 * cube->GetComponentScale().Z);
			FVector vertexSW = cube->GetComponentLocation() + FVector(0, -50 * cube->GetComponentScale().Y, -50 * cube->GetComponentScale().Z);

			//Move the skeleton a little bit away from the edges
			vertexNE -= FVector(0, FMath::Min(50 * cube->GetComponentScale().Y, 50.0f), FMath::Min(50 * cube->GetComponentScale().Z, 50.0f));
			vertexNW -= FVector(0, -FMath::Min(50 * cube->GetComponentScale().Y, 50.0f), FMath::Min(50 * cube->GetComponentScale().Z, 50.0f));
			vertexSE -= FVector(0, FMath::Min(50 * cube->GetComponentScale().Y, 50.0f), -FMath::Min(50 * cube->GetComponentScale().Z, 50.0f));
			vertexSW -= FVector(0, -FMath::Min(50 * cube->GetComponentScale().Y, 50.0f), -FMath::Min(50 * cube->GetComponentScale().Z, 50.0f));

			if (cube->GetComponentScale().Z == cube->GetComponentScale().Y) {
				FCubeSkeleton cubeSkeleton = FCubeSkeleton(vertexNE, vertexNW, vertexSE, vertexSW, cube->GetComponentLocation());
				cubeSkeletons.Add(cubeSkeleton);
			}
			else if (cube->GetRelativeScale3D().Z < cube->GetRelativeScale3D().Y) {
				float halfDist = FMath::Abs((vertexNE.Z - vertexSE.Z) / 2);
				FVector midA = vertexNE - FVector(0, halfDist, halfDist);
				FVector midB = vertexSW + FVector(0, halfDist, halfDist);
				FCubeSkeleton cubeSkeleton = FCubeSkeleton(vertexNE, vertexNW, vertexSE, vertexSW, cube->GetComponentLocation(), midA, midB, true);
				cubeSkeletons.Add(cubeSkeleton);
			}
			else {
				float halfDist = FMath::Abs((vertexNE.Y - vertexNW.Y) / 2);
				FVector midA = vertexNE - FVector(0, halfDist, halfDist);
				FVector midB = vertexSW + FVector(0, halfDist, halfDist);
				FCubeSkeleton cubeSkeleton = FCubeSkeleton(vertexNE, vertexNW, vertexSE, vertexSW, cube->GetComponentLocation(), midA, midB, false);
				cubeSkeletons.Add(cubeSkeleton);
			}
		}
		else if (surface == ESurfaceType::EastWall || surface == ESurfaceType::WestWall) {
			FVector vertexNE = cube->GetComponentLocation() + FVector(50 * cube->GetComponentScale().X, 0, 50 * cube->GetComponentScale().Z);
			FVector vertexNW = cube->GetComponentLocation() + FVector(50 * cube->GetComponentScale().X, 0, -50 * cube->GetComponentScale().Z);
			FVector vertexSE = cube->GetComponentLocation() + FVector(-50 * cube->GetComponentScale().X, 0, 50 * cube->GetComponentScale().Z);
			FVector vertexSW = cube->GetComponentLocation() + FVector(-50 * cube->GetComponentScale().X, 0, -50 * cube->GetComponentScale().Z);

			//Move the skeleton a little bit away from the edges
			vertexNE -= FVector(FMath::Min(50 * cube->GetComponentScale().X, 50.0f), 0, FMath::Min(50 * cube->GetComponentScale().Z, 50.0f));
			vertexNW -= FVector(FMath::Min(50 * cube->GetComponentScale().X, 50.0f), 0, -FMath::Min(50 * cube->GetComponentScale().Z, 50.0f));
			vertexSE -= FVector(-FMath::Min(50 * cube->GetComponentScale().X, 50.0f), 0, FMath::Min(50 * cube->GetComponentScale().Z, 50.0f));
			vertexSW -= FVector(-FMath::Min(50 * cube->GetComponentScale().X, 50.0f), 0, -FMath::Min(50 * cube->GetComponentScale().Z, 50.0f));

			if (cube->GetComponentScale().X == cube->GetComponentScale().Z) {
				FCubeSkeleton cubeSkeleton = FCubeSkeleton(vertexNE, vertexNW, vertexSE, vertexSW, cube->GetComponentLocation());
				cubeSkeletons.Add(cubeSkeleton);
			}
			else if (cube->GetRelativeScale3D().X < cube->GetRelativeScale3D().Z) {
				float halfDist = FMath::Abs((vertexNE.X - vertexSE.X) / 2);
				FVector midA = vertexNE - FVector(halfDist, 0, halfDist);
				FVector midB = vertexSW + FVector(halfDist, 0, halfDist);
				FCubeSkeleton cubeSkeleton = FCubeSkeleton(vertexNE, vertexNW, vertexSE, vertexSW, cube->GetComponentLocation(), midA, midB, true);
				cubeSkeletons.Add(cubeSkeleton);
			}
			else {
				float halfDist = FMath::Abs((vertexNE.Z - vertexNW.Z) / 2);
				FVector midA = vertexNE - FVector(halfDist, 0, halfDist);
				FVector midB = vertexSW + FVector(halfDist, 0, halfDist);
				FCubeSkeleton cubeSkeleton = FCubeSkeleton(vertexNE, vertexNW, vertexSE, vertexSW, cube->GetComponentLocation(), midA, midB, false);
				cubeSkeletons.Add(cubeSkeleton);
			}
		}
		else {
			FVector vertexNE = cube->GetComponentLocation() + FVector(50 * cube->GetComponentScale().X, 50 * cube->GetComponentScale().Y, 0);
			FVector vertexNW = cube->GetComponentLocation() + FVector(50 * cube->GetComponentScale().X, -50 * cube->GetComponentScale().Y, 0);
			FVector vertexSE = cube->GetComponentLocation() + FVector(-50 * cube->GetComponentScale().X, 50 * cube->GetComponentScale().Y, 0);
			FVector vertexSW = cube->GetComponentLocation() + FVector(-50 * cube->GetComponentScale().X, -50 * cube->GetComponentScale().Y, 0);

			//Move the skeleton a little bit away from the edges
			vertexNE -= FVector(FMath::Min(50 * cube->GetComponentScale().X, 50.0f), FMath::Min(50 * cube->GetComponentScale().Y, 50.0f), 0);
			vertexNW -= FVector(FMath::Min(50 * cube->GetComponentScale().X, 50.0f), -FMath::Min(50 * cube->GetComponentScale().Y, 50.0f), 0);
			vertexSE -= FVector(-FMath::Min(50 * cube->GetComponentScale().X, 50.0f), FMath::Min(50 * cube->GetComponentScale().Y, 50.0f), 0);
			vertexSW -= FVector(-FMath::Min(50 * cube->GetComponentScale().X, 50.0f), -FMath::Min(50 * cube->GetComponentScale().Y, 50.0f), 0);

			if (cube->GetComponentScale().X == cube->GetComponentScale().Y) {
				FCubeSkeleton cubeSkeleton = FCubeSkeleton(vertexNE, vertexNW, vertexSE, vertexSW, cube->GetComponentLocation());
				cubeSkeletons.Add(cubeSkeleton);
			}
			else if (cube->GetRelativeScale3D().X < cube->GetRelativeScale3D().Y) {
				float halfDist = FMath::Abs((vertexNE.X - vertexSE.X) / 2);
				FVector midA = vertexNE - FVector(halfDist, halfDist, 0);
				FVector midB = vertexSW + FVector(halfDist, halfDist, 0);
				FCubeSkeleton cubeSkeleton = FCubeSkeleton(vertexNE, vertexNW, vertexSE, vertexSW, cube->GetComponentLocation(), midA, midB, true);
				cubeSkeletons.Add(cubeSkeleton);
			}
			else {
				float halfDist = FMath::Abs((vertexNE.Y - vertexNW.Y) / 2);
				FVector midA = vertexNE - FVector(halfDist, halfDist, 0);
				FVector midB = vertexSW + FVector(halfDist, halfDist, 0);
				FCubeSkeleton cubeSkeleton = FCubeSkeleton(vertexNE, vertexNW, vertexSE, vertexSW, cube->GetComponentLocation(), midA, midB, false);
				cubeSkeletons.Add(cubeSkeleton);
			}
		}
	}

	if (cubeSkeletons.Num() > 1) {
		for (int32 i = 0; i < cubeSkeletons.Num(); i++) {
			for (int32 j = i + 1; j < cubeSkeletons.Num(); j++) {
				FCubeSkeleton csA = cubeSkeletons[i];
				FCubeSkeleton csB = cubeSkeletons[j];

				for (int32 p = 0; p < polygon.vertices.Num(); p++) {
					int32 q = (p + 1) % polygon.vertices.Num();
					if (!polygon.intersects(polygon.vertices[p], polygon.vertices[q], FVector2D(csA.middle.X, csA.middle.Y), FVector2D(csB.middle.X, csB.middle.Y))) {
						skeleton.AddUnique(FPolygonEdge3D(csA.middle, csB.middle, true));
					}
				}
			}
		}
	}

	for (FCubeSkeleton cs : cubeSkeletons) {
		if (!cs.square) {
			skeleton.AddUnique(cs.middleEdgeA); skeletonSize += (cs.middleEdgeA.vA - cs.middleEdgeA.vB).Size();
			skeleton.AddUnique(cs.middleEdgeB); skeletonSize += (cs.middleEdgeB.vA - cs.middleEdgeB.vB).Size();
		}
		skeleton.AddUnique(cs.edgeNE); skeletonSize += (cs.edgeNE.vA - cs.edgeNE.vB).Size();
		skeleton.AddUnique(cs.edgeNW); skeletonSize += (cs.edgeNW.vA - cs.edgeNW.vB).Size();
		skeleton.AddUnique(cs.edgeSE); skeletonSize += (cs.edgeSE.vA - cs.edgeSE.vB).Size();
		skeleton.AddUnique(cs.edgeSW); skeletonSize += (cs.edgeSW.vA - cs.edgeSW.vB).Size();
	}

	return skeleton;
}

TArray<FBisectorPair> ASurfaceArea::findBisectorPairs(TArray<FPolygonEdge> bisectors)
{
	TArray<FBisectorPair> returnValue;

	//For each bisector, find intersecting bisectors
	for (FPolygonEdge a : bisectors) {
		UE_LOG(LogTemp, Log, TEXT("Check new bisector"));
		//Find all bisectors intersecting A
		TArray<FBisectorPair> aPairs;
		for (FPolygonEdge b : bisectors) {
			if (!(a == b)) {
				if (polygon.intersects(a.vA, a.vB, b.vA, b.vB)) { UE_LOG(LogTemp, Log, TEXT("Intersection found")); aPairs.AddUnique(FBisectorPair(a, b)); }
			}
		}

		//Now find the bisector that intersects closest to the vertex of A
		if (aPairs.Num() > 0) {
			aPairs.Sort(FSortByBisectorDist());
			FBisectorPair possibleAddition = aPairs[0];

			//Add the bisector pair if none of its vertices of origin are in the array yet. If one is, add the one with the shortest distance
			bool canAdd = true;
			TArray<FBisectorPair> possibleRemovals;
			for (FBisectorPair bPair : returnValue) {
				if (possibleAddition.bA.vA == bPair.bA.vA) {
					if (possibleAddition.distA > bPair.distA) { canAdd = false; }
					else { possibleRemovals.Add(bPair); }
				}
				else if (possibleAddition.bA.vA == bPair.bB.vA) {
					if (possibleAddition.distA > bPair.distB) { canAdd = false; }
					else { possibleRemovals.Add(bPair); }
				}
				else if (possibleAddition.bB.vA == bPair.bA.vA) {
					if (possibleAddition.distB > bPair.distA) { canAdd = false; }
					else { possibleRemovals.Add(bPair); }
				}
				else if (possibleAddition.bB.vA == bPair.bB.vA) {
					if (possibleAddition.distB > bPair.distB) { canAdd = false; }
					else { possibleRemovals.Add(bPair); }
				}
			}

			if (canAdd) {
				for (FBisectorPair bPair : possibleRemovals) { returnValue.Remove(bPair); }
				returnValue.AddUnique(possibleAddition);
			}
		}
	}

	//Draw the bisectors in the pairs array and their intersections
	for (FBisectorPair aPair : returnValue) {
		//DrawDebugPoint(GetWorld(), FVector(aPair.bA.vA, GetActorLocation().Z), 20, FColor::Yellow, false, 15, 0); 
		//DrawDebugPoint(GetWorld(), FVector(aPair.bA.vB, GetActorLocation().Z), 20, FColor::Yellow, false, 15, 0); 
		//DrawDebugPoint(GetWorld(), FVector(aPair.bB.vA, GetActorLocation().Z), 20, FColor::Magenta, false, 15, 0); 
		//DrawDebugPoint(GetWorld(), FVector(aPair.bB.vB, GetActorLocation().Z), 20, FColor::Magenta, false, 15, 0);
		DrawDebugPoint(GetWorld(), FVector(aPair.intersection, GetActorLocation().Z), 20, FColor::Red, false, 15, 0);
		DrawDebugLine(GetWorld(), FVector(aPair.bA.vA, GetActorLocation().Z), FVector(aPair.intersection, GetActorLocation().Z), FColor::Green, false, 15, 0, 3);
		DrawDebugLine(GetWorld(), FVector(aPair.bB.vA, GetActorLocation().Z), FVector(aPair.intersection, GetActorLocation().Z), FColor::Blue, false, 15, 0, 3);
	}

	return returnValue;
}

bool ASurfaceArea::vertexOnSegment(FVector2D vertex1, FVector2D vertex2, FVector2D vertex3) {
	return vertex2.X <= FMath::Max(vertex1.X, vertex3.X) && vertex2.X >= FMath::Min(vertex1.X, vertex3.X) &&
		vertex2.Y <= FMath::Max(vertex1.Y, vertex3.Y) && vertex2.Y >= FMath::Min(vertex1.Y, vertex3.Y);
}

FVector ASurfaceArea::shiftToMedialAxis(FVector point)
{
	//DrawDebugPoint(GetWorld(), point, 20, FColor::Red, false, 15, 0);
	FVector returnValue = point;
	float minDist = 999999;

	for (FPolygonEdge3D edge : straightSkeleton) {
		FVector segment = edge.vB - edge.vA;
		FVector projectionSegment = point - edge.vA;
		float alpha = FVector::DotProduct(segment, projectionSegment) / FVector::DotProduct(segment, segment);
		alpha = FMath::Clamp(alpha, 0.0f, 1.0f);
		
		if ((alpha * segment).Size() < minDist) { returnValue = edge.vA + alpha * segment; minDist = (alpha * segment).Size(); }
	}

	return returnValue;
}
