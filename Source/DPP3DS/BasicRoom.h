// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/PointLightComponent.h"
#include "SurfaceArea.h"
#include "ProjectionCuboid.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Utils.h"
#include "BasicRoom.generated.h"

UCLASS()
class DPP3DS_API ABasicRoom : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ABasicRoom();
	//The root of the room
	UPROPERTY(VisibleAnywhere, Category = "Root")
	USceneComponent* root;

	//Size of the room in the x, y and z directions, assuming no rotations
	UPROPERTY(EditAnywhere, Category = "Room")
		FVector size;

	//Wall in the +x direction
	UPROPERTY(EditAnywhere, Category = "Room")
		bool hasNorthWall;
	UPROPERTY(EditAnywhere, Category = "Visibility")
		bool visNorthWall;
	UPROPERTY(EditAnywhere, Category = "Surface Area")
		bool generateNorthWallSurface;

	//Wall in the -x direction
	UPROPERTY(EditAnywhere, Category = "Room")
		bool hasSouthWall;
	UPROPERTY(EditAnywhere, Category = "Visibility")
		bool visSouthWall;
	UPROPERTY(EditAnywhere, Category = "Surface Area")
		bool generateSouthWallSurface;

	//Wall in the +y direction
	UPROPERTY(EditAnywhere, Category = "Room")
		bool hasEastWall;
	UPROPERTY(EditAnywhere, Category = "Visibility")
		bool visEastWall;
	UPROPERTY(EditAnywhere, Category = "Surface Area")
		bool generateEastWallSurface;

	//Wall in the -y direction
	UPROPERTY(EditAnywhere, Category = "Room")
		bool hasWestWall;
	UPROPERTY(EditAnywhere, Category = "Visibility")
		bool visWestWall;
	UPROPERTY(EditAnywhere, Category = "Surface Area")
		bool generateWestWallSurface;

	//Wall in the +z direction
	UPROPERTY(EditAnywhere, Category = "Room")
		bool hasCeiling;
	UPROPERTY(EditAnywhere, Category = "Visibility")
		bool visCeiling;
	UPROPERTY(EditAnywhere, Category = "Surface Area")
		bool generateCeilingSurface;

	//Wall in the -z direction
	UPROPERTY(EditAnywhere, Category = "Room")
		bool hasFloor;
	UPROPERTY(EditAnywhere, Category = "Visibility")
		bool visFloor;
	UPROPERTY(EditAnywhere, Category = "Surface Area")
		bool generateFloorSurface;

	UStaticMeshComponent* northWall;
	UStaticMeshComponent* southWall;
	UStaticMeshComponent* eastWall;
	UStaticMeshComponent* westWall;
	UStaticMeshComponent* ceiling;
	UStaticMeshComponent* floor;

	UPROPERTY(EditAnywhere, Category = "Room")
		bool isStairs;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Root")
		UPointLightComponent* light;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(CallInEditor, Category = "Room")
		void UpdateSize();

	UFUNCTION(CallInEditor, Category = "Room")
		void UpdateWalls();

	//Function to generate SurfaceAreas on all walls of this room. Only use this for non-connected rooms (no two same walls next to one another)
	UFUNCTION(CallInEditor, Category = "Surface Area")
		void GenerateSurfaces();

	void spawnSurface(UStaticMeshComponent* wall, FVector extent, FVector relativeLocation, ESurfaceType surfaceType);

	UFUNCTION(CallInEditor, Category = "Room")
	void spawnCuboid();
};
