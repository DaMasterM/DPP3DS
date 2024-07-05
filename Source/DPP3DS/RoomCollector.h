// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BasicRoom.h"
#include "RoomCollector.generated.h"

UCLASS()
class DPP3DS_API ARoomCollector : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ARoomCollector();

	UPROPERTY(VisibleAnywhere, Category = "Room")
	TArray<ABasicRoom*> rooms;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(CallInEditor, Category = "Room")
		void collectRooms();

	UFUNCTION(CallInEditor, Category = "Room")
		void updateRooms();

	UFUNCTION(CallInEditor, Category = "Room")
		void spawnCuboids();

};
