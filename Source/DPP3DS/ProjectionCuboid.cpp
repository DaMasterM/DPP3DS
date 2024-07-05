// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectionCuboid.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DrawDebugHelpers.h"
#include "BasicRoom.h"

// Sets default values
AProjectionCuboid::AProjectionCuboid()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	
	root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = root;
	root->SetMobility(EComponentMobility::Static);

	//The box is there for visual aid, not for collisions, so turn those off for good measure.
	box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	box->SetCollisionProfileName("NoCollision");
	box->SetupAttachment(root);
}

// Called when the game starts or when spawned
void AProjectionCuboid::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AProjectionCuboid::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AProjectionCuboid::updateSize()
{
	//Sets the size to be the same as the inside of a basic room with the same size
	SetActorLocation(spawnLocation);
	tne = GetActorLocation() + 50 * size * GetActorScale3D();
	bsw = GetActorLocation() - 50 * size * GetActorScale3D();
	
	//Move around the box based on the surfaces that are present
	if (hasCeiling) { tne.Z -= 50; SetActorLocation(GetActorLocation() - FVector(0, 0, 25)); }
	if (hasFloor) { bsw.Z += 50; SetActorLocation(GetActorLocation() + FVector(0, 0, 25)); }
	if (hasNorthWall) { tne.X -= 50; SetActorLocation(GetActorLocation() - FVector(25, 0, 0)); }
	if (hasSouthWall) { bsw.X += 50; SetActorLocation(GetActorLocation() + FVector(25, 0, 0)); }
	if (hasEastWall) { tne.Y -= 50; SetActorLocation(GetActorLocation() - FVector(0, 25, 0)); }
	if (hasWestWall) { bsw.Y += 50; SetActorLocation(GetActorLocation() + FVector(0, 25, 0)); }
	box->SetBoxExtent(FVector((tne - bsw) / 2 + FVector(25)));

	//Now move the tne and bsw into the right positions
	tne += FVector(25);
	bsw -= FVector(25);
}

void AProjectionCuboid::reset()
{
	updateSize();
	setVertexCounts();
}

void AProjectionCuboid::setSurfaces(bool c, bool f, bool n, bool s, bool e, bool w)
{
	hasCeiling = c;
	hasFloor = f;
	hasNorthWall = n;
	hasSouthWall = s;
	hasEastWall = e;
	hasWestWall = w;
}

void AProjectionCuboid::setVertexCounts()
{
	// Vertex count is area size * inverseVertexDensity. We divide by 10000 so that the size is similar to the size of a surface
	vertexCountX = (tne - bsw).Y * (tne - bsw).Z * inverseVertexDensity / 10000;
	vertexCountY = (tne - bsw).X * (tne - bsw).Z * inverseVertexDensity / 10000;
	vertexCountZ = (tne - bsw).X * (tne - bsw).Y * inverseVertexDensity / 10000;
	verticesGeneratedCeiling = 0;
	verticesGeneratedFloor = 0;
	verticesGeneratedNorthWall = 0;
	verticesGeneratedSouthWall = 0;
	verticesGeneratedEastWall = 0;
	verticesGeneratedWestWall = 0;

	//If the vertex count is 0, set it to 1 to ensure that some vertex could be generated
	if (vertexCountX == 0) { vertexCountX = 1; } if (vertexCountY == 0) { vertexCountY = 1; } if (vertexCountZ == 0) { vertexCountZ = 1; }
}

void AProjectionCuboid::findPRMS(TArray<APRM*> prms)
{
	ceilingPRM = findPRM(GetActorLocation(), FVector(GetActorLocation().X, GetActorLocation().Y, tne.Z), prms);
	floorPRM = findPRM(GetActorLocation(), FVector(GetActorLocation().X, GetActorLocation().Y, bsw.Z), prms);
	northPRM = findPRM(GetActorLocation(), FVector(tne.X, GetActorLocation().Y, GetActorLocation().Z), prms);
	southPRM = findPRM(GetActorLocation(), FVector(bsw.X, GetActorLocation().Y, GetActorLocation().Z), prms);
	eastPRM = findPRM(GetActorLocation(), FVector(GetActorLocation().X, tne.Y, GetActorLocation().Z), prms);
	westPRM = findPRM(GetActorLocation(), FVector(GetActorLocation().X, bsw.Y, GetActorLocation().Z), prms);
}

APRM* AProjectionCuboid::findPRM(FVector traceStart, FVector traceEnd, TArray<APRM*> prms)
{
	//Surfaces hit by the trace
	TArray<FHitResult> hits;

	//Actors to ignore
	TArray<AActor*> ignores;

	//Object types to trace
	TArray<TEnumAsByte<EObjectTypeQuery>> traceObjectTypesLine;
	traceObjectTypesLine.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));

	//Do the line trace in two directions
	UKismetSystemLibrary::LineTraceMultiForObjects(GetWorld(), traceStart, traceEnd, traceObjectTypesLine, true, ignores, EDrawDebugTrace::None, hits, true, FLinearColor::Yellow, FLinearColor::Red, 60);

	//Find out which surface was hit, if any
	for (FHitResult hit : hits) {
		if (hit.GetActor()->GetClass() == ASurfaceArea::StaticClass()) {
			ASurfaceArea* hitSurface = (ASurfaceArea*)hit.GetActor();

			//Find the prm that contains the surface that was found and return that prm
			for (APRM* prm : prms) { if (prm->surfaces.Contains(hitSurface)) { return prm; } }
		}
	}

	//No surface was hit or no prm contains this surface, so return a nullptr instead
	return nullptr;
}

void AProjectionCuboid::generateRandomVertices(bool knn, bool knn3d, TArray<FPRMConnection> inConnections, TArray<FPRMConnection> &outConnections, int32 inVertexID, int32 inEdgeID, int32 &outVertexID, int32 &outEdgeID)
{
	int32 vertexID = inVertexID;
	int32 edgeID = inEdgeID;
	agentSize = 50.0f;

	//We generate at most 6n vertices, where n is the highest vertex count. This to ensure the highest count can be reached.
	int32 maxVertices = FMath::Max3(vertexCountX, vertexCountY, vertexCountZ);
	
	for (int i = 0; i < maxVertices; i++) {
		//Find a random point to project
		FVector randomPoint = getRandomPointInCuboid();
		
		//For every surface that exists that can still have a vertex added, project this point to the appropriate surface
		if (verticesGeneratedCeiling < vertexCountZ && hasCeiling) { projectToSurface(randomPoint, ceilingPRM, knn, knn3d, inConnections, outConnections, vertexID, edgeID, vertexID, edgeID); }
		if (verticesGeneratedFloor < vertexCountZ && hasFloor) { projectToSurface(randomPoint, floorPRM, knn, knn3d, inConnections, outConnections, vertexID, edgeID, vertexID, edgeID); }
		if (verticesGeneratedNorthWall < vertexCountX && hasNorthWall) { projectToSurface(randomPoint, northPRM, knn, knn3d, inConnections, outConnections, vertexID, edgeID, vertexID, edgeID); }
		if (verticesGeneratedSouthWall < vertexCountX && hasSouthWall) { projectToSurface(randomPoint, southPRM, knn, knn3d, inConnections, outConnections, vertexID, edgeID, vertexID, edgeID); }
		if (verticesGeneratedEastWall < vertexCountY && hasEastWall) { projectToSurface(randomPoint, eastPRM, knn, knn3d, inConnections, outConnections, vertexID, edgeID, vertexID, edgeID); }
		if (verticesGeneratedWestWall < vertexCountY && hasWestWall) { projectToSurface(randomPoint, westPRM, knn, knn3d, inConnections, outConnections, vertexID, edgeID, vertexID, edgeID); }
	}

	outVertexID = vertexID;
	outEdgeID = edgeID;
}

