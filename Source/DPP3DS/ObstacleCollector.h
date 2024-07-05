// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Obstacle.h"
#include "ObstacleCollector.generated.h"

UCLASS()
class DPP3DS_API AObstacleCollector : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AObstacleCollector(); 
	
	UPROPERTY(VisibleAnywhere, Category = "Obstacles")
		TArray<AObstacle*> obstacles;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(CallInEditor, Category = "Obstacles")
		void collectObstacles();

	UFUNCTION(CallInEditor, Category = "Obstacles")
		void updateObstacles();

};
