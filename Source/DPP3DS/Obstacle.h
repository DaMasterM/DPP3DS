// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/DecalComponent.h"
#include "Obstacle.generated.h"

UCLASS()
class DPP3DS_API AObstacle : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AObstacle();

	UPROPERTY(VisibleAnywhere, Category = "Root")
		USceneComponent* root;

	//Size of the obstacle
	UPROPERTY(EditAnywhere, Category = "Rendering")
		FVector size;

	//Option for showing either the decals or the cube
	UPROPERTY(EditAnywhere, Category = "Rendering")
		bool showCube;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	//Six decals to form the obstacle
	UDecalComponent* northDecal;
	UDecalComponent* southDecal;
	UDecalComponent* eastDecal;
	UDecalComponent* westDecal;
	UDecalComponent* ceilingDecal;
	UDecalComponent* floorDecal;

	//Cube to show the obstacle in 3D space instead of as a decal
	UStaticMeshComponent* cube;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(CallInEditor, Category = "Rendering")
		void UpdateSize();

	UFUNCTION(CallInEditor, Category = "Rendering")
		void UpdateVisuals();
};