void AProjectionCuboid::generateApproximateMedialVertices(bool knn, bool knn3d, TArray<FPRMConnection> inConnections, TArray<FPRMConnection> &outConnections, int32 inVertexID, int32 inEdgeID, int32 &outVertexID, int32 &outEdgeID)
{
	int32 vertexID = inVertexID;
	int32 edgeID = inEdgeID;
	agentSize = 50.0f;

	//We generate at most 6n vertices, where n is the highest vertex count. This to ensure the highest count can be reached.
	int32 maxVertices = FMath::Max3(vertexCountX, vertexCountY, vertexCountZ);

	for (int i = 0; i < maxVertices; i++) {
		//Find a random point to project
		FVector randomPoint = getRandomPointInCuboid();

		//For every surface that exists that can still have a vertex added, project this point to the appropriate surface
		if (verticesGeneratedCeiling < vertexCountZ && hasCeiling) { projectToSurfaceMedial(randomPoint, ceilingPRM, knn, knn3d, inConnections, outConnections, vertexID, edgeID, vertexID, edgeID); }
		if (verticesGeneratedFloor < vertexCountZ && hasFloor) { projectToSurfaceMedial(randomPoint, floorPRM, knn, knn3d, inConnections, outConnections, vertexID, edgeID, vertexID, edgeID); }
		if (verticesGeneratedNorthWall < vertexCountX && hasNorthWall) { projectToSurfaceMedial(randomPoint, northPRM, knn, knn3d, inConnections, outConnections, vertexID, edgeID, vertexID, edgeID); }
		if (verticesGeneratedSouthWall < vertexCountX && hasSouthWall) { projectToSurfaceMedial(randomPoint, southPRM, knn, knn3d, inConnections, outConnections, vertexID, edgeID, vertexID, edgeID); }
		if (verticesGeneratedEastWall < vertexCountY && hasEastWall) { projectToSurfaceMedial(randomPoint, eastPRM, knn, knn3d, inConnections, outConnections, vertexID, edgeID, vertexID, edgeID); }
		if (verticesGeneratedWestWall < vertexCountY && hasWestWall) { projectToSurfaceMedial(randomPoint, westPRM, knn, knn3d, inConnections, outConnections, vertexID, edgeID, vertexID, edgeID); }
	}

	outVertexID = vertexID;
	outEdgeID = edgeID;
}

void AProjectionCuboid::generateExactMedialVertices(bool knn, bool knn3d, TArray<FPRMConnection> inConnections, TArray<FPRMConnection>& outConnections, int32 inVertexID, int32 inEdgeID, int32 & outVertexID, int32 & outEdgeID)
{
	int32 vertexID = inVertexID;
	int32 edgeID = inEdgeID;
	agentSize = 50.0f;

	//We generate at most 6n vertices, where n is the highest vertex count. This to ensure the highest count can be reached.
	int32 maxVertices = FMath::Max3(vertexCountX, vertexCountY, vertexCountZ);

	for (int i = 0; i < maxVertices; i++) {
		//Find a random point to project
		FVector randomPoint = getRandomPointInCuboid();

		//For every surface that exists that can still have a vertex added, project this point to the appropriate surface
		if (verticesGeneratedCeiling < vertexCountZ && hasCeiling) { projectToSurfaceExactMedial(randomPoint, ceilingPRM, knn, knn3d, inConnections, outConnections, vertexID, edgeID, vertexID, edgeID); }
		if (verticesGeneratedFloor < vertexCountZ && hasFloor) { projectToSurfaceExactMedial(randomPoint, floorPRM, knn, knn3d, inConnections, outConnections, vertexID, edgeID, vertexID, edgeID); }
		if (verticesGeneratedNorthWall < vertexCountX && hasNorthWall) { projectToSurfaceExactMedial(randomPoint, northPRM, knn, knn3d, inConnections, outConnections, vertexID, edgeID, vertexID, edgeID); }
		if (verticesGeneratedSouthWall < vertexCountX && hasSouthWall) { projectToSurfaceExactMedial(randomPoint, southPRM, knn, knn3d, inConnections, outConnections, vertexID, edgeID, vertexID, edgeID); }
		if (verticesGeneratedEastWall < vertexCountY && hasEastWall) { projectToSurfaceExactMedial(randomPoint, eastPRM, knn, knn3d, inConnections, outConnections, vertexID, edgeID, vertexID, edgeID); }
		if (verticesGeneratedWestWall < vertexCountY && hasWestWall) { projectToSurfaceExactMedial(randomPoint, westPRM, knn, knn3d, inConnections, outConnections, vertexID, edgeID, vertexID, edgeID); }
	}

	outVertexID = vertexID;
	outEdgeID = edgeID;
}

AVertex * AProjectionCuboid::createVertex(FVector location, ESurfaceType surface, int32 vertexID, bool showVertex) {
	AVertex* returnValue = nullptr;
	
	//if (!isInObstacleHelper(location, agentSize)) {
		//Now create the vertex. Handle the rest of the vertex creation later
		returnValue = GetWorld()->SpawnActor<AVertex>(location, FRotator(0), FActorSpawnParameters());
		returnValue->id = vertexID;
		returnValue->surface = surface;

		if (showVertex) { returnValue->showVertex(); }
		else { returnValue->hideVertex(); }

		return returnValue;
	//}
}

AVertex * AProjectionCuboid::generateVertex(FVector location, int32 vertexID, APRM * prm, ESurfaceType surface)
{
	//For the sake of preventing infinite loops
	int32 tries = 0;
	int32 newID = vertexID;

	//Generate the vertex
	AVertex* newVertex = nullptr;

	//As long as the new vertex is not created, try to create one
	while (newVertex == nullptr && tries < 100) {
		newVertex = createVertex(location, surface, vertexID, true);
		tries++;
	}

	//Add this vertex to the list of vertices in the prm
	if (newVertex != nullptr) {
		prm->vertices.AddUnique(newVertex);
		newID++;
	}

	return newVertex;
}

