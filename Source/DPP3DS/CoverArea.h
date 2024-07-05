// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Utils.h"
#include "CoverArea.generated.h"

UCLASS()
class DPP3DS_API ACoverArea : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ACoverArea();

	UPROPERTY(VisibleAnywhere, Category = "Root")
		UStaticMeshComponent* northEdge;

	UPROPERTY(VisibleAnywhere, Category = "Root")
		UStaticMeshComponent* southEdge;

	UPROPERTY(VisibleAnywhere, Category = "Root")
		UStaticMeshComponent* eastEdge;

	UPROPERTY(VisibleAnywhere, Category = "Root")
		UStaticMeshComponent* westEdge;

	//ID of this cover area
	UPROPERTY(VisibleAnywhere, Category = "Cover")
	int32 id;

	//ID of the partnering cover area
	UPROPERTY(VisibleAnywhere, Category = "Cover")
	int32 partnerID;

	//Size of the cover area
	float coverSize;

	//Surface that the cover is on
	ESurfaceType surface;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	//Rootcomponent
	UPROPERTY(VisibleAnywhere, Category = "Root")
		USceneComponent* root;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
