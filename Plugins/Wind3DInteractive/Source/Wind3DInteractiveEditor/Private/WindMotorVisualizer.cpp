#include "WindMotorVisualizer.h"
#include "WindVolumeComponent.h"
#include "WindSubsystem.h"
#include "IWindSolver.h"
#include "WindTypes.h"
#include "SceneManagement.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

static TAutoConsoleVariable<int32> CVarShowWindField(
	TEXT("Wind3D.ShowWindField"),
	0,
	TEXT("1 = grid bounds + wind arrows | 2 = grid bounds + all cells + arrows"),
	ECVF_Default);

void FWindMotorVisualizer::DrawVisualization(
	const UActorComponent* Component,
	const FSceneView* View,
	FPrimitiveDrawInterface* PDI)
{
	const UWindVolumeComponent* WindComp = Cast<UWindVolumeComponent>(Component);
	if (!WindComp) return;

	const AActor* Owner = WindComp->GetOwner();
	if (!Owner) return;

	const FTransform ActorTransform = Owner->GetActorTransform();

	// Draw shape gizmo based on motor shape
	switch (WindComp->Shape)
	{
	case EWindMotorShape::Sphere:
		DrawSphereShape(ActorTransform, WindComp->Radius, PDI);
		break;
	case EWindMotorShape::Cylinder:
		DrawCylinderShape(ActorTransform, WindComp->Radius, WindComp->Height, PDI);
		break;
	case EWindMotorShape::Cone:
		DrawConeShape(ActorTransform, WindComp->Radius, WindComp->Height, PDI);
		break;
	}

	// Draw emission direction indicators
	DrawEmissionArrows(ActorTransform, static_cast<uint8>(WindComp->EmissionType),
		WindComp->Radius, WindComp->Strength, PDI);

	UWorld* World = Component->GetWorld();
	if (!World) return;

	// Draw wind field debug visualization (CVar: Wind3D.ShowWindField 1/2)
	const int32 ShowLevel = CVarShowWindField.GetValueOnGameThread();
	if (ShowLevel > 0)
	{
		DrawWindFieldDebug(World, View, PDI, ShowLevel);
	}
}

void FWindMotorVisualizer::DrawSphereShape(const FTransform& T, float Radius, FPrimitiveDrawInterface* PDI) const
{
	const FVector Center = T.GetLocation();
	const FColor Color = FColor::Cyan;
	const int32 Segments = 32;

	// Draw 3 orthogonal circles
	DrawCircle(PDI, Center, T.GetUnitAxis(EAxis::X), T.GetUnitAxis(EAxis::Y),
		Color, Radius, Segments, SDPG_Foreground);
	DrawCircle(PDI, Center, T.GetUnitAxis(EAxis::X), T.GetUnitAxis(EAxis::Z),
		Color, Radius, Segments, SDPG_Foreground);
	DrawCircle(PDI, Center, T.GetUnitAxis(EAxis::Y), T.GetUnitAxis(EAxis::Z),
		Color, Radius, Segments, SDPG_Foreground);
}

void FWindMotorVisualizer::DrawCylinderShape(const FTransform& T, float Radius, float Height, FPrimitiveDrawInterface* PDI) const
{
	const FVector Center = T.GetLocation();
	const FVector Up = T.GetUnitAxis(EAxis::Z);
	const FVector Forward = T.GetUnitAxis(EAxis::X);
	const FVector Right = T.GetUnitAxis(EAxis::Y);
	const FColor Color = FColor::Cyan;
	const int32 Segments = 32;
	const float HalfH = Height * 0.5f;

	// Top and bottom circles
	const FVector Top = Center + Up * HalfH;
	const FVector Bottom = Center - Up * HalfH;

	DrawCircle(PDI, Top, Forward, Right, Color, Radius, Segments, SDPG_Foreground);
	DrawCircle(PDI, Bottom, Forward, Right, Color, Radius, Segments, SDPG_Foreground);

	// Vertical lines connecting top and bottom
	const int32 NumVerts = 8;
	for (int32 I = 0; I < NumVerts; I++)
	{
		const float Angle = (float)I / (float)NumVerts * 2.f * PI;
		const FVector Offset = Forward * FMath::Cos(Angle) * Radius + Right * FMath::Sin(Angle) * Radius;
		PDI->DrawLine(Top + Offset, Bottom + Offset, Color, SDPG_Foreground);
	}
}

