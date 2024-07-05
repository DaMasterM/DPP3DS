// Fill out your copyright notice in the Description page of Project Settings.


#include "CoverArea.h"

//DEPRECATED CLASS!!!!!!

// Sets default values
ACoverArea::ACoverArea()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	UStaticMesh* cubeMesh = ConstructorHelpers::FObjectFinder<UStaticMesh>(TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'")).Object;
	UMaterialInstance* mat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/CoverMI.CoverMI'")).Object;
	
	root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = root;
	root->SetMobility(EComponentMobility::Static);

	northEdge = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("North Edge"));
	northEdge->SetStaticMesh(cubeMesh);
	northEdge->SetMaterial(0, mat);
	northEdge->SetupAttachment(root);
	northEdge->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	northEdge->SetCollisionProfileName("EdgeTraceBlocker");
	northEdge->SetWorldScale3D(FVector(0.01f, 1, 1));
	northEdge->SetRelativeLocation(FVector(0, 50, 0));

	southEdge = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("South Edge"));
	southEdge->SetStaticMesh(cubeMesh);
	southEdge->SetMaterial(0, mat);
	southEdge->SetupAttachment(root);
	southEdge->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	southEdge->SetCollisionProfileName("EdgeTraceBlocker");
	southEdge->SetWorldScale3D(FVector(0.01f, 1, 1));
	southEdge->SetRelativeLocation(FVector(0, -50, 0));

	eastEdge = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("East Edge"));
	eastEdge->SetStaticMesh(cubeMesh);
	eastEdge->SetMaterial(0, mat);
	eastEdge->SetupAttachment(root);
	eastEdge->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	eastEdge->SetCollisionProfileName("EdgeTraceBlocker");
	eastEdge->SetWorldScale3D(FVector(1, 0.01f, 1));
	eastEdge->SetRelativeLocation(FVector(50, 0, 0));

	westEdge = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("West Edge"));
	westEdge->SetStaticMesh(cubeMesh);
	westEdge->SetMaterial(0, mat);
	westEdge->SetupAttachment(root);
	westEdge->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	westEdge->SetCollisionProfileName("EdgeTraceBlocker");
	westEdge->SetWorldScale3D(FVector(1, 0.01f, 1));
	westEdge->SetRelativeLocation(FVector(-50, 0, 0));
}

// Called when the game starts or when spawned
void ACoverArea::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ACoverArea::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}