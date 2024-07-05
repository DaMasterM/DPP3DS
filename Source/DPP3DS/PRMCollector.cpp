// Fill out your copyright notice in the Description page of Project Settings.


#include "PRMCollector.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "PRM.h"
#include "BasicRoom.h"

// Sets default values
APRMCollector::APRMCollector()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void APRMCollector::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APRMCollector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void APRMCollector::collectPRMS() {
	//PRM Generation starts here, so set the start time to now.
	startTimeMoment = FDateTime::Now();

	//Prepare all the surfaces
	connectivityGraph->CollectSurfaces();
	connectivityGraph->ConnectSurfaces();

	//Start with an empty PRM array
	PRMS.Empty();
	int32 id = 0;

	//Find all PRMS
	TArray<AActor*> temp;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APRM::StaticClass(), temp);

	for (AActor* actor : temp) {
		APRM* newPRM = (APRM*)actor;
		newPRM->id = id;
		id++;
		PRMS.AddUnique(newPRM);

		//Set the vertex count for every PRM
		newPRM->calculateWeights();
		newPRM->setVertexCount(inverseVertexDensity);
	}

	//Find all projection cuboids
	temp = {};
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AProjectionCuboid::StaticClass(), temp);

	for (AActor* actor : temp) {
		AProjectionCuboid* newCuboid = (AProjectionCuboid*)actor;
		projectionCuboids.AddUnique(newCuboid);

		//Set cuboid values
		newCuboid->inverseVertexDensity = inverseVertexDensity;
		newCuboid->agentSize = agentSize;
		newCuboid->setVertexCounts();
		newCuboid->findPRMS(PRMS);
	}

	//Reset the inter prm connections
	interPRMConnections = {};
}

void APRMCollector::generatePRMS() {
	startVertexID = 0;
	startEdgeID = 0;

	//Depending on which PRM method is used, generate a PRM
	if (useMedialAxis) {
		//Approximate medial axis method
		if (approximateMedialAxis) { generateApproximateMedialPRM(); }
		//Exact medial axis method
		else { generateExactMedialPRM(); }
	}
	//Purely random method
	else { generateRandomPRM(); }

	//Add all created vertices into an array
	TArray<AActor*> temp;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVertex::StaticClass(), temp);
	for (AActor* actor : temp) { 
		AVertex* newVertex = (AVertex*)actor; 
		if (newVertex != nullptr) { vertices.AddUnique(newVertex); }
	}

	//Add all created edges into an array
	temp = {};
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APRMEdge::StaticClass(), temp);
	for (AActor* actor : temp) {
		APRMEdge* newEdge = (APRMEdge*)actor;
		if (newEdge != nullptr) { edges.AddUnique(newEdge); }
	}
}

void APRMCollector::setNeighboursPRMS()
{
	//Make a temporary array to loop over such that PRMS can be safely removed
	TArray<APRM*> tempPRMS;
	for (APRM* PRM : PRMS) {
		tempPRMS.AddUnique(PRM);
		PRM->neighbours.Empty();
	}

	totalConnections = 0;

	//Go over all PRM pairs to check which should be connected
	for (APRM* a : PRMS) {
		for (APRM* b : tempPRMS) {
			if (a != b) {
				bool madeNeighbour = false;
				
				//Don't check if a and b are already neighbours.
				if (!b->isNeighbour(a->id)) {
					
					//Go over all pairs of surfaces with one surface in PRM a and one in b
					for (int i = 0; i < a->surfaces.Num(); i++) {
						for (int j = 0; j < b->surfaces.Num(); j++) {
							
							//If a surface in a and a surface in b are neighbours, the PRMs are also neighbours
							if (a->surfaces[i]->isNeighbour(b->surfaces[j]->id)) {
								EConnectionMethod neighbourCM = a->surfaces[i]->getNeighbourFromID(b->surfaces[j]->id).cm;

								//Set the PRMs to be neighbours
								FPRMNeighbour neighbourA = FPRMNeighbour(b->id, neighbourCM, a->surfaces[i]->id, b->surfaces[j]->id);
								FPRMNeighbour neighbourB = FPRMNeighbour(a->id, neighbourCM, b->surfaces[j]->id, a->surfaces[i]->id);
								a->neighbours.Add(neighbourA);
								b->neighbours.Add(neighbourB);
								madeNeighbour = true;
							}
						}
					}
				}

				//Count this neighbour pair as a new connection
				if (madeNeighbour) { totalConnections++; }
			}
		}
		//Remove a from the temporary array, since it has already been connected to its neighbours
		if (tempPRMS.Contains(a)) { tempPRMS.Remove(a); }
	}

	//Set the 3D vertex counts
	for (APRM* PRM : PRMS) {
		PRM->edgeCount3D = PRM->edgeCount;
		for (FPRMNeighbour PRMNeighbour : PRM->neighbours) { PRM->edgeCount3D += PRMS[PRMNeighbour.neighbourID]->edgeCount; }
		//PRM->edgeCount3D /= (PRM->neighbours.Num() / 3) + 1;
	}
}

