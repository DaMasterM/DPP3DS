// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Agent.h"
#include "Target.generated.h"

/**
 * 
 */
UCLASS()
class DPP3DS_API ATarget : public AAgent
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ATarget(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaTime) override;

	float verticesMoved;
	
};
