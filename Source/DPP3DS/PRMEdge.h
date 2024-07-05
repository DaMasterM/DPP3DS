// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Utils.h"
#include "Components/SplineMeshComponent.h"
#include "PRMEdge.generated.h"

UCLASS()
class DPP3DS_API APRMEdge : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APRMEdge();

	//Which surface this vertex is on.
	UPROPERTY(VisibleAnywhere, Category = "PRM")
		ESurfaceType surface;

	//Vertices of this edge
	UPROPERTY(EditAnywhere, Category = "PRM")
		TArray<int32> endVertices;

	//ID of the edge to keep it in line with how vertices are dealt with
	UPROPERTY(VisibleAnywhere, Category = "PRM")
		int32 id;

	//Whether to show the edge upon construction
	UPROPERTY(EditAnywhere, Category = "Rendering")
		int32 edgeVisibility;

	//Spline that represents the edge (visually, only for testing purposes)
	UPROPERTY(VisibleAnywhere, Category = "Rendering")
		USplineMeshComponent* spline;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	//Rootcomponent
	UPROPERTY(VisibleAnywhere, Category = "Root")
		USceneComponent* root;

	//Materials
	UMaterialInstance* blackMat;
	UMaterialInstance* regularMat;
	UMaterialInstance* pathMat;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Shows the edge
	UFUNCTION(CallInEditor, Category = "Rendering")
		void showEdge();

	// Hides the edge
	UFUNCTION(CallInEditor, Category = "Rendering")
		void hideEdge();

	// Hightlights the edge in blue
	UFUNCTION(CallInEditor, Category = "Rendering")
		void highlightEdge();

	// Delights the edge in black
	UFUNCTION(CallInEditor, Category = "Rendering")
		void delightEdge();

	// Hightlights the edge in blue
	UFUNCTION(CallInEditor, Category = "Rendering")
		void makePathEdge();

	// Delights the edge in black
	UFUNCTION(CallInEditor, Category = "Rendering")
		void removePathEdge();

	// Checks if a vertex id is an end vertex
	UFUNCTION(CallInEditor, Category = "PRM")
		bool isEndVertex(int32 vertexID);

	// Creates the line
	void setEdge(FVector vertexLocationA, FVector vertexLocationB);

};
