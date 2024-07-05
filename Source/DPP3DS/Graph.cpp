// Fill out your copyright notice in the Description page of Project Settings.


#include "Graph.h"

//TEST CLASS FOR LATER. USED BEFORE PRMS WERE INTRODUCED TO TEST GRAPHS. NO NEED TO USE IT ANYMORE!

// Sets default values
AGraph::AGraph()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void AGraph::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AGraph::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AGraph::connectAll()
{
	// Set an ID for every vertex
	for (int i = 0; i < vertices.Num(); i++) {
		vertices[i]->id = i;
	}

	//Counter for giving the right ID to an edge
	int edgeId = 0;

	//Create an edge between each pair of vertices, give the edge and ID and add the edge to the graph
	for (int i = 0; i < vertices.Num(); i++) {
		for (int j = i + 1; j < vertices.Num(); j++) {
			APRMEdge* newEdge = GetWorld()->SpawnActor<APRMEdge>(FVector(vertices[i]->GetActorLocation()), FRotator(0), FActorSpawnParameters());
			newEdge->id = edgeId;
			edgeId++;
			newEdge->endVertices.AddUnique(vertices[i]->id);
			newEdge->endVertices.AddUnique(vertices[j]->id);
			newEdge->setEdge(vertices[i]->GetActorLocation(), vertices[j]->GetActorLocation());
			edges.AddUnique(newEdge);
		}
	}
}

void AGraph::showGraph()
{
	for (AVertex* vertex : vertices) {
		vertex->showVertex();
	}

	for (APRMEdge* edge : edges) {
		edge->showEdge();
	}
}

void AGraph::hideGraph()
{
	for (AVertex* vertex : vertices) {
		vertex->hideVertex();
	}

	for (APRMEdge* edge : edges) {
		edge->hideEdge();
	}
}

