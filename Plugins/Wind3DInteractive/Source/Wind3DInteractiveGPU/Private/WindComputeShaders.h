#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RHI.h"
#include "RHICommandList.h"

// ============================================================
// GPU-side motor data is now defined in WindGridGPU.h
// ============================================================

// ============================================================
// FWindDecayCS — lerp velocities toward ambient wind
// Shader: /Wind3DInteractiveGPU/WindDecay.usf  MainCS
// ============================================================
class FWindDecayCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FWindDecayCS);
	SHADER_USE_PARAMETER_STRUCT(FWindDecayCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWTexture3D<float4>, VelocityTex)
		SHADER_PARAMETER_SRV(Texture3D<uint>,     SolidsTexture)
		SHADER_PARAMETER(FVector3f,               AmbientWind)
		SHADER_PARAMETER(float,                   DecayFactor)
		SHADER_PARAMETER(float,                   TurbDecayFactor)
		SHADER_PARAMETER(FIntVector,              GridSize)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// ============================================================
// FWindDiffuseCS — 6-neighbor Laplacian diffusion (ping-pong)
// Shader: /Wind3DInteractiveGPU/WindDiffuse.usf  MainCS
// ============================================================
class FWindDiffuseCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FWindDiffuseCS);
	SHADER_USE_PARAMETER_STRUCT(FWindDiffuseCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWTexture3D<float4>, VelocityOut)
		SHADER_PARAMETER_SRV(Texture3D<float4>,   VelocityIn)
		SHADER_PARAMETER_SRV(Texture3D<uint>,     SolidsTexture)
		SHADER_PARAMETER(float,                   Alpha)
		SHADER_PARAMETER(FIntVector,              GridSize)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// ============================================================
// FWindAdvectCS — semi-Lagrangian reverse advection
// Shader: /Wind3DInteractiveGPU/WindAdvect.usf  MainCS
// ============================================================
class FWindAdvectCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FWindAdvectCS);
	SHADER_USE_PARAMETER_STRUCT(FWindAdvectCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWTexture3D<float4>, VelocityOut)
		SHADER_PARAMETER_SRV(Texture3D<float4>,   VelocityIn)
		SHADER_PARAMETER_SRV(Texture3D<uint>,     SolidsTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState,    LinearSampler)
		SHADER_PARAMETER(float,                   Factor)
		SHADER_PARAMETER(FIntVector,              GridSize)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// ============================================================
// FWindInjectMotorCS — inject motor forces into velocity grid
// Shader: /Wind3DInteractiveGPU/WindInjectMotor.usf  MainCS
// ============================================================
class FWindInjectMotorCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FWindInjectMotorCS);
	SHADER_USE_PARAMETER_STRUCT(FWindInjectMotorCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWTexture3D<float4>,           VelocityTex)
		SHADER_PARAMETER_SRV(Texture3D<uint>,               SolidsTexture)
		SHADER_PARAMETER_SRV(StructuredBuffer<FGPUMotorData>, Motors)
		SHADER_PARAMETER(int32,                             MotorCount)
		SHADER_PARAMETER(FIntVector,                        GridSize)
		SHADER_PARAMETER(FVector3f,                         GridOrigin)
		SHADER_PARAMETER(float,                             GridCellSize)
		SHADER_PARAMETER(float,                             DeltaTime)
		SHADER_PARAMETER(float,                             MaxSpeed)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// ============================================================
// FWindPressureCS — Jacobi pressure iteration + gradient subtraction
// Shader: /Wind3DInteractiveGPU/WindPressure.usf  MainCS
// Uses PassType permutation: 0 = Jacobi, 1 = GradientSubtract
// ============================================================
class FWindPressureCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FWindPressureCS);
	SHADER_USE_PARAMETER_STRUCT(FWindPressureCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWTexture3D<float>,  PressureOut)
		SHADER_PARAMETER_SRV(Texture3D<float>,    PressureIn)
		SHADER_PARAMETER_SRV(Texture3D<float4>,   VelocityTex)
		SHADER_PARAMETER_SRV(Texture3D<uint>,     SolidsTexture)
		SHADER_PARAMETER_UAV(RWTexture3D<float4>, VelocityOut)
		SHADER_PARAMETER(FIntVector,              GridSize)
		SHADER_PARAMETER(uint32,                  PassType)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// ============================================================
// FWindBoundaryCS — smooth fade-out at grid boundary
// Shader: /Wind3DInteractiveGPU/WindBoundary.usf  MainCS
// ============================================================
class FWindBoundaryCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FWindBoundaryCS);
	SHADER_USE_PARAMETER_STRUCT(FWindBoundaryCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWTexture3D<float4>, VelocityTex)
		SHADER_PARAMETER(FIntVector,              GridSize)
		SHADER_PARAMETER(int32,                   FadeCells)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// ============================================================
// FWindShiftCS — shift grid data by integer cell offset
// Shader: /Wind3DInteractiveGPU/WindShift.usf  MainCS
// ============================================================
class FWindShiftCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FWindShiftCS);
	SHADER_USE_PARAMETER_STRUCT(FWindShiftCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(Texture3D<float4>,   VelocityIn)
		SHADER_PARAMETER_UAV(RWTexture3D<float4>, VelocityOut)
		SHADER_PARAMETER(FIntVector,              CellOffset)
		SHADER_PARAMETER(FIntVector,              GridSize)
		SHADER_PARAMETER(FVector3f,               AmbientWind)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