void APRMCollector::connectPRMS()
{
	int32 notNearby = 0;
	int32 nearbyNotConnected = 0;
	madeConnections = 0; TArray<TSet<APRM*>> prmConnections;

	//If there are no interPRMConnections yet, find them. This is used in all cases exact KNN3D
	if (interPRMConnections.Num() < 1){
		//Put all PRMS in a separate array to ensure they can be removed from an array
		//TArray<APRM*> tempPRMS;
		//for (APRM* PRM : PRMS) { tempPRMS.AddUnique(PRM); }
		//UE_LOG(LogTemp, Log, TEXT("There are no connections yet"));

		//Go over every PRM in a way that each neighbouring pair is only handled once
		for (APRM* a : PRMS) {
			//Keeps track of the IDs of the PRMS that were connected to already.
			TArray<int32> connectedIDS;

			for (FPRMNeighbour neighbourStruct : a->neighbours) {
				bool addedEdge = false;
				if (neighbourStruct.neighbourID > 0 && neighbourStruct.neighbourID < PRMS.Num()) {
					//Find the PRM related to this neighbour
					APRM* b = PRMS[neighbourStruct.neighbourID];

					//Find the surfaces of A and B that are used in this neighbour connection
					ASurfaceArea* surfaceA = a->getSurfaceFromStruct(neighbourStruct);
					ASurfaceArea* surfaceB = a->getNeighbourSurfaceFromStruct(b, neighbourStruct);

					//Make sure surfaceB exists! If not, something went wrong with the surface connections!
					if (!surfaceB) { UE_LOG(LogTemp, Log, TEXT("No surfaceB found! a: %s; Surface A: %s; id: %d; b: %s"), *a->GetName(), *surfaceA->GetName(), surfaceA->id, *b->GetName()); return; }

					//Find the surface structs and ensure that the cubes exist
					FSurfaceNeighbour surfaceStructA = surfaceA->getNeighbourFromID(surfaceB->id);
					if (surfaceStructA.cube == nullptr) {
						UE_LOG(LogTemp, Log, TEXT("No cube found! Surfaces: %s to %s"), *surfaceA->GetName(), *surfaceB->GetName());
						return;
					}
					FSurfaceNeighbour surfaceStructB = surfaceB->getNeighbourFromID(surfaceA->id);
					if (surfaceStructB.cube == nullptr) {
						UE_LOG(LogTemp, Log, TEXT("No cube found! Surfaces: %s from %s"), *surfaceB->GetName(), *surfaceA->GetName());
						return;
					}

					///Declare all variables used in the case distinction!
					//Arrays used for overlap functions
					TArray<AVertex*> aVertices;
					TArray<AVertex*> bVertices;

					//Extra vertices in case connections have to be guaranteed
					//AVertex* extraVertexA = nullptr;
					//AVertex* extraVertexB = nullptr;

					//For defining the nearby areas and overlapping area
					FVector overlapCenter = FVector(0);
					FVector overlapExtentBox = FVector(0);
					FVector aCenter = FVector(0);
					FVector aExtentBox = FVector(0);
					FVector bCenter = FVector(0);
					FVector bExtentBox = FVector(0);

					//For the extents of the surface cube and the overlap area
					FVector overlapTNE = surfaceStructA.tne;
					FVector overlapBSW = surfaceStructA.bsw;
					FVector aTNE = surfaceStructA.cube->GetComponentLocation() + 50 * surfaceStructA.cube->GetComponentScale();
					FVector aBSW = surfaceStructA.cube->GetComponentLocation() - 50 * surfaceStructA.cube->GetComponentScale();
					FVector bTNE = surfaceStructB.cube->GetComponentLocation() + 50 * surfaceStructB.cube->GetComponentScale();
					FVector bBSW = surfaceStructB.cube->GetComponentLocation() - 50 * surfaceStructB.cube->GetComponentScale();

					//The overlaps are different in case of the stairs
					if (neighbourStruct.cm == EConnectionMethod::SCB) {
						if (surfaceStructA.connectionCube == nullptr) {
							UE_LOG(LogTemp, Log, TEXT("surfaceStructA does not have a connection cube: %s and %s"), *surfaceA->GetName(), *surfaceB->GetName());
							return;
						}
						overlapTNE = surfaceStructA.connectionCube->GetComponentLocation() + 50 * surfaceStructA.connectionCube->GetComponentScale();
						overlapBSW = surfaceStructA.connectionCube->GetComponentLocation() - 50 * surfaceStructA.connectionCube->GetComponentScale();
					}

					//Find the nearby areas for a and b (add to the TNE and BSW up to the maximum of the cube)
					findTNEBSW(overlapTNE, overlapBSW, aTNE, aBSW, bTNE, bBSW, aTNE, aBSW, bTNE, bBSW);
					findNearbyAreas(overlapTNE, overlapBSW, aTNE, aBSW, bTNE, bBSW, overlapCenter, overlapExtentBox, aCenter, aExtentBox, bCenter, bExtentBox);

					//Find all vertices in the nearby areas to (possibly) connect

					findVertices(aCenter, aExtentBox, bCenter, bExtentBox, aVertices, bVertices);

					//Prepare connections between vertex pairs
					for (AVertex* vertexA : aVertices) {
						for (AVertex* vertexB : bVertices) {
							FPRMConnection connection = FPRMConnection(vertexA, vertexB);
							interPRMConnections.AddUnique(connection);
						}
					}
				}
			}

			/*//Prepare the IDs for the vertices and edges
			int32 newID = vertices.Num();
			int32 nextNewID = newID;
			int32 newEdgeID = edges.Num();

			//Normals and projections
			FVector normalA = FVector(0);
			FVector normalB = FVector(0);
			FVector projectionA = FVector(0);
			FVector projectionB = FVector(0);
			FVector projectionA2 = FVector(0);
			FVector projectionB2 = FVector(0);

			//Helper vertices, edges and their locations
			FVector helperLocationA = FVector(0);
			FVector helperLocationB = FVector(0);
			FVector searchExtent = FVector(0);
			AVertex* newHelperVertexA = nullptr;
			AVertex* newHelperVertexB = nullptr;
			APRMEdge* helperEdgeA = nullptr;
			APRMEdge* helperEdgeB = nullptr;
			APRMEdge* helperEdgeC = nullptr;

			//For the extents of the surface cube and the overlap area
			FVector overlapTNE = surfaceStructA.tne;
			FVector overlapBSW = surfaceStructA.bsw;
			FVector aTNE = surfaceStructA.cube->GetComponentLocation() + 50 * surfaceStructA.cube->GetComponentScale();
			FVector aBSW = surfaceStructA.cube->GetComponentLocation() - 50 * surfaceStructA.cube->GetComponentScale();
			FVector bTNE = surfaceStructB.cube->GetComponentLocation() + 50 * surfaceStructB.cube->GetComponentScale();
			FVector bBSW = surfaceStructB.cube->GetComponentLocation() - 50 * surfaceStructB.cube->GetComponentScale();

			//The overlaps are different in case of the stairs
			if (neighbourStruct.cm == EConnectionMethod::SCB) {
				overlapTNE = surfaceStructA.connectionCube->GetComponentLocation() + 50 * surfaceStructA.connectionCube->GetComponentScale();
				overlapBSW = surfaceStructA.connectionCube->GetComponentLocation() - 50 * surfaceStructA.connectionCube->GetComponentScale();
			}

			//Find the nearby areas for a and b (add to the TNE and BSW up to the maximum of the cube)
			findTNEBSW(overlapTNE, overlapBSW, aTNE, aBSW, bTNE, bBSW, aTNE, aBSW, bTNE, bBSW);
			findNearbyAreas(overlapTNE, overlapBSW, aTNE, aBSW, bTNE, bBSW, overlapCenter, overlapExtentBox, aCenter, aExtentBox, bCenter, bExtentBox);

			//Find all vertices in the nearby areas to (possibly) connect
			findVertices(aCenter, aExtentBox, bCenter, bExtentBox, aVertices, bVertices);

			//Prepare connections between vertex pairs
			for (AVertex* vertexA : aVertices) { for (AVertex* vertexB : bVertices) {
				FPRMConnection connection = FPRMConnection(vertexA, vertexB);
				interPRMConnections.AddUnique(connection);
			} }


			//If either PRM has no nearby vertices and the guarantee connections option is on, generate an extra vertex in the nearby area
			/*if (guaranteeNearbyVertices && aVertices.Num() < 1) {
					int32 oldID = newEdgeID;
					newEdgeID = generateExtraVertex(extraVertexA, a, surfaceA, surfaceStructA.cube, newID, newEdgeID, aTNE, aBSW, true, aVertices);

					if (oldID != newEdgeID) { notNearby++; }
				}

			if (guaranteeNearbyVertices && bVertices.Num() < 1) {
					int32 oldID = newEdgeID;
					newEdgeID = generateExtraVertex(extraVertexB, b, surfaceB, surfaceStructB.cube, newID, newEdgeID, bTNE, bBSW, true, bVertices);

					if (oldID != newEdgeID) { notNearby++; }
				}*/
			/*
				//Connect the PRMS based on the connection method
				switch (neighbourStruct.cm) {
					//Double Connection Box
				case EConnectionMethod::DCB:
					for (AVertex* vertexA : aVertices) {
						for (AVertex* vertexB : bVertices) {
							if ((vertexA->GetActorLocation() - vertexB->GetActorLocation()).Size() < 400) {
								//Find the projection of A onto the surface of B and vice versa, as well as the normals and the orthogonal to the normals
								FVector orthogonalAB = FVector(0);
								FVector connectionNormal = FVector(0);
								findNormalsDCB(neighbourStruct.cm, surfaceA, surfaceB, overlapCenter, overlapExtentBox, vertexA, vertexB, normalA, normalB, orthogonalAB, connectionNormal);
								projectVerticesDCB(vertexA, vertexB, normalA, normalB, connectionNormal, aExtentBox, bExtentBox, aTNE, aBSW, bTNE, bBSW, projectionA, projectionB);

								//In case the two surfaces are opposites, ensure that the distance between the vertices around the corner is not too big, or that the distance in general is not too big
								float projectionsDist = (projectionA - vertexA->GetActorLocation()).Size() + (projectionB - vertexB->GetActorLocation()).Size();
								if (projectionsDist < 800) {
									//Find the location of the helper vertices
									findHelperLocations(neighbourStruct.cm, vertexA, vertexB, projectionA, projectionB, normalA, normalB, orthogonalAB, connectionNormal, helperLocationA, helperLocationB);

									//Create the helper vertices
									if (checkNearSurface(helperLocationA)) { newHelperVertexA = createHelperVertex(surfaceA, helperLocationA, FVector(50), surfaceA->surface, newID, nextNewID, vertexA, vertexB); }
									newID = nextNewID;
									if (checkNearSurface(helperLocationB)) { newHelperVertexB = createHelperVertex(surfaceB, helperLocationB, FVector(50), surfaceB->surface, newID, nextNewID, vertexA, vertexB); }
									newID = nextNewID;

									//Create the edges between the (helper) vertices
									addedEdge = connectHelperVertices(a, b, vertexA, vertexB, newHelperVertexA, newHelperVertexB, helperEdgeA, helperEdgeB, helperEdgeC, newEdgeID, newEdgeID);
								}
							}
						}
					}
					break;

					//Single Connection Box. Only the stairs case is handled, as the neighbouring case is already handled by the regular PRM connections
				case EConnectionMethod::SCB:
					if (surfaceA->surface == ESurfaceType::Stairs || surfaceA->surface == ESurfaceType::StairsCeiling || surfaceB->surface == ESurfaceType::Stairs || surfaceB->surface == ESurfaceType::StairsCeiling) {
						for (AVertex* vertexA : aVertices) {
							for (AVertex* vertexB : bVertices) {
								if ((vertexA->GetActorLocation() - vertexB->GetActorLocation()).Size() < 400) {
									//Find the projection of A onto the surface of B and vice versa
									findNormals(neighbourStruct.cm, surfaceA, surfaceB, overlapCenter, vertexA, vertexB, normalA, normalB);

									//Find the normal of the side, which would be the direction from a to b if a is above
									FVector normalC = findNormalC(normalA, aCenter, bCenter);
									FVector orthogonalAC = FVector::CrossProduct(normalA, normalC).GetAbs();

									//Project the vertices onto the sides of the surfaces
									projectVerticesSCB(vertexA, vertexB, normalA, normalB, normalC, aExtentBox, bExtentBox, aTNE, aBSW, bTNE, bBSW, projectionA, projectionB);

									//Find the location for the helper vertices in between the two projections
									findHelperLocations(neighbourStruct.cm, vertexA, vertexB, projectionA, projectionB, normalA, normalB, normalC, orthogonalAC, helperLocationA, helperLocationB);

									//Find out if the transition is on the floor or the ceiling
									ESurfaceType transitionType = findStairsTransition(surfaceA->surface, surfaceB->surface);

									//Ensure that connections are only made on stairs, not with any surface!
									if (transitionType != ESurfaceType::TransitionEdge) {
										newHelperVertexA = createHelperVertex(surfaceA, helperLocationA, FVector(20), transitionType, newID, nextNewID, vertexA, vertexB);
										newID = nextNewID;
										newHelperVertexB = createHelperVertex(surfaceB, helperLocationB, FVector(20), transitionType, newID, nextNewID, vertexA, vertexB);
										newID = nextNewID;

										//Create the edges between the (helper) vertices
										addedEdge = connectHelperVertices(a, b, vertexA, vertexB, newHelperVertexA, newHelperVertexB, helperEdgeA, helperEdgeB, helperEdgeC, newEdgeID, newEdgeID);
									}
								}
							}
						}
					}
					break;

					//Direct Connection
				case EConnectionMethod::Direct:
					for (AVertex* vertexA : aVertices) {
						for (AVertex* vertexB : bVertices) {
							if ((vertexA->GetActorLocation() - vertexB->GetActorLocation()).Size() < 400) {
								//Find the projection of A onto the surface of B and vice versa
								findNormals(neighbourStruct.cm, surfaceA, surfaceB, overlapCenter, vertexA, vertexB, normalA, normalB);

								//Find the projections of the vertices onto each other's surfaces
								findDirectProjection(vertexA, vertexB, normalA, normalB, overlapTNE, overlapBSW, projectionA, projectionB);

								//Find the location of the helper vertex
								findHelperLocation(vertexA, vertexB, projectionA, projectionB, helperLocationA);

								//There is a direct connection between two vertices, so check for an edge!
								if (!helperLocationA.ContainsNaN()) {
									FMath::Clamp(helperLocationA.X, overlapBSW.X, overlapTNE.X);
									FMath::Clamp(helperLocationA.Y, overlapBSW.Y, overlapTNE.Y);
									FMath::Clamp(helperLocationA.Z, overlapBSW.Z, overlapTNE.Z);
									searchExtent = pairwiseMult((FVector(1) - normalA.GetAbs() - normalB.GetAbs()), FVector(50)) + FVector(5);

									//Add a helper vertex to keep the edges on the surfaces (except if the likely helper vertex is close to another one)
									if (!a->isEdgeOverVoid(helperLocationA, vertexA->GetActorLocation()) && !b->isEdgeOverVoid(helperLocationA, vertexB->GetActorLocation()) && checkNearSurface(helperLocationA)) {
										if (!a->isEdgeBlockedHelper(vertexA, vertexB, 0.25f) && !b->isEdgeBlockedHelper(vertexB, vertexA, 0.25f)) {
											ESurfaceType transitionType = findTransitionType(surfaceA->surface, surfaceB->surface);
											newHelperVertexA = createHelperVertex(surfaceB, helperLocationA, searchExtent, transitionType, newID, nextNewID, vertexA, vertexB);
											newID = nextNewID;

											//Create the edges (or any other helper vertex that is so close the new vertex wasn't made)
											if (!a->doesEdgeExist(vertexA, newHelperVertexA)) {
												helperEdgeA = a->generateEdge(vertexA, newHelperVertexA, newEdgeID, vertexA->surface, false);
												if (helperEdgeA != nullptr) {
													edges.AddUnique(helperEdgeA);
													newEdgeID++;
													addedEdge = true;
												}
											}
											if (!b->doesEdgeExist(vertexB, newHelperVertexA)) {
												helperEdgeB = b->generateEdge(vertexB, newHelperVertexA, newEdgeID, vertexB->surface, false);
												if (helperEdgeB != nullptr) {
													edges.AddUnique(helperEdgeB);
													newEdgeID++;
													addedEdge = true;
												}
											}
										}
									}
								}

								//A helper vertex is needed
								else {
									if (!a->doesEdgeExist(vertexA, vertexB) && !b->doesEdgeExist(vertexA, vertexB) && vertexA != vertexB) {
										ESurfaceType transitionType = findTransitionType(surfaceA->surface, surfaceB->surface);
										helperEdgeA = a->generateEdge(vertexA, vertexB, newEdgeID, transitionType, false);
										if (helperEdgeA != nullptr) {
											edges.AddUnique(helperEdgeA);
											newEdgeID++;
											addedEdge = true;
										}
									}
								}
							}
						}
					}
					break;
				}

				//If an edge is added and b has not been handled yet, indicate that this connection has been made!
				if (addedEdge && tempPRMS.Contains(b) && !connectedIDS.Contains(neighbourStruct.neighbourID)) {
					madeConnections++;
					connectedIDS.AddUnique(neighbourStruct.neighbourID);
				}
			}
		}*/

		//Remove a from the temporary array, since it has already been connected to its neighbours
		//if (tempPRMS.Contains(a)) { tempPRMS.Remove(a); }
		}
	}

	//Go over all pairs of vertices that might be feasible to connect
	for (FPRMConnection connection : interPRMConnections) {

		//Find the PRM that has this vertex, if there is one. Note that only this PRM will need to be used for this vertex, so we break afterwards
		for (APRM* prmA : PRMS) {
			if (prmA->vertices.Contains(connection.vertexA)) {

				//Now find the neighbouring PRM that contains the second vertex of the connection
				APRM* prmB = nullptr;
				bool addedEdge = false;
				for (FPRMNeighbour neighbourStruct : prmA->neighbours) {
					prmB = PRMS[neighbourStruct.neighbourID];

					//Connect the two vertices if prmB contains the second vertex
					if (prmB->vertices.Contains(connection.vertexB)) {
						//Create sets to indicate two prms are connected
						TSet<APRM*> newConnection; newConnection.Add(prmA); newConnection.Add(prmB);

						//Find the surfaces of A and B that are used in this neighbour connection
						ASurfaceArea* surfaceA = prmA->getSurfaceFromStruct(neighbourStruct);
						ASurfaceArea* surfaceB = prmA->getNeighbourSurfaceFromStruct(prmB, neighbourStruct);

						//Make sure the surfaces are correct
						if ((prmA->checkIfInSurface(connection.vertexA, surfaceA) || prmA->checkIfInSurface(connection.vertexA, surfaceB)) && (prmB->checkIfInSurface(connection.vertexB, surfaceA) || prmB->checkIfInSurface(connection.vertexB, surfaceB))) {

							//Try to connect the two prms
							addedEdge = connectTwoVertices(false, neighbourStruct, prmA, prmB, surfaceA, surfaceB, connection.vertexA, connection.vertexB);

							//If the two prms have been connected for the first time, add one to the counter
							if (addedEdge) {
								bool alreadyConnected = false;
								for (TSet<APRM*> prmConnection : prmConnections) {
									if (prmConnection.Contains(prmA) && prmConnection.Contains(prmB)) {
										alreadyConnected = true;
										break;
									}
								}
								if (!alreadyConnected) {
									prmConnections.Add(newConnection);
									madeConnections++;
								}
							}
							break;
						}
					}
					if (prmB == nullptr) { UE_LOG(LogTemp, Log, TEXT("%s and %s are not in neighbouring PRMS"), *connection.vertexA->GetName(), *connection.vertexB->GetName()); }
				}

				break;
			}
		}
	}

	//If no edge has been generated yet and we want to guarantee a connection between the two PRMs, do so!
	/*if (!addedEdge && guaranteeConnections) {
		UE_LOG(LogTemp, Log, TEXT("PRMS are not yet connected. Add an edge!"));

		//Some variables used in the case distinctions
		ESurfaceType transitionType = ESurfaceType::TransitionEdge;
		FVector orthogonalAB = FVector(0);
		FVector connectionNormal = FVector(0);
		float projectionsDist = 0;

		//Empty array to use generateExtraVertex
		TArray<AVertex*> x;

		//Limit the TNE and BSW to ensure a connection will exist
		FVector aTNE2 = FVector(FMath::Min(aCenter.X + 250, aTNE.X), FMath::Min(aCenter.Y + 250, aTNE.Y), FMath::Min(aCenter.Z + 250, aTNE.Z));
		FVector aBSW2 = FVector(FMath::Max(aCenter.X - 250, aBSW.X), FMath::Max(aCenter.Y - 250, aBSW.Y), FMath::Max(aCenter.Z - 250, aBSW.Z));
		FVector bTNE2 = FVector(FMath::Min(bCenter.X + 250, bTNE.X), FMath::Min(bCenter.Y + 250, bTNE.Y), FMath::Min(bCenter.Z + 250, bTNE.Z));
		FVector bBSW2 = FVector(FMath::Max(bCenter.X - 250, bBSW.X), FMath::Max(bCenter.Y - 250, bBSW.Y), FMath::Max(bCenter.Z - 250, bBSW.Z));

		//Generate extra vertices really close to each other on the two separate surfaces
		newEdgeID = generateExtraVertex(extraVertexA, a, surfaceA, surfaceStructA.cube, newID, newEdgeID, aTNE2, aBSW2, false, x);
		newEdgeID = generateExtraVertex(extraVertexB, b, surfaceB, surfaceStructB.cube, newID, newEdgeID, bTNE2, bBSW2, false, x);

		if (extraVertexA != nullptr && extraVertexB != nullptr){
			nearbyNotConnected++;
			switch (neighbourStruct.cm) {
			case EConnectionMethod::DCB:
				findNormalsDCB(neighbourStruct.cm, surfaceA, surfaceB, overlapCenter, overlapExtentBox, extraVertexA, extraVertexB, normalA, normalB, orthogonalAB, connectionNormal, projectionA, projectionB);

				//FVector projectionC = getPointProjectionOntoPlane(projectionA, normalA, projectionB);
				//projectionC.Z = (projectionA.Z + projectionB.Z) / 2;
				//DrawDebugDirectionalArrow(GetWorld(), projectionA, projectionA + 50 * normalA, 50, FColor::Yellow, false, 15, 0, 2);
				//DrawDebugPoint(GetWorld(), projectionA, 20, FColor::Red, false, 60, 0);
				//DrawDebugPoint(GetWorld(), projectionB, 20, FColor::Blue, false, 60, 0);
				//DrawDebugPoint(GetWorld(), projectionC, 20, FColor::Green, false, 15, 0);
				//DrawDebugDirectionalArrow(GetWorld(), overlapCenter, overlapCenter + 50 * orthogonalAB, 50, FColor::Cyan, false, 15, 0, 2);

				findProjections(surfaceA, surfaceB, neighbourStruct.cm, extraVertexA, extraVertexB, projectionA, projectionB, projectionA2, projectionB2);

				//In case the two surfaces are opposites, ensure that the distance between the vertices around the corner is not too big, or that the distance in general is not too big
				projectionsDist = (projectionA2 - extraVertexA->GetActorLocation()).Size() + (projectionB2 - extraVertexB->GetActorLocation()).Size();
				//if (!((normalA.GetAbs() == normalB.GetAbs() && projectionsDist > 400))) {
				if (projectionsDist < 800) {

					//DrawDebugPoint(GetWorld(), projectionA2, 20, FColor::Orange, false, 60, 0);
					//DrawDebugPoint(GetWorld(), projectionB2, 20, FColor::Cyan, false, 60, 0);

					//Find the location of the helper vertices
					findHelperLocations(neighbourStruct.cm, extraVertexA, extraVertexB, projectionA2, projectionB2, normalA, normalB, orthogonalAB, connectionNormal, helperLocationA, helperLocationB);

					//DrawDebugPoint(GetWorld(), helperLocationA, 20, FColor::Magenta, false, 60, 0);
					//UE_LOG(LogTemp, Log, TEXT("vertexA location: %s"), *vertexA->GetActorLocation().ToCompactString());
					//UE_LOG(LogTemp, Log, TEXT("vertexB location: %s"), *vertexB->GetActorLocation().ToCompactString());
					//UE_LOG(LogTemp, Log, TEXT("projectionA: %s"), *projectionA.ToCompactString());
					//UE_LOG(LogTemp, Log, TEXT("projectionB: %s"), *projectionB.ToCompactString());
					//UE_LOG(LogTemp, Log, TEXT("projectionA2: %s"), *projectionA2.ToCompactString());
					//UE_LOG(LogTemp, Log, TEXT("projectionB2: %s"), *projectionB2.ToCompactString());
					//UE_LOG(LogTemp, Log, TEXT("helperLocationA: %s"), *helperLocationA.ToCompactString());
					//UE_LOG(LogTemp, Log, TEXT("helperLocationB: %s"), *helperLocationB.ToCompactString());
					//UE_LOG(LogTemp, Log, TEXT("A3 Z Diff: %f"), checkDirection(vertexA->GetActorLocation(), vertexB->GetActorLocation(), orthogonalAB));
					//UE_LOG(LogTemp, Log, TEXT("B3 Z Diff: %f"), checkDirection(vertexB->GetActorLocation(), vertexA->GetActorLocation(), orthogonalAB));
					//DrawDebugPoint(GetWorld(), helperLocationB, 20, FColor::Turquoise, false, 60, 0);

					//Create the helper vertices
					if (checkNearSurface(helperLocationA)) { newHelperVertexA = createHelperVertex(surfaceA, helperLocationA, FVector(50), surfaceA->surface, newID, nextNewID); }
					newID = nextNewID;
					if (checkNearSurface(helperLocationB)) { newHelperVertexB = createHelperVertex(surfaceB, helperLocationB, FVector(50), surfaceB->surface, newID, nextNewID); }
					newID = nextNewID;
					/*TArray<AActor*> nearbyHelperActors;
						UKismetSystemLibrary::BoxOverlapActors(GetWorld(), projectionA3, FVector(50), traceObjectTypes, AHelperVertex::StaticClass(), ignores, nearbyHelperActors);
						UKismetSystemLibrary::BoxOverlapActors(GetWorld(), projectionA3, FVector(5), traceObjectTypes, AVertex::StaticClass(), ignores, nearbyVertices);
						if (nearbyHelperActors.Num() > 0 && nearbyVertices.Num() > 0) {
							newHelperVertexA = (AVertex*)nearbyVertices[0];
						}
						else if (nearbyHelperActors.Num() > 0) {
							newHelperVertexA = (AHelperVertex*)nearbyHelperActors[0];
						}
						else {
							newHelperVertexA = neighbourSurfaceA->generateHelperVertex(newID, projectionA3);
							newID++;
						}
						ignoresHelper.Add(newHelperVertexA);
						DrawDebugBox(GetWorld(), projectionA3, FVector(5), FColor::Red, false, 15, 0, 3);

						UKismetSystemLibrary::BoxOverlapActors(GetWorld(), projectionB3, FVector(50), traceObjectTypes, AHelperVertex::StaticClass(), ignores, nearbyHelperActors);
						UKismetSystemLibrary::BoxOverlapActors(GetWorld(), projectionB3, FVector(5), traceObjectTypes, AVertex::StaticClass(), ignores, nearbyVertices);
						if (nearbyHelperActors.Num() > 0 && nearbyVertices.Num() > 0) {
							newHelperVertexB = (AVertex*)nearbyVertices[0];
						}
						else if (nearbyHelperActors.Num() > 0) {
							newHelperVertexB = (AHelperVertex*)nearbyHelperActors[0];
						}
						else {
							newHelperVertexB = neighbourSurfaceA->generateHelperVertex(newID, projectionB3);
							newID++;
						}
						ignoresHelper.Add(newHelperVertexB);
						DrawDebugBox(GetWorld(), projectionB3, FVector(5), FColor::Blue, false, 15, 0, 3);*/
	/*
																//Create the edges (or any other helper vertex that is so close the new vertex wasn't made)
															if (newHelperVertexA) {
																if (!a->doesEdgeExist(extraVertexA, newHelperVertexA)) {
																	helperEdgeA = a->generateEdge(extraVertexA, newHelperVertexA, newEdgeID, extraVertexA->surface, false);
																	if (helperEdgeA != nullptr) {
																		edges.AddUnique(helperEdgeA);
																		newEdgeID++;
																		addedEdge = true;
																	}
																}
															}
															if (newHelperVertexB) {
																if (!b->doesEdgeExist(extraVertexB, newHelperVertexB)) {
																	helperEdgeB = b->generateEdge(extraVertexB, newHelperVertexB, newEdgeID, extraVertexB->surface, false);
																	if (helperEdgeB != nullptr) {
																		edges.AddUnique(helperEdgeB);
																		newEdgeID++;
																		addedEdge = true;
																	}
																}
															}
															if (newHelperVertexA && newHelperVertexB) {
																if (!a->doesEdgeExist(newHelperVertexA, newHelperVertexB) && !a->isEdgeBlocked(newHelperVertexA, newHelperVertexB, 0.25f)) {
																	helperEdgeC = a->generateEdge(newHelperVertexA, newHelperVertexB, newEdgeID, ESurfaceType::TransitionEdge, false);
																	if (helperEdgeC != nullptr) {
																		edges.AddUnique(helperEdgeC);
																		newEdgeID++;
																		addedEdge = true;
																	}
																}
															}
															//}
														}
														//else {
														//UE_LOG(LogTemp, Log, TEXT("Vertices too far from projections. ProjectionA: %s; VertexA: %s ProjectionB: %s; VertexB: %s"), *projectionA.ToCompactString(), *vertexA->GetName(), *projectionB.ToCompactString(), *vertexB->GetName());
														//}
														break;
													case EConnectionMethod::SCB:
														//UE_LOG(LogTemp, Log, TEXT("vertex A: %s"), *vertexA->GetName());
														//UE_LOG(LogTemp, Log, TEXT("vertex B: %s"), *vertexB->GetName());

														//Find the projection of A onto the surface of B and vice versa
														findNormals(neighbourStruct.cm, surfaceA, surfaceB, overlapCenter, extraVertexA, extraVertexB, normalA, normalB, projectionA, projectionB);
														//DrawDebugPoint(GetWorld(), projectionA, 20, FColor::Red, false, 60, 0);
														//DrawDebugPoint(GetWorld(), projectionB, 20, FColor::Blue, false, 60, 0);

														//Do a line trace to find the edges of the stairs
														findProjections(surfaceA, surfaceB, neighbourStruct.cm, extraVertexA, extraVertexB, projectionA, projectionB, projectionA2, projectionB2);
														//DrawDebugPoint(GetWorld(), projectionA2, 20, FColor::Yellow, false, 60, 0);
														//DrawDebugPoint(GetWorld(), projectionB2, 20, FColor::Cyan, false, 60, 0);

														//Find the location for the helper vertices in between the two projections
														findHelperLocations(neighbourStruct.cm, extraVertexA, extraVertexB, projectionA2, projectionB2, normalA, normalB, normalA, normalB, helperLocationA, helperLocationB);
														//DrawDebugPoint(GetWorld(), helperLocationA, 20, FColor::Green, false, 60, 0);
														//DrawDebugPoint(GetWorld(), helperLocationB, 20, FColor::Magenta, false, 60, 0);
														transitionType = findStairsTransition(surfaceA->surface, surfaceB->surface);

														//Add a helper vertex to keep the edges on the surfaces (except if the likely helper vertex is close to another one)
														if (checkNearSurface(helperLocationA)) { newHelperVertexA = createHelperVertex(surfaceA, helperLocationA, FVector(25), transitionType, newID, nextNewID); }
														else { UE_LOG(LogTemp, Log, TEXT("Helper location not near surface")); }
														newID = nextNewID;
														if (checkNearSurface(helperLocationB)) { newHelperVertexB = createHelperVertex(surfaceB, helperLocationB, FVector(25), transitionType, newID, nextNewID); }
														else { UE_LOG(LogTemp, Log, TEXT("Helper location not near surface")); }
														newID = nextNewID;

														/*TArray<AActor*> nearbyHelperActors;
															UKismetSystemLibrary::BoxOverlapActors(GetWorld(), helperLocationA, FVector(25), traceObjectTypes, AHelperVertex::StaticClass(), ignores, nearbyHelperActors);
															UKismetSystemLibrary::BoxOverlapActors(GetWorld(), helperLocationA, FVector(5), traceObjectTypes, AVertex::StaticClass(), ignores, nearbyVertices);
															if (nearbyHelperActors.Num() > 0 && nearbyVertices.Num() > 0) {
																newHelperVertexA = (AVertex*)nearbyVertices[0];
															}
															else if (nearbyHelperActors.Num() > 0) {
																newHelperVertexA = (AHelperVertex*)nearbyHelperActors[0];
															}
															else {
																newHelperVertexA = neighbourSurfaceA->generateHelperVertex(newID, helperLocationA);
																newID++;
															}
															ignoresHelper.Add(newHelperVertexA);
															DrawDebugBox(GetWorld(), helperLocationA, FVector(5), FColor::Red, false, 15, 0, 3);

															UKismetSystemLibrary::BoxOverlapActors(GetWorld(), helperLocationB, FVector(25), traceObjectTypes, AHelperVertex::StaticClass(), ignores, nearbyHelperActors);
															UKismetSystemLibrary::BoxOverlapActors(GetWorld(), helperLocationB, FVector(5), traceObjectTypes, AVertex::StaticClass(), ignores, nearbyVertices);
															if (nearbyHelperActors.Num() > 0 && nearbyVertices.Num() > 0) {
																newHelperVertexB = (AVertex*)nearbyVertices[0];
															}
															else if (nearbyHelperActors.Num() > 0) {
																newHelperVertexB = (AHelperVertex*)nearbyHelperActors[0];
															}
															else {
																newHelperVertexB = neighbourSurfaceA->generateHelperVertex(newID, helperLocationB);
																newID++;
															}
															ignoresHelper.Add(newHelperVertexB);
															DrawDebugBox(GetWorld(), helperLocationB, FVector(5), FColor::Blue, false, 15, 0, 3);*/
	/*
																												if (!newHelperVertexA) {
																													UE_LOG(LogTemp, Log, TEXT("Did not generate helper vertex A! HelperLocation: %s; projectionA2: %s; projectionA: %s; vertexA: %s; vertexB: %s"), *helperLocationA.ToCompactString(), *projectionA2.ToCompactString(), *projectionA.ToCompactString(), *vertexA->GetName(), *vertexB->GetName());
																												}

																												if (!newHelperVertexB) {
																													UE_LOG(LogTemp, Log, TEXT("Did not generate helper vertex B! HelperLocation: %s; projectionB2: %s; projectionB: %s; vertexA: %s; vertexB: %s"), *helperLocationB.ToCompactString(), *projectionB2.ToCompactString(), *projectionB.ToCompactString(), *vertexA->GetName(), *vertexB->GetName());
																												}*/
	/*
																																													//Create the edges (or any other helper vertex that is so close the new vertex wasn't made). If the edge is not on a surface (in this case if none of the directions is 0), don't generate it
																																												if (newHelperVertexA) {
																																													if (!a->doesEdgeExist(extraVertexA, newHelperVertexA) && isOnSurface(extraVertexA, newHelperVertexA)) {
																																														helperEdgeA = a->generateEdge(extraVertexA, newHelperVertexA, newEdgeID, extraVertexA->surface, false);
																																														if (helperEdgeA != nullptr) {
																																															edges.AddUnique(helperEdgeA);
																																															newEdgeID++;
																																															addedEdge = true;
																																														}
																																													}
																																												}
																																												if (newHelperVertexB) {
																																													if (!b->doesEdgeExist(extraVertexB, newHelperVertexB) && isOnSurface(extraVertexB, newHelperVertexB)) {
																																														helperEdgeB = b->generateEdge(extraVertexB, newHelperVertexB, newEdgeID, extraVertexB->surface, false);
																																														if (helperEdgeB != nullptr) {
																																															edges.AddUnique(helperEdgeB);
																																															newEdgeID++;
																																															addedEdge = true;
																																														}
																																													}
																																												}
																																												if (newHelperVertexA && newHelperVertexB) {
																																													if (!a->doesEdgeExist(newHelperVertexA, newHelperVertexB) && isOnSurface(newHelperVertexA, newHelperVertexB)) {
																																														helperEdgeC = a->generateEdge(newHelperVertexA, newHelperVertexB, newEdgeID, ESurfaceType::TransitionEdge, false);
																																														if (helperEdgeC != nullptr) {
																																															edges.Add(helperEdgeC);
																																															newEdgeID++;
																																															addedEdge = true;
																																														}
																																													}
																																												}
																																												break;
																																											case EConnectionMethod::Direct://Find the projection of A onto the surface of B and vice versa
																																												findNormals(neighbourStruct.cm, surfaceA, surfaceB, overlapCenter, extraVertexA, extraVertexB, normalA, normalB, projectionA, projectionB);

																																												//Find the location of the helper vertex
																																												findHelperLocation(extraVertexA, extraVertexB, projectionA, projectionB, helperLocationA);
																																												//UE_LOG(LogTemp, Log, TEXT("helperLocationA: %s"), *helperLocationA.ToCompactString());

																																												//There is a direct connection between two vertices, so check for an edge!
																																												if (!helperLocationA.ContainsNaN()) {
																																													FMath::Clamp(helperLocationA.X, overlapBSW.X, overlapTNE.X);
																																													FMath::Clamp(helperLocationA.Y, overlapBSW.Y, overlapTNE.Y);
																																													FMath::Clamp(helperLocationA.Z, overlapBSW.Z, overlapTNE.Z);
																																													searchExtent = pairwiseMult((FVector(1) - normalA.GetAbs() - normalB.GetAbs()), FVector(50)) + FVector(5);
																																													//DrawDebugBox(GetWorld(), helperLocationA, searchExtent, FColor::Red, false, 60, 0, 1);

																																													//DrawDebugPoint(GetWorld(), overlapCenter, 20, FColor::Red, false, 15, 0);
																																													//DrawDebugPoint(GetWorld(), vertexA->GetActorLocation(), 20, FColor::Blue, false, 15, 0);
																																													//DrawDebugDirectionalArrow(GetWorld(), overlapCenter, overlapCenter + 50 * normalB, 50, FColor::Yellow, false, 15, 0, 2);
																																													//DrawDebugPoint(GetWorld(), projectionA, 20, FColor::Green, false, 15, 0);
																																													//DrawDebugPoint(GetWorld(), projectionB, 20, FColor::Red, false, 15, 0);
																																													//DrawDebugPoint(GetWorld(), helperLocation, 20, FColor::Blue, false, 15, 0);
																																													//DrawDebugBox(GetWorld(), helperLocation, FVector(20), FColor::Blue, false, 15, 0, 2);
																																													//UE_LOG(LogTemp, Log, TEXT("Helper vertex location"), *helperLocation.ToCompactString());

																																													//Add a helper vertex to keep the edges on the surfaces (except if the likely helper vertex is close to another one)
																																													if (!a->isEdgeOverVoid(helperLocationA, extraVertexA->GetActorLocation()) && !b->isEdgeOverVoid(helperLocationA, extraVertexB->GetActorLocation()) && checkNearSurface(helperLocationA)) {
																																														transitionType = findTransitionType(surfaceA->surface, surfaceB->surface);
																																														newHelperVertexA = createHelperVertex(surfaceB, helperLocationA, searchExtent, transitionType, newID, nextNewID);
																																														newID = nextNewID;

																																														//Create the edges (or any other helper vertex that is so close the new vertex wasn't made)
																																														if (!a->doesEdgeExist(extraVertexA, newHelperVertexA)) {
																																															helperEdgeA = a->generateEdge(extraVertexA, newHelperVertexA, newEdgeID, extraVertexA->surface, false);
																																															if (helperEdgeA != nullptr) {
																																																edges.AddUnique(helperEdgeA);
																																																newEdgeID++;
																																																addedEdge = true;
																																															}
																																														}
																																														if (!b->doesEdgeExist(extraVertexB, newHelperVertexA)) {
																																															helperEdgeB = b->generateEdge(extraVertexB, newHelperVertexA, newEdgeID, extraVertexB->surface, false);
																																															if (helperEdgeB != nullptr) {
																																																edges.AddUnique(helperEdgeB);
																																																newEdgeID++;
																																																addedEdge = true;
																																															}
																																														}
																																													}
																																												}

																																												//A helper vertex is needed
																																												else {
																																													if (!a->doesEdgeExist(extraVertexA, extraVertexB) && !b->doesEdgeExist(extraVertexA, extraVertexB) && extraVertexA != extraVertexB) {
																																														transitionType = findTransitionType(surfaceA->surface, surfaceB->surface);
																																														helperEdgeA = a->generateEdge(extraVertexA, extraVertexB, newEdgeID, transitionType, false);
																																														if (helperEdgeA != nullptr) {
																																															edges.AddUnique(helperEdgeA);
																																															newEdgeID++;
																																															addedEdge = true;
																																														}
																																													}
																																												}
																																												break;
																																											default:
																																												break;
																																											}
																																										}
																																										else {
																																											if (extraVertexA != nullptr) {
																																												UE_LOG(LogTemp, Log, TEXT("No vertex could be connected to this one: %s (A)"), *extraVertexA->GetName());

																																											}
																																											if (extraVertexB != nullptr) {
																																												//UE_LOG(LogTemp, Log, TEXT("No vertex could be connected to this one: %s (B)"), *extraVertexB->GetName());
																																											}
																																											if (extraVertexA == nullptr && extraVertexB == nullptr) {
																																												UE_LOG(LogTemp, Log, TEXT("Neither extraVertexA nor extraVertexB could be generated. Is something wrong?"));
																																												UE_LOG(LogTemp, Log, TEXT("Surface A: %s"), *surfaceA->GetName());
																																												UE_LOG(LogTemp, Log, TEXT("Surface B: %s"), *surfaceB->GetName());
																																											}
																																										}
																																									}*/

	//Set the end time for the PRM generation
	endTimeMoment = FDateTime::Now();
	FTimespan startTime = startTimeMoment.GetTimeOfDay();
	FTimespan endTime = endTimeMoment.GetTimeOfDay();
	FTimespan totalTime = endTime - startTime;

	//Save all values to a save game
	USaveGame* baseSaveFile = UGameplayStatics::LoadGameFromSlot(saveName, 0);

	if (!baseSaveFile) { baseSaveFile = UGameplayStatics::CreateSaveGameObject(UPRMBuildSave::StaticClass()); }
	saveFile = (UPRMBuildSave*)baseSaveFile;

	if (saveFile) {
		saveFile->buildTime = totalTime;
		saveFile->vertexCount = vertices.Num();
		saveFile->edgeCount = edges.Num();
		saveFile = (UPRMBuildSave*)baseSaveFile;
		saveFile->buildTime = totalTime;
		saveFile->vertexCount = vertices.Num();
		saveFile->edgeCount = edges.Num();
		saveFile->madeConnections = madeConnections;
		saveFile->totalConnections = totalConnections;
		if (!kNearestNeighbours3D) {
			saveFile->clNearbyArea = notNearby;
			saveFile->clFarApart = nearbyNotConnected;
		}
		UGameplayStatics::SaveGameToSlot(saveFile, saveName, 0);
	}
	else { UE_LOG(LogTemp, Log, TEXT("No save file found. Build data not saved.")); }
}

