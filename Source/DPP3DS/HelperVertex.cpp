// Fill out your copyright notice in the Description page of Project Settings.


#include "HelperVertex.h"

AHelperVertex::AHelperVertex(const FObjectInitializer& ObjectInitializer) : 
Super(ObjectInitializer)
{
	cube->SetMaterial(0, connectionMat);
}