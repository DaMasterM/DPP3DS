// Fill out your copyright notice in the Description page of Project Settings.


#include "BasicRoom.h"

//Check if a wall should exist and make it (non)existent
void ConstructWall(UStaticMeshComponent* wall, bool has, bool vis) {
	if (has) {
		wall->SetVisibility(vis);
		wall->SetCollisionProfileName("EdgeTraceBlocker");
	}
	else {
		wall->SetVisibility(false);
		wall->SetCollisionProfileName("NoCollision");
	}
}

// Constructor
ABasicRoom::ABasicRoom()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false; 
	
	//Find the cube component to make walls with
	UStaticMesh* cube = ConstructorHelpers::FObjectFinder<UStaticMesh>(TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'")).Object;

	//Find the standard wall materials
	UMaterialInstance* nWallMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/WallNorthMI.WallNorthMI'")).Object;
	UMaterialInstance* sWallMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/WallSouthMI.WallSouthMI'")).Object;
	UMaterialInstance* eWallMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/WallEastMI.WallEastMI'")).Object;
	UMaterialInstance* wWallMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/WallWestMI.WallWestMI'")).Object;
	UMaterialInstance* ceilingMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/CeilingMI.CeilingMI'")).Object;
	UMaterialInstance* floorMat = ConstructorHelpers::FObjectFinder<UMaterialInstance>(TEXT("Material'/Game/Materials/FloorMI.FloorMI'")).Object;

	//Create a root object
	root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = root;
	root->SetMobility(EComponentMobility::Static);

	//Set the initial size and walls
	if (size.Equals(FVector(0))) {
		size = FVector(1);
		hasNorthWall = true;
		hasSouthWall = true;
		hasEastWall = true;
		hasWestWall = true;
		hasCeiling = true;
		hasFloor = true;
		visNorthWall = true;
		visSouthWall = true;
		visEastWall = true;
		visWestWall = true;
		visCeiling = true;
		visFloor = true;
	}

	//Add a light for visibility in the lit environment
	light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Point Light"));
	light->SetupAttachment(root);
	light->SetMobility(EComponentMobility::Static);
	light->SetIntensity(500);

	//Create the walls of the room
		northWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("North Wall"));
		northWall->SetStaticMesh(cube);
		northWall->SetMaterial(0, nWallMat);
		northWall->SetupAttachment(root);
		northWall->SetMobility(EComponentMobility::Static);

		southWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("South Wall"));
		southWall->SetStaticMesh(cube);
		southWall->SetMaterial(0, sWallMat);
		southWall->SetupAttachment(root);

		eastWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("East Wall"));
		eastWall->SetStaticMesh(cube);
		eastWall->SetMaterial(0, eWallMat);
		eastWall->SetupAttachment(root);

		westWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("West Wall"));
		westWall->SetStaticMesh(cube);
		westWall->SetMaterial(0, wWallMat);
		westWall->SetupAttachment(root);

		ceiling = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ceiling"));
		ceiling->SetStaticMesh(cube);
		ceiling->SetMaterial(0, ceilingMat);
		ceiling->SetupAttachment(root);

		floor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
		floor->SetStaticMesh(cube);
		floor->SetMaterial(0, floorMat);
		floor->SetupAttachment(root);

		//Now update the size of the cubes
		UpdateSize();
		UpdateWalls();
}

