// Fill out your copyright notice in the Description page of Project Settings.


#include "PRMEdge.h"

// Sets default values
APRMEdge::APRMEdge()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	
	//Find the cube mesh and vertex material
	UStaticMesh* cubeMesh = ConstructorHelpers::FObjectFinder<UStaticMesh>(TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'")).Object;
	blackMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/EdgeBlackMI.EdgeBlackMI'")).Object;
	regularMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/EdgeRegularMI.EdgeRegularMI'")).Object;
	pathMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/EdgePathMI.EdgePathMI'")).Object;

	//Create the root of this vertex
	root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = root;
	root->SetMobility(EComponentMobility::Static);

	//Create the cube representing the vertex
	spline = CreateDefaultSubobject<USplineMeshComponent>(TEXT("Spline"));
	spline->SetStaticMesh(cubeMesh);
	spline->SetMaterial(0, blackMat);
	spline->SetupAttachment(root);
	spline->SetMobility(EComponentMobility::Static);
	spline->SetCollisionProfileName("CoverTraceBlocker");
	//spline->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	spline->SetStartScale(FVector2D(0.08f));
	spline->SetEndScale(FVector2D(0.08f));

	if (edgeVisibility) { showEdge(); }
	else { hideEdge(); }
}

// Called when the game starts or when spawned
void APRMEdge::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APRMEdge::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void APRMEdge::showEdge() {
	edgeVisibility = true;
	spline->SetVisibility(true);
}

void APRMEdge::hideEdge() {
	edgeVisibility = false;
	spline->SetVisibility(false);
}

void APRMEdge::highlightEdge() {
	showEdge();
	spline->SetMaterial(0, regularMat);
}

void APRMEdge::delightEdge() {
	spline->SetMaterial(0, blackMat);
}

void APRMEdge::makePathEdge()
{
	showEdge();
	spline->SetMaterial(0, pathMat);
}

void APRMEdge::removePathEdge()
{
	spline->SetMaterial(0, blackMat);
}

bool APRMEdge::isEndVertex(int32 vertexID)
{
	return endVertices.Contains(vertexID);
}

void APRMEdge::setEdge(FVector vertexLocationA, FVector vertexLocationB) {
	FVector localEnd = vertexLocationB - vertexLocationA;
	FVector direction = localEnd.GetSafeNormal();
	
	if (localEnd.X == localEnd.Y && localEnd.X == 0) {
		spline->SplineUpDir = FVector(1, 0, 0);
		if (localEnd.Z < 0) { spline->SetStartAndEnd(FVector(0), FVector(0, 0, -1), localEnd, FVector(0, 0, -1), true); }
		else { spline->SetStartAndEnd(FVector(0), FVector(0, 0, 1), localEnd, FVector(0, 0, 1), true); }
	}
	else { spline->SetStartAndEnd(FVector(0), direction, localEnd, direction, true); }
}