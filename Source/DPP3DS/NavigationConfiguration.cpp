// Fill out your copyright notice in the Description page of Project Settings.


#include "NavigationConfiguration.h"

// Sets default values
ANavigationConfiguration::ANavigationConfiguration()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void ANavigationConfiguration::BeginPlay()
{
	Super::BeginPlay();
	if (target == nullptr) { UE_LOG(LogTemp, Log, TEXT("Target does not exist?")); }
	else if (targetStart == nullptr) { UE_LOG(LogTemp, Log, TEXT("Target start does not exist?")); }
	else {
		target->currentVertex = targetStart;
		chaser->currentVertex = chaserStart;
	}
}

// Called every frame
void ANavigationConfiguration::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ANavigationConfiguration::setConfiguration()
{
	//Ensure that the configuration is complete!
	if (targetStartVertices.IsValidIndex(configuration) && chaserStartVertices.IsValidIndex(configuration) && objectiveCounts.IsValidIndex(configuration)) {
		//Put the target and chaser at the right places
		targetStart = targetStartVertices[configuration];
		chaserStart = chaserStartVertices[configuration];
		target->SetActorLocation(targetStart->GetActorLocation());
		chaser->SetActorLocation(chaserStart->GetActorLocation());

		//Find the objectives
		objectives.Empty();
		int32 startIndex = 0;

		for (int32 i = 0; i < configuration; i++) { startIndex += objectiveCounts[i]; }

		for (int32 j = startIndex; j < startIndex + objectiveCounts[configuration]; j++) {
			//Add this configuration's objectives into the objectives array
			if (allObjectives.IsValidIndex(j)) { objectives.Add(allObjectives[j]); }
			else { UE_LOG(LogTemp, Log, TEXT("J is not a valid index in allObjectives. J: %d; allObjectives.Num(): %d"), j, allObjectives.Num()); }
		}

		//Add the objectives and start vertex to the target
		target->currentVertex = targetStart;
		target->objectives.Empty();
		
		for (AVertex* objective : objectives) { target->objectives.Add(objective); }

		//Prepare the chaser
		chaser->currentVertex = chaserStart;
		chaser->target = target;
		chaser->objectives.Empty();
		chaser->objectives.Add(targetStart);
	}
	else { UE_LOG(LogTemp, Log, TEXT("Configuration is not valid. Do the arrays have enough entries?")); }
}