int32 AProjectionCuboid::connectVertex(AVertex * newVertex, APRM * prm, int32 edgeID, bool knn, bool knn3d, TArray<FPRMConnection> inConnections, TArray<FPRMConnection> &outConnections)
{
	int returnValue = edgeID;
	TArray<AVertex*> possibleNeighbours;

	if (knn3d) {
		TArray<AVertex*> empty;

		//Find the extent of the overlap box to check for neighbours
		FVector extent = FVector(5000);
		possibleNeighbours = findNeighbours(newVertex, extent);
		//UE_LOG(LogTemp, Log, TEXT("Possible neighbours: %d"), possibleNeighbours.Num());

		//Sort the neighbours so that the nearest neighbour is first in the array
		Algo::SortBy(possibleNeighbours, [&](AVertex* vertexA)
		{
			const float& distance = (vertexA->GetActorLocation() - newVertex->GetActorLocation()).Size();
			return distance;
		});

		int32 edgesGenerated = 0;

		//Create an edge between two vertices if not blocked and not over a hole. Also make sure, since it's KNN, that 
		if (edgesGenerated < prm->edgeCount3D) {
			for (AVertex* neighbour : possibleNeighbours) {
				//Direct connection between two vertices on the same plane (the surfaces of this PRM)
				if (!isEdgeBlocked(newVertex, neighbour) && !isEdgeOverVoid(newVertex->GetActorLocation(), neighbour->GetActorLocation())) {
					generateEdge(newVertex, neighbour, edgeID, prm, newVertex->surface, false);
					edgeID++;
					edgesGenerated++;
				}
				//If the vertices are not on the same PRM and they are not helper vertices, connect them later on and for now indicate that they should be connected.
				else if (!prm->vertices.Contains(neighbour) && neighbour->GetClass() != AHelperVertex::StaticClass()) {
					//Check if the neighbour is in a neighbouring surface
					if (checkNeighbouringSurfaces(newVertex, neighbour)) {
						FPRMConnection connection = FPRMConnection(newVertex, neighbour);
						outConnections.Add(connection);
						edgesGenerated++;
						newVertex->possibleNeighbours.AddUnique(neighbour->id);
					}
					else {
						checkNeighbouringSurfacesTest(newVertex, neighbour);
					}
				}

				//If enough edges have been generated, stop trying to generate more
				if (edgesGenerated >= prm->edgeCount3D) { break; }
			}
		}
	}

	else if (knn) {
		TArray<AVertex*> empty;

		//Find the extent of the overlap box to check for neighbours
		FVector extent = FVector(7500);
		switch (newVertex->surface) {
		case ESurfaceType::Floor: extent.Z = 10; break;
		case ESurfaceType::Ceiling: extent.Z = 10; break;
		case ESurfaceType::NorthWall: extent.X = 10; break;
		case ESurfaceType::SouthWall: extent.X = 10; break;
		case ESurfaceType::EastWall: extent.Y = 10; break;
		case ESurfaceType::WestWall: extent.Y = 10; break;
		case ESurfaceType::Stairs: extent.Z = 10; break;
		case ESurfaceType::StairsCeiling: extent.Z = 10; break;
		default: break;
		}
		possibleNeighbours = findNeighbours(newVertex, extent);

		//Sort the neighbours so that the nearest neighbour is first in the array
		Algo::SortBy(possibleNeighbours, [&](AVertex* vertexA)
		{
			const float& distance = (vertexA->GetActorLocation() - newVertex->GetActorLocation()).Size();
			return distance;
		});

		int32 edgesGenerated = 0;

		//Create an edge between two vertices if not blocked and not over a hole. Also make sure, since it's KNN, that 
		if (edgesGenerated < prm->edgeCount) {
			for (AVertex* neighbour : possibleNeighbours) {
				if (!isEdgeBlocked(newVertex, neighbour) && !isEdgeOverVoid(newVertex->GetActorLocation(), neighbour->GetActorLocation())) {
					generateEdge(newVertex, neighbour, edgeID, prm, newVertex->surface, false);
					edgeID++;
					edgesGenerated++;
				}

				//If enough edges have been generated, stop trying to generate more
				if (edgesGenerated >= prm->edgeCount) { break; }
			}
		}
	}

	else {
		//Find the extent in which to search for neighbours
		FVector extent = FVector(750);
		switch (newVertex->surface) {
		case ESurfaceType::Floor:
			extent.Z = 10;
			break;
		case ESurfaceType::Ceiling:
			extent.Z = 10;
			break;
		case ESurfaceType::NorthWall:
			extent.X = 10;
			break;
		case ESurfaceType::SouthWall:
			extent.X = 10;
			break;
		case ESurfaceType::EastWall:
			extent.Y = 10;
			break;
		case ESurfaceType::WestWall:
			extent.Y = 10;
			break;
		case ESurfaceType::Stairs:
			extent.Z = 10;
			break;
		case ESurfaceType::StairsCeiling:
			extent.Z = 10;
			break;
		default:
			break;
		}

		//Find the neighbours of this vertex
		possibleNeighbours = findNeighbours(newVertex, extent);

		//Create an edge between two vertices if not blocked
		for (AVertex* neighbour : possibleNeighbours) {
			if (!isEdgeBlocked(newVertex, neighbour) && !isEdgeOverVoid(newVertex->GetActorLocation(), neighbour->GetActorLocation())) {
				APRMEdge* newEdge = generateEdge(newVertex, neighbour, edgeID, prm, newVertex->surface, false);
				if (newEdge != nullptr) { returnValue++; }
			}
		}
	}

	return returnValue;
}

TArray<AVertex*> AProjectionCuboid::findNeighbours(AVertex* newVertex, FVector extent) {
	//Overlapped vertices
	TArray<AVertex*> returnValue;
	TArray<AActor*> tempActors;

	//Actors to ignore, so the vertex itself
	TArray<AActor*> ignores;
	ignores.Add(newVertex);

	//Object types to trace
	TArray<TEnumAsByte<EObjectTypeQuery>> traceObjectTypes;
	traceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));

	//Find all vertices overlapping the cube
	UKismetSystemLibrary::BoxOverlapActors(GetWorld(), newVertex->GetActorLocation(), extent, traceObjectTypes, AVertex::StaticClass(), ignores, tempActors);

	//Cast to vertices
	for (AActor* actor : tempActors) { returnValue.AddUnique((AVertex*)actor); }

	return returnValue;
}

void AProjectionCuboid::projectToSurface(FVector point, APRM* prm, bool knn, bool knn3d, TArray<FPRMConnection> inConnections, TArray<FPRMConnection> &outConnections, int32 vertexID, int32 edgeID, int32 & nextVertexID, int32 & nextEdgeID)
{
	nextVertexID = vertexID;
	nextEdgeID = edgeID;

	// Find the surface belonging to the PRM
	ESurfaceType surface = ESurfaceType::TransitionEdge;
	if (prm != nullptr) {
		if (prm->surfaces.Num() > 0) {
			ASurfaceArea* surfaceArea = prm->surfaces[0];
			if (surfaceArea != nullptr) { surface = surfaceArea->surface; }
		}

		//Continue if a surface is found
		if (surface != ESurfaceType::TransitionEdge) {
			// Find the point projected to the given surface based on the type
			FVector projectedPoint = point;
			switch (surface) {
			case ESurfaceType::Ceiling:
				projectedPoint.Z = tne.Z - 5;
				break;
			case ESurfaceType::Floor:
				projectedPoint.Z = bsw.Z + 5;
				break;
			case ESurfaceType::NorthWall:
				projectedPoint.X = tne.X - 5;
				break;
			case ESurfaceType::SouthWall:
				projectedPoint.X = bsw.X + 5;
				break;
			case ESurfaceType::EastWall:
				projectedPoint.Y = tne.Y - 5;
				break;
			case ESurfaceType::WestWall:
				projectedPoint.Y = bsw.Y + 5;
				break;
			case ESurfaceType::Stairs:
				projectedPoint.Z = bsw.Z + 5;
				break;
			case ESurfaceType::StairsCeiling:
				projectedPoint.Z = tne.Z - 5;
				break;
			default:
				break;
			}

			//Try to generate the new vertex
			AVertex* newVertex = generateVertex(projectedPoint, vertexID, prm, surface);
			if (newVertex != nullptr) {
				nextVertexID = newVertex->id + 1;
				nextEdgeID = connectVertex(newVertex, prm, edgeID, knn, knn3d, inConnections, outConnections);

				//Add 1 to the amount of vertices generated depending on which surface it is generated on
				switch (surface) {
				case ESurfaceType::Ceiling:
					verticesGeneratedCeiling++;
					break;
				case ESurfaceType::Floor:
					verticesGeneratedFloor++;
					break;
				case ESurfaceType::NorthWall:
					verticesGeneratedNorthWall++;
					break;
				case ESurfaceType::SouthWall:
					verticesGeneratedSouthWall++;
					break;
				case ESurfaceType::EastWall:
					verticesGeneratedEastWall++;
					break;
				case ESurfaceType::WestWall:
					verticesGeneratedWestWall++;
					break;
				case ESurfaceType::Stairs:
					verticesGeneratedFloor++;
					break;
				case ESurfaceType::StairsCeiling:
					verticesGeneratedCeiling++;
					break;
				default:
					break;
				}
			}
		}
	}
}

