// Fill out your copyright notice in the Description page of Project Settings.

#include "Chaser.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"

AChaser::AChaser(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	cube->SetMaterial(0, chaserMat);
}

void AChaser::BeginPlay() {
	Super::BeginPlay();
	edgeDistance = 0;
	surfaceChanges = 0;
	startTimeMoment = FDateTime::Now();
	computationStartTimeMomentA = FDateTime::Now();
	computationStartTimeMomentD = FDateTime::Now();
	computationTimeTotal = 0;
	timeUntilDynamic = 0;
	movementTimeTotal = 0;
	firstCall = true;
}

void AChaser::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	//Dynamic Programming Based Method
	if (pathPlanningMethod == EPathPlanningMethod::Dynamic) {
		//Keep trying as long as the chaser has not failed and is not done
		if (!finished && !failed) {
			dynamicTick(DeltaTime);
		}
	}
	
	//Combined Method
	else if (pathPlanningMethod == EPathPlanningMethod::Combined) {
		//Keep trying as long as the chaser has not failed and is not done
		if (!finished && !failed) {
			//If the current vertex has been handled by the dynamic approach and the chaser has stopped moving, start using the dynamic approach
			if (currentVertex != nullptr) {
				if (!isMoving && !useDynamic && currentVertex->dpRound > 0) { 
					target->verticesMoved = 0;
					useDynamic = true; 
					canMove = true;

					//Calculate the movement time before using the dynamic approach
					movementEndTimeMomentA = FDateTime::Now();
					movementStartTimeMomentD = FDateTime::Now();
					FTimespan movementStartTime = movementStartTimeMomentA.GetTimeOfDay();
					FTimespan movementEndTime = movementEndTimeMomentA.GetTimeOfDay();
					movementTimeTotal += movementEndTime - movementStartTime;
				}
			}

			if (!useDynamic) {
				aStarTick(DeltaTime);

				//While we only want to move with aStar, we still want to calculate the dynamic approach
				if (target->currentVertex != nullptr && currentVertex != nullptr) {
					//If the target is not at the goal vertex, reset the goal. Only do this while not moving to prevent weird jumps.
					if (!isMoving && goalVertex != target->currentVertex) {
						//The chaser is not at the current vertex of the target, so move there!
						globalRound++;
						resetGoal(target->currentVertex, globalRound);
					}
						
					//If the chaser is at the target's current vertex and has not finished yet, go to the target's next vertex
					else if (!isMoving && currentVertex == target->currentVertex) {
						if (target->nextVertex != nullptr) {
							if (goalVertex != target->nextVertex) {
								globalRound++;
								resetGoal(target->nextVertex, globalRound);
							}
						}
					}

					//Put all open vertices in a temporary set
					TArray<int32> tempSet;
					for (int32 openID : openVertexSet) { tempSet.Add(openID); }

					//Expand all vertices in the temporary set if the distance is at most 0
					for (int32 openID : tempSet) {
						AVertex* openVertex = prmCollector->getVertex(openID);
						if (openVertex->dpDistance <= 0) {
							openVertexSet.Remove(openID);
							handleDynamicVertex(openVertex);
						}
			
						//The distance is larger than 0, so the dynamic wave has not hit the vertex yet. Therefore, don't handle it yet and instead "move the wave"
						else { openVertex->dpDistance = openVertex->dpDistance - 2 * movementSpeed * DeltaTime; }
					}
				}
			}
			
			//useDynamic, so use the dynamic approach!
			else { dynamicTick(DeltaTime); }
		}

		//Handle the failure state used in the A* Based Method
		if (failed && !useDynamic) {
			if (currentVertex != target->currentVertex) { objectives.Add(target->currentVertex); }
			else { objectives.Add(target->nextVertex); }
			failed = false;
			computationStartTimeMomentA = FDateTime::Now();
		}

		else if (failed) {
			UE_LOG(LogTemp, Log, TEXT("Chaser failed during dynamic somehow. Just ignore this!"));
			failed = false;
		}
	}

	//A* Based Method
	else {
		//We only need to do stuff if navigation is not done yet
		if (!failed && !finished) { aStarTick(DeltaTime); }

		//The chaser has failed, so try again!
		if (failed) {
			if (currentVertex != target->currentVertex) { objectives.Add(target->currentVertex); }
			else { objectives.Add(target->nextVertex); }
			failed = false;
			computationStartTimeMomentA = FDateTime::Now();
		}
	}
}

