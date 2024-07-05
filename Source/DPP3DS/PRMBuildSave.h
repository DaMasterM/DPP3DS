// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PRMBuildSave.generated.h"

/**
 * 
 */
UCLASS()
class DPP3DS_API UPRMBuildSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "PRM Save")
		FTimespan buildTime;

	UPROPERTY(VisibleAnywhere, Category = "PRM Save")
		int32 vertexCount;

	UPROPERTY(VisibleAnywhere, Category = "PRM Save")
		int32 edgeCount;

	UPROPERTY(VisibleAnywhere, Category = "PRM Save")
		float areaCovered;

	UPROPERTY(VisibleAnywhere, Category = "PRM Save")
		float totalArea;

	UPROPERTY(VisibleAnywhere, Category = "PRM Save")
		int32 totalConnections;

	UPROPERTY(VisibleAnywhere, Category = "PRM Save")
		int32 madeConnections;

	UPROPERTY(VisibleAnywhere, Category = "PRM Save")
		int32 clNearbyArea;

	UPROPERTY(VisibleAnywhere, Category = "PRM Save")
		int32 clFarApart;
	
};