void AProjectionCuboid::projectToSurfaceMedial(FVector point, APRM* prm, bool knn, bool knn3d, TArray<FPRMConnection> inConnections, TArray<FPRMConnection> &outConnections, int32 vertexID, int32 edgeID, int32 & nextVertexID, int32 & nextEdgeID)
{
	nextVertexID = vertexID;
	nextEdgeID = edgeID;

	// Find the surface belonging to the PRM
	ESurfaceType surface = ESurfaceType::TransitionEdge;
	ASurfaceArea* surfaceArea = nullptr;
	if (prm != nullptr) {
		if (prm->surfaces.Num() > 0) {
			ESurfaceType surfaceType = prm->surfaces[0]->surface;
			FVector traceEnd = point;

			//Find the end location for the trace to find the exact surface
			switch (surfaceType){
			case ESurfaceType::Ceiling: traceEnd.Z = prm->surfaces[0]->GetActorLocation().Z; break;
			case ESurfaceType::Floor: traceEnd.Z = prm->surfaces[0]->GetActorLocation().Z; break;
			case ESurfaceType::Stairs: traceEnd.Z = prm->surfaces[0]->GetActorLocation().Z; break;
			case ESurfaceType::StairsCeiling: traceEnd.Z = prm->surfaces[0]->GetActorLocation().Z; break;
			case ESurfaceType::NorthWall: traceEnd.X = prm->surfaces[0]->GetActorLocation().X; break;
			case ESurfaceType::SouthWall: traceEnd.X = prm->surfaces[0]->GetActorLocation().X; break;
			case ESurfaceType::EastWall: traceEnd.Y = prm->surfaces[0]->GetActorLocation().Y; break;
			case ESurfaceType::WestWall: traceEnd.Y = prm->surfaces[0]->GetActorLocation().Y; break;
			default: break;

			}

			//Output array for hits
			TArray<FHitResult> hits;

			//Actors to ignore
			TArray<AActor*> ignores;

			//Object types to trace
			TArray<TEnumAsByte<EObjectTypeQuery>> traceObjectTypesLine;
			traceObjectTypesLine.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic)); 
			
			UKismetSystemLibrary::LineTraceMultiForObjects(GetWorld(), point, traceEnd, traceObjectTypesLine, true, ignores, EDrawDebugTrace::None, hits, true, FLinearColor::Yellow, FLinearColor::Red, 60);

			for (FHitResult hit : hits){if (hit.GetActor()->GetClass() == ASurfaceArea::StaticClass()) {surfaceArea = (ASurfaceArea*)hit.GetActor();} }
			if (surfaceArea != nullptr) { surface = surfaceArea->surface; }
		}

		//Continue if a surface is found
		if (surface != ESurfaceType::TransitionEdge) {
			// Find the point projected to the given surface based on the type
			FVector projectedPoint = point;
			switch (surface) {
			case ESurfaceType::Ceiling: projectedPoint.Z = tne.Z - 5; break;
			case ESurfaceType::Floor: projectedPoint.Z = bsw.Z + 5; break;
			case ESurfaceType::NorthWall: projectedPoint.X = tne.X - 5; break;
			case ESurfaceType::SouthWall: projectedPoint.X = bsw.X + 5; break;
			case ESurfaceType::EastWall: projectedPoint.Y = tne.Y - 5; break;
			case ESurfaceType::WestWall: projectedPoint.Y = bsw.Y + 5; break;
			case ESurfaceType::Stairs: projectedPoint.Z = bsw.Z + 5; break;
			case ESurfaceType::StairsCeiling: projectedPoint.Z = tne.Z - 5; break;
			default: break;
			}

			//Move the point towards the medial axis
			bool canGenerate = false;
			projectedPoint = approximateMedialAxisP(projectedPoint, surfaceArea, canGenerate);

			//Try to generate the new vertex
			if (canGenerate) {
				AVertex* newVertex = generateVertex(projectedPoint, vertexID, prm, surface);
				if (newVertex != nullptr) {
					nextVertexID = newVertex->id + 1;
					nextEdgeID = connectVertex(newVertex, prm, edgeID, knn, knn3d, inConnections, outConnections);

					//Add 1 to the amount of vertices generated depending on which surface it is generated on
					switch (surface) {
					case ESurfaceType::Ceiling:
						verticesGeneratedCeiling++;
						break;
					case ESurfaceType::Floor:
						verticesGeneratedFloor++;
						break;
					case ESurfaceType::NorthWall:
						verticesGeneratedNorthWall++;
						break;
					case ESurfaceType::SouthWall:
						verticesGeneratedSouthWall++;
						break;
					case ESurfaceType::EastWall:
						verticesGeneratedEastWall++;
						break;
					case ESurfaceType::WestWall:
						verticesGeneratedWestWall++;
						break;
					case ESurfaceType::Stairs:
						verticesGeneratedFloor++;
						break;
					case ESurfaceType::StairsCeiling:
						verticesGeneratedCeiling++;
						break;
					default:
						break;
					}
				}
			}
		}
	}
}