void FWindMotorVisualizer::DrawConeShape(const FTransform& T, float Radius, float Height, FPrimitiveDrawInterface* PDI) const
{
	const FVector Center = T.GetLocation();
	const FVector Forward = T.GetUnitAxis(EAxis::X);
	const FVector Right = T.GetUnitAxis(EAxis::Y);
	const FVector Up = T.GetUnitAxis(EAxis::Z);
	const FColor Color = FColor::Cyan;
	const int32 Segments = 32;

	// Apex at actor location, base circle at Height distance along forward
	const FVector Apex = Center;
	const FVector BaseCenter = Center + Forward * Height;

	// Base circle
	DrawCircle(PDI, BaseCenter, Right, Up, Color, Radius, Segments, SDPG_Foreground);

	// Lines from apex to base
	const int32 NumLines = 8;
	for (int32 I = 0; I < NumLines; I++)
	{
		const float Angle = (float)I / (float)NumLines * 2.f * PI;
		const FVector BasePoint = BaseCenter + Right * FMath::Cos(Angle) * Radius + Up * FMath::Sin(Angle) * Radius;
		PDI->DrawLine(Apex, BasePoint, Color, SDPG_Foreground);
	}
}

void FWindMotorVisualizer::DrawEmissionArrows(const FTransform& T, uint8 EmissionType, float Radius, float Strength, FPrimitiveDrawInterface* PDI) const
{
	const FVector Center = T.GetLocation();
	const FVector Forward = T.GetUnitAxis(EAxis::X);
	const FVector Right = T.GetUnitAxis(EAxis::Y);
	const FVector Up = T.GetUnitAxis(EAxis::Z);
	const FColor ArrowColor = FColor::Yellow;
	const float ArrowLen = FMath::Clamp(Strength * 0.2f, 30.f, 200.f);

	const EWindEmissionType Emission = static_cast<EWindEmissionType>(EmissionType);

	switch (Emission)
	{
	case EWindEmissionType::Directional:
	{
		// Draw arrows pointing forward at several positions
		const int32 Num = 5;
		for (int32 I = 0; I < Num; I++)
		{
			const float Frac = (float)I / (float)(Num - 1) - 0.5f;
			const FVector Start = Center + Right * Frac * Radius;
			const FVector End = Start + Forward * ArrowLen;
			PDI->DrawLine(Start, End, ArrowColor, SDPG_Foreground);
			// Arrowhead
			PDI->DrawLine(End, End - Forward * 15.f + Right * 8.f, ArrowColor, SDPG_Foreground);
			PDI->DrawLine(End, End - Forward * 15.f - Right * 8.f, ArrowColor, SDPG_Foreground);
		}
		break;
	}
	case EWindEmissionType::Omni:
	{
		// Draw arrows pointing outward in 8 directions
		const int32 Num = 8;
		for (int32 I = 0; I < Num; I++)
		{
			const float Angle = (float)I / (float)Num * 2.f * PI;
			const FVector Dir = Forward * FMath::Cos(Angle) + Right * FMath::Sin(Angle);
			const FVector Start = Center + Dir * Radius * 0.3f;
			const FVector End = Start + Dir * ArrowLen;
			PDI->DrawLine(Start, End, ArrowColor, SDPG_Foreground);
		}
		break;
	}
	case EWindEmissionType::Vortex:
	{
		// Draw curved arrows suggesting rotation
		const int32 Num = 8;
		for (int32 I = 0; I < Num; I++)
		{
			const float Angle = (float)I / (float)Num * 2.f * PI;
			const float NextAngle = Angle + 0.4f;
			const FVector P0 = Center + (Forward * FMath::Cos(Angle) + Right * FMath::Sin(Angle)) * Radius * 0.6f;
			const FVector P1 = Center + (Forward * FMath::Cos(NextAngle) + Right * FMath::Sin(NextAngle)) * Radius * 0.6f;
			PDI->DrawLine(P0, P1, FColor::Magenta, SDPG_Foreground);
		}
		break;
	}
	case EWindEmissionType::Moving:
	{
		// Forward arrows showing motion direction
		const int32 Num = 5;
		for (int32 I = 0; I < Num; I++)
		{
			const float Frac = (float)I / (float)(Num - 1) - 0.5f;
			const FVector Start = Center + Right * Frac * Radius;
			const FVector End = Start + Forward * ArrowLen;
			PDI->DrawLine(Start, End, FColor::Orange, SDPG_Foreground);
			PDI->DrawLine(End, End - Forward * 15.f + Right * 8.f, FColor::Orange, SDPG_Foreground);
			PDI->DrawLine(End, End - Forward * 15.f - Right * 8.f, FColor::Orange, SDPG_Foreground);
		}
		// Wake trail behind the motor
		for (int32 I = 1; I <= 4; I++)
		{
			const float T = (float)I * 0.25f;
			const FVector P = Center - Forward * Radius * T;
			const float S = 8.f * (1.f - T * 0.5f);
			PDI->DrawLine(P - Right * S, P + Right * S, FColor(255, 140, 0), SDPG_Foreground);
		}
		break;
	}
	}
}

