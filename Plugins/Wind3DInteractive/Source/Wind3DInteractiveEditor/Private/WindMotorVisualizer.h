#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

class FWindMotorVisualizer : public FComponentVisualizer
{
public:
	virtual void DrawVisualization(
		const UActorComponent* Component,
		const FSceneView* View,
		FPrimitiveDrawInterface* PDI) override;

private:
	void DrawSphereShape(const FTransform& T, float Radius, FPrimitiveDrawInterface* PDI) const;
	void DrawCylinderShape(const FTransform& T, float Radius, float Height, FPrimitiveDrawInterface* PDI) const;
	void DrawConeShape(const FTransform& T, float Radius, float Height, FPrimitiveDrawInterface* PDI) const;
	void DrawEmissionArrows(const FTransform& T, uint8 EmissionType, float Radius, float Strength, FPrimitiveDrawInterface* PDI) const;
	void DrawWindFieldDebug(UWorld* World, const FSceneView* View, FPrimitiveDrawInterface* PDI, int32 ShowLevel) const;
	void DrawGridBounds(const struct IWindSolver& Grid, FPrimitiveDrawInterface* PDI) const;
};