// Called when the game starts or when spawned
void ABasicRoom::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ABasicRoom::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ABasicRoom::UpdateSize()
{
	//Change the light
	light->SetMobility(EComponentMobility::Stationary);
	light->SetIntensity(1000);
	light->SetMobility(EComponentMobility::Static);

	//The ceiling and floor are always in the same location regardless of the walls
	ceiling->SetRelativeLocation(FVector(0, 0, 50 * size.Z));
	floor->SetRelativeLocation(FVector(0, 0, -50 * size.Z));

	//Room has floor and ceiling
	if (hasFloor && hasCeiling) {
		//Set position and size of the north and south walls
		northWall->SetRelativeLocation(FVector(50 * size.X, 0, 0));
		southWall->SetRelativeLocation(FVector(-50 * size.X, 0, 0));
		northWall->SetWorldScale3D(FVector(0.5, (size.Y + 0.5), (size.Z-0.5)));
		southWall->SetWorldScale3D(FVector(0.5, (size.Y + 0.5), (size.Z-0.5)));
		
		//Set position and size of the east and west walls
		//Room has north and south walls
		if (hasNorthWall && hasSouthWall) {
			eastWall->SetRelativeLocation(FVector(0, 50 * size.Y, 0));
			westWall->SetRelativeLocation(FVector(0, -50 * size.Y, 0));
			eastWall->SetWorldScale3D(FVector((size.X-0.5), 0.5, (size.Z-0.5)));
			westWall->SetWorldScale3D(FVector((size.X-0.5), 0.5, (size.Z-0.5)));
		}

		//Room has either a north or a south wall
		else if (hasNorthWall || hasSouthWall) {
			if (hasNorthWall) {
				eastWall->SetRelativeLocation(FVector(-25, 50 * size.Y, 0));
				westWall->SetRelativeLocation(FVector(-25, -50 * size.Y, 0));
			}
			else {
				eastWall->SetRelativeLocation(FVector(25, 50 * size.Y, 0));
				westWall->SetRelativeLocation(FVector(25, -50 * size.Y, 0));
			}
			eastWall->SetWorldScale3D(FVector(size.X, 0.5, (size.Z-0.5)));
			westWall->SetWorldScale3D(FVector(size.X, 0.5, (size.Z-0.5)));
		}

		//Room has no north or south wall
		else {
			eastWall->SetRelativeLocation(FVector(0, 50 * size.Y, 0));
			westWall->SetRelativeLocation(FVector(0, -50 * size.Y, 0));
			eastWall->SetWorldScale3D(FVector((size.X + 0.5), 0.5, (size.Z-0.5)));
			westWall->SetWorldScale3D(FVector((size.X + 0.5), 0.5, (size.Z-0.5)));
		}
	}

	//Room has either a floor or a ceiling
	else if (hasFloor || hasCeiling) {

		//Room has a floor
		if (hasFloor) {
			northWall->SetRelativeLocation(FVector(50 * size.X, 0, 25));
			southWall->SetRelativeLocation(FVector(-50 * size.X, 0, 25));

			//Room has north and south walls
			if (hasNorthWall && hasSouthWall) {
				eastWall->SetRelativeLocation(FVector(0, 50 * size.Y, 25));
				westWall->SetRelativeLocation(FVector(0, -50 * size.Y, 25));
				eastWall->SetWorldScale3D(FVector((size.X-0.5), 0.5, size.Z));
				westWall->SetWorldScale3D(FVector((size.X-0.5), 0.5, size.Z));
			}

			//Room has either a north or a south wall
			else if (hasNorthWall || hasSouthWall) {
				if (hasNorthWall) {
					eastWall->SetRelativeLocation(FVector(-25, 50 * size.Y, 25));
					westWall->SetRelativeLocation(FVector(-25, -50 * size.Y, 25));
				}
				else {
					eastWall->SetRelativeLocation(FVector(25, 50 * size.Y, 25));
					westWall->SetRelativeLocation(FVector(25, -50 * size.Y, 25));
				}
				eastWall->SetWorldScale3D(FVector(size.X, 0.5, size.Z));
				westWall->SetWorldScale3D(FVector(size.X, 0.5, size.Z));
			}

			//Room has no north or south walls
			else {
				eastWall->SetRelativeLocation(FVector(0, 50 * size.Y, 25));
				westWall->SetRelativeLocation(FVector(0, -50 * size.Y, 25));
				eastWall->SetWorldScale3D(FVector((size.X + 0.5), 0.5, size.Z));
				westWall->SetWorldScale3D(FVector((size.X + 0.5), 0.5, size.Z));
			}
		}

		//Room has a ceiling
		else {
			northWall->SetRelativeLocation(FVector(50 * size.X, 0, -25));
			southWall->SetRelativeLocation(FVector(-50 * size.X, 0, -25));

			//Room has north and south walls
			if (hasNorthWall && hasSouthWall) {
				eastWall->SetRelativeLocation(FVector(0, 50 * size.Y, -25));
				westWall->SetRelativeLocation(FVector(0, -50 * size.Y, -25));
				eastWall->SetWorldScale3D(FVector((size.X-0.5), 0.5, size.Z));
				westWall->SetWorldScale3D(FVector((size.X-0.5), 0.5, size.Z));
			}

			//Room has either a north or a south wall
			else if (hasNorthWall || hasSouthWall) {
				if (hasNorthWall) {
					eastWall->SetRelativeLocation(FVector(-25, 50 * size.Y, -25));
					westWall->SetRelativeLocation(FVector(-25, -50 * size.Y, -25));
				}
				else {
					eastWall->SetRelativeLocation(FVector(25, 50 * size.Y, -25));
					westWall->SetRelativeLocation(FVector(25, -50 * size.Y, -25));
				}
				eastWall->SetWorldScale3D(FVector(size.X, 0.5, size.Z));
				westWall->SetWorldScale3D(FVector(size.X, 0.5, size.Z));
			}

			//Room has no north or south walls
			else {
				eastWall->SetRelativeLocation(FVector(0, 50 * size.Y, -25));
				westWall->SetRelativeLocation(FVector(0, -50 * size.Y, -25));
				eastWall->SetWorldScale3D(FVector((size.X + 0.5), 0.5, size.Z));
				westWall->SetWorldScale3D(FVector((size.X + 0.5), 0.5, size.Z));
			}
		}

		northWall->SetWorldScale3D(FVector(0.5, (size.Y + 0.5), size.Z));
		southWall->SetWorldScale3D(FVector(0.5, (size.Y + 0.5), size.Z));
	}

	//Room has no floor or ceiling
	else {
		northWall->SetRelativeLocation(FVector(50 * size.X, 0, 0));
		southWall->SetRelativeLocation(FVector(-50 * size.X, 0, 0));
		northWall->SetWorldScale3D(FVector(0.5, (size.Y + 0.5), (size.Z + 0.5)));
		southWall->SetWorldScale3D(FVector(0.5, (size.Y + 0.5), (size.Z + 0.5)));

		//Room has north and south walls
		if (hasNorthWall && hasSouthWall) {
			eastWall->SetRelativeLocation(FVector(0, 50 * size.Y, 0));
			westWall->SetRelativeLocation(FVector(0, -50 * size.Y, 0));
			eastWall->SetWorldScale3D(FVector((size.X-0.5), 0.5, (size.Z + 0.5)));
			westWall->SetWorldScale3D(FVector((size.X-0.5), 0.5, (size.Z + 0.5)));
		}

		//Room has either a north or a south wall
		else if (hasNorthWall || hasSouthWall) {
			if (hasNorthWall) {
				eastWall->SetRelativeLocation(FVector(-25, 50 * size.Y, 0));
				westWall->SetRelativeLocation(FVector(-25, -50 * size.Y, 0));
			}
			else {
				eastWall->SetRelativeLocation(FVector(25, 50 * size.Y, 0));
				westWall->SetRelativeLocation(FVector(25, -50 * size.Y, 0));
			}
			eastWall->SetWorldScale3D(FVector(size.X, 0.5, (size.Z + 0.5)));
			westWall->SetWorldScale3D(FVector(size.X, 0.5, (size.Z + 0.5)));
		}

		//Room has no north or south wall
		else {
			eastWall->SetRelativeLocation(FVector(0, 50 * size.Y, 0));
			westWall->SetRelativeLocation(FVector(0, -50 * size.Y, 0));
			eastWall->SetWorldScale3D(FVector((size.X + 0.5), 0.5, (size.Z + 0.5)));
			westWall->SetWorldScale3D(FVector((size.X + 0.5), 0.5, (size.Z + 0.5)));
		}
	}
	ceiling->SetWorldScale3D(FVector((size.X + 0.5), (size.Y + 0.5), 0.5));
	floor->SetWorldScale3D(FVector((size.X + 0.5), (size.Y + 0.5), 0.5));

	//If the wall does not exist according to hasX, make its size 1
	if (!hasNorthWall) { northWall->SetWorldScale3D(FVector(1)); }
	if (!hasSouthWall) { southWall->SetWorldScale3D(FVector(1)); }
	if (!hasEastWall) { eastWall->SetWorldScale3D(FVector(1)); }
	if (!hasWestWall) { westWall->SetWorldScale3D(FVector(1)); }
	if (!hasCeiling) { ceiling->SetWorldScale3D(FVector(1)); }
	if (!hasFloor) { floor->SetWorldScale3D(FVector(1)); }

}

