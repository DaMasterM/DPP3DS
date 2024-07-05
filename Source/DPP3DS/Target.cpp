// Fill out your copyright notice in the Description page of Project Settings.


#include "Target.h"

ATarget::ATarget(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	cube->SetMaterial(0, targetMat);
	verticesMoved = 0;
}

void ATarget::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	//We only need to do stuff if navigation is not done yet
	if (!failed && !finished) {
		//If the agent should be moving, move it towards its current destination
		if (isMoving && nextVertex) {
			SetActorLocation(FMath::VInterpConstantTo(GetActorLocation(), nextVertex->GetActorLocation(), DeltaTime, movementSpeed));

			//Stop moving if the next vertex has been reached
			if ((GetActorLocation() - nextVertex->GetActorLocation()).IsNearlyZero()) {
				isMoving = false;
				verticesMoved += 1;

				//If the path should be shown, highlight this edge now
				if (showPath && currentVertex && nextVertex) {
					if (currentVertex != goalVertex) {
						int32 endIDA = currentVertex->id;
						int32 endIDB = nextVertex->id;
						if (endIDA != endIDB) {
							APRMEdge* travelledEdge = prmCollector->findEdge(endIDA, endIDB);

							if (travelledEdge != nullptr) {
								if (greenPath) { travelledEdge->makePathEdge(); }
								else { travelledEdge->highlightEdge(); }
							}
						}
					}
				}

				//Indicate success if all objectives have been reached
				if ((GetActorLocation() - goalVertex->GetActorLocation()).IsNearlyZero() && objectives.Num() <= 0) {
					finished = true;
					currentVertex = goalVertex;
				}

				//If it is not the final one, do still set the current vertex to the goal vertex
				else if ((GetActorLocation() - goalVertex->GetActorLocation()).IsNearlyZero()) { currentVertex = goalVertex; }
			}
		}

		//If the target is moving but has no next vertex, this is an error!
		else if (isMoving) {
			UE_LOG(LogTemp, Log, TEXT("We're not done yet but there is no next vertex"));
		}

		//There is no destination to go to, so find the next one
		else {
			//Check if there is another vertex in the path. If so, go there.
			if (path.Num() > 0) {
				int32 nextDestination = path[path.Num() - 1];
				moveToVertex(nextDestination);
				path.Remove(nextDestination);
			}
			//Check if there is another objective. If so, navigate there.
			else {
				if (objectives.Num() > 0) {
					AVertex* chosenObjective = objectives[0];
					bool navigationSucceeded = navigate(currentVertex, chosenObjective);
					objectives.Remove(chosenObjective);

					if (!navigationSucceeded) {
						UE_LOG(LogTemp, Log, TEXT("No path exists between the current vertex and the objective"));
						failed = true;
					}
				}

				//If there is no place to go but the target is not done, something went wrong
				else { UE_LOG(LogTemp, Log, TEXT("There are no objectives, yet the agent is not done. Something went wrong here")); }
			}
		}
	}
}