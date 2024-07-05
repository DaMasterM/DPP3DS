// Fill out your copyright notice in the Description page of Project Settings.


#include "Vertex.h"

// Sets default values
AVertex::AVertex(const FObjectInitializer&)
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	//Find the cube mesh and vertex material
	UStaticMesh* cubeMesh = ConstructorHelpers::FObjectFinder<UStaticMesh>(TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'")).Object;
	regularMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/VertexRegularMI.VertexRegularMI'")).Object;
	connectionMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/VertexConnectionMI.VertexConnectionMI'")).Object;
	pathMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/VertexPathMI.VertexPathMI'")).Object;

	//Create the root of this vertex
	root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = root;
	root->SetMobility(EComponentMobility::Static);

	//Create the cube representing the vertex
	cube = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cube"));
	cube->SetStaticMesh(cubeMesh);
	cube->SetMaterial(0, regularMat);
	cube->SetupAttachment(root);
	cube->SetMobility(EComponentMobility::Static);
	cube->SetWorldScale3D(FVector(0.09f));
	cube->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);

	showVertex();
}

// Called when the game starts or when spawned
void AVertex::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AVertex::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AVertex::showVertex()
{
	vertexVisibility = true;
	cube->SetVisibility(true);
}

void AVertex::hideVertex()
{
	vertexVisibility = true;
	cube->SetVisibility(false);
}