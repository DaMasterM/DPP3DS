// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Utils.h"
#include "Vertex.h"
#include "PRMCollector.h"
#include "Agent.generated.h"

UCLASS()
class DPP3DS_API AAgent : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AAgent(const FObjectInitializer&);

	//Whether the agent is moving
	UPROPERTY(EditAnywhere, Category = "Movement")
		float movementSpeed;

	//Whether this agent is able to climb on all surfaces or can only walk on the floor
	UPROPERTY(EditAnywhere, Category = "Movement")
		bool canClimb;

	//Whether to show the path that the agent will take
	UPROPERTY(EditAnywhere, Category = "Navigation")
		bool showPath;

	//Whether to show the path in green (true) or blue (false)
	UPROPERTY(EditAnywhere, Category = "Navigation")
		bool greenPath;

	//Whether the agent is done with its path planning and navigation
	UPROPERTY(EditAnywhere, Category = "Movement")
		bool finished;

	//Whether the agent has failed its path planning and navigation
	UPROPERTY(VisibleAnywhere, Category = "Movement")
		bool failed;

	//Whether the agent is moving
	UPROPERTY(VisibleAnywhere, Category = "Movement")
		bool isMoving;

	//Method by which this agent should move. A* for the target, anything for the crawler
	UPROPERTY(EditAnywhere, Category = "Navigation")
		EPathPlanningMethod pathPlanningMethod;

	//Amount of steps to use in A* stepwise
	UPROPERTY(EditAnywhere, Category = "A* Stepwise")
		int32 steps;

	//Path that the agent will take
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Navigation")
		TArray<int32> path;

	//Mapping from a vertex id to the id of the predecessor vertex
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Navigation")
		TMap<int32, int32> predecessorMap;

	//Path that the agent will take
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Navigation")
		TArray<int32> openVertexSet;

	//Path that the agent will take
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Navigation")
		TArray<int32> closedVertexSet;

	//The PRM collector to consult when looking for vertices
	UPROPERTY(EditAnywhere, Category = "PRM")
		APRMCollector* prmCollector;

	//Vertices that are the goal of this agent. For the chaser, this is an array with length 1.
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Navigation")
		TArray<AVertex*> objectives;

	//Current vertex and vertex to move to

	//Vertices that are the goal of this agent. For the chaser, this is an array with length 1.
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Navigation")
	AVertex* currentVertex;

	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Navigation")
	AVertex* nextVertex;

	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Navigation")
	AVertex* goalVertex;

	//Round in the Dynamic Programming Approach
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Navigation")
	int32 globalRound;

	//Indicates that this agent can move; used in the Dynamic Programming Approach
	bool canMove;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	//Rootcomponent
	UPROPERTY(VisibleAnywhere, Category = "Root")
		USceneComponent* root;

	//Cube that represents the vertex (visually, only for testing purposes)
	UStaticMeshComponent* cube;

	//Materials
	UMaterialInstance* regularMat;
	UMaterialInstance* targetMat;
	UMaterialInstance* chaserMat;

	//Prepares the path planning algorithm
	bool preparePathPlanning(int32 start, int32 goal);

	//Finds the next point to use in the open vertex set
	int32 findNextVertexInOpenSet();

	//FInd the neighbouring points of a vertex with a certain id
	void findNeighbours(AVertex* vertex, AVertex* goal);

	//Construct a path based on the pathmap. Note that the path is given in reverse
	void createPath(int32 goal);

	//Resets the goal in the Dynamic Programming Method
	void resetGoal(AVertex* goal, int32 round);

	//Handles a vertex in the Dynamic Programming Method
	void handleDynamicVertex(AVertex* vertex);

	// Main path planning algorithm, which uses a subroutine based on the path planning method
	bool navigate(AVertex* start, AVertex* goal);

	// A* algorithm
	bool aStar(int32 start, int32 goal);

	// Stepwise A* algorithm
	bool aStarStepwise(int32 start, int32 goal);

	//Basic movement function
	void moveToVertex(int32 destination);

	//Dynamic Path Planning version of moveToVertex
	void moveToVertexDynamic(int32 destination);

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