void FWindMotorVisualizer::DrawGridBounds(const IWindSolver& Grid, FPrimitiveDrawInterface* PDI) const
{
	// Compute 8 corners of the grid bounding box
	const FVector Min = Grid.GetWorldOrigin();
	const FVector Max = Grid.GetWorldOrigin() + FVector(
		Grid.GetSizeX() * Grid.GetCellSize(),
		Grid.GetSizeY() * Grid.GetCellSize(),
		Grid.GetSizeZ() * Grid.GetCellSize());

	const FColor BoxColor = FColor(0, 200, 255); // bright cyan

	// Bottom face
	PDI->DrawLine(FVector(Min.X, Min.Y, Min.Z), FVector(Max.X, Min.Y, Min.Z), BoxColor, SDPG_Foreground);
	PDI->DrawLine(FVector(Max.X, Min.Y, Min.Z), FVector(Max.X, Max.Y, Min.Z), BoxColor, SDPG_Foreground);
	PDI->DrawLine(FVector(Max.X, Max.Y, Min.Z), FVector(Min.X, Max.Y, Min.Z), BoxColor, SDPG_Foreground);
	PDI->DrawLine(FVector(Min.X, Max.Y, Min.Z), FVector(Min.X, Min.Y, Min.Z), BoxColor, SDPG_Foreground);

	// Top face
	PDI->DrawLine(FVector(Min.X, Min.Y, Max.Z), FVector(Max.X, Min.Y, Max.Z), BoxColor, SDPG_Foreground);
	PDI->DrawLine(FVector(Max.X, Min.Y, Max.Z), FVector(Max.X, Max.Y, Max.Z), BoxColor, SDPG_Foreground);
	PDI->DrawLine(FVector(Max.X, Max.Y, Max.Z), FVector(Min.X, Max.Y, Max.Z), BoxColor, SDPG_Foreground);
	PDI->DrawLine(FVector(Min.X, Max.Y, Max.Z), FVector(Min.X, Min.Y, Max.Z), BoxColor, SDPG_Foreground);

	// Vertical edges
	PDI->DrawLine(FVector(Min.X, Min.Y, Min.Z), FVector(Min.X, Min.Y, Max.Z), BoxColor, SDPG_Foreground);
	PDI->DrawLine(FVector(Max.X, Min.Y, Min.Z), FVector(Max.X, Min.Y, Max.Z), BoxColor, SDPG_Foreground);
	PDI->DrawLine(FVector(Max.X, Max.Y, Min.Z), FVector(Max.X, Max.Y, Max.Z), BoxColor, SDPG_Foreground);
	PDI->DrawLine(FVector(Min.X, Max.Y, Min.Z), FVector(Min.X, Max.Y, Max.Z), BoxColor, SDPG_Foreground);

	// Label: center dot
	const FVector Center = (Min + Max) * 0.5f;
	const float Half = 15.f;
	PDI->DrawLine(Center - FVector(Half, 0, 0), Center + FVector(Half, 0, 0), FColor::White, SDPG_Foreground);
	PDI->DrawLine(Center - FVector(0, Half, 0), Center + FVector(0, Half, 0), FColor::White, SDPG_Foreground);
	PDI->DrawLine(Center - FVector(0, 0, Half), Center + FVector(0, 0, Half), FColor::White, SDPG_Foreground);
}

