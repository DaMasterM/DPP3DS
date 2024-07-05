// Fill out your copyright notice in the Description page of Project Settings.


#include "ConnectivityGraph.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"

// Sets default values
AConnectivityGraph::AConnectivityGraph()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void AConnectivityGraph::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void AConnectivityGraph::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AConnectivityGraph::CollectSurfaces() {
	//Clean up the array first
	surfaces.Empty();

	//Prepare the ID for each surface
	int32 newId = 0;

	//Find all surface objects
	TArray<AActor*> foundSurfaces;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASurfaceArea::StaticClass(), foundSurfaces);

	//For each surface, cast to the right class, give it a unique ID and add it to the array
	for (AActor* surface : foundSurfaces) {
		ASurfaceArea* surf = (ASurfaceArea*)surface;
		surf->id = newId;
		newId++;
		surfaces.AddUnique(surf);

		//Calculate the weights of the surface
		surf->calculateWeights();
	}
}

void AConnectivityGraph::ConnectSurfaces() {
	for (ASurfaceArea* surface : surfaces) { surface->findNeighbours(); }
}