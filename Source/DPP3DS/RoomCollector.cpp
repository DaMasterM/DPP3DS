// Fill out your copyright notice in the Description page of Project Settings.

#include "RoomCollector.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"

// Sets default values
ARoomCollector::ARoomCollector()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void ARoomCollector::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ARoomCollector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ARoomCollector::collectRooms() {
	rooms.Empty();
	TArray<AActor*> tempActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABasicRoom::StaticClass(), tempActors);

	for (AActor* temp : tempActors) { rooms.AddUnique((ABasicRoom*)temp); }
}

void ARoomCollector::updateRooms() {
	for (ABasicRoom* room : rooms) {
		room->UpdateSize();
		room->UpdateWalls();
	}
}

void ARoomCollector::spawnCuboids()
{
	for (ABasicRoom* room : rooms) { room->spawnCuboid(); }
}