void APRMCollector::reset()
{
	//Reset the connectivity graph
	connectivityGraph->surfaces.Empty();

	//Reset all projection cuboids
	for (AProjectionCuboid* projector : projectionCuboids) { projector->reset(); }
	projectionCuboids.Empty();

	//Reset all PRMS
	for (APRM* PRM : PRMS) { PRM->vertices.Empty(); PRM->edges.Empty(); }
	PRMS.Empty();

	//Reset all vertices
	for (AVertex* vertex : vertices) { if (vertex) { vertex->Destroy(); } }
	vertices.Empty();

	//Reset all edges
	for (APRMEdge* edge : edges) { if (edge) { edge->Destroy(); } }
	edges.Empty();

	//Reset all interPRMConnections
	interPRMConnections.Empty();
}

void APRMCollector::hidePRMS() {
	for (APRM* PRM : PRMS) { PRM->hidePRM(); }
	for (AVertex* vertex : vertices) { vertex->hideVertex(); }
}

void APRMCollector::showPRMS() {
	for (APRM* PRM : PRMS) { PRM->showPRM(); }
	for (AVertex* vertex : vertices) { vertex->showVertex(); }
}

void APRMCollector::calculateCoverSize() {
	coverSize = 0;
	totalSurfaceSize = 0;

	//Calculate the size of a cover of each PRM and also add the PRM surface size to the total
	for (APRM* PRM : PRMS) { coverSize += PRM->calculateCoverSize(agentSize); totalSurfaceSize += PRM->totalSize; }

	//Save the cover size data
	USaveGame* baseSaveFile = UGameplayStatics::LoadGameFromSlot(saveName, 0);
	saveFile = (UPRMBuildSave*)baseSaveFile;
	if (saveFile) {
		saveFile->areaCovered = coverSize;
		saveFile->totalArea = totalSurfaceSize;
		UGameplayStatics::SaveGameToSlot(saveFile, saveName, 0);
	}

	//If no save file can be found, indicate this.
	else { UE_LOG(LogTemp, Log, TEXT("No save file found. Build data not saved.")); }
}