void FWindMotorVisualizer::DrawWindFieldDebug(UWorld* World, const FSceneView* View, FPrimitiveDrawInterface* PDI, int32 ShowLevel) const
{
	if (!World) return;

	UWindSubsystem* WindSys = World->GetSubsystem<UWindSubsystem>();
	if (!WindSys) return;

	const IWindSolver& Grid = WindSys->GetWindGrid();

	// Always draw the grid bounding box so you can see coverage
	DrawGridBounds(Grid, PDI);

	const TArray<FVector>& Vels = Grid.GetVelocities();
	const int32 TotalCells = Vels.Num();
	if (TotalCells == 0) return;

	const float MaxArrowLen = Grid.GetCellSize() * 0.8f;

	// Downsample: limit to ~2000 arrows to keep editor responsive
	const int32 Step = FMath::Max(1, FMath::CeilToInt(FMath::Pow((float)TotalCells / 2000.f, 1.f / 3.f)));

	for (int32 Z = 0; Z < Grid.GetSizeZ(); Z += Step)
	{
		for (int32 Y = 0; Y < Grid.GetSizeY(); Y += Step)
		{
			for (int32 X = 0; X < Grid.GetSizeX(); X += Step)
			{
				const FVector CellCenter = Grid.CellToWorld(X, Y, Z);

				// Frustum check: skip cells not visible
				if (!View->ViewFrustum.IntersectBox(CellCenter, FVector(Grid.GetCellSize() * 0.5f)))
					continue;

				// Direct array access — no trilinear interpolation needed at cell centers
				const int32 Idx = Grid.CellIndex(X, Y, Z);
				const FVector& Vel = Vels[Idx];
				const float Speed = Vel.Size();

				if (Speed < 1.f)
				{
					if (ShowLevel >= 2)
					{
						const float S = Grid.GetCellSize() * 0.05f;
						const FColor EmptyColor(60, 60, 60);
						PDI->DrawLine(CellCenter - FVector(S, 0, 0), CellCenter + FVector(S, 0, 0), EmptyColor, SDPG_World);
						PDI->DrawLine(CellCenter - FVector(0, S, 0), CellCenter + FVector(0, S, 0), EmptyColor, SDPG_World);
						PDI->DrawLine(CellCenter - FVector(0, 0, S), CellCenter + FVector(0, 0, S), EmptyColor, SDPG_World);
					}
					continue;
				}

				// Color gradient: Green -> Yellow -> Orange -> Red by speed
				FColor Color = FColor::Green;
				if (Speed > 500.f) Color = FColor::Yellow;
				if (Speed > 1000.f) Color = FColor::Orange;
				if (Speed > 1500.f) Color = FColor::Red;

				// Arrow via PDI (consistent with rest of visualizer, works in all editor viewports)
				const FVector Dir = Vel / Speed;
				const float ArrowLen = FMath::Clamp(Speed * 0.1f, 5.f, MaxArrowLen);
				const FVector Tip = CellCenter + Dir * ArrowLen;
				PDI->DrawLine(CellCenter, Tip, Color, SDPG_World);

				// Arrowhead
				const float HeadLen = ArrowLen * 0.25f;
				FVector Perp, PerpUnused;
				Dir.FindBestAxisVectors(Perp, PerpUnused);
				PDI->DrawLine(Tip, Tip - Dir * HeadLen + Perp * HeadLen * 0.4f, Color, SDPG_World);
				PDI->DrawLine(Tip, Tip - Dir * HeadLen - Perp * HeadLen * 0.4f, Color, SDPG_World);
			}
		}
	}
}