void ABasicRoom::UpdateWalls()
{
	//If a wall is visible, show it. If not, make it invisible
	ConstructWall(northWall, hasNorthWall, visNorthWall);
	ConstructWall(southWall, hasSouthWall, visSouthWall);
	ConstructWall(eastWall, hasEastWall, visEastWall);
	ConstructWall(westWall, hasWestWall, visWestWall);
	ConstructWall(ceiling, hasCeiling, visCeiling);
	ConstructWall(floor, hasFloor, visFloor);
}

void ABasicRoom::GenerateSurfaces() 
{
	//Set the potential sizes of all the surfaces
	FVector xSurfSize = FVector(0.1f, (size.Y - 0.5f), (size.Z - 0.5f));
	FVector ySurfSize = FVector((size.X - 0.5f), 0.1f, (size.Z - 0.5f));
	FVector zSurfSize = FVector((size.X - 0.5f), (size.Y - 0.5f), 0.1f);

	FVector northSurfRelativeLocation = FVector((50 * size.X -30), 0, 0);
	FVector southSurfRelativeLocation = FVector((-50 * size.X + 30), 0, 0);
	FVector eastSurfRelativeLocation = FVector(0, (50 * size.Y - 30), 0);
	FVector westSurfRelativeLocation = FVector(0, (-50 * size.Y + 30), 0);
	FVector ceilingSurfRelativeLocation = FVector(0, 0, (50 * size.Z - 30));
	FVector floorSurfRelativeLocation = FVector(0, 0, (-50 * size.Z + 30));

	if (!hasNorthWall) {
		ySurfSize.X += 0.5f;
		zSurfSize.X += 0.5f;
		eastSurfRelativeLocation.X += 25;
		westSurfRelativeLocation.X += 25;
		ceilingSurfRelativeLocation.X += 25;
		floorSurfRelativeLocation.X += 25;
	}
	if (!hasSouthWall) {
		ySurfSize.X += 0.5f;
		zSurfSize.X += 0.5f;
		eastSurfRelativeLocation.X -= 25;
		westSurfRelativeLocation.X -= 25;
		ceilingSurfRelativeLocation.X -= 25;
		floorSurfRelativeLocation.X -= 25;
	}
	if (!hasEastWall) {
		xSurfSize.Y += 0.5f;
		zSurfSize.Y += 0.5f;
		northSurfRelativeLocation.Y += 25;
		southSurfRelativeLocation.Y += 25;
		ceilingSurfRelativeLocation.Y += 25;
		floorSurfRelativeLocation.Y += 25;
	}
	if (!hasWestWall) {
		xSurfSize.Y += 0.5f;
		zSurfSize.Y += 0.5f;
		northSurfRelativeLocation.Y -= 25;
		southSurfRelativeLocation.Y -= 25;
		ceilingSurfRelativeLocation.Y -= 25;
		floorSurfRelativeLocation.Y -= 25;
	}
	if (!hasCeiling) {
		xSurfSize.Z += 0.5f;
		ySurfSize.Z += 0.5f;
		northSurfRelativeLocation.Z += 25;
		southSurfRelativeLocation.Z += 25;
		eastSurfRelativeLocation.Z += 25;
		westSurfRelativeLocation.Z += 25;
	}
	if (!hasFloor) {
		xSurfSize.Z += 0.5f;
		ySurfSize.Z += 0.5f;
		northSurfRelativeLocation.Z -= 25;
		southSurfRelativeLocation.Z -= 25;
		eastSurfRelativeLocation.Z -= 25;
		westSurfRelativeLocation.Z -= 25;
	}

	if (hasNorthWall && generateNorthWallSurface) {
		spawnSurface(northWall, xSurfSize, northSurfRelativeLocation, ESurfaceType::NorthWall);
	}
	if (hasSouthWall && generateSouthWallSurface) {
		spawnSurface(southWall, xSurfSize, southSurfRelativeLocation, ESurfaceType::SouthWall);
	}
	if (hasEastWall && generateEastWallSurface) {
		spawnSurface(eastWall, ySurfSize, eastSurfRelativeLocation, ESurfaceType::EastWall);
	}
	if (hasWestWall && generateWestWallSurface) {
		spawnSurface(westWall, ySurfSize, westSurfRelativeLocation, ESurfaceType::WestWall);
	}
	if (hasCeiling && generateCeilingSurface) {
		spawnSurface(ceiling, zSurfSize, ceilingSurfRelativeLocation, ESurfaceType::Ceiling);
	}
	if (hasFloor && generateFloorSurface) {
		spawnSurface(floor, zSurfSize, floorSurfRelativeLocation, ESurfaceType::Floor);
	}
}

void ABasicRoom::spawnSurface(UStaticMeshComponent* wall, FVector extent, FVector relativeLocation, ESurfaceType surfaceType) {
	FActorSpawnParameters SpawnInfo;
	FVector spawnLocation = GetActorLocation() + relativeLocation;

	ASurfaceArea* newSurface = GetWorld()->SpawnActor<ASurfaceArea>(spawnLocation, FRotator(0, 0, 0), SpawnInfo);
	newSurface->surface = surfaceType;
	newSurface->updateMaterials();
	newSurface->addBox();
	newSurface->cubes[0]->SetWorldScale3D(extent);
}

void ABasicRoom::spawnCuboid() {
	FActorSpawnParameters SpawnInfo;
	FVector spawnLocation = this->GetActorLocation(); 
	
	AProjectionCuboid* newCuboid = GetWorld()->SpawnActor<AProjectionCuboid>(spawnLocation, FRotator(0, 0, 0), SpawnInfo);
	newCuboid->spawnLocation = GetActorLocation();
	newCuboid->setSurfaces(hasCeiling, hasFloor, hasNorthWall, hasSouthWall, hasEastWall, hasWestWall);
	newCuboid->size = size;
	newCuboid->updateSize();
}