APRMEdge * APRMCollector::findEdge(int32 a, int32 b)
{
	//Go over every edge and see if the vertices a and b are its endpoints
	for (APRMEdge* edge : edges) {
		if (edge->isEndVertex(a)) {
			if (edge->isEndVertex(b)) {
				return edge;
			}
		}
	}

	//Edge cannot be found
	UE_LOG(LogTemp, Log, TEXT("This edge does not exist according to findEdge. a: %d, b: %d"), a, b);
	return nullptr;
}

void APRMCollector::applyOptions(FString fileName, bool project, bool maprm, bool approx, bool knn, bool knn3d, bool apsmo, bool assmo, bool guaneavert, bool guacon)
{
	saveName = fileName;
	projectOntoSurfaces = project;
	useMedialAxis = maprm;
	approximateMedialAxis = approx;
	kNearestNeighbours = knn;
	kNearestNeighbours3D = knn3d;
	allPartialSurfacesMinimumOne = apsmo;
	allSubSurfacesMinimumOne = assmo;
	guaranteeNearbyVertices = guaneavert;
	guaranteeConnections = guacon;
}

void APRMCollector::generateRandomPRM()
{
	//If the projection option is used, generate vertices based on projection
	if (projectOntoSurfaces) {
		for (AProjectionCuboid* projector : projectionCuboids) { projector->generateRandomVertices(kNearestNeighbours, kNearestNeighbours3D, interPRMConnections, interPRMConnections, startVertexID, startEdgeID, startVertexID, startEdgeID); }
		
		//KNN3D misses some connections due to the order of handling the stairs (random). Therefore, if there are no connections with a stair, try again since after handling the rest this should be possible
		if (kNearestNeighbours3D) {
			for (APRM* PRM : PRMS) {
				if (PRM->surfaces[0]->surface == ESurfaceType::Stairs || PRM->surfaces[0]->surface == ESurfaceType::StairsCeiling) {
					for (AVertex* vertex : PRM->vertices) {
						bool isUsed = false;
						for (FPRMConnection connection : interPRMConnections) {
							if (connection.vertexA == vertex || connection.vertexB == vertex) { isUsed = true; break; }
						}
						int32 oldNum = interPRMConnections.Num();
						if (!isUsed) { startEdgeID = projectionCuboids[0]->connectVertex(vertex, PRM, startEdgeID, kNearestNeighbours, kNearestNeighbours3D, interPRMConnections, interPRMConnections); }
					}
				}
			}
		}
	}

	//Projection is not used, so use the standard variant
	else {
		//Generate every PRM individually
		for (APRM* PRM : PRMS) {
			startVertexID = PRM->generateRandomVertices(startVertexID, agentSize, allPartialSurfacesMinimumOne, allSubSurfacesMinimumOne);
			startEdgeID = PRM->generateEdges(startEdgeID, agentSize, interPRMConnections, interPRMConnections, kNearestNeighbours, kNearestNeighbours3D);
		}
	}
}

void APRMCollector::generateApproximateMedialPRM()
{
	//If the projection option is used, generate vertices based on projection
	if (projectOntoSurfaces) {
		for (AProjectionCuboid* projector : projectionCuboids) { projector->generateApproximateMedialVertices(kNearestNeighbours, kNearestNeighbours3D, interPRMConnections, interPRMConnections, startVertexID, startEdgeID, startVertexID, startEdgeID); }

		//KNN3D misses some connections due to the order of handling the stairs (random). Therefore, if there are no connections with a stair, try again since after handling the rest this should be possible
		if (kNearestNeighbours3D) {
			for (APRM* PRM : PRMS) {
				if (PRM->surfaces[0]->surface == ESurfaceType::Stairs || PRM->surfaces[0]->surface == ESurfaceType::StairsCeiling) {
					for (AVertex* vertex : PRM->vertices) {
						bool isUsed = false;
						for (FPRMConnection connection : interPRMConnections) {
							if (connection.vertexA == vertex || connection.vertexB == vertex) { isUsed = true; break; }
						}
						if (!isUsed) { startEdgeID = projectionCuboids[0]->connectVertex(vertex, PRM, startEdgeID, kNearestNeighbours, kNearestNeighbours3D, interPRMConnections, interPRMConnections); }
					}
				}
			}
		}
	}

	//Projection is not used, so use the standard variant
	else {
		for (APRM* PRM : PRMS) {
			startVertexID = PRM->generateApproximateMedialVertices(startVertexID, agentSize);
			startEdgeID = PRM->generateEdges(startEdgeID, agentSize, interPRMConnections, interPRMConnections, kNearestNeighbours, kNearestNeighbours3D);
		}
	}
}

void APRMCollector::generateExactMedialPRM()
{
	//If the projection option is used, generate vertices based on projection
	if (projectOntoSurfaces) {
		for (AProjectionCuboid* projector : projectionCuboids) { projector->generateExactMedialVertices(kNearestNeighbours, kNearestNeighbours3D, interPRMConnections, interPRMConnections, startVertexID, startEdgeID, startVertexID, startEdgeID); }

		//KNN3D misses some connections due to the order of handling the stairs (random). Therefore, if there are no connections with a stair, try again since after handling the rest this should be possible
		if (kNearestNeighbours3D) {
			for (APRM* PRM : PRMS) {
				if (PRM->surfaces[0]->surface == ESurfaceType::Stairs || PRM->surfaces[0]->surface == ESurfaceType::StairsCeiling) {
					for (AVertex* vertex : PRM->vertices) {
						bool isUsed = false;
						for (FPRMConnection connection : interPRMConnections) {
							if (connection.vertexA == vertex || connection.vertexB == vertex) { isUsed = true; break; }
						}
						if (!isUsed) { startEdgeID = projectionCuboids[0]->connectVertex(vertex, PRM, startEdgeID, kNearestNeighbours, kNearestNeighbours3D, interPRMConnections, interPRMConnections); }
					}
				}
			}
		}
	}

	//Projection is not used, so use the standard variant
	else {
		for (APRM* PRM : PRMS) {
			startVertexID = PRM->generateExactMedialVertices(startVertexID, agentSize);
			startEdgeID = PRM->generateEdges(startEdgeID, agentSize, interPRMConnections, interPRMConnections, kNearestNeighbours, kNearestNeighbours3D);
		}
	}
}

void APRMCollector::reconnectPRM()
{
	//Reset the connectivity graph
	connectivityGraph->surfaces.Empty(); 
	
	//Reset the edges and the neighbours of vertices
	for (APRM* PRM : PRMS) { 
		for (APRMEdge* edge : PRM->edges) { edge->Destroy(); }
		PRM->edges.Empty(); 
		edges.Empty();
		PRM->neighbours.Empty();
	}

	//Remove helper vertices
	TArray<AVertex*> tempVertices = vertices;
	for (AVertex* vertex : tempVertices) {
		vertex->neighbours.Empty();
		if (vertex->GetClass() == AHelperVertex::StaticClass()) { vertices.Remove(vertex); vertex->Destroy(); }
	}

	//Prepare all the surfaces
	connectivityGraph->CollectSurfaces();
	connectivityGraph->ConnectSurfaces();

	//Reset the inter prm connections
	interPRMConnections = {};

	//Set which PRMS are neighbours
	setNeighboursPRMS(); 
	
	startVertexID = vertices.Num();
	startEdgeID = 0;

	//Now create the edges again
	for (APRM* PRM : PRMS) { startEdgeID = PRM->generateEdges(startEdgeID, agentSize, interPRMConnections, interPRMConnections, kNearestNeighbours, kNearestNeighbours3D); }

	//Connect the partial PRMS
	connectPRMS();

	//Add all created vertices into an array
	TArray<AActor*> temp;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVertex::StaticClass(), temp);
	for (AActor* actor : temp) {
		AVertex* newVertex = (AVertex*)actor;
		if (newVertex != nullptr) { vertices.AddUnique(newVertex); }
	}

	//Add all created edges into an array
	temp = {};
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APRMEdge::StaticClass(), temp);
	for (AActor* actor : temp) {
		APRMEdge* newEdge = (APRMEdge*)actor;
		if (newEdge != nullptr) { edges.AddUnique(newEdge); }
	}
}