void AProjectionCuboid::projectToSurfaceExactMedial(FVector point, APRM * prm, bool knn, bool knn3d, TArray<FPRMConnection> inConnections, TArray<FPRMConnection>& outConnections, int32 vertexID, int32 edgeID, int32 & nextVertexID, int32 & nextEdgeID)
{
	nextVertexID = vertexID;
	nextEdgeID = edgeID;

	// Find the surface belonging to the PRM
	ESurfaceType surface = ESurfaceType::TransitionEdge;
	ASurfaceArea* surfaceArea = nullptr;
	if (prm != nullptr) {
		if (prm->surfaces.Num() > 0) {
			ESurfaceType surfaceType = prm->surfaces[0]->surface;
			FVector traceEnd = point;

			//Find the end location for the trace to find the exact surface
			switch (surfaceType) {
			case ESurfaceType::Ceiling: traceEnd.Z = prm->surfaces[0]->GetActorLocation().Z; break;
			case ESurfaceType::Floor: traceEnd.Z = prm->surfaces[0]->GetActorLocation().Z; break;
			case ESurfaceType::Stairs: traceEnd.Z = prm->surfaces[0]->GetActorLocation().Z; break;
			case ESurfaceType::StairsCeiling: traceEnd.Z = prm->surfaces[0]->GetActorLocation().Z; break;
			case ESurfaceType::NorthWall: traceEnd.X = prm->surfaces[0]->GetActorLocation().X; break;
			case ESurfaceType::SouthWall: traceEnd.X = prm->surfaces[0]->GetActorLocation().X; break;
			case ESurfaceType::EastWall: traceEnd.Y = prm->surfaces[0]->GetActorLocation().Y; break;
			case ESurfaceType::WestWall: traceEnd.Y = prm->surfaces[0]->GetActorLocation().Y; break;
			default: break;
			}

			//Output array for hits
			TArray<FHitResult> hits;

			//Actors to ignore
			TArray<AActor*> ignores;

			//Object types to trace
			TArray<TEnumAsByte<EObjectTypeQuery>> traceObjectTypesLine;
			traceObjectTypesLine.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));

			UKismetSystemLibrary::LineTraceMultiForObjects(GetWorld(), point, traceEnd, traceObjectTypesLine, true, ignores, EDrawDebugTrace::None, hits, true, FLinearColor::Yellow, FLinearColor::Red, 60);

			for (FHitResult hit : hits) { if (hit.GetActor()->GetClass() == ASurfaceArea::StaticClass()) { surfaceArea = (ASurfaceArea*)hit.GetActor(); } }
			if (surfaceArea != nullptr) { surface = surfaceArea->surface; }
		}

		//Continue if a surface is found
		if (surface != ESurfaceType::TransitionEdge) {
			// Find the point projected to the given surface based on the type
			FVector projectedPoint = point;
			switch (surface) {
			case ESurfaceType::Ceiling: projectedPoint.Z = tne.Z - 5; break;
			case ESurfaceType::Floor: projectedPoint.Z = bsw.Z + 5; break;
			case ESurfaceType::NorthWall: projectedPoint.X = tne.X - 5; break;
			case ESurfaceType::SouthWall: projectedPoint.X = bsw.X + 5; break;
			case ESurfaceType::EastWall: projectedPoint.Y = tne.Y - 5; break;
			case ESurfaceType::WestWall: projectedPoint.Y = bsw.Y + 5; break;
			case ESurfaceType::Stairs: projectedPoint.Z = bsw.Z + 5; break;
			case ESurfaceType::StairsCeiling: projectedPoint.Z = tne.Z - 5; break;
			default: break;
			}

			projectedPoint = surfaceArea->shiftToMedialAxis(projectedPoint);

			//Try to generate the new vertex
			AVertex* newVertex = generateVertex(projectedPoint, vertexID, prm, surface);
			if (newVertex != nullptr) {
				nextVertexID = newVertex->id + 1;
				nextEdgeID = connectVertex(newVertex, prm, edgeID, knn, knn3d, inConnections, outConnections);

				//Add 1 to the amount of vertices generated depending on which surface it is generated on
				switch (surface) {
				case ESurfaceType::Ceiling: verticesGeneratedCeiling++; break;
				case ESurfaceType::Floor: verticesGeneratedFloor++; break;
				case ESurfaceType::NorthWall: verticesGeneratedNorthWall++; break;
				case ESurfaceType::SouthWall: verticesGeneratedSouthWall++; break;
				case ESurfaceType::EastWall: verticesGeneratedEastWall++; break;
				case ESurfaceType::WestWall: verticesGeneratedWestWall++; break;
				case ESurfaceType::Stairs: verticesGeneratedFloor++; break;
				case ESurfaceType::StairsCeiling: verticesGeneratedCeiling++; break;
				default: break;
				}
			}
		}
	}
}

FVector AProjectionCuboid::getRandomPointInCuboid()
{
	return FVector(FMath::RandRange(bsw.X + agentSize, tne.X - agentSize), FMath::RandRange(bsw.Y + agentSize, tne.Y - agentSize), FMath::RandRange(bsw.Z + agentSize, tne.Z - agentSize));
}

bool AProjectionCuboid::isEdgeBlocked(AVertex* a, AVertex* b) {
	//Start and end of the trace
	FVector traceStartBase = a->GetActorLocation();
	FVector traceEndBase = b->GetActorLocation();
	FVector traceStart = traceStartBase;
	FVector traceEnd = traceEndBase;
	float offset = agentSize - 4;

	//Add an offset to the start and end points such that the surface that the vertices are on are not used in computing whether there is a blocked edge
	switch (a->surface) {
	case ESurfaceType::Floor:
		traceStart += FVector(0, 0, offset);
		traceEnd += FVector(0, 0, offset);
		break;
	case ESurfaceType::Ceiling:
		traceStart += FVector(0, 0, -offset);
		traceEnd += FVector(0, 0, -offset);
		break;
	case ESurfaceType::NorthWall:
		traceStart += FVector(-offset, 0, 0);
		traceEnd += FVector(-offset, 0, 0);
		break;
	case ESurfaceType::SouthWall:
		traceStart += FVector(offset, 0, 0);
		traceEnd += FVector(offset, 0, 0);
		break;
	case ESurfaceType::EastWall:
		traceStart += FVector(0, -offset, 0);
		traceEnd += FVector(0, -offset, 0);
		break;
	case ESurfaceType::WestWall:
		traceStart += FVector(0, offset, 0);
		traceEnd += FVector(0, offset, 0);
		break;
	case ESurfaceType::Stairs:
		traceStart += FVector(0, 0, offset);
		traceEnd += FVector(0, 0, offset);
		break;
	case ESurfaceType::StairsCeiling:
		traceStart += FVector(0, 0, -offset);
		traceEnd += FVector(0, 0, -offset);
		break;
	default:
		break;
	}

	//Output array
	FHitResult roomHit;
	TArray<FHitResult> vertexHits;

	//Actors to ignore
	TArray<AActor*> ignores;

	//Object types to trace
	TArray<TEnumAsByte<EObjectTypeQuery>> traceObjectTypesLine;
	traceObjectTypesLine.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));

	//Do a box trace from a to b with the size of the mobile agent
	UKismetSystemLibrary::BoxTraceSingleByProfile(GetWorld(), traceStart, traceEnd, FVector(agentSize), FRotator(0), "EdgeTraceBlocker", false, ignores, EDrawDebugTrace::None, roomHit, true, FLinearColor::Blue, FLinearColor::Green, 60);
	UKismetSystemLibrary::LineTraceMultiForObjects(GetWorld(), traceStartBase, traceEndBase, traceObjectTypesLine, true, ignores, EDrawDebugTrace::None, vertexHits, true, FLinearColor::Yellow, FLinearColor::Red, 60);
	
	//Check whether a room has been hit
	if (IsValid(roomHit.GetActor())) {
		if (roomHit.GetActor()->GetClass() == ABasicRoom::StaticClass()) {
			ABasicRoom* room = (ABasicRoom*)roomHit.GetActor();

			//If the room that was hit is a stairs room, ensure that it was not simply hit on top or at the bottom due to the vertex being on a wall!
			if (room->isStairs) {
				UKismetSystemLibrary::BoxTraceSingleByProfile(GetWorld(), traceStart, traceEnd, FVector(agentSize / 2), FRotator(0), "EdgeTraceBlocker", false, ignores, EDrawDebugTrace::None, roomHit, true, FLinearColor::Blue, FLinearColor::Green, 60);
				if (IsValid(roomHit.GetActor())) {
					if (roomHit.GetActor()->GetClass() == ABasicRoom::StaticClass()) { return true; }
				}
			}
		}
	}

	//Check whether a vertex has been hit
	for (FHitResult hit : vertexHits) {
		//Check that there is a hit with an actor
		if (IsValid(hit.GetActor()))
		{
			//If the hit is a vertex, check if it's b, which a should connect to.
			if (hit.GetActor()->GetClass() == AVertex::StaticClass() || hit.GetActor()->GetClass() == AHelperVertex::StaticClass()) {
				return !(hit.GetActor() == b);
			}
		}
	}
	// This area should not be reached in theory, but if so, it means vertex b is not found and thus the edge should not be generated
	return true;
}

