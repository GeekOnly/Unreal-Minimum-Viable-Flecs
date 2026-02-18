#pragma once

#include "CoreMinimal.h"

// Wind Motor — attached to a flecs entity per AWindMotorActor
struct FWindMotorData
{
	FVector  WorldPosition = FVector::ZeroVector;
	FVector  Direction = FVector(1.f, 0.f, 0.f);
	float    Strength = 500.f;
	float    Radius = 300.f;
	float    InnerRadius = 0.f;
	float    Height = 500.f;
	float    VortexAngularSpeed = 1.f;
	uint8    Shape = 0;        // EWindMotorShape
	uint8    EmissionType = 0; // EWindEmissionType
	uint8    bEnabled = 1;
	uint8    _pad = 0;
};

// Wind Receiver — attached to every foliage entity
struct FWindReceiver
{
	float Sensitivity = 1.f;
	float DisplacementCurrent = 0.f;
	float DisplacementVelocity = 0.f;
	float StiffnessK = 10.f;
	float DampingC = 2.f;
};

// Foliage Instance Data — links Flecs entity to HISM slot
struct FFoliageInstanceData
{
	void*  HISMComponentPtr = nullptr;
	int32  InstanceIndex = -1;
	int32  CPDSlotDisplace = 0;
	int32  CPDSlotTurb = 1;
};

// Cached world position for foliage entity
struct FFoliageWorldPosition
{
	FVector Location = FVector::ZeroVector;
};

// Wind velocity result — written by simulation, read by foliage response
struct FWindVelocityAtEntity
{
	FVector Velocity = FVector::ZeroVector;
	float   Turbulence = 0.f;
};

// Tags
struct FWindMotorTag {};
struct FFoliageTag {};
