// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurfaceArea.h"
#include "ConnectivityGraph.generated.h"

UCLASS()
class DPP3DS_API AConnectivityGraph : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AConnectivityGraph();

	//All surfaces in the environment
	UPROPERTY(EditAnywhere, Category = "Construction")
		TArray<ASurfaceArea*> surfaces;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	//Find all surfaces in the environment
	UFUNCTION(CallInEditor, Category = "Construction")
		void CollectSurfaces();

	//Connect overlapping surfaces with one another
	UFUNCTION(CallInEditor, Category = "Construction")
		void ConnectSurfaces();

};
