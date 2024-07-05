// Fill out your copyright notice in the Description page of Project Settings.

#include "PRMGenerator.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"

// Sets default values
APRMGenerator::APRMGenerator()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void APRMGenerator::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APRMGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void APRMGenerator::generatePRM() {
	PRMCollector->collectPRMS();
	PRMCollector->setNeighboursPRMS();
	PRMCollector->generatePRMS();
	PRMCollector->connectPRMS();
	PRMCollector->calculateCoverSize();
}

void APRMGenerator::resetPRM() {
	PRMCollector->reset();
}

void APRMGenerator::reconnectPRM()
{
	PRMCollector->reconnectPRM();
	PRMCollector->calculateCoverSize();
}

void APRMGenerator::showPRM() {
	PRMCollector->showPRMS();
}

void APRMGenerator::hidePRM() {
	PRMCollector->hidePRMS();
}

void APRMGenerator::showStats() {
	USaveGame* baseSaveFile = UGameplayStatics::LoadGameFromSlot(saveName, 0);
	if (baseSaveFile) {
		UPRMBuildSave* saveFile = (UPRMBuildSave*)baseSaveFile;
		UE_LOG(LogTemp, Log, TEXT("Values for save file: %s"), *saveName);
		UE_LOG(LogTemp, Log, TEXT("Build time: %s"), *saveFile->buildTime.ToString());
		UE_LOG(LogTemp, Log, TEXT("Vertex count: %d"), saveFile->vertexCount);
		UE_LOG(LogTemp, Log, TEXT("Edge count: %d"), saveFile->edgeCount);
		UE_LOG(LogTemp, Log, TEXT("Ratio of PRM connections: %d/%d"), saveFile->madeConnections, saveFile->totalConnections);
		UE_LOG(LogTemp, Log, TEXT("Ratio of area covered: %f/%f"), saveFile->areaCovered, saveFile->totalArea);
		UE_LOG(LogTemp, Log, TEXT("Nearby areas that had no vertices: %d"), saveFile->clNearbyArea);
		UE_LOG(LogTemp, Log, TEXT("Partial PRMs that were not connected initially: %d"), saveFile->clFarApart);
	}
}

void APRMGenerator::showStatsCSV()
{
	USaveGame* baseSaveFile = UGameplayStatics::LoadGameFromSlot(saveName, 0);
	if (baseSaveFile) {
		UPRMBuildSave* saveFile = (UPRMBuildSave*)baseSaveFile;
		UE_LOG(LogTemp, Log, TEXT("%s;%d;%d;%d;;%f"), *FString::SanitizeFloat(saveFile->buildTime.GetTotalSeconds()), saveFile->vertexCount, saveFile->edgeCount, saveFile->madeConnections, saveFile->areaCovered);
	}
}

void APRMGenerator::applyOptions()
{
	PRMCollector->applyOptions(saveName, projectOntoSurfaces, useMedialAxis, approximateMedialAxis, kNearestNeighbours, kNearestNeighbours3D, allPartialSurfacesMinimumOne, allSubSurfacesMinimumOne, guaranteeNearbyVertices, guaranteeConnections);
}