FVector APRMCollector::getPointProjectionOntoPlane(FVector planePos, FVector planeNormal, FVector point) {
	float t = (FVector::DotProduct(planePos, planeNormal) - FVector::DotProduct(point, planeNormal)) / (FMath::Pow(planeNormal.X, 2) + FMath::Pow(planeNormal.Y, 2) + FMath::Pow(planeNormal.Z, 2));
	return point + t * planeNormal;
}

FVector APRMCollector::getPointProjectionOnPlaneAlongDirection(FVector planeNormal, FVector point, FVector direction) {
	FVector returnValue = direction;
	if (FVector::Orthogonal(planeNormal, direction)) {
		return returnValue;
	}
	returnValue.X = direction.X * planeNormal.Z + direction.X * planeNormal.Y;
	returnValue.Y = direction.Y * planeNormal.X + direction.Y * planeNormal.Z;
	returnValue.Z = direction.Z * planeNormal.Y + direction.Z * planeNormal.X;

	return returnValue;
}

FVector APRMCollector::pairwiseMult(FVector a, FVector b) {
	return FVector(a.X * b.X, a.Y * b.Y, a.Z * b.Z);
}

bool APRMCollector::connectTwoVertices(bool checkDistance, FPRMNeighbour neighbourStruct, APRM * prmA, APRM * prmB, ASurfaceArea * surfaceA, ASurfaceArea * surfaceB, AVertex * vertexA, AVertex * vertexB)
{
	bool addedEdge = false;
	//Find the surface structs and ensure that the cubes exist
	FSurfaceNeighbour surfaceStructA = surfaceA->getNeighbourFromID(surfaceB->id);
	if (surfaceStructA.cube == nullptr) {
		UE_LOG(LogTemp, Log, TEXT("No cube found! Surfaces: %s to %s"), *surfaceA->GetName(), *surfaceB->GetName());
		return false;
	}
	FSurfaceNeighbour surfaceStructB = surfaceB->getNeighbourFromID(surfaceA->id);
	if (surfaceStructB.cube == nullptr) {
		UE_LOG(LogTemp, Log, TEXT("No cube found! Surfaces: %s from %s"), *surfaceB->GetName(), *surfaceA->GetName());
		return false;
	}

	//For defining the nearby areas and overlapping area
	FVector overlapCenter = FVector(0);
	FVector overlapExtentBox = FVector(0);
	FVector aCenter = FVector(0);
	FVector aExtentBox = FVector(0);
	FVector bCenter = FVector(0);
	FVector bExtentBox = FVector(0);

	//Prepare the IDs for the vertices and edges
	int32 newID = vertices.Num();
	int32 nextNewID = newID;
	int32 newEdgeID = edges.Num();

	//Normals and projections
	FVector normalA = FVector(0);
	FVector normalB = FVector(0);
	FVector projectionA = FVector(0);
	FVector projectionB = FVector(0);
	FVector projectionA2 = FVector(0);
	FVector projectionB2 = FVector(0);

	//Helper vertices, edges and their locations
	FVector helperLocationA = FVector(0);
	FVector helperLocationB = FVector(0);
	FVector searchExtent = FVector(0);
	AVertex* newHelperVertexA = nullptr;
	AVertex* newHelperVertexB = nullptr;
	APRMEdge* helperEdgeA = nullptr;
	APRMEdge* helperEdgeB = nullptr;
	APRMEdge* helperEdgeC = nullptr;

	//For the extents of the surface cube and the overlap area
	FVector overlapTNE = surfaceStructA.tne;
	FVector overlapBSW = surfaceStructA.bsw;
	FVector aTNE = surfaceStructA.cube->GetComponentLocation() + 50 * surfaceStructA.cube->GetComponentScale();
	FVector aBSW = surfaceStructA.cube->GetComponentLocation() - 50 * surfaceStructA.cube->GetComponentScale();
	FVector bTNE = surfaceStructB.cube->GetComponentLocation() + 50 * surfaceStructB.cube->GetComponentScale();
	FVector bBSW = surfaceStructB.cube->GetComponentLocation() - 50 * surfaceStructB.cube->GetComponentScale();

	//The overlaps are different in case of the stairs
	if (neighbourStruct.cm == EConnectionMethod::SCB) {
		overlapTNE = surfaceStructA.connectionCube->GetComponentLocation() + 50 * surfaceStructA.connectionCube->GetComponentScale();
		overlapBSW = surfaceStructA.connectionCube->GetComponentLocation() - 50 * surfaceStructA.connectionCube->GetComponentScale();
	}

	//Find the nearby areas for a and b (add to the TNE and BSW up to the maximum of the cube)
	findTNEBSW(overlapTNE, overlapBSW, aTNE, aBSW, bTNE, bBSW, aTNE, aBSW, bTNE, bBSW);
	findNearbyAreas(overlapTNE, overlapBSW, aTNE, aBSW, bTNE, bBSW, overlapCenter, overlapExtentBox, aCenter, aExtentBox, bCenter, bExtentBox);

	//Connect the PRMS based on the connection method
	switch (neighbourStruct.cm) {
		//Double Connection Box
	case EConnectionMethod::DCB:
		if (!checkDistance || (vertexA->GetActorLocation() - vertexB->GetActorLocation()).Size() < 400) {
			//Find the projection of A onto the surface of B and vice versa, as well as the normals and the orthogonal to the normals
			FVector orthogonalAB = FVector(0);
			FVector connectionNormal = FVector(0);
			findNormalsDCB(neighbourStruct.cm, surfaceA, surfaceB, overlapCenter, overlapExtentBox, vertexA, vertexB, normalA, normalB, orthogonalAB, connectionNormal);
			projectVerticesDCB(vertexA, vertexB, normalA, normalB, connectionNormal, aExtentBox, bExtentBox, aTNE, aBSW, bTNE, bBSW, projectionA, projectionB);

			//In case the two surfaces are opposites, ensure that the distance between the vertices around the corner is not too big, or that the distance in general is not too big
			float projectionsDist = (projectionA - vertexA->GetActorLocation()).Size() + (projectionB - vertexB->GetActorLocation()).Size();
			if (!checkDistance || projectionsDist < 800) {
				//Find the location of the helper vertices
				findHelperLocations(neighbourStruct.cm, vertexA, vertexB, projectionA, projectionB, normalA, normalB, orthogonalAB, connectionNormal, helperLocationA, helperLocationB);

				//Ensure that a connection can be made. If not, do not generate these vertices
				if (!prmA->isFutureEdgeBlocked(helperLocationA, helperLocationB, surfaceA->surface, surfaceB->surface, 0.25f) && 
					!prmA->isFutureEdgeBlocked(helperLocationA, vertexA->GetActorLocation(), surfaceA->surface, surfaceA->surface, 0.25f) &&
					!prmB->isFutureEdgeBlocked(vertexB->GetActorLocation(), helperLocationB, surfaceB->surface, surfaceB->surface, 0.25f)) {
					//Create the helper vertices
					if (checkNearSurface(helperLocationA)) { 
						newHelperVertexA = createHelperVertex(surfaceA, helperLocationA, FVector(50), surfaceA->surface, newID, nextNewID, vertexA, vertexB); }
					else { UE_LOG(LogTemp, Log, TEXT("Could not generate helper vertex from %s to %s at %s. ProjectionA: %s. aTNE: %s. aBSW: %s. OverlapTNE: %s. OverlapBSW: %s. SurfaceStructTNE: %s. SurfaceStructBSW: %s. SurfaceStructTNE 2: %s. SurfaceStructBSW 2: %s. SurfaceA: %s. SurfaceB: %s"), *vertexA->GetName(), *vertexB->GetName(), *helperLocationA.ToCompactString(), *projectionA.ToCompactString(), *aTNE.ToCompactString(), *aBSW.ToCompactString(), *overlapTNE.ToCompactString(), *overlapBSW.ToCompactString(), *surfaceStructA.tne.ToCompactString(), *surfaceStructA.bsw.ToCompactString(), *surfaceStructB.tne.ToCompactString(), *surfaceStructB.bsw.ToCompactString(), *surfaceA->GetName(), *surfaceB->GetName()); }
					newID = nextNewID;
					if (checkNearSurface(helperLocationB)) { 
						newHelperVertexB = createHelperVertex(surfaceB, helperLocationB, FVector(50), surfaceB->surface, newID, nextNewID, vertexA, vertexB); }
					else { UE_LOG(LogTemp, Log, TEXT("Could not generate helper vertex from %s to %s at %s. ProjectionB: %s. bTNE: %s. bBSW: %s. OverlapTNE: %s. OverlapBSW: %s. SurfaceStructTNE: %s. SurfaceStructBSW: %s. SurfaceStructTNE 2: %s. SurfaceStructBSW 2: %s. SurfaceA: %s. SurfaceB: %s"), *vertexA->GetName(), *vertexB->GetName(), *helperLocationB.ToCompactString(), *projectionB.ToCompactString(), *bTNE.ToCompactString(), *bBSW.ToCompactString(), *overlapTNE.ToCompactString(), *overlapBSW.ToCompactString(), *surfaceStructA.tne.ToCompactString(), *surfaceStructA.bsw.ToCompactString(), *surfaceStructB.tne.ToCompactString(), *surfaceStructB.bsw.ToCompactString(), *surfaceA->GetName(), *surfaceB->GetName()); }
					
					newID = nextNewID;

					//Create the edges between the (helper) vertices
					addedEdge = connectHelperVertices(prmA, prmB, vertexA, vertexB, newHelperVertexA, newHelperVertexB, helperEdgeA, helperEdgeB, helperEdgeC, newEdgeID, newEdgeID);
				}
			}
		}
		break;

		//Single Connection Box. Only the stairs case is handled, as the neighbouring case is already handled by the regular PRM connections
	case EConnectionMethod::SCB:
		if (surfaceA->surface == ESurfaceType::Stairs || surfaceA->surface == ESurfaceType::StairsCeiling || surfaceB->surface == ESurfaceType::Stairs || surfaceB->surface == ESurfaceType::StairsCeiling) {
			if (!checkDistance || (vertexA->GetActorLocation() - vertexB->GetActorLocation()).Size() < 400) {
				//Find the projection of A onto the surface of B and vice versa
				findNormals(neighbourStruct.cm, surfaceA, surfaceB, overlapCenter, vertexA, vertexB, normalA, normalB);

				//Find the normal of the side, which would be the direction from a to b if a is above
				FVector normalC = findNormalC(normalA, aCenter, bCenter);
				FVector orthogonalAC = FVector::CrossProduct(normalA, normalC).GetAbs();

				//Project the vertices onto the sides of the surfaces
				projectVerticesSCB(vertexA, vertexB, normalA, normalB, normalC, aExtentBox, bExtentBox, aTNE, aBSW, bTNE, bBSW, projectionA, projectionB);

				//Find the location for the helper vertices in between the two projections
				findHelperLocations(neighbourStruct.cm, vertexA, vertexB, projectionA, projectionB, normalA, normalB, normalC, orthogonalAC, helperLocationA, helperLocationB);

				//Find out if the transition is on the floor or the ceiling
				ESurfaceType transitionType = findStairsTransition(surfaceA->surface, surfaceB->surface);

				//Ensure that connections are only made on stairs, not with any surface!
				if (transitionType != ESurfaceType::TransitionEdge) {
					//Ensure that a connection can be made. If not, do not generate this vertex
					if (!prmA->isFutureEdgeBlocked(helperLocationA, helperLocationB, surfaceA->surface, surfaceB->surface, 0.25f)) {
						newHelperVertexA = createHelperVertex(surfaceA, helperLocationA, FVector(20), transitionType, newID, nextNewID, vertexA, vertexB);
						newID = nextNewID;
						newHelperVertexB = createHelperVertex(surfaceB, helperLocationB, FVector(20), transitionType, newID, nextNewID, vertexA, vertexB);
						newID = nextNewID;

						//Create the edges between the (helper) vertices
						addedEdge = connectHelperVertices(prmA, prmB, vertexA, vertexB, newHelperVertexA, newHelperVertexB, helperEdgeA, helperEdgeB, helperEdgeC, newEdgeID, newEdgeID);
					}
				}
			}
		}
		break;

		//Direct Connection
	case EConnectionMethod::Direct:
		if (!checkDistance || (vertexA->GetActorLocation() - vertexB->GetActorLocation()).Size() < 400) {
			//Find the projection of A onto the surface of B and vice versa
			findNormals(neighbourStruct.cm, surfaceA, surfaceB, overlapCenter, vertexA, vertexB, normalA, normalB);

			//Find the projections of the vertices onto each other's surfaces
			findDirectProjection(vertexA, vertexB, normalA, normalB, overlapTNE, overlapBSW, projectionA, projectionB);

			//Find the location of the helper vertex
			findHelperLocation(vertexA, vertexB, projectionA, projectionB, helperLocationA);

			//There is a direct connection between two vertices, so check for an edge!
			if (!helperLocationA.ContainsNaN()) {
				FMath::Clamp(helperLocationA.X, overlapBSW.X, overlapTNE.X);
				FMath::Clamp(helperLocationA.Y, overlapBSW.Y, overlapTNE.Y);
				FMath::Clamp(helperLocationA.Z, overlapBSW.Z, overlapTNE.Z);
				searchExtent = pairwiseMult((FVector(1) - normalA.GetAbs() - normalB.GetAbs()), FVector(50)) + FVector(5);

				//Add a helper vertex to keep the edges on the surfaces (except if the likely helper vertex is close to another one)
				if (!prmA->isEdgeOverVoid(helperLocationA, vertexA->GetActorLocation()) && !prmB->isEdgeOverVoid(helperLocationA, vertexB->GetActorLocation()) && checkNearSurface(helperLocationA)) {
					if (!prmA->isEdgeBlockedHelper(vertexA, vertexB, 0.25f) && !prmB->isEdgeBlockedHelper(vertexB, vertexA, 0.25f)) {
						ESurfaceType transitionType = findTransitionType(surfaceA->surface, surfaceB->surface);
						newHelperVertexA = createHelperVertex(surfaceB, helperLocationA, searchExtent, transitionType, newID, nextNewID, vertexA, vertexB);
						newID = nextNewID;

						//Create the edges (or any other helper vertex that is so close the new vertex wasn't made)
						if (!prmA->doesEdgeExist(vertexA, newHelperVertexA)) {
							helperEdgeA = prmA->generateEdge(vertexA, newHelperVertexA, newEdgeID, vertexA->surface, false);
							if (helperEdgeA != nullptr) {
								edges.AddUnique(helperEdgeA);
								newEdgeID++;
								addedEdge = true;
							}
						}
						if (!prmB->doesEdgeExist(vertexB, newHelperVertexA)) {
							helperEdgeB = prmB->generateEdge(vertexB, newHelperVertexA, newEdgeID, vertexB->surface, false);
							if (helperEdgeB != nullptr) {
								edges.AddUnique(helperEdgeB);
								newEdgeID++;
								addedEdge = true;
							}
						}
					}
				}
			}

			//A helper vertex is needed
			else {
				if (!prmA->doesEdgeExist(vertexA, vertexB) && !prmB->doesEdgeExist(vertexA, vertexB) && vertexA != vertexB) {
					ESurfaceType transitionType = findTransitionType(surfaceA->surface, surfaceB->surface);
					helperEdgeA = prmA->generateEdge(vertexA, vertexB, newEdgeID, transitionType, false);
					if (helperEdgeA != nullptr) {
						edges.AddUnique(helperEdgeA);
						newEdgeID++;
						addedEdge = true;
					}
				}
			}
		}
		break;
	}

	return addedEdge;
}