void AChaser::printData() {
	USaveGame* baseSaveFile = UGameplayStatics::LoadGameFromSlot(saveName, 0);

	if (!baseSaveFile) { baseSaveFile = UGameplayStatics::CreateSaveGameObject(UPathPlanningSave::StaticClass()); }
	saveFile = (UPathPlanningSave*)baseSaveFile;

	if (saveFile) {
		UE_LOG(LogTemp, Log, TEXT("Data for path planning save file: %s"), *saveName);
		UE_LOG(LogTemp, Log, TEXT("Computation Time: %s"), *saveFile->computationTime.ToString());
		if (pathPlanningMethod == EPathPlanningMethod::Combined) { UE_LOG(LogTemp, Log, TEXT("Time until dynamic: %s"), *saveFile->dynamicTime.ToString()); }
		UE_LOG(LogTemp, Log, TEXT("Travel Time: %s"), *saveFile->pathTime.ToString());
		UE_LOG(LogTemp, Log, TEXT("Total Time: %s"), *saveFile->totalTime.ToString());
		UE_LOG(LogTemp, Log, TEXT("Distance: %f"), saveFile->distance);
		UE_LOG(LogTemp, Log, TEXT("Surface changes: %d"), saveFile->surfaceChanges);
		UE_LOG(LogTemp, Log, TEXT("Smoothness: %f"), saveFile->smoothness);
		UE_LOG(LogTemp, Log, TEXT("Outdatedness: %f"), saveFile->outdatedness);
		UE_LOG(LogTemp, Log, TEXT("Target reached on time: %s"), (saveFile->targetFinished ? TEXT("false") : TEXT("true")));
	}
}

void AChaser::printDataCSV()
{
	USaveGame* baseSaveFile = UGameplayStatics::LoadGameFromSlot(saveName, 0);

	if (!baseSaveFile) { baseSaveFile = UGameplayStatics::CreateSaveGameObject(UPathPlanningSave::StaticClass()); }
	saveFile = (UPathPlanningSave*)baseSaveFile;

	if (saveFile) {
		UE_LOG(LogTemp, Log, TEXT("%s;%s;%s;%s;%s;%f;%d;%f;%f"), (saveFile->targetFinished ? TEXT("false") : TEXT("true")), 
			*FString::SanitizeFloat(saveFile->computationTime.GetTotalSeconds()), *FString::SanitizeFloat(saveFile->dynamicTime.GetTotalSeconds()),
			*FString::SanitizeFloat(saveFile->pathTime.GetTotalSeconds()), *FString::SanitizeFloat(saveFile->totalTime.GetTotalSeconds()),
			saveFile->distance, saveFile->surfaceChanges, saveFile->smoothness, saveFile->outdatedness);
	}
}

