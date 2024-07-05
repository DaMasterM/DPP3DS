// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Agent.h"
#include "Target.h"
#include "PathPlanningSave.h"
#include "Chaser.generated.h"

/**
 * 
 */
UCLASS()
class DPP3DS_API AChaser : public AAgent
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AChaser(const FObjectInitializer& ObjectInitializer);

	// The target to follow
	UPROPERTY(EditAnywhere, Category = "Navigation")
		ATarget* target;

	//Name of the save file that saves the path data
	UPROPERTY(EditAnywhere, Category = "Save")
		FString saveName;

	//Save object and the values to use
	UPathPlanningSave* saveFile;
	FDateTime startTimeMoment;
	FDateTime computationStartTimeMomentA;
	FDateTime computationEndTimeMomentA;
	FDateTime movementStartTimeMomentA;
	FDateTime movementEndTimeMomentA;
	FDateTime computationStartTimeMomentD;
	FDateTime computationEndTimeMomentD;
	FDateTime movementStartTimeMomentD;
	FDateTime movementEndTimeMomentD;
	FDateTime endTimeMoment;
	FTimespan computationTimeTotal;
	FTimespan timeUntilDynamic;
	FTimespan movementTimeTotal;
	int32 surfaceChanges;
	float pathSmoothness;
	float pathOutdatedness;

	//Distance that has been travelled so far
	UPROPERTY(VisibleAnywhere, Category = "Save")
		float edgeDistance;

	//The path that was followed
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Navigation")
		TArray<int32> finalPath;

	//The outdatedness at every segment
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Navigation")
		TArray<float> outdatednessPerSegment;

	//Used to indicate that computation time has ended for the Dynamic Programming Approach
	bool firstCall;

	//Used to indicate that the Combined method should use the Dynamic part
	bool useDynamic;

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	//Reads the path planning data
	UFUNCTION(CallInEditor, Category = "Save")
		void printData();

	//Reads the path planning data
	UFUNCTION(CallInEditor, Category = "Save")
		void printDataCSV();

	//Performs AStar path planning during a tick
	void aStarTick(float DeltaTime);

	//Performs Dynamic path planning during a tick
	void dynamicTick(float DeltaTime);

	//Saves the path planning data
	void saveData(FTimespan totalTime);

	//Highlights an edge
	void showPathEdge(int32 IDA, int32 IDB);

	float calculateSmoothness();

	float calculateOutdatedness();
	
};
