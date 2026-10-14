// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MyProjectPawn.h"
#include "MyProjectSportsCar.generated.h"

/**
 *  Sports car wheeled vehicle implementation
 */
UCLASS(abstract)
class AMyProjectSportsCar : public AMyProjectPawn
{
	GENERATED_BODY()
	
public:

	AMyProjectSportsCar();
};
