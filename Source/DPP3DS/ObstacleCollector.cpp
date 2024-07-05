// Fill out your copyright notice in the Description page of Project Settings.

#include "ObstacleCollector.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"

// Sets default values
AObstacleCollector::AObstacleCollector()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void AObstacleCollector::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AObstacleCollector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AObstacleCollector::collectObstacles() {
	//Find all obstacles
	TArray<AActor*> tempActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AObstacle::StaticClass(), tempActors);

	//Put all obstacles in an array
	for (AActor* temp : tempActors) { obstacles.AddUnique((AObstacle*)temp); }
}

void AObstacleCollector::updateObstacles() {
	for (AObstacle* obstacle : obstacles) {
		obstacle->UpdateSize();
		obstacle->UpdateVisuals();
	}
}