bool AProjectionCuboid::isEdgeOverVoid(FVector traceStart, FVector traceEnd) {
	//Surfaces hit by the trace
	TArray<FHitResult> hitsAB;
	TArray<FHitResult> hitsBA;

	//Actors to ignore
	TArray<AActor*> ignores;

	//Object types to trace
	TArray<TEnumAsByte<EObjectTypeQuery>> traceObjectTypesLine;
	traceObjectTypesLine.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));

	//Do the line trace in two directions
	UKismetSystemLibrary::LineTraceMultiForObjects(GetWorld(), traceStart, traceEnd, traceObjectTypesLine, true, ignores, EDrawDebugTrace::None, hitsAB, true, FLinearColor::Yellow, FLinearColor::Red, 60);
	UKismetSystemLibrary::LineTraceMultiForObjects(GetWorld(), traceEnd, traceStart, traceObjectTypesLine, true, ignores, EDrawDebugTrace::None, hitsBA, true, FLinearColor::Red, FLinearColor::Yellow, 60);
	
	TArray<FHitResult> surfaceHitsAB;
	TArray<FHitResult> surfaceHitsBA;

	//Make sure to find all hits that belong to surfaces, excluding connection cube hits
	for (FHitResult hit : hitsAB) {
		if (hit.GetActor()->GetClass() == ASurfaceArea::StaticClass()) {
			ASurfaceArea* hitSurface = (ASurfaceArea*)hit.GetActor();

			bool addHit = true;
			for (UStaticMeshComponent* hitConnector : hitSurface->connectionCubes) {
				if (hit.GetComponent() == hitConnector) { addHit = false; }
			}

			if (addHit) { surfaceHitsAB.Add(hit); }
		}
	}

	for (FHitResult hit : hitsBA) {
		if (hit.GetActor()->GetClass() == ASurfaceArea::StaticClass()) {
			ASurfaceArea* hitSurface = (ASurfaceArea*)hit.GetActor();

			bool addHit = true;
			for (UStaticMeshComponent* hitConnector : hitSurface->connectionCubes) {
				if (hit.GetComponent() == hitConnector) { addHit = false; }
			}

			if (addHit) { surfaceHitsBA.Add(hit); }
		}
	}

	//Check all hits of trace ab to see if the impact points are the same as the impact points of trace ba in reverse order (approximately)
	//If so, there is no hole
	for (int i = 0; i < surfaceHitsAB.Num(); i++) {
		//Ensure that the index j is always in range
		int j = surfaceHitsBA.Num() - i - 1;

		if (j >= 0) {
			// Check that the impact point of ab is the same as the reverse impact point of ba. If not, there is a hole, so return true
			if (!surfaceHitsAB[i].ImpactPoint.Equals(surfaceHitsBA[j].ImpactPoint, 1)) { return true; }
		}
	}

	return false;
}

APRMEdge* AProjectionCuboid::generateEdge(AVertex* a, AVertex* b, int32 ID, APRM* prm, ESurfaceType surfaceType, bool showEdge) {
	//Create an edge between each pair of vertices, give the edge and ID and add the edge to the graph
	if (a != b) {
		APRMEdge* newEdge = GetWorld()->SpawnActor<APRMEdge>(a->GetActorLocation(), FRotator(0), FActorSpawnParameters());

		if (newEdge == nullptr) { return nullptr; }

		//Give the edge an ID
		newEdge->id = ID;

		//Set the vertices as end points of the edge
		newEdge->endVertices.AddUnique(a->id);
		newEdge->endVertices.AddUnique(b->id);

		//Set the surface the edge is on
		newEdge->surface = surfaceType;
		newEdge->setEdge(a->GetActorLocation(), b->GetActorLocation());

		//Shape the edge
		if (showEdge) { newEdge->showEdge(); }
		else { newEdge->hideEdge(); }

		//Set the two vertices as neighbours
		a->neighbours.AddUnique(b->id);
		b->neighbours.AddUnique(a->id);

		//Add the edge to the array for this PRM
		prm->edges.AddUnique(newEdge);
		return newEdge;
	}
	return nullptr;
}

FVector AProjectionCuboid::approximateMedialAxisP(FVector inLocation, ASurfaceArea* surface, bool &canGenerate)
{
	//Ensure that the locations given are relative to the surface's center, else we get p.X = 1000, q.X = -4000, r.X = 6000, so way out of bounds!
	FVector returnValue = inLocation - surface->GetActorLocation();
	//DrawDebugPoint(GetWorld(), inLocation, 20, FColor::Red, false, 60, 0);

	//Ensure that inLocation is inside the polygon
	if (surface->polygon.isInsidePolygon(inLocation, surface->surface)) {
		//Find the nearest obstacle and the helper location
		FVector obstacleLocation = findClosestObstacleP(inLocation, surface) - surface->GetActorLocation();
		//DrawDebugPoint(GetWorld(), obstacleLocation + surface->GetActorLocation() , 20, FColor::Green, false, 60, 0);
		FVector helperLocation = 2 * returnValue - obstacleLocation;
		//DrawDebugPoint(GetWorld(), helperLocation + surface->GetActorLocation(), 20, FColor::Blue, false, 60, 0);

		//In order to prevent the system from thinking there can be an infinite loop
		int32 moves = 0;

		//Move the location for the new vertex towards the medial axis
		while (moves < 100 && surface->polygon.isInsidePolygon(helperLocation + surface->GetActorLocation(), surface->surface)) {
			// p = (p+r)/2; r = 2p - q;
			returnValue = (returnValue + helperLocation) / 2;
			//DrawDebugPoint(GetWorld(), returnValue + surface->GetActorLocation(), 20, FColor::Yellow, false, 60, 0);
			helperLocation = 2 * returnValue - obstacleLocation;
			//DrawDebugPoint(GetWorld(), helperLocation + surface->GetActorLocation(), 20, FColor::Cyan, false, 60, 0);
			moves++;
		}

		//Ensure that the found location is inside the polygon. If so, we can generate a vertex here.
		if (surface->polygon.isInsidePolygon(returnValue + surface->GetActorLocation(), surface->surface)) {
			canGenerate = true;
			returnValue += surface->GetActorLocation();
			//DrawDebugPoint(GetWorld(), returnValue, 20, FColor::Magenta, false, 60, 0);
		}
	}
	//If inLocation is not inside the polygon, then the algorithm won't work, so don't generate a point
	else { canGenerate = false; }

	return returnValue;
}