float APRMCollector::checkDirection(FVector a, FVector b, FVector orthogonal) {
	//Return the sign of b - a for the position in the direction of the orthogonal
	if (orthogonal.X == 0) {
		if (orthogonal.Y == 0) { return FMath::Sign(b.Z - a.Z); }
		else { return  FMath::Sign(b.Y - a.Y); }
	}
	else { return  FMath::Sign(b.X - a.X); }
}

void APRMCollector::findTNEBSW(FVector overlapTNE, FVector overlapBSW, FVector inATNE, FVector inABSW, FVector inBTNE, FVector inBBSW, FVector & outATNE, FVector & outABSW, FVector & outBTNE, FVector & outBBSW)
{
	outATNE.X = FMath::Min(overlapTNE.X + 200, inATNE.X); outATNE.Y = FMath::Min(overlapTNE.Y + 200, inATNE.Y); outATNE.Z = FMath::Min(overlapTNE.Z + 200, inATNE.Z);
	outABSW.X = FMath::Max(overlapBSW.X - 200, inABSW.X); outABSW.Y = FMath::Max(overlapBSW.Y - 200, inABSW.Y); outABSW.Z = FMath::Max(overlapBSW.Z - 200, inABSW.Z);
	outBTNE.X = FMath::Min(overlapTNE.X + 200, inBTNE.X); outBTNE.Y = FMath::Min(overlapTNE.Y + 200, inBTNE.Y); outBTNE.Z = FMath::Min(overlapTNE.Z + 200, inBTNE.Z);
	outBBSW.X = FMath::Max(overlapBSW.X - 200, inBBSW.X); outBBSW.Y = FMath::Max(overlapBSW.Y - 200, inBBSW.Y); outBBSW.Z = FMath::Max(overlapBSW.Z - 200, inBBSW.Z);
}

void APRMCollector::findNearbyAreas(FVector oTNE, FVector oBSW, FVector aTNE, FVector aBSW, FVector bTNE, FVector bBSW, FVector & oCenter, FVector & oExtent, FVector & aCenter, FVector & aExtent, FVector & bCenter, FVector & bExtent)
{
	oCenter = (oTNE + oBSW) / 2;
	oExtent = (oTNE - oBSW) / 2;
	aCenter = (aTNE + aBSW) / 2;
	aExtent = (aTNE - aBSW) / 2;
	bCenter = (bTNE + bBSW) / 2;
	bExtent = (bTNE - bBSW) / 2;
}

void APRMCollector::findVertices(FVector aCenter, FVector aExtent, FVector bCenter, FVector bExtent, TArray<AVertex*>& outA, TArray<AVertex*>& outB)
{
	TArray<TEnumAsByte<EObjectTypeQuery>> traceObjectTypes;
	traceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));
	
	TArray<AActor*> ignores;
	TArray<AActor*> aVertexActors;
	TArray<AActor*> bVertexActors;
	
	UKismetSystemLibrary::BoxOverlapActors(GetWorld(), aCenter, aExtent, traceObjectTypes, AVertex::StaticClass(), ignores, aVertexActors);
	UKismetSystemLibrary::BoxOverlapActors(GetWorld(), bCenter, bExtent, traceObjectTypes, AVertex::StaticClass(), ignores, bVertexActors);

	for (AActor* actor : aVertexActors) { outA.Add((AVertex*)actor); }
	for (AActor* actor : bVertexActors) { outB.Add((AVertex*)actor); }
}

void APRMCollector::findNormals(EConnectionMethod cm, ASurfaceArea * surfaceA, ASurfaceArea * surfaceB, FVector oCenter, AVertex * vertexA, AVertex * vertexB, FVector & normalA, FVector & normalB)
{
	normalA = surfaceA->getNormal();
	normalB = surfaceB->getNormal(); 
}

void APRMCollector::findNormalsDCB(EConnectionMethod cm, ASurfaceArea * surfaceA, ASurfaceArea * surfaceB, FVector oCenter, FVector oExtent, AVertex * vertexA, AVertex * vertexB, FVector & normalA, FVector & normalB, FVector & oAB, FVector & normalC)
{
	//Find the normals of surfaces A and B
	normalA = surfaceA->getNormal();
	normalB = surfaceB->getNormal();

	//If the surfaces are opposite each other, use an alternative method.
	if (cm == EConnectionMethod::DCB && normalA.GetAbs() == normalB.GetAbs()) {
		//The c normal points from the surfaces to the side they are connected around
		if (oExtent.X == 5) {
			normalC = FVector(oCenter.X - vertexA->GetActorLocation().X, 0, 0);
			normalC.Normalize();
		}
		else if (oExtent.Y == 5) {
			normalC = FVector(0, oCenter.Y - vertexA->GetActorLocation().Y, 0);
			normalC.Normalize();
		}
		else if (oExtent.Z == 5) {
			normalC = FVector(0, 0, oCenter.Z - vertexA->GetActorLocation().Z);
			normalC.Normalize();
		}

		//Find the orthogonal of (A or B) and C
		oAB = FVector::CrossProduct(normalA, normalC).GetAbs();

		//Find the middle of the overlap area
		FVector middle = pairwiseMult(oAB, ((vertexA->GetActorLocation() + vertexB->GetActorLocation()) / 2));
		oCenter = pairwiseMult((FVector(1) - oAB.GetAbs()), oCenter) + middle;
	}
	else {
		//Find the orthogonal of A and B
		oAB = FVector::CrossProduct(normalA, normalB).GetAbs();
	}
}

void APRMCollector::findDirectProjection(AVertex * a, AVertex * b, FVector normalA, FVector normalB, FVector overlapTNE, FVector overlapBSW, FVector & projectionA, FVector & projectionB)
{
	//Project each vertex onto the surface of the other vertex
	projectionA = getPointProjectionOntoPlane(b->GetActorLocation(), normalB, a->GetActorLocation());
	projectionB = getPointProjectionOntoPlane(a->GetActorLocation(), normalA, b->GetActorLocation());

	//Limit the projections to the overlapping part of the surfaces
	projectionA.X = FMath::Min(projectionA.X, overlapTNE.X); projectionB.X = FMath::Min(projectionB.X, overlapTNE.X);
	projectionA.Y = FMath::Min(projectionA.Y, overlapTNE.Y); projectionB.Y = FMath::Min(projectionB.Y, overlapTNE.Y);
	projectionA.Z = FMath::Min(projectionA.Z, overlapTNE.Z); projectionB.Z = FMath::Min(projectionB.Z, overlapTNE.Z);
	projectionA.X = FMath::Max(projectionA.X, overlapBSW.X); projectionB.X = FMath::Max(projectionB.X, overlapBSW.X);
	projectionA.Y = FMath::Max(projectionA.Y, overlapBSW.Y); projectionB.Y = FMath::Max(projectionB.Y, overlapBSW.Y);
	projectionA.Z = FMath::Max(projectionA.Z, overlapBSW.Z); projectionB.Z = FMath::Max(projectionB.Z, overlapBSW.Z);
}

void APRMCollector::projectVerticesDCB(AVertex* vertexA, AVertex* vertexB, FVector normalA, FVector normalB, FVector normalC, FVector aExtent, FVector bExtent, FVector aTNE, FVector aBSW, FVector bTNE, FVector bBSW, FVector &projectionA, FVector &projectionB) 
{
	//Find the edge of the surface A is on and project the vertex on there. Similar for B.
	FVector vertexOffsetA = FVector(0);
	FVector vertexOffsetB = FVector(0);

	if (normalA.GetAbs() == normalB.GetAbs()){
		//Direction the vertex should be projected towards at the length of the extent. Project towards the overlap area
		vertexOffsetA = pairwiseMult(aExtent, normalC);
		vertexOffsetB = pairwiseMult(bExtent, normalC);
	}
	else {
		//Direction the vertex should be projected towards at the length of the extent. Project towards the other surface
		vertexOffsetA = pairwiseMult(aExtent, normalB);
		vertexOffsetB = pairwiseMult(bExtent, normalA);
	}

	//Project the vertices
	projectionA = vertexA->GetActorLocation() + 50 * vertexOffsetA;
	projectionB = vertexB->GetActorLocation() + 50 * vertexOffsetB;

	//Limit the projection to the surface 
	projectionA.X = FMath::Min(projectionA.X, aTNE.X); projectionB.X = FMath::Min(projectionB.X, bTNE.X);
	projectionA.Y = FMath::Min(projectionA.Y, aTNE.Y); projectionB.Y = FMath::Min(projectionB.Y, bTNE.Y);
	projectionA.Z = FMath::Min(projectionA.Z, aTNE.Z); projectionB.Z = FMath::Min(projectionB.Z, bTNE.Z);
	projectionA.X = FMath::Max(projectionA.X, aBSW.X); projectionB.X = FMath::Max(projectionB.X, bBSW.X);
	projectionA.Y = FMath::Max(projectionA.Y, aBSW.Y); projectionB.Y = FMath::Max(projectionB.Y, bBSW.Y);
	projectionA.Z = FMath::Max(projectionA.Z, aBSW.Z); projectionB.Z = FMath::Max(projectionB.Z, bBSW.Z);
}

void APRMCollector::projectVerticesSCB(AVertex* vertexA, AVertex* vertexB, FVector normalA, FVector normalB, FVector normalC, FVector aExtent, FVector bExtent, FVector aTNE, FVector aBSW, FVector bTNE, FVector bBSW, FVector &projectionA, FVector &projectionB) {
	//Find the edge of the surface A is on and project the vertex on there. Similar for B.
	FVector vertexOffsetA = pairwiseMult(aExtent, normalC);
	FVector vertexOffsetB = pairwiseMult(bExtent, normalC);

	//A is above B
	if (vertexA->GetActorLocation().Z > vertexB->GetActorLocation().Z) {
		projectionA = vertexA->GetActorLocation() + 2 * vertexOffsetA;
		projectionB = vertexB->GetActorLocation() - 2 * vertexOffsetB;
	}
	//B is above A
	else {
		projectionA = vertexA->GetActorLocation() - 2 * vertexOffsetA;
		projectionB = vertexB->GetActorLocation() + 2 * vertexOffsetB;
	}

	//Limit the projections to the surfaces
	projectionA.X = FMath::Min(projectionA.X, aTNE.X); projectionB.X = FMath::Min(projectionB.X, bTNE.X);
	projectionA.Y = FMath::Min(projectionA.Y, aTNE.Y); projectionB.Y = FMath::Min(projectionB.Y, bTNE.Y);
	projectionA.Z = FMath::Min(projectionA.Z, aTNE.Z); projectionB.Z = FMath::Min(projectionB.Z, bTNE.Z);
	projectionA.X = FMath::Max(projectionA.X, aBSW.X); projectionB.X = FMath::Max(projectionB.X, bBSW.X);
	projectionA.Y = FMath::Max(projectionA.Y, aBSW.Y); projectionB.Y = FMath::Max(projectionB.Y, bBSW.Y);
	projectionA.Z = FMath::Max(projectionA.Z, aBSW.Z); projectionB.Z = FMath::Max(projectionB.Z, bBSW.Z);

	//Slightly adjust the projections in the direction of the c normal, or away based on whether the surfaces are floors or stairs
	if (normalA.X < 0 || normalA.Y < 0 || normalA.Z < 0) {
		projectionA -= 4.5f * normalC;
		projectionB -= 4.5f * normalC;
	}
	else {
		projectionA += 4.5f * normalC;
		projectionB += 4.5f * normalC;
	}
	projectionA -= 0.5f * normalA;
	projectionB -= 0.5f * normalB;
}

FVector APRMCollector::findNormalC(FVector normalA, FVector aCenter, FVector bCenter)
{
	FVector returnValue = FVector(0); 
	
	//Ensure normalA is normalised
	normalA.Normalize();

	//Depending on whether surface A or B is above, change he direction of the normal
	if (aCenter.Z > bCenter.Z) { returnValue = pairwiseMult(bCenter - aCenter, FVector(1) - normalA.GetAbs()); }
	else { returnValue = pairwiseMult(aCenter - bCenter, FVector(1) - normalA.GetAbs()); }
	
	//Find whether the normal should be in the X or Y direction
	if (FMath::Abs(returnValue.X) > FMath::Abs(returnValue.Y)) { returnValue.Y = 0; }
	else { returnValue.X = 0; }

	//The normal should not be in the Z direction and be normalized
	returnValue.Z = 0;
	returnValue = returnValue.GetSafeNormal();
	
	return returnValue;
}

