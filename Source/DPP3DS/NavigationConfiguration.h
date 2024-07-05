// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Agent.h"
#include "Target.h"
#include "Chaser.h"
#include "Vertex.h"
#include "NavigationConfiguration.generated.h"

UCLASS()
class DPP3DS_API ANavigationConfiguration : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ANavigationConfiguration();

	//Target and agent objects
	UPROPERTY(EditAnywhere, Category = "Target")
	ATarget* target;

	UPROPERTY(EditAnywhere, Category = "Chaser")
	AChaser* chaser ;

	//The current configuration
	UPROPERTY(EditAnywhere, Category = "Configuration")
	int32 configuration;

	//Starting vertices for the target and chaser
	UPROPERTY(EditAnywhere, Category = "Target")
		AVertex* targetStart;

	UPROPERTY(EditAnywhere, Category = "Chaser")
		AVertex* chaserStart;

	UPROPERTY(EditAnywhere, Category = "Target")
	TArray<AVertex*> targetStartVertices;

	UPROPERTY(EditAnywhere, Category = "Chaser")
	TArray<AVertex*> chaserStartVertices;

	//Objectives for the target to reach
	UPROPERTY(EditAnywhere, Category = "Objectives")
	TArray<AVertex*> objectives;

	//All objectives in the configurations
	UPROPERTY(EditAnywhere, Category = "Objectives")
		TArray<AVertex*> allObjectives;

	//Lengths of the amount of objectives in each configuration
	UPROPERTY(EditAnywhere, Category = "Objectives")
		TArray<int32> objectiveCounts;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(CallInEditor, Category = "Configuration")
		void setConfiguration();

};
