// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Vertex.h"
#include "PRMEdge.h"
#include "Graph.generated.h"

UCLASS()
class DPP3DS_API AGraph : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AGraph();

	UPROPERTY(EditAnywhere, Category = "Construction")
		TArray<AVertex*> vertices;

	UPROPERTY(EditAnywhere, Category = "Construction")
		TArray<APRMEdge*> edges;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(CallInEditor, Category = "Construction")
		void connectAll();

	// Shows the graph
	UFUNCTION(CallInEditor, Category = "Rendering")
		void showGraph();

	// Hides the graph
	UFUNCTION(CallInEditor, Category = "Rendering")
		void hideGraph();

};