void APRMCollector::findProjections(ASurfaceArea* surfaceA, ASurfaceArea* surfaceB, EConnectionMethod cm, AVertex * vertexA, AVertex * vertexB, FVector projectionA, FVector projectionB, FVector & projectionA2, FVector & projectionB2)
{
	TArray<AActor*> ignores;
	TArray<FHitResult> lineHitsA;
	TArray<FHitResult> lineHitsB;

	FVector projectionNormalA = FVector(0);
	FVector projectionNormalB = FVector(0);
	FVector projectionHelperA = FVector(0);
	FVector projectionHelperB = FVector(0);
	int32 surfaceHitsA = 0;
	int32 surfaceHitsB = 0;

	switch (cm) {
	case EConnectionMethod::DCB:
		UKismetSystemLibrary::LineTraceMulti(GetWorld(), projectionA, vertexA->GetActorLocation(), ETraceTypeQuery::TraceTypeQuery1, false, ignores, EDrawDebugTrace::None, lineHitsA, false, FLinearColor::Red, FLinearColor::Green, 15);
		UKismetSystemLibrary::LineTraceMulti(GetWorld(), projectionB, vertexB->GetActorLocation(), ETraceTypeQuery::TraceTypeQuery1, false, ignores, EDrawDebugTrace::None, lineHitsB, false, FLinearColor::Blue, FLinearColor::Yellow, 15);

		if (lineHitsA.Num() < 1) { UKismetSystemLibrary::LineTraceMulti(GetWorld(), projectionA, projectionA + 2 * (vertexA->GetActorLocation() - projectionA), ETraceTypeQuery::TraceTypeQuery1, false, ignores, EDrawDebugTrace::None, lineHitsA, false, FLinearColor::Red, FLinearColor::Green, 15); }
		if (lineHitsB.Num() < 1) { UKismetSystemLibrary::LineTraceMulti(GetWorld(), projectionB, projectionB + 2 * (vertexB->GetActorLocation() - projectionB), ETraceTypeQuery::TraceTypeQuery1, false, ignores, EDrawDebugTrace::None, lineHitsB, false, FLinearColor::Blue, FLinearColor::Yellow, 15); }

		for (FHitResult hitA : lineHitsA) {
			if (hitA.GetActor() == surfaceA) {
				if (surfaceA->cubes.Contains(hitA.GetComponent())) {
					//UE_LOG(LogTemp, Log, TEXT("This vertex has an impact with surface A: %s"), *vertexA->GetName());
					projectionA2 = hitA.ImpactPoint;
					surfaceHitsA++;
				}
			}
		}
		for (FHitResult hitB : lineHitsB) {
			if (hitB.GetActor() == surfaceB) {
				if (surfaceB->cubes.Contains(hitB.GetComponent())) {
					//UE_LOG(LogTemp, Log, TEXT("This vertex has an impact with surface B: %s"), *vertexB->GetName());
					projectionB2 = hitB.ImpactPoint;
					surfaceHitsB++;
				}
			}
		}

		if (surfaceA->getNormal().GetAbs() == (surfaceB->getNormal().GetAbs())) {
			if (surfaceHitsA == 0) { projectionA2 = projectionA; }
			if (surfaceHitsB == 0) { projectionB2 = projectionB; }
		}
		else {
			projectionA2 -= 5 * surfaceB->getNormal();
			projectionB2 -= 5 * surfaceA->getNormal();
		}
		break;

	case EConnectionMethod::SCB:
		ignores.Add(vertexA);
		ignores.Add(vertexB);

		UKismetSystemLibrary::LineTraceMulti(GetWorld(), projectionA, vertexB->GetActorLocation(), ETraceTypeQuery::TraceTypeQuery1, false, ignores, EDrawDebugTrace::None, lineHitsA, false, FLinearColor::Red, FLinearColor::Green, 15);
		UKismetSystemLibrary::LineTraceMulti(GetWorld(), projectionB, vertexA->GetActorLocation(), ETraceTypeQuery::TraceTypeQuery1, false, ignores, EDrawDebugTrace::None, lineHitsB, false, FLinearColor::Blue, FLinearColor::Yellow, 15);

		if (lineHitsA.Num() < 1) { UKismetSystemLibrary::LineTraceMulti(GetWorld(), projectionA, projectionA + 2 * (vertexA->GetActorLocation() - projectionA), ETraceTypeQuery::TraceTypeQuery1, false, ignores, EDrawDebugTrace::None, lineHitsA, false, FLinearColor::Red, FLinearColor::Green, 15); }
		if (lineHitsB.Num() < 1) { UKismetSystemLibrary::LineTraceMulti(GetWorld(), projectionB, projectionB + 2 * (vertexB->GetActorLocation() - projectionB), ETraceTypeQuery::TraceTypeQuery1, false, ignores, EDrawDebugTrace::None, lineHitsB, false, FLinearColor::Blue, FLinearColor::Yellow, 15); }

		if (lineHitsA.Num() < 1) {
			UE_LOG(LogTemp, Log, TEXT("Amount of hits A: %d"), lineHitsA.Num());
		}
		if (lineHitsB.Num() < 1) {
			UE_LOG(LogTemp, Log, TEXT("Amount of hits B: %d"), lineHitsB.Num());
		}

		for (FHitResult hit : lineHitsA) {
			if (hit.GetActor() == surfaceA || hit.GetActor() == surfaceB) {
				projectionNormalA = hit.ImpactNormal;
				projectionHelperA = hit.ImpactPoint;
			}
		}
		for (FHitResult hit : lineHitsB) {
			if (hit.GetActor() == surfaceB || hit.GetActor() == surfaceA) {
				projectionNormalB = hit.ImpactNormal;
				projectionHelperB = hit.ImpactPoint;
			}
		}

		//If one of the two normals is not found, assume that the second normal is the inverse of the first and vice versa. Set the location to the corresponding vertex!
		if (projectionNormalA == FVector(0)) {
			//DrawDebugLine(GetWorld(), projectionA, vertexB->GetActorLocation(), FColor::Magenta, false, 60, 0, 1);
			projectionNormalA = projectionNormalB;
			projectionHelperA = projectionHelperB + (vertexA->GetActorLocation() - projectionA);
		}

		if (projectionNormalB == FVector(0)) {
			//DrawDebugLine(GetWorld(), projectionB, vertexA->GetActorLocation(), FColor::Orange, false, 60, 0, 1);
			projectionNormalB = projectionNormalA;
			projectionHelperB = projectionHelperA + (vertexB->GetActorLocation() - projectionB);
		}

		//DrawDebugDirectionalArrow(GetWorld(), projectionHelperA, projectionHelperA + 50 * projectionNormalA, 2, FColor::Yellow, false, 60, 0, 1);
		//DrawDebugDirectionalArrow(GetWorld(), projectionHelperB, projectionHelperB + 50 * projectionNormalB, 2, FColor::Cyan, false, 60, 0, 1);

		projectionA2 = getPointProjectionOntoPlane(projectionHelperA, projectionNormalA, vertexA->GetActorLocation());
		projectionB2 = getPointProjectionOntoPlane(projectionHelperB, projectionNormalB, vertexB->GetActorLocation());
		break;

	default:
		break;
	}
}

void APRMCollector::findHelperLocations(EConnectionMethod cm, AVertex* vertexA, AVertex* vertexB, FVector projectionA, FVector projectionB, FVector normalA, FVector normalB, FVector oAB, FVector cNormal, FVector & helperLocationA, FVector & helperLocationB)
{
	// Calculate distances
	float aDist = (projectionA - vertexA->GetActorLocation()).Size();
	float bDist = (projectionB - vertexB->GetActorLocation()).Size();
	float orthoDist = (pairwiseMult(normalA, vertexA->GetActorLocation()) - pairwiseMult(normalA, vertexB->GetActorLocation())).Size();
	float middleDist = (projectionA - projectionB).Size();
	float helperRatioA = 0;
	float helperRatioB = 0;

	//Variables used in the switch case
	FVector projectionHelperA = FVector(0);
	FVector projectionHelperB = FVector(0);
	FVector heightA = FVector(0);
	FVector heightB = FVector(0);

	switch (cm) {
	case EConnectionMethod::DCB:
		//Find the ratios of A and B: how close is each one to the projection
		helperRatioA = aDist / (aDist + bDist + middleDist);
		helperRatioB = bDist / (aDist + bDist + middleDist);

		//Location of the projections without taking the "height" into account
		projectionHelperA = pairwiseMult((FVector(1) - oAB.GetAbs()), projectionA);
		projectionHelperB = pairwiseMult((FVector(1) - oAB.GetAbs()), projectionB);

		//Find the "height" (distance along the ortogonal normal) of the projections
		heightA = pairwiseMult(oAB, vertexA->GetActorLocation()) + helperRatioA * orthoDist * oAB * checkDirection(vertexA->GetActorLocation(), vertexB->GetActorLocation(), oAB);
		heightB = pairwiseMult(oAB, vertexB->GetActorLocation()) + helperRatioB * orthoDist * oAB * checkDirection(vertexB->GetActorLocation(), vertexA->GetActorLocation(), oAB);

		//Find the helper vertex locations
		helperLocationA = projectionHelperA + heightA;
		helperLocationB = projectionHelperB + heightB;

		//Case for parallel surfaces
		if (normalA.GetAbs() == normalB.GetAbs()) {
			//Find the helper locations and move them along the c normal
			helperLocationA += 4.5f * cNormal;
			helperLocationB += 4.5f * cNormal;

			//Slight adjustment in the direction of the surface and towards the border of the surface
			helperLocationA -= 0.5f * normalA;
			helperLocationB -= 0.5f * normalB;
		}

		//Case where the two surfaces are not parallel
		else {
			//Slight adjustment in the direction of the surface and towards the border of the surface
			helperLocationA -= 0.5f * normalA;
			helperLocationB -= 0.5f * normalB;
			helperLocationA += 4.5f * normalB;
			helperLocationB += 4.5f * normalA;
		}
		break;

	case EConnectionMethod::SCB:
		//Find the ratios of A and B: how close is each one to the projection
		helperRatioA = aDist / (aDist + bDist + orthoDist);
		helperRatioB = bDist / (aDist + bDist + orthoDist);

		//Find the location of the projections (from a to surface b and vice versa)
		projectionHelperA = FVector(projectionB.X, projectionB.Y, projectionA.Z);
		projectionHelperB = FVector(projectionA.X, projectionA.Y, projectionB.Z);

		//Find the helper locations
		helperLocationA = (1 - helperRatioA) * projectionA + helperRatioA * projectionHelperA;
		helperLocationB = (1 - helperRatioB) * projectionB + helperRatioB * projectionHelperB;
		break;

	default:
		break;
	}
}

void APRMCollector::findHelperLocation(AVertex* vertexA, AVertex* vertexB, FVector projectionA, FVector projectionB, FVector & helperLocation)
{
	//Find the location for the helper vertex in between the two projections
	float aDist = (vertexA->GetActorLocation() - projectionA).Size();
	float bDist = (vertexB->GetActorLocation() - projectionB).Size();

	float helperRatio = aDist / (aDist + bDist);
	helperLocation = (1 - helperRatio) * projectionA + helperRatio * projectionB;
}

bool APRMCollector::connectHelperVertices(APRM* a, APRM* b, AVertex* vertexA, AVertex* vertexB, AVertex* newVertexA, AVertex* newVertexB, APRMEdge* edgeA, APRMEdge* edgeB, APRMEdge* edgeC, int32  inEdgeID, int32 &newEdgeID)
{
	bool addedEdge = false;
	int32 edgeID = inEdgeID;

	if (newVertexA) {
		if (!a->doesEdgeExist(vertexA, newVertexA) && !a->isEdgeBlockedHelper(vertexA, newVertexA, 0.25f)) {
			edgeA = a->generateEdge(vertexA, newVertexA, newEdgeID, vertexA->surface, false);
			if (edgeA != nullptr) {
				edges.AddUnique(edgeA);
				edgeID++;
				addedEdge = true;
			}
		}
	}
	if (newVertexB) {
		if (!b->doesEdgeExist(vertexB, newVertexB) && !b->isEdgeBlockedHelper(vertexB, newVertexB, 0.25f)) {
			edgeB = b->generateEdge(vertexB, newVertexB, newEdgeID, vertexB->surface, false);
			if (edgeB != nullptr) {
				edges.AddUnique(edgeB);
				edgeID++;
				addedEdge = true;
			}
		}
	}
	if (newVertexA && newVertexB) {
		if (!a->doesEdgeExist(newVertexA, newVertexB) && !a->isEdgeBlockedHelper(newVertexA, newVertexB, 0.25f)) {
			edgeC = a->generateEdge(newVertexA, newVertexB, newEdgeID, ESurfaceType::TransitionEdge, false);
			if (edgeC != nullptr) {
				edges.AddUnique(edgeC);
				edgeID++;
				addedEdge = true;
			}
		}
	}

	newEdgeID = edgeID;
	return addedEdge;
}

bool APRMCollector::connectHelperVerticesSingle(APRM* a, APRM* b, AVertex* vertexA, AVertex* vertexB, AVertex* newVertexA, AVertex* newVertexB, APRMEdge* edgeA, APRMEdge* edgeB, int32  inEdgeID, int32 &newEdgeID)
{
	bool addedEdge = false;
	int32 edgeID = inEdgeID;

	if (newVertexA) {
		if (!a->doesEdgeExist(vertexA, newVertexA) && !a->isEdgeBlockedHelper(vertexA, newVertexA, 0.25f)) {
			edgeA = a->generateEdge(vertexA, newVertexA, newEdgeID, vertexA->surface, false);
			if (edgeA != nullptr) {
				edges.AddUnique(edgeA);
				edgeID++;
				addedEdge = true;
			}
		}
	}
	if (newVertexB) {
		if (!b->doesEdgeExist(vertexB, newVertexB) && !b->isEdgeBlockedHelper(vertexB, newVertexB, 0.25f)) {
			edgeB = b->generateEdge(vertexB, newVertexB, newEdgeID, vertexB->surface, false);
			if (edgeB != nullptr) {
				edges.AddUnique(edgeB);
				edgeID++;
				addedEdge = true;
			}
		}
	}

	newEdgeID = edgeID;
	return addedEdge;
}