FVector AProjectionCuboid::findClosestObstacleP(FVector location, ASurfaceArea* surface)
{
	//Return value and the end points of all traces
	FVector returnValue = FVector(0);
	FVector traceNorthEnd = FVector(0);
	FVector traceSouthEnd = FVector(0);
	FVector traceEastEnd = FVector(0);
	FVector traceWestEnd = FVector(0);

	//Prepare the line traces
	FHitResult northHit; FHitResult southHit; FHitResult eastHit; FHitResult westHit;
	FHitResult cornerHit;

	//Actors to ignore
	TArray<AActor*> ignores;

	//Object types to trace
	TArray<TEnumAsByte<EObjectTypeQuery>> traceObjectTypes;
	traceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));

	//Set the extent of the vertex in the directions parallel to the surface. Also set the directional trace end locations]
	switch (surface->surface) {
	case ESurfaceType::Ceiling:
		traceNorthEnd = location + FVector(10000, 0, 0);
		traceSouthEnd = location - FVector(10000, 0, 0);
		traceEastEnd = location + FVector(0, 10000, 0);
		traceWestEnd = location - FVector(0, 10000, 0);
		break;
	case ESurfaceType::Floor:
		traceNorthEnd = location + FVector(10000, 0, 0);
		traceSouthEnd = location - FVector(10000, 0, 0);
		traceEastEnd = location + FVector(0, 10000, 0);
		traceWestEnd = location - FVector(0, 10000, 0);
		break;
	case ESurfaceType::Stairs:
		traceNorthEnd = location + FVector(10000, 0, 0);
		traceSouthEnd = location - FVector(10000, 0, 0);
		traceEastEnd = location + FVector(0, 10000, 0);
		traceWestEnd = location - FVector(0, 10000, 0);
		break;
	case ESurfaceType::StairsCeiling:
		traceNorthEnd = location + FVector(10000, 0, 0);
		traceSouthEnd = location - FVector(10000, 0, 0);
		traceEastEnd = location + FVector(0, 10000, 0);
		traceWestEnd = location - FVector(0, 10000, 0);
		break;
	case ESurfaceType::NorthWall:
		traceNorthEnd = location + FVector(0, 0, 10000);
		traceSouthEnd = location - FVector(0, 0, 10000);
		traceEastEnd = location + FVector(0, 10000, 0);
		traceWestEnd = location - FVector(0, 10000, 0);
		break;
	case ESurfaceType::SouthWall:
		traceNorthEnd = location + FVector(0, 0, 10000);
		traceSouthEnd = location - FVector(0, 0, 10000);
		traceEastEnd = location + FVector(0, 10000, 0);
		traceWestEnd = location - FVector(0, 10000, 0);
		break;
	case ESurfaceType::EastWall:
		traceNorthEnd = location + FVector(10000, 0, 0);
		traceSouthEnd = location - FVector(10000, 0, 0);
		traceEastEnd = location + FVector(0, 0, 10000);
		traceWestEnd = location - FVector(0, 0, 10000);
		break;
	case ESurfaceType::WestWall:
		traceNorthEnd = location + FVector(10000, 0, 0);
		traceSouthEnd = location - FVector(10000, 0, 0);
		traceEastEnd = location + FVector(0, 0, 10000);
		traceWestEnd = location - FVector(0, 0, 10000);
		break;
	default:
		break;
	}

	UKismetSystemLibrary::LineTraceSingleByProfile(GetWorld(), location, traceNorthEnd, "EdgeTraceBlocker", false, ignores, EDrawDebugTrace::None, northHit, true, FLinearColor::Yellow, FLinearColor::White, 60);
	UKismetSystemLibrary::LineTraceSingleByProfile(GetWorld(), location, traceSouthEnd, "EdgeTraceBlocker", false, ignores, EDrawDebugTrace::None, southHit, true, FLinearColor::Yellow, FLinearColor::White, 60);
	UKismetSystemLibrary::LineTraceSingleByProfile(GetWorld(), location, traceEastEnd, "EdgeTraceBlocker", false, ignores, EDrawDebugTrace::None, eastHit, true, FLinearColor::Yellow, FLinearColor::White, 60);
	UKismetSystemLibrary::LineTraceSingleByProfile(GetWorld(), location, traceWestEnd, "EdgeTraceBlocker", false, ignores, EDrawDebugTrace::None, westHit, true, FLinearColor::Yellow, FLinearColor::White, 60);

	//Find the trace hit with the shortest distance
	float shortestDistance = 999999;

	if (northHit.Actor != nullptr) { returnValue = northHit.ImpactPoint; shortestDistance = northHit.Distance; }
	if (southHit.Actor != nullptr) { if (southHit.Distance < shortestDistance) { returnValue = southHit.ImpactPoint; shortestDistance = southHit.Distance; } }
	if (eastHit.Actor != nullptr) { if (eastHit.Distance < shortestDistance) { returnValue = eastHit.ImpactPoint; shortestDistance = eastHit.Distance; } }
	if (westHit.Actor != nullptr) { if (westHit.Distance < shortestDistance) { returnValue = westHit.ImpactPoint; shortestDistance = westHit.Distance; } }

	for (FVector vertex : surface->polygon.vertices3D) {
		UKismetSystemLibrary::LineTraceSingleByProfile(GetWorld(), location, vertex, "EdgeTraceBlocker", false, ignores, EDrawDebugTrace::None, cornerHit, true, FLinearColor::Yellow, FLinearColor::White, 60);
		if (cornerHit.Actor != nullptr) { if (cornerHit.Distance < shortestDistance) { returnValue = cornerHit.ImpactPoint; shortestDistance = cornerHit.Distance; } }
	}

	return returnValue;
}

bool AProjectionCuboid::checkNeighbouringSurfaces(AVertex * base, AVertex * neighbour)
{
	bool baseInSurface = false;
	bool neighbourInSurface = false;

	//Hit check array
	TArray<FHitResult> overlapsBase;
	TArray<FHitResult> overlapsNeighbour;

	//Actors to ignore, so the surface itself
	TArray<AActor*> ignores;
	ignores.Add(this);

	//Object types to trace
	TArray<TEnumAsByte<EObjectTypeQuery>> traceObjectTypes;
	traceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));

	//Find the surfaces of the two vertices
	UKismetSystemLibrary::BoxTraceMultiForObjects(GetWorld(), base->GetActorLocation(), base->GetActorLocation(), FVector(5), FRotator(0), traceObjectTypes, false, ignores, EDrawDebugTrace::None, overlapsBase, true, FLinearColor::Green, FLinearColor::Red, 60);
	UKismetSystemLibrary::BoxTraceMultiForObjects(GetWorld(), neighbour->GetActorLocation(), neighbour->GetActorLocation(), FVector(5), FRotator(0), traceObjectTypes, false, ignores, EDrawDebugTrace::None, overlapsNeighbour, true, FLinearColor::Green, FLinearColor::Red, 60);

	ASurfaceArea* baseSurface = nullptr;
	ASurfaceArea* neighbourSurface = nullptr;
	UStaticMeshComponent* baseSubsurface = nullptr;
	UStaticMeshComponent* neighbourSubsurface = nullptr;

	//Find the subsurfaces the two vertices are in
	for (FHitResult hit : overlapsBase) {
		AActor* baseSurfaceActor = hit.GetActor();
		if (baseSurfaceActor->GetClass() == ASurfaceArea::StaticClass()) {
			ASurfaceArea* potentialBaseSurface = (ASurfaceArea*)baseSurfaceActor;
			if (potentialBaseSurface->cubes.Contains(hit.GetComponent())) {
				baseSurface = potentialBaseSurface;
				baseSubsurface = (UStaticMeshComponent*)hit.GetComponent();
			}
		}
	}
	for (FHitResult hit : overlapsNeighbour) {
		AActor* neighbourSurfaceActor = hit.GetActor();
		if (neighbourSurfaceActor->GetClass() == ASurfaceArea::StaticClass()) {
			ASurfaceArea* potentialNeighbourSurface = (ASurfaceArea*)neighbourSurfaceActor;
			if (potentialNeighbourSurface->cubes.Contains(hit.GetComponent())) {
				neighbourSurface = potentialNeighbourSurface;
				neighbourSubsurface = (UStaticMeshComponent*)hit.GetComponent();
			}
		}
	}

	//Check if these subsurfaces are neighbours
	if (baseSurface != nullptr && neighbourSurface != nullptr) {
		for (FSurfaceNeighbour surfaceNeighbour : baseSurface->neighbours) {
			if (surfaceNeighbour.cube == baseSubsurface && surfaceNeighbour.neighbourID == neighbourSurface->id) { baseInSurface = true; }
		}

		for (FSurfaceNeighbour surfaceNeighbour : neighbourSurface->neighbours) {
			if (surfaceNeighbour.cube == neighbourSubsurface && surfaceNeighbour.neighbourID == baseSurface->id) { neighbourInSurface = true; }
		}
	}

	return baseInSurface && neighbourInSurface;
}

bool AProjectionCuboid::checkNeighbouringSurfacesTest(AVertex * base, AVertex * neighbour)
{
	//UE_LOG(LogTemp, Log, TEXT("Checking potential neighbour: %s"), *neighbour->GetName());
	bool baseInSurface = false;
	bool neighbourInSurface = false;

	//Hit check array
	TArray<FHitResult> overlapsBase;
	TArray<FHitResult> overlapsNeighbour;

	//Actors to ignore, so the surface itself
	TArray<AActor*> ignores;
	ignores.Add(this);

	//Object types to trace
	TArray<TEnumAsByte<EObjectTypeQuery>> traceObjectTypes;
	traceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));

	TArray<AActor*> baseSurfaceActors;
	TArray<AActor*> neighbourSurfaceActors;

	//Find the surfaces of the two vertices
	UKismetSystemLibrary::BoxTraceMultiForObjects(GetWorld(), base->GetActorLocation(), base->GetActorLocation(), FVector(5), FRotator(0), traceObjectTypes, false, ignores, EDrawDebugTrace::ForDuration, overlapsBase, true, FLinearColor::Green, FLinearColor::Red, 60);
	UKismetSystemLibrary::BoxTraceMultiForObjects(GetWorld(), neighbour->GetActorLocation(), neighbour->GetActorLocation(), FVector(5), FRotator(0), traceObjectTypes, false, ignores, EDrawDebugTrace::ForDuration, overlapsNeighbour, true, FLinearColor::Green, FLinearColor::Red, 60);
	//UKismetSystemLibrary::BoxOverlapActors(GetWorld(), base->GetActorLocation(), FVector(5), traceObjectTypes, ASurfaceArea::StaticClass(), ignores, baseSurfaceActors);
	//UKismetSystemLibrary::BoxOverlapActors(GetWorld(), neighbour->GetActorLocation(), FVector(5), traceObjectTypes, ASurfaceArea::StaticClass(), ignores, neighbourSurfaceActors);

	ASurfaceArea* baseSurface = nullptr;
	ASurfaceArea* neighbourSurface = nullptr;
	UStaticMeshComponent* baseSubsurface = nullptr;
	UStaticMeshComponent* neighbourSubsurface = nullptr;

	//Find the subsurfaces the two vertices are in
	for (FHitResult hit : overlapsBase) {
		AActor* baseSurfaceActor = hit.GetActor();
		if (baseSurfaceActor->GetClass() == ASurfaceArea::StaticClass()) {
			ASurfaceArea* potentialBaseSurface = (ASurfaceArea*)baseSurfaceActor;
			if (potentialBaseSurface->cubes.Contains(hit.GetComponent())) { 
				baseSurface = potentialBaseSurface;
				baseSubsurface = (UStaticMeshComponent*)hit.GetComponent(); 
			}
		}
	}
	for (FHitResult hit : overlapsNeighbour) {
		AActor* neighbourSurfaceActor = hit.GetActor();
		if (neighbourSurfaceActor->GetClass() == ASurfaceArea::StaticClass()) {
			ASurfaceArea* potentialNeighbourSurface = (ASurfaceArea*)neighbourSurfaceActor;
			if (potentialNeighbourSurface->cubes.Contains(hit.GetComponent())) {
				neighbourSurface = potentialNeighbourSurface;
				neighbourSubsurface = (UStaticMeshComponent*)hit.GetComponent();
			}
		}
	}

	//for (AActor* baseSurfaceActor : baseSurfaceActors) { baseSurface = (ASurfaceArea*)baseSurfaceActor; break; }
	//for (AActor* neighbourSurfaceActor : neighbourSurfaceActors) { neighbourSurface = (ASurfaceArea*)neighbourSurfaceActor; break; }

	//if (baseSurface != nullptr) { UE_LOG(LogTemp, Log, TEXT("Base surface: %s"), *baseSurface->GetName()); }
	//if (neighbourSurface != nullptr) { UE_LOG(LogTemp, Log, TEXT("Neighbour surface: %s"), *neighbourSurface->GetName()); }

	//Check if these subsurfaces are neighbours
	if (baseSurface != nullptr && neighbourSurface != nullptr) {
		for (FSurfaceNeighbour surfaceNeighbour : baseSurface->neighbours) {
			if (surfaceNeighbour.neighbourID == neighbourSurface->id) { baseInSurface = true; }//UE_LOG(LogTemp, Log, TEXT("Base is in a good surface")); break; //surfaceNeighbour.cube == baseSubsurface && surfaceNeighbour.neighbourID == neighbourSurface->id) { baseInSurface = true; break; }
			//else { UE_LOG(LogTemp, Log, TEXT("BaseID is wrong")); }
		}

		for (FSurfaceNeighbour surfaceNeighbour : neighbourSurface->neighbours) {
			if (surfaceNeighbour.neighbourID == baseSurface->id) { neighbourInSurface = true; }// UE_LOG(LogTemp, Log, TEXT("Neighbour is in a good surface")); break; //surfaceNeighbour.cube == neighbourSubsurface && surfaceNeighbour.neighbourID == baseSurface->id) { neighbourInSurface = true; break; }
			//else { UE_LOG(LogTemp, Log, TEXT("NeighbourID is wrong")); }
		}
	}

	return baseInSurface && neighbourInSurface;
}