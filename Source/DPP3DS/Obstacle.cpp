// Fill out your copyright notice in the Description page of Project Settings.


#include "Obstacle.h"

// Sets default values
AObstacle::AObstacle()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	//Create the root
	root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = root;
	root->SetMobility(EComponentMobility::Static);

	//Find the obstacle material and cube mesh
	UMaterialInstance* obstacleMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/ObstacleMI.ObstacleMI'")).Object;
	UMaterialInstance* cubeMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/ObstacleMICube.ObstacleMICube'")).Object;
	UStaticMesh* cubeMesh = ConstructorHelpers::FObjectFinder<UStaticMesh>(TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'")).Object;

	//Create decal objects
	northDecal = CreateDefaultSubobject<UDecalComponent>("northDecal");
	northDecal->SetupAttachment(root);
	northDecal->SetDecalMaterial(obstacleMat); 
	northDecal->DecalSize = FVector(1.0f, 50*size.Y, 50*size.Z);
	northDecal->SetRelativeLocation(FVector(50*size.X, 0, 0));

	southDecal = CreateDefaultSubobject<UDecalComponent>("southDecal");
	southDecal->SetupAttachment(root);
	southDecal->SetDecalMaterial(obstacleMat);
	southDecal->DecalSize = FVector(1.0f, 50*size.Y, 50*size.Z);
	southDecal->SetRelativeLocation(FVector(-50 * size.X, 0, 0));

	eastDecal = CreateDefaultSubobject<UDecalComponent>("eastDecal");
	eastDecal->SetupAttachment(root);
	eastDecal->SetDecalMaterial(obstacleMat);
	eastDecal->DecalSize = FVector(50*size.X, 1.0f, 50*size.Z);
	eastDecal->SetRelativeLocation(FVector(0, 50 * size.Y, 0));

	westDecal = CreateDefaultSubobject<UDecalComponent>("westDecal");
	westDecal->SetupAttachment(root);
	westDecal->SetDecalMaterial(obstacleMat);
	westDecal->DecalSize = FVector(50*size.X, 1.0f, 50*size.Z);
	westDecal->SetRelativeLocation(FVector(0, -50 * size.Y, 0));

	ceilingDecal = CreateDefaultSubobject<UDecalComponent>("ceilingDecal");
	ceilingDecal->SetupAttachment(root);
	ceilingDecal->SetDecalMaterial(obstacleMat);
	ceilingDecal->DecalSize = FVector(50*size.X, 50*size.Y, 1.0f);
	ceilingDecal->SetRelativeLocation(FVector(0, 0, 50 * size.Z));

	floorDecal = CreateDefaultSubobject<UDecalComponent>("floorDecal");
	floorDecal->SetupAttachment(root);
	floorDecal->SetDecalMaterial(obstacleMat);
	floorDecal->DecalSize = FVector(50*size.X, 50*size.Y, 1.0f);
	floorDecal->SetRelativeLocation(FVector(0, 0, -50 * size.Z));

	//Create cube object
	cube = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cube"));
	cube->SetStaticMesh(cubeMesh);
	cube->SetMaterial(0, cubeMat);
	cube->SetupAttachment(RootComponent);
	cube->SetMobility(EComponentMobility::Static);
	cube->SetWorldScale3D(size);
	cube->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);

	//Set the initial size
	if (size.Equals(FVector(0))) {
		size = FVector(1);
		showCube = true;
	}
}

// Called when the game starts or when spawned
void AObstacle::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AObstacle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AObstacle::UpdateSize()
{
	//Change sizes and positions
	northDecal->DecalSize = FVector(1.0f, 50*size.Y, 50*size.Z);
	northDecal->SetRelativeLocation(FVector(50 * size.X, 0, 0));
	southDecal->DecalSize = FVector(1.0f, 50*size.Y, 50*size.Z);
	southDecal->SetRelativeLocation(FVector(-50 * size.X, 0, 0));
	eastDecal->DecalSize = FVector(50*size.X, 1.0f, 50*size.Z);
	eastDecal->SetRelativeLocation(FVector(0, 50 * size.Y, 0));
	westDecal->DecalSize = FVector(50*size.X, 1.0f, 50*size.Z);
	westDecal->SetRelativeLocation(FVector(0, -50 * size.Y, 0));
	ceilingDecal->DecalSize = FVector(50*size.X, 50*size.Y, 1.0f);
	ceilingDecal->SetRelativeLocation(FVector(0, 0, 50 * size.Z));
	floorDecal->DecalSize = FVector(50*size.X, 50*size.Y, 1.0f);
	floorDecal->SetRelativeLocation(FVector(0, 0, -50 * size.Z));

	//Change the size of the cube
	cube->SetWorldScale3D(size);
}

void AObstacle::UpdateVisuals()
{
	if (showCube) {
		cube->SetVisibility(true);
		northDecal->SetVisibility(false);
		southDecal->SetVisibility(false);
		eastDecal->SetVisibility(false);
		westDecal->SetVisibility(false);
		ceilingDecal->SetVisibility(false);
		floorDecal->SetVisibility(false);
	}
	else {
		cube->SetVisibility(false);
		northDecal->SetVisibility(true);
		southDecal->SetVisibility(true);
		eastDecal->SetVisibility(true);
		westDecal->SetVisibility(true);
		ceilingDecal->SetVisibility(true);
		floorDecal->SetVisibility(true);
	}
}