AVertex * APRMCollector::createHelperVertex(ASurfaceArea * surface, FVector location, FVector extent, ESurfaceType surfaceType, int32 inID, int32 & outID, AVertex* goalA, AVertex* goalB)
{
	AVertex*  returnValue = nullptr;

	//Preparation for the overlap functions
	TArray<TEnumAsByte<EObjectTypeQuery>> traceObjectTypes;
	traceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));

	//Look for any overlapping rooms
	TArray<AActor*> overlappingRooms;
	TArray<AActor*> ignores;
	UKismetSystemLibrary::BoxOverlapActors(GetWorld(), location, FVector(4), traceObjectTypes, ABasicRoom::StaticClass(), ignores, overlappingRooms);

	//Only proceed if the helper vertex does not overlap a room
	if (overlappingRooms.Num() <= 0) {

		//Find any overlapping (helper) vertices
		TArray<AActor*> nearbyHelpers;
		TArray<AActor*> nearbyVertices;
		UKismetSystemLibrary::BoxOverlapActors(GetWorld(), location, FVector(10), traceObjectTypes, AHelperVertex::StaticClass(), ignores, nearbyHelpers);
		UKismetSystemLibrary::BoxOverlapActors(GetWorld(), location, FVector(4), traceObjectTypes, AVertex::StaticClass(), ignores, nearbyVertices);

		// Don't generate a helper vertex if another (helper) vertex is really close or overlapping. If not, generate a new helper vertex
		if (nearbyVertices.Num() > 0) { returnValue = (AVertex*)nearbyVertices[0]; }
		else if (nearbyHelpers.Num() > 0) { returnValue = (AHelperVertex*)nearbyHelpers[0]; }
		else {
			returnValue = surface->generateHelperVertex(inID, location, surfaceType, agentSize, true, goalA, goalB);
			outID = inID + 1;
		}

		//Add the generated vertex to the vertices array
		if (returnValue != nullptr) { vertices.AddUnique(returnValue); }
	}

	//Return the generated or nearby vertex.
	return returnValue;
}

bool APRMCollector::checkNearSurface(FVector location) {
	TArray<TEnumAsByte<EObjectTypeQuery>> traceObjectTypes;
	traceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));
	TArray<AActor*> ignores;
	TArray<AActor*> foundSurfaces;

	UKismetSystemLibrary::BoxOverlapActors(GetWorld(), location, FVector(10), traceObjectTypes, ASurfaceArea::StaticClass(), ignores, foundSurfaces);
	return foundSurfaces.Num() > 0;
}

bool APRMCollector::isOnSurface(AVertex* vertexA, AVertex* vertexB) {
	bool xSame = FMath::Abs(vertexA->GetActorLocation().X - vertexB->GetActorLocation().X) < 10;
	bool ySame = FMath::Abs(vertexA->GetActorLocation().Y - vertexB->GetActorLocation().Y) < 10;
	bool zSame = FMath::Abs(vertexA->GetActorLocation().Z - vertexB->GetActorLocation().Z) < 10;
	return xSame || ySame || zSame;
}

AVertex* APRMCollector::findVertex(int32 findID) {
	AVertex* returnValue = nullptr;

	//Looks for the vertex with the given ID
	for (AVertex* vertex : vertices) {
		if (vertex->id == findID) {
			returnValue = vertex;
			break;
		}
	}

	return returnValue;
}

AVertex* APRMCollector::getVertex(int32 getID) {
	AVertex* returnValue = nullptr;

	//Ensure the ID is in range
	if (!vertices.IsValidIndex(getID)) {
		UE_LOG(LogTemp, Log, TEXT("getID out of range. id: %d; vertices: %d"), getID, vertices.Num());
		return returnValue;
	}

	//Get the vertex at the position of the ID. If this vertex does not have the required ID, find it manually
	returnValue = vertices[getID];
	if (returnValue->id != getID) {	returnValue = findVertex(getID); }

	//Give an error if the ID cannot be found in the array
	if (returnValue == nullptr) { UE_LOG(LogTemp, Log, TEXT("getID is not present in the vertices array. id: %d"), getID); }

	return returnValue;

}

ESurfaceType APRMCollector::findTransitionType(ESurfaceType a, ESurfaceType b) {
	ESurfaceType returnValue = ESurfaceType::Floor;

	switch (a) {
	case ESurfaceType::NorthWall:
		switch (b) {
		case ESurfaceType::EastWall:
			returnValue = ESurfaceType::TransitionNE;
			break;
		case ESurfaceType::WestWall:
			returnValue = ESurfaceType::TransitionNW;
			break;
		case ESurfaceType::Ceiling:
			returnValue = ESurfaceType::TransitionNC;
			break;
		case ESurfaceType::StairsCeiling:
			returnValue = ESurfaceType::TransitionNC;
			break;
		case ESurfaceType::Floor:
			returnValue = ESurfaceType::TransitionNF;
			break;
		case ESurfaceType::Stairs:
			returnValue = ESurfaceType::TransitionNF;
		default:
			break;

		}
		break;
	case ESurfaceType::SouthWall:
		switch (b) {
		case ESurfaceType::EastWall:
			returnValue = ESurfaceType::TransitionSE;
			break;
		case ESurfaceType::WestWall:
			returnValue = ESurfaceType::TransitionSW;
			break;
		case ESurfaceType::Ceiling:
			returnValue = ESurfaceType::TransitionSC;
			break;
		case ESurfaceType::StairsCeiling:
			returnValue = ESurfaceType::TransitionSC;
			break;
		case ESurfaceType::Floor:
			returnValue = ESurfaceType::TransitionSF;
			break;
		case ESurfaceType::Stairs:
			returnValue = ESurfaceType::TransitionSF;
		default:
			break;
		}
		break;
	case ESurfaceType::EastWall:
		switch (b) {
		case ESurfaceType::NorthWall:
			returnValue = ESurfaceType::TransitionNE;
			break;
		case ESurfaceType::SouthWall:
			returnValue = ESurfaceType::TransitionSE;
			break;
		case ESurfaceType::Ceiling:
			returnValue = ESurfaceType::TransitionEC;
			break;
		case ESurfaceType::StairsCeiling:
			returnValue = ESurfaceType::TransitionEC;
			break;
		case ESurfaceType::Floor:
			returnValue = ESurfaceType::TransitionEF;
			break;
		case ESurfaceType::Stairs:
			returnValue = ESurfaceType::TransitionEF;
		default:
			break;
		}
		break;
	case ESurfaceType::WestWall:
		switch (b) {
		case ESurfaceType::NorthWall:
			returnValue = ESurfaceType::TransitionNW;
			break;
		case ESurfaceType::SouthWall:
			returnValue = ESurfaceType::TransitionSW;
			break;
		case ESurfaceType::Ceiling:
			returnValue = ESurfaceType::TransitionWC;
			break;
		case ESurfaceType::StairsCeiling:
			returnValue = ESurfaceType::TransitionWC;
			break;
		case ESurfaceType::Floor:
			returnValue = ESurfaceType::TransitionWF;
			break;
		case ESurfaceType::Stairs:
			returnValue = ESurfaceType::TransitionWF;
		default:
			break;
		}
		break;
	case ESurfaceType::Ceiling:
		switch (b) {
		case ESurfaceType::NorthWall:
			returnValue = ESurfaceType::TransitionNC;
			break;
		case ESurfaceType::SouthWall:
			returnValue = ESurfaceType::TransitionSC;
			break;
		case ESurfaceType::EastWall:
			returnValue = ESurfaceType::TransitionEC;
			break;
		case ESurfaceType::WestWall:
			returnValue = ESurfaceType::TransitionWC;
			break;
		default:
			break;
		}
		break;
	case ESurfaceType::StairsCeiling:
		switch (b) {
		case ESurfaceType::NorthWall:
			returnValue = ESurfaceType::TransitionNC;
			break;
		case ESurfaceType::SouthWall:
			returnValue = ESurfaceType::TransitionSC;
			break;
		case ESurfaceType::EastWall:
			returnValue = ESurfaceType::TransitionEC;
			break;
		case ESurfaceType::WestWall:
			returnValue = ESurfaceType::TransitionWC;
			break;
		default:
			break;
		}
		break;
	case ESurfaceType::Floor:
		switch (b) {
		case ESurfaceType::NorthWall:
			returnValue = ESurfaceType::TransitionNF;
			break;
		case ESurfaceType::SouthWall:
			returnValue = ESurfaceType::TransitionSF;
			break;
		case ESurfaceType::EastWall:
			returnValue = ESurfaceType::TransitionEF;
			break;
		case ESurfaceType::WestWall:
			returnValue = ESurfaceType::TransitionWF;
			break;
		default:
			break;
		}
		break;
	case ESurfaceType::Stairs:
		switch (b) {
		case ESurfaceType::NorthWall:
			returnValue = ESurfaceType::TransitionNF;
			break;
		case ESurfaceType::SouthWall:
			returnValue = ESurfaceType::TransitionSF;
			break;
		case ESurfaceType::EastWall:
			returnValue = ESurfaceType::TransitionEF;
			break;
		case ESurfaceType::WestWall:
			returnValue = ESurfaceType::TransitionWF;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return returnValue;
}

ESurfaceType APRMCollector::findStairsTransition(ESurfaceType a, ESurfaceType b) {
	ESurfaceType returnValue = ESurfaceType::TransitionEdge;

	if ((a == ESurfaceType::Stairs || a == ESurfaceType::Floor) && (b == ESurfaceType::Stairs || b == ESurfaceType::Floor)) {
		returnValue = ESurfaceType::TransitionStairs;
	}

	if ((a == ESurfaceType::StairsCeiling || a == ESurfaceType::Ceiling) && (b == ESurfaceType::StairsCeiling || b == ESurfaceType::Ceiling)) {
		returnValue = ESurfaceType::TransitionStairsCeiling;
	}

	return returnValue;
}

void APRMCollector::findExtraNeighbours(AVertex * vertex, APRM* prm, FVector location, FVector extent)
{
	//Overlapped vertices
	TArray<AVertex*> returnValue;
	TArray<AActor*> tempActors;

	//Actors to ignore, so the vertex itself
	TArray<AActor*> ignores;
	ignores.Add(vertex);

	//Object types to trace
	TArray<TEnumAsByte<EObjectTypeQuery>> traceObjectTypes;
	traceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));

	//Find all vertices overlapping the cube
	//UKismetSystemLibrary::BoxOverlapActors(GetWorld(), location, extent, traceObjectTypes, AVertex::StaticClass(), ignores, tempActors);

	//Cast to vertices
	for (AActor* actor : tempActors) {
		AVertex* neighbour = (AVertex*)actor;
		if (prm->vertices.Contains(neighbour)) { vertex->neighbours.AddUnique(neighbour->id); }
	}

//	if (vertex->neighbours.Num() < 2){ DrawDebugBox(GetWorld(), vertex->GetActorLocation(), extent, FQuat(0, 0, 0, 1), FColor::Red, false, 60, 0, 2); }

}

int32 APRMCollector::generateExtraVertex(AVertex* &extraVertex, APRM* prm, ASurfaceArea* surface, UStaticMeshComponent* cube, int32 newID, int32 newEdgeID, FVector tne, FVector bsw, bool addToExtraArray, TArray<AVertex*> extraArray ) {
	int32 returnValue = newEdgeID;

	//Create a new vertex in a given range
	extraVertex = surface->generateVertexInRange(newID, agentSize, tne, bsw, true);

	//Ensure the new vertex exists
	if (extraVertex != nullptr) {
		//Add the new vertex to the arrays and increase the ID
		vertices.Add(extraVertex);
		prm->vertices.Add(extraVertex);
		newID++;

		//If there is an extraArray, add it to it
		if (addToExtraArray){ extraArray.Add(extraVertex); }

		//Find the extent of the overlap box to check for neighbours
		FVector extent = FVector(750);
		switch (extraVertex->surface) {
		case ESurfaceType::Floor:
			extent.Z = 10;
			break;
		case ESurfaceType::Ceiling:
			extent.Z = 10;
			break;
		case ESurfaceType::NorthWall:
			extent.X = 10;
			break;
		case ESurfaceType::SouthWall:
			extent.X = 10;
			break;
		case ESurfaceType::EastWall:
			extent.Y = 10;
			break;
		case ESurfaceType::WestWall:
			extent.Y = 10;
			break;
		case ESurfaceType::Stairs:
			extent.Z = 10;
			break;
		case ESurfaceType::StairsCeiling:
			extent.Z = 10;
			break;
		default:
			break;
		}
		
		FVector searchTNE = extraVertex->GetActorLocation() + extent;
		FVector searchBSW = extraVertex->GetActorLocation() - extent;
		FVector location = (searchTNE + searchBSW) / 2;

		//Find the neighbours of the extra vertex
		findExtraNeighbours(extraVertex, prm, location, extent);

		//Create an edge to every extra neighbour
		for (int32 neighbourID : extraVertex->neighbours) {
			AVertex* neighbour = getVertex(neighbourID);
			returnValue = generateExtraEdge(extraVertex, neighbour, prm, newEdgeID, extraVertex->surface, true);
		}
	}

	return returnValue;
}

int32 APRMCollector::generateExtraEdge(AVertex * a, AVertex * b, APRM* prm, int32 newID, ESurfaceType surfaceType, bool showEdge)
{
	int32 returnValue = newID;

	if (a != b && !prm->isEdgeOverVoid(a->GetActorLocation(), b->GetActorLocation())) {
		APRMEdge* newEdge = GetWorld()->SpawnActor<APRMEdge>(a->GetActorLocation(), FRotator(0), FActorSpawnParameters());

		if (newEdge != nullptr) {
			//Give the edge an ID
			newEdge->id = newID;

			//Set the vertices as end points of the edge
			newEdge->endVertices.AddUnique(a->id);
			newEdge->endVertices.AddUnique(b->id);

			//Set the surface the edge is on
			newEdge->surface = surfaceType;
			newEdge->setEdge(a->GetActorLocation(), b->GetActorLocation());

			//Shape the edge
			if (showEdge) { newEdge->showEdge(); }
			else { newEdge->hideEdge(); }

			//Set the two vertices as neighbours
			a->neighbours.AddUnique(b->id);
			b->neighbours.AddUnique(a->id);

			//Add the edge to the array for this PRM
			edges.AddUnique(newEdge);
			returnValue++;
		}
	}
	return returnValue;
}

//MEDIAL AXIS APPROACH:
// 1. COMPUTE THE MEDIAL AXIS
//// A. CREATE A POLYGON OUT OF THE SURFACE
//// B. ORDER THE VERTICES CLOCKWISE STARTING AT THE MINIMUM OF X,Y,Z PER CONNECTED COMPONENT
//// C. DELAUNAY TRIANGULATION
//// D. FIXING TRIANGULATION ACCORDING TO THE PAPER
//// E. CREATE THE MEDIAL AXIS
// 2. GENERATE POINTS AND MOVE THEM TOWARDS THE MEDIAL AXIS
// 3. CONNECT