void AChaser::aStarTick(float DeltaTime){
	//Movement function during during the tick. Only move if there is a vertex to move to
	if (isMoving && nextVertex) {
		SetActorLocation(FMath::VInterpConstantTo(GetActorLocation(), nextVertex->GetActorLocation(), DeltaTime, movementSpeed));
		edgeDistance += DeltaTime * movementSpeed;

		//Check whether the target has been reached
		if ((target->GetActorLocation() - GetActorLocation()).Size() < 75
			&& !((currentVertex->surface == ESurfaceType::Ceiling || currentVertex->surface == ESurfaceType::StairsCeiling)
			&& (target->currentVertex->surface == ESurfaceType::Floor || currentVertex->surface == ESurfaceType::Stairs || currentVertex->surface == ESurfaceType::TransitionStairs)))  {
			finished = true;
			target->failed = true;
			UE_LOG(LogTemp, Log, TEXT("Target reached"));

			//At the target, stop movement time
			movementEndTimeMomentA = FDateTime::Now();
			FTimespan movementStartTime = movementStartTimeMomentA.GetTimeOfDay();
			FTimespan movementEndTime = movementEndTimeMomentA.GetTimeOfDay();
			movementTimeTotal += movementEndTime - movementStartTime;

			//Now save the path planning data
			endTimeMoment = FDateTime::Now();
			FTimespan startTime = startTimeMoment.GetTimeOfDay();
			FTimespan endTime = endTimeMoment.GetTimeOfDay();
			FTimespan totalTime = endTime - startTime;

			saveData(totalTime);
		}

		//Stop moving if the next vertex has been reached
		if ((GetActorLocation() - nextVertex->GetActorLocation()).IsNearlyZero()) {
			isMoving = false;

			//For checking the final path that was created. Ensure that the same vertex is not added twice in a row!
			if (currentVertex != nullptr && finalPath.Num() > 0) {
				if (finalPath[finalPath.Num() - 1] != currentVertex->id) {
					finalPath.Add(currentVertex->id);
					outdatednessPerSegment.Add(target->verticesMoved);
				}
			}

			//If there is no vertex in the final path yet, it can be safely added!
			else if (currentVertex != nullptr) {
				finalPath.Add(currentVertex->id);
				outdatednessPerSegment.Add(target->verticesMoved);
			}

			//If the path should be shown, highlight this edge now
			if (showPath && currentVertex && nextVertex) { showPathEdge(currentVertex->id, nextVertex->id); }

			//If the goal is reached and the target is not there, the chaser failed and should try again
			if ((GetActorLocation() - goalVertex->GetActorLocation()).IsNearlyZero()) {
				failed = true;

				//Update the current vertex
				currentVertex = nextVertex;

				//At the goal vertex, stop movement time
				movementEndTimeMomentA = FDateTime::Now();
				FTimespan movementStartTime = movementStartTimeMomentA.GetTimeOfDay();
				FTimespan movementEndTime = movementEndTimeMomentA.GetTimeOfDay();
				movementTimeTotal += movementEndTime - movementStartTime;
			}
		};
	}

	//Happens if the chaser is moving but has no next vertex to move to. This should never be reached!
	else if (isMoving) { UE_LOG(LogTemp, Log, TEXT("We're not done yet but there is no next vertex")); }

	//There is no destination to go to, so find the next one
	else {
		//There is a vertex on the path to go to.
		if (path.Num() > 0) {
			int32 nextDestination = path[path.Num() - 1];
			moveToVertex(nextDestination);
			path.Remove(nextDestination);

			//Check if the chaser will change from one surface to another
			AVertex* surfaceCheckVertex = prmCollector->getVertex(nextDestination);
			if (currentVertex && surfaceCheckVertex) {
				if (currentVertex->surface != surfaceCheckVertex->surface) { surfaceChanges++; }
			}
		}

		//There is no vertex on the path, but there is an objective to go to (the target)
		else {
			if (objectives.Num() > 0) {
				AVertex* chosenObjective = objectives[0];
				bool navigationSucceeded = navigate(currentVertex, chosenObjective);
				target->verticesMoved = 0;
				objectives.Remove(chosenObjective);

				// Check if a path exists. If so, fail. If not, calculate the time it took to calculate the path
				if (!navigationSucceeded) {
					UE_LOG(LogTemp, Log, TEXT("No path exists between the current vertex and the target"));
					failed = true;
				}
				else {
					computationEndTimeMomentA = FDateTime::Now();
					movementStartTimeMomentA = FDateTime::Now();
					FTimespan computationStartTime = computationStartTimeMomentA.GetTimeOfDay();
					FTimespan computationEndTime = computationEndTimeMomentA.GetTimeOfDay();
					computationTimeTotal += computationEndTime - computationStartTime;
				}
			}

			//No objectives anymore. That can only be if the target is reached, so this should never happen!
			else { UE_LOG(LogTemp, Log, TEXT("There are no objectives, yet the agent is not done. Something went wrong here")); }
		}
	}
}

void AChaser::dynamicTick(float DeltaTime) {
	//Assert that the current vertices of the target and chaser exist
	if (target->currentVertex != nullptr && currentVertex != nullptr) {

		//If the target is not at the goal vertex, reset the goal. Only do this while not moving to prevent weird jumps.
		if (goalVertex != target->currentVertex) {
			
			//The chaser is not at the current vertex of the target, so move towards there!
			globalRound++;
			resetGoal(target->currentVertex, globalRound);
			
			//If this is not the first update round, reset the outdatedness to 0
			if (!firstCall){ target->verticesMoved = 0; }
		}

		//If the chaser is at the target's current vertex and has not finished yet, go to the target's next vertex, if it exists
		else if (currentVertex == target->currentVertex) {
			if (target->nextVertex != nullptr) {
				if (goalVertex != target->nextVertex) {

					globalRound++;
					resetGoal(target->nextVertex, globalRound);

					//If this is not the first update round, reset the outdatedness to 0
					if (!firstCall) { target->verticesMoved = 0; }
				}
			}
		}

		//Put all open vertices in a temporary set
		TArray<int32> tempSet;
		for (int32 openID : openVertexSet) { tempSet.Add(openID); }

		//Expand all vertices in the temporary set if the distance is at most 0
		for (int32 openID : tempSet) {
			AVertex* openVertex = prmCollector->getVertex(openID);
			if (openVertex->dpDistance <= 0) {
				openVertexSet.Remove(openID);
				handleDynamicVertex(openVertex);
			}
			//The distance is larger than 0, so the dynamic wave has not hit the vertex yet. Therefore, don't handle it yet and instead "move the wave"
			else { openVertex->dpDistance = openVertex->dpDistance - 2 * movementSpeed * DeltaTime; }
		}

		//Check that computation time has ended
		if (firstCall && canMove) {
			firstCall = false;
			pathOutdatedness = target->verticesMoved;

			//Calculate the computation time
			computationEndTimeMomentD = FDateTime::Now();
			movementStartTimeMomentD = FDateTime::Now();
			FTimespan computationStartTime = computationStartTimeMomentD.GetTimeOfDay();
			FTimespan computationEndTime = computationEndTimeMomentD.GetTimeOfDay();
			
			//In the combined method, add this computation time to "Time until dynamic", otherwise to the total computation time
			if (pathPlanningMethod != EPathPlanningMethod::Combined) { computationTimeTotal += computationEndTime - computationStartTime; }
			else { timeUntilDynamic += computationEndTime - computationStartTime; }
		}

		//If the agent can move and isn't moving yet, start moving
		if (canMove && !isMoving) {
			//Add the current vertex to the final path if there is no vertex on that path yet
			if (finalPath.Num() <= 0) {
				finalPath.Add(currentVertex->id);
				outdatednessPerSegment.Add(target->verticesMoved);
			}

			//If the vertex is not the last vertex in the final path, add it
			if (finalPath[finalPath.Num() - 1] != currentVertex->id) {
				finalPath.Add(currentVertex->id);
				outdatednessPerSegment.Add(target->verticesMoved);
			}

			//Now find the next destination
			moveToVertex(currentVertex->dpDirection); }

		//Movement function
		if (isMoving && nextVertex) {
			SetActorLocation(FMath::VInterpConstantTo(GetActorLocation(), nextVertex->GetActorLocation(), DeltaTime, movementSpeed));
			edgeDistance += DeltaTime * movementSpeed;

			//Stop moving if the next vertex has been reached
			if ((GetActorLocation() - nextVertex->GetActorLocation()).IsNearlyZero()) {
				isMoving = false;

				//If the path should be shown, highlight this edge now
				if (showPath && currentVertex && nextVertex) { showPathEdge(currentVertex->id, nextVertex->id); }

				//Check if there was a surface change
				ESurfaceType currentSurface = currentVertex->surface;

				//Update the current vertex
				currentVertex = nextVertex;
				if (currentVertex->surface != currentSurface) { surfaceChanges++; }
			}
		}

		//Indicate success if the target has been reached
		if ((target->GetActorLocation() - GetActorLocation()).Size() < 75) {
			finished = true;
			target->failed = true;

			//Calculate the movement time
			movementEndTimeMomentD = FDateTime::Now();
			FTimespan movementStartTime = movementStartTimeMomentD.GetTimeOfDay();
			FTimespan movementEndTime = movementEndTimeMomentD.GetTimeOfDay();
			movementTimeTotal += movementEndTime - movementStartTime;

			//Now save the path planning data
			endTimeMoment = FDateTime::Now();
			FTimespan startTime = startTimeMoment.GetTimeOfDay();
			FTimespan endTime = endTimeMoment.GetTimeOfDay();
			FTimespan totalTime = endTime - startTime;

			saveData(totalTime);
		}

		//No open vertices, no movement and not allowed to move. This should never happen
		if (openVertexSet.Num() == 0 && !canMove && !isMoving) {
			failed = true;
			UE_LOG(LogTemp, Log, TEXT("There are no open vertices, the agent cannot move and isn't moving. Something is wrong."));
		}
	}

	//There is no current vertex for neither the target or chaser. This should not happen
	else { UE_LOG(LogTemp, Log, TEXT("Target or Chaser has no current vertex?")); }
}

void AChaser::saveData(FTimespan totalTime) {
	//Calculate the smoothness and outdatedness
	pathSmoothness = calculateSmoothness();
	pathOutdatedness += calculateOutdatedness();

	//Find the proper save file, or create one if it doesn't exist yet
	USaveGame* baseSaveFile = UGameplayStatics::LoadGameFromSlot(saveName, 0);

	if (!baseSaveFile) { baseSaveFile = UGameplayStatics::CreateSaveGameObject(UPathPlanningSave::StaticClass()); }
	saveFile = (UPathPlanningSave*)baseSaveFile;

	//Save all the values
	if (saveFile) {
		saveFile->totalTime = totalTime;
		saveFile->pathTime = movementTimeTotal;
		saveFile->computationTime = computationTimeTotal;
		saveFile->distance = edgeDistance;
		saveFile->surfaceChanges = surfaceChanges;
		saveFile->targetFinished = target->finished;
		saveFile->smoothness = pathSmoothness;
		saveFile->dynamicTime = timeUntilDynamic;
		saveFile->outdatedness = pathOutdatedness;
		UGameplayStatics::SaveGameToSlot(saveFile, saveName, 0);
	}

	//If there is no save file at all, and it cannot be created, show an error. This should never happen
	else { UE_LOG(LogTemp, Log, TEXT("No save file found. Build data not saved.")); }
}

void AChaser::showPathEdge(int32 IDA, int32 IDB)
{
	//Don't highlight an edge to some vertex from the goal
	if (currentVertex != goalVertex) {
		//There are no selfloops, so no need to highlight an edge if current and next are the same
		if (IDA != IDB) {
			//Find the edge to highlight
			APRMEdge* travelledEdge = prmCollector->findEdge(IDA, IDB);

			//Give the edge the proper colour
			if (travelledEdge != nullptr) {
				if (greenPath) { travelledEdge->makePathEdge(); }
				else { travelledEdge->highlightEdge(); }
			}
		}
	}
}

float AChaser::calculateSmoothness() {
	float returnValue = 0;

	//If the final path consists of at most 2 vertices, then the path is by definition smooth, since it's the shortest possible path!
	if (finalPath.Num() == 0 || finalPath.Num() == 1 || finalPath.Num() == 2) { returnValue = 1; }

	//The final path contains 3 vertices
	else if (finalPath.Num() == 3) {
		//Find the vertices
		AVertex* va = prmCollector->getVertex(finalPath[0]);
		AVertex* vb = prmCollector->getVertex(finalPath[1]);
		AVertex* vc = prmCollector->getVertex(finalPath[2]);
		AVertex* vg = nullptr;
		AVertex* vh = nullptr;

		//Find the length of the final path
		float realPathLength = 0;
		realPathLength += (va->GetActorLocation() - vb->GetActorLocation()).Size();
		realPathLength += (vb->GetActorLocation() - vc->GetActorLocation()).Size();

		//Create a new path from the first to the last vertex
		preparePathPlanning(va->id, vc->id);
		aStar(va->id, vc->id);

		//Find the shortest possible path length from the first to the last vertex
		float shortcutLength = 0;
		for (int32 i = 0; i < finalPath.Num() - 1; i++) {
			vg = prmCollector->getVertex(path[i]);
			vh = prmCollector->getVertex(path[i]);
			shortcutLength += (vg->GetActorLocation() - vh->GetActorLocation()).Size();
		}

		if (vh != nullptr) { shortcutLength += (vh->GetActorLocation() - va->GetActorLocation()).Size(); }

		//Smoothness is the shortcut length over the real path length
		returnValue = shortcutLength / realPathLength;
	}

	//The path contains 4 edges. Works similar to the case above
	else if (finalPath.Num() == 4) {
		AVertex* va = prmCollector->getVertex(finalPath[0]);
		AVertex* vb = prmCollector->getVertex(finalPath[1]);
		AVertex* vc = prmCollector->getVertex(finalPath[2]);
		AVertex* vd = prmCollector->getVertex(finalPath[3]);
		AVertex* vg = nullptr;
		AVertex* vh = nullptr;

		float realPathLength = 0;
		realPathLength += (va->GetActorLocation() - vb->GetActorLocation()).Size();
		realPathLength += (vb->GetActorLocation() - vc->GetActorLocation()).Size();
		realPathLength += (vc->GetActorLocation() - vd->GetActorLocation()).Size();

		preparePathPlanning(va->id, vd->id);
		aStar(va->id, vd->id);
		float shortcutLength = 0;
		for (int32 i = 0; i < path.Num() - 1; i++) {
			vg = prmCollector->getVertex(path[i]);
			vh = prmCollector->getVertex(path[i]);
			shortcutLength += (vg->GetActorLocation() - vh->GetActorLocation()).Size();
		}

		if (vh != nullptr) {
			shortcutLength += (vh->GetActorLocation() - va->GetActorLocation()).Size();
		}

		returnValue = shortcutLength / realPathLength;
	}

	//The path contains 5 edges. Works similar to the case above
	else if (finalPath.Num() == 5) {
		AVertex* va = prmCollector->getVertex(finalPath[0]);
		AVertex* vb = prmCollector->getVertex(finalPath[1]);
		AVertex* vc = prmCollector->getVertex(finalPath[2]);
		AVertex* vd = prmCollector->getVertex(finalPath[3]);
		AVertex* ve = prmCollector->getVertex(finalPath[4]);
		AVertex* vg = nullptr;
		AVertex* vh = nullptr;

		float realPathLength = 0;
		realPathLength += (va->GetActorLocation() - vb->GetActorLocation()).Size();
		realPathLength += (vb->GetActorLocation() - vc->GetActorLocation()).Size();
		realPathLength += (vc->GetActorLocation() - vd->GetActorLocation()).Size();
		realPathLength += (vd->GetActorLocation() - ve->GetActorLocation()).Size();

		preparePathPlanning(va->id, ve->id);
		aStar(va->id, vc->id);
		float shortcutLength = 0;
		for (int32 i = 0; i < path.Num() - 1; i++) {
			vg = prmCollector->getVertex(path[i]);
			vh = prmCollector->getVertex(path[i]);
			shortcutLength += (vg->GetActorLocation() - vh->GetActorLocation()).Size();
		}

		if (vh != nullptr) {
			shortcutLength += (vh->GetActorLocation() - va->GetActorLocation()).Size();
		}

		returnValue = shortcutLength / realPathLength;
	}

	//The path contains 6 or more edges. Works similar to the case above, but we calculate more paths
	else {
		float segments = 0;

		//Go over all segments of the final path consisting of 5 edges
		for (int32 i = 0; i < finalPath.Num() - 6; i++) {
			segments += 1;
			AVertex* va = prmCollector->getVertex(finalPath[i]);
			AVertex* vb = prmCollector->getVertex(finalPath[i + 1]);
			AVertex* vc = prmCollector->getVertex(finalPath[i + 2]);
			AVertex* vd = prmCollector->getVertex(finalPath[i + 3]);
			AVertex* ve = prmCollector->getVertex(finalPath[i + 4]);
			AVertex* vf = prmCollector->getVertex(finalPath[i + 5]);
			AVertex* vg = nullptr;
			AVertex* vh = nullptr;

			//Find the length of the segment
			float realPathLength = 0;
			realPathLength += (va->GetActorLocation() - vb->GetActorLocation()).Size();
			realPathLength += (vb->GetActorLocation() - vc->GetActorLocation()).Size();
			realPathLength += (vc->GetActorLocation() - vd->GetActorLocation()).Size();
			realPathLength += (vd->GetActorLocation() - ve->GetActorLocation()).Size();
			realPathLength += (ve->GetActorLocation() - vf->GetActorLocation()).Size();

			//Do path planning from the start to the end of the segment
			preparePathPlanning(va->id, vf->id);
			aStar(va->id, vf->id);

			//Calculate the shortest path length from the start of the segment to its end
			float shortcutLength = 0;
			for (int32 j = 0; j < path.Num() - 2; j++) {
				vg = prmCollector->getVertex(path[j]);
				vh = prmCollector->getVertex(path[j + 1]);
				shortcutLength += (vg->GetActorLocation() - vh->GetActorLocation()).Size();
			}

			if (vh != nullptr) { shortcutLength += (vh->GetActorLocation() - va->GetActorLocation()).Size(); }

			//Add the shortcut length over the real path length to the smoothness
			returnValue += shortcutLength / realPathLength;
		}

		//The smoothness of the path in total is the smoothness of each 5-edge segment divided by the total amount of segments.
		returnValue /= segments;
	}

	return returnValue;
}

float AChaser::calculateOutdatedness() {
	float returnValue = 0;

	//Find the amount of segments
	float segments = outdatednessPerSegment.Num();

	//Add the outdatedness of every segment and divide by the amount of segments to get the average outdatedness
	for (float outdatednessSegment : outdatednessPerSegment) { returnValue += outdatednessSegment; }
	returnValue /= segments;

	return returnValue;
}