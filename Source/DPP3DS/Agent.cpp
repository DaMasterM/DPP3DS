// Fill out your copyright notice in the Description page of Project Settings.


#include "Agent.h"

// Sets default values
AAgent::AAgent(const FObjectInitializer&)
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	//Find the cube mesh and vertex material
	UStaticMesh* cubeMesh = ConstructorHelpers::FObjectFinder<UStaticMesh>(TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'")).Object;
	regularMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/AgentMI.AgentMI'")).Object;
	targetMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/TargetMI.TargetMI'")).Object;
	chaserMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/ChaserMI.ChaserMI'")).Object;
	
	//Create the root of this vertex
	root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = root;

	//Create the cube representing the vertex
	cube = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cube"));
	cube->SetStaticMesh(cubeMesh);
	cube->SetMaterial(0, regularMat);
	cube->SetupAttachment(root);
}

// Called when the game starts or when spawned
void AAgent::BeginPlay()
{
	Super::BeginPlay();
	
}

bool AAgent::preparePathPlanning(int32 start, int32 goal)
{
	//Reset the path, map, open vertex set and closed vertex set
	path.Empty();
	predecessorMap.Empty();
	openVertexSet.Empty();
	closedVertexSet.Empty();

	//Reset the G value for every vertex
	for (AVertex* vertex : prmCollector->vertices) { if (canClimb) { vertex->gC = 999999; } else { vertex->g = 999999; } }

	//Stop preparations if the start of goal id's are not valid for the vertex array
	if (start < 0 || start > prmCollector->vertices.Num() || goal < 0 || goal > prmCollector->vertices.Num()) {
		UE_LOG(LogTemp, Log, TEXT("Start or goal is invalid. Start: %d; goal: %d; vertices: %d"), start, goal, prmCollector->vertices.Num());
		return false;
	}

	//Prepare the start location. First find the vertex, which should be in the prmCollector at the position of the startID
	AVertex* startVertex = prmCollector->getVertex(start);
	goalVertex = prmCollector->getVertex(goal);

	//If there is no startVertex or goalVertex, stop path planning entirely
	if (startVertex == nullptr || goalVertex == nullptr) {
		UE_LOG(LogTemp, Log, TEXT("Start or goal vertex not found. Start: %d; goal: %d"), start, goal);
		return false;
	}

	//Prepare the start location
	if (canClimb) {
		startVertex->gC = 0;
		startVertex->fC = (goalVertex->GetActorLocation() - startVertex->GetActorLocation()).Size();
	}
	else {
		startVertex->g = 0;
		startVertex->f = (goalVertex->GetActorLocation() - startVertex->GetActorLocation()).Size();
	}
	openVertexSet.Add(startVertex->id);

	return true;
}

int32 AAgent::findNextVertexInOpenSet()
{
	int32 returnValue = -1;
	float lowestCost = 999999;

	//Go over all vertices in the open vertex set to find the one with the lowest f value
	for (int32 vertexID : openVertexSet) {
		AVertex* vertex = prmCollector->getVertex(vertexID);
		if (vertex) {
			if (canClimb) {
				float cost = vertex->fC;

				if (cost < lowestCost) {
					lowestCost = cost;
					returnValue = vertexID;
				}
			}
			else {
				float cost = vertex->f;

				if (cost < lowestCost) {
					lowestCost = cost;
					returnValue = vertexID;
				}
			}
		}
		else {
			UE_LOG(LogTemp, Log, TEXT("No vertex with id %d exists!"), vertexID);
		}
	}

	return returnValue;
}

void AAgent::findNeighbours(AVertex* vertex, AVertex* goal)
{
	int32 vertexID = vertex->id;

	//Check all neighbours that are not in the closed vertex set
	for (int32 neighbourID : vertex->neighbours) {
		if (!closedVertexSet.Contains(neighbourID)) {
			AVertex* neighbour = prmCollector->getVertex(neighbourID);

			//If the agent cannot climb, don't consider non-floors and non-stair floors as candidates
			if (canClimb || neighbour->surface == ESurfaceType::Floor || neighbour->surface == ESurfaceType::Stairs || neighbour->surface == ESurfaceType::TransitionStairs) {
				//The new g value is the one for the predecessor + the distance between predecessor and neighbour
				if (canClimb) {
					float newG = vertex->gC + (neighbour->GetActorLocation() - vertex->GetActorLocation()).Size();
					//Update the values if the new g value is lower
					if (newG < neighbour->gC) {
						predecessorMap.Add(neighbourID, vertexID);
						neighbour->gC = newG;

						//Add a penalty for moving to another surface
						float surfacePenalty = 0;
						if (canClimb && neighbour->surface != vertex->surface) { surfacePenalty = 400; }

						//Add a penalty for using stairs as a climber
						float stairsPenalty = 0;
						if (canClimb && (neighbour->surface == ESurfaceType::Stairs || neighbour->surface == ESurfaceType::StairsCeiling || neighbour->surface == ESurfaceType::TransitionStairs)) {
							stairsPenalty = 400;
						}

						//f = g + h + surfacePenalty + stairsPenalty. h = Euclidean Distance to the goal
						neighbour->fC = newG + (goal->GetActorLocation() - neighbour->GetActorLocation()).Size() + surfacePenalty + stairsPenalty;

						if (!openVertexSet.Contains(neighbour->id)) { openVertexSet.Add(neighbour->id); }
					}
				}
				else {
					float newG = vertex->g + (neighbour->GetActorLocation() - vertex->GetActorLocation()).Size();
					//Update the values if the new g value is lower
					if (newG < neighbour->g) {
						predecessorMap.Add(neighbourID, vertexID);
						neighbour->g = newG;

						//Add a penalty for moving to another surface
						float surfacePenalty = 0;
						if (canClimb && neighbour->surface != vertex->surface) { surfacePenalty = 400; }

						//Add a penalty for using stairs as a climber
						float stairsPenalty = 0;
						if (canClimb && (neighbour->surface == ESurfaceType::Stairs || neighbour->surface == ESurfaceType::StairsCeiling || neighbour->surface == ESurfaceType::TransitionStairs)) {
							stairsPenalty = 400;
						}

						//f = g + h + surfacePenalty + stairsPenalty. h = Euclidean Distance to the goal
						neighbour->f = newG + (goal->GetActorLocation() - neighbour->GetActorLocation()).Size() + surfacePenalty + stairsPenalty;

						if (!openVertexSet.Contains(neighbour->id)) { openVertexSet.Add(neighbour->id); }
					}
				}
			}
		}
	}
}

void AAgent::createPath(int32 goal)
{
	//Create the path starting at the end
	int32 currentID = goal;
	path.Add(currentID);

	//For each vertex, find the predecessor and add it to the path. Then find that vertex's predecessor, etc. The start vertex has no predecessor.
	while (predecessorMap.Contains(currentID)) {
		currentID = predecessorMap[currentID];
		path.Add(currentID);
	}
}

void AAgent::resetGoal(AVertex* goal, int32 round) {
	goalVertex = goal;
	goalVertex->dpRound = round;
	goalVertex->dpDistance = 0;
	goalVertex->dpDirection = goal->id;
	openVertexSet.Add(goal->id);
}

void AAgent::handleDynamicVertex(AVertex * vertex)
{
	//Go over all neighbours of the vertex
	for (int32 neighbourID : vertex->neighbours) {
		AVertex* neighbour = prmCollector->getVertex(neighbourID);
		//Only update this vertex if it has not been handled in the latest round
		if (neighbour->dpRound < vertex->dpRound) {
			neighbour->dpRound = vertex->dpRound;
			neighbour->dpDistance = (neighbour->GetActorLocation() - vertex->GetActorLocation()).Size();

			//Add a penalty for moving to another surface
			if (neighbour->surface != vertex->surface) { neighbour->dpDistance += 400; }

			//Add a penalty for using the stairs
			if (neighbour->surface == ESurfaceType::Stairs || neighbour->surface == ESurfaceType::StairsCeiling || neighbour->surface == ESurfaceType::TransitionStairs) {
				neighbour->dpDistance += 400;
			}

			neighbour->dpDirection = vertex->id;
			openVertexSet.Add(neighbourID);

			//If this is the current vertex of the agent, allow movement starting now
			if (!canMove && neighbour == currentVertex) {
				canMove = true; 
				nextVertex = vertex;
			}
		}
	}
}

// Called every frame
void AAgent::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool AAgent::navigate(AVertex * start, AVertex * goal)
{
	bool returnValue = false;
	if (start == nullptr) {
		UE_LOG(LogTemp, Log, TEXT("Start does not exist"));
		return false;
	}
	if (goal == nullptr) {
		UE_LOG(LogTemp, Log, TEXT("Goal does not exist"));
		return false;
	}

	if (!preparePathPlanning(start->id, goal->id)) {
		UE_LOG(LogTemp, Log, TEXT("PreparePathPlanning failed"));
		return false;
	}

	switch (pathPlanningMethod) {
	case EPathPlanningMethod::AStar:
		returnValue = aStar(start->id, goal->id);
		break;
	case EPathPlanningMethod::AStarStep:
		returnValue = aStarStepwise(start->id, goal->id);
		break;
	case EPathPlanningMethod::Combined:
		returnValue = aStar(start->id, goal->id);
		break;
	default:
		break;
	}

	return returnValue;
}

bool AAgent::aStar(int32 start, int32 goal)
{

	//While there are still vertices to check, do so
	while (openVertexSet.Num() > 0) {
		int32 nextID = findNextVertexInOpenSet();
		AVertex* successorVertex = prmCollector->getVertex(nextID);
			
		//Stop pathfinding if some vertexID does not exist. This should never be reached.
		if (successorVertex == nullptr) {
			UE_LOG(LogTemp, Log, TEXT("The next point in the open set does not exist? id: %d"), nextID);
			return false;
		}
		else {
			//If the goal has been reached, reconstruct the path towards the goal. Path planning has been a success.
			if (nextID == goal) {
				createPath(goal);
				return true;
			}

			//This is not the goal, so finish off this vertex.
			openVertexSet.Remove(nextID);
			closedVertexSet.Add(nextID);
			findNeighbours(successorVertex, prmCollector->getVertex(goal));
		}
	}

	//Path planning has failed to find the goal vertex
	UE_LOG(LogTemp, Log, TEXT("No path exists between the start (%d) and goal (%d)"), start, goal);
	return false;
}

bool AAgent::aStarStepwise(int32 start, int32 goal)
{
	bool returnValue = aStar(start, goal);

	if (path.Num() > 0) {
		//Now reduce the path to a certain length
		TArray<int32> tempPath;

		for (int32 step : path) { tempPath.Add(step); }

		path.Empty();
		for (int32 j = FMath::Min(steps, tempPath.Num() - 1); j >= 0; j--) {
			path.Add(tempPath[tempPath.Num() - 1 - j]);
		}

		//Change the goal vertex to this closer one in order to keep the tick function working as intended
		goalVertex = prmCollector->findVertex(path[0]);
	}

	return returnValue;
}

void AAgent::moveToVertex(int32 destination)
{
	currentVertex = nextVertex;
	nextVertex = prmCollector->getVertex(destination);
	isMoving = true;
}

void AAgent::moveToVertexDynamic(int32 destination)
{
	currentVertex = nextVertex;
	nextVertex = prmCollector->getVertex(destination);
}

