// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Vertex.h"
#include "HelperVertex.generated.h"

/**
 * 
 */
UCLASS()
class DPP3DS_API AHelperVertex : public AVertex
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AHelperVertex(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(VisibleAnywhere, Category = "Debug")
		AVertex* goalA;
	
	UPROPERTY(VisibleAnywhere, Category = "Debug")
		AVertex* goalB;
	
};
