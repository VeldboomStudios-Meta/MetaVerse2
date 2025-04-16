// Copyright Epic Games, Inc. All Rights Reserved.

#include "NWheelVehicleGameMode.h"
#include "NWheelVehicleCharacter.h"
#include "UObject/ConstructorHelpers.h"

ANWheelVehicleGameMode::ANWheelVehicleGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
