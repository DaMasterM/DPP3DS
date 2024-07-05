// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Utils.h"
#include "Vertex.generated.h"

UCLASS()
class DPP3DS_API AVertex : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AVertex(const FObjectInitializer&);

	//Which surface this vertex is on.
	UPROPERTY(VisibleAnywhere, Category = "PRM")
		ESurfaceType surface;

	//Neighbours of this vertex
	UPROPERTY(EditAnywhere, Category = "PRM")
		TArray<int32> neighbours;

	//Possible neighbours of this vertex
	UPROPERTY(EditAnywhere, Category = "PRM")
		TArray<int32> possibleNeighbours;

	//ID of the vertex to prevent issues with connecting a lot of vertices through pointers
	UPROPERTY(VisibleAnywhere, Category = "PRM")
		int32 id;

	//Whether to show the vertex upon construction
	UPROPERTY(EditAnywhere, Category = "Rendering")
		int32 vertexVisibility;

	//Materials
	UMaterialInstance* regularMat;
	UMaterialInstance* connectionMat;
	UMaterialInstance* pathMat;

	//F-value for the A* algorithm
	UPROPERTY(VisibleAnywhere, Category = "Path Planning")
		float f;

	//G-value for the A* algorithm
	UPROPERTY(VisibleAnywhere, Category = "Path Planning")
		float g;

	//H-value for the A* algorithm
	UPROPERTY(VisibleAnywhere, Category = "Path Planning")
		float h;

	//F-value for the A* algorithm, chaser
	UPROPERTY(VisibleAnywhere, Category = "Path Planning")
		float fC;

	//G-value for the A* algorithm, chaser
	UPROPERTY(VisibleAnywhere, Category = "Path Planning")
		float gC;

	//H-value for the A* algorithm, chaser
	UPROPERTY(VisibleAnywhere, Category = "Path Planning")
		float hC;

	//Distance value for the Dynamic Programming Approach
	UPROPERTY(VisibleAnywhere, Category = "Path Planning")
		float dpDistance;

	//Update round that the vertex has been handled in. Used in the Dynamic Programming Approach
	UPROPERTY(VisibleAnywhere, Category = "Path Planning")
		int32 dpRound;

	//Neighbouring vertex that leads towards the target for the Dynamic Programming approach
	UPROPERTY(VIsibleAnywhere, Category = "Path Planning")
		int32 dpDirection;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	//Rootcomponent
	UPROPERTY(VisibleAnywhere, Category = "Root")
		USceneComponent* root;

	//Cube that represents the vertex (visually, only for testing purposes)
	UStaticMeshComponent* cube;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Shows this vertex
	UFUNCTION(CallInEditor, Category = "Rendering")
		void showVertex();

	// Hides the vertex
	UFUNCTION(CallInEditor, Category = "Rendering")
		void hideVertex();

};
