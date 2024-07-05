// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PathPlanningSave.generated.h"

/**
 * 
 */
UCLASS()
class DPP3DS_API UPathPlanningSave : public USaveGame
{
	GENERATED_BODY()
	
public:
	UPROPERTY(VisibleAnywhere, Category = "PRM Save")
		FTimespan totalTime;

	UPROPERTY(VisibleAnywhere, Category = "PRM Save")
		FTimespan pathTime;

	UPROPERTY(VisibleAnywhere, Category = "PRM Save")
		FTimespan computationTime;

	UPROPERTY(VisibleAnywhere, Category = "PRM Save")
		FTimespan dynamicTime;

	UPROPERTY(VisibleAnywhere, Category = "PRM Save")
		float distance;

	UPROPERTY(VisibleAnywhere, Category = "PRM Save")
		int32 surfaceChanges;

	UPROPERTY(VisibleAnywhere, Category = "PRM Save")
		bool targetFinished;

	UPROPERTY(VisibleAnywhere, Category = "PRM Save")
		float smoothness;

	UPROPERTY(VisibleAnywhere, Category = "PRM Save")
		float outdatedness;
	
};
