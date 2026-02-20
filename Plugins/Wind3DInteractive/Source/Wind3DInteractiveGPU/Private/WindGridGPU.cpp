#include "WindGridGPU.h"
#include "WindComputeShaders.h"
#include "WindComponents.h"
#include "WindTypes.h"

#include "RHI.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "RHIGPUReadback.h"
#include "RenderingThread.h"
#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "RHIStaticStates.h"

// ============================================================
// IMPLEMENT_GLOBAL_SHADER — binds each C++ class to a .usf file
// ============================================================
IMPLEMENT_GLOBAL_SHADER(FWindDecayCS,       "/Wind3DInteractiveGPU/WindDecay.usf",       "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FWindDiffuseCS,     "/Wind3DInteractiveGPU/WindDiffuse.usf",     "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FWindAdvectCS,      "/Wind3DInteractiveGPU/WindAdvect.usf",      "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FWindInjectMotorCS, "/Wind3DInteractiveGPU/WindInjectMotor.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FWindPressureCS,    "/Wind3DInteractiveGPU/WindPressure.usf",    "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FWindBoundaryCS,    "/Wind3DInteractiveGPU/WindBoundary.usf",    "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FWindShiftCS,       "/Wind3DInteractiveGPU/WindShift.usf",       "MainCS", SF_Compute);

// ============================================================
// Internal helpers (render-thread only)
// ============================================================
namespace WindGPUHelper
{
	static FTextureRHIRef CreateVelocityTex(FRHICommandListBase& RHICmdList, const TCHAR* Name, int32 X, int32 Y, int32 Z)
	{
		FRHITextureCreateDesc Desc = FRHITextureCreateDesc::Create3D(Name, X, Y, Z, PF_FloatRGBA);
		Desc.AddFlags(ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV);
		return RHICmdList.CreateTexture(Desc);
	}

	static FTextureRHIRef CreatePressureTex(FRHICommandListBase& RHICmdList, const TCHAR* Name, int32 X, int32 Y, int32 Z)
	{
		FRHITextureCreateDesc Desc = FRHITextureCreateDesc::Create3D(Name, X, Y, Z, PF_R32_FLOAT);
		Desc.AddFlags(ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV);
		return RHICmdList.CreateTexture(Desc);
	}

	static FTextureRHIRef CreateSolidsTex(FRHICommandListBase& RHICmdList, const TCHAR* Name, int32 X, int32 Y, int32 Z)
	{
		// R32_UINT: written from CPU each frame via UpdateTexture3D
		FRHITextureCreateDesc Desc = FRHITextureCreateDesc::Create3D(Name, X, Y, Z, PF_R32_UINT);
		Desc.AddFlags(ETextureCreateFlags::ShaderResource | ETextureCreateFlags::Dynamic);
		return RHICmdList.CreateTexture(Desc);
	}

	static FUnorderedAccessViewRHIRef CreateUAV(FRHICommandListBase& RHICmdList, FTextureRHIRef Tex)
	{
		return RHICmdList.CreateUnorderedAccessView(Tex, FRHIViewDesc::CreateTextureUAV().SetDimensionFromTexture(Tex));
	}

	static FShaderResourceViewRHIRef CreateSRV(FRHICommandListBase& RHICmdList, FTextureRHIRef Tex)
	{
		return RHICmdList.CreateShaderResourceView(Tex, FRHIViewDesc::CreateTextureSRV().SetDimensionFromTexture(Tex));
	}

	static FShaderResourceViewRHIRef CreateSolidsSRV(FRHICommandListBase& RHICmdList, FTextureRHIRef Tex)
	{
		return RHICmdList.CreateShaderResourceView(Tex, FRHIViewDesc::CreateTextureSRV().SetDimensionFromTexture(Tex));
	}
} // namespace WindGPUHelper

// ============================================================
// Destructor
// ============================================================
FWindGridGPU::~FWindGridGPU()
{
	ReleaseGPUResources();
}

void FWindGridGPU::ReleaseGPUResources()
{
	FlushRenderingCommands();

	VelocityTexA.SafeRelease();   VelocityTexB.SafeRelease();
	VelocityUAV_A.SafeRelease();  VelocityUAV_B.SafeRelease();
	VelocitySRV_A.SafeRelease();  VelocitySRV_B.SafeRelease();

	PressureTexA.SafeRelease();   PressureTexB.SafeRelease();
	PressureUAV_A.SafeRelease();  PressureUAV_B.SafeRelease();
	PressureSRV_A.SafeRelease();  PressureSRV_B.SafeRelease();

	SolidsTex.SafeRelease();
	SolidsSRV.SafeRelease();

	MotorBuffer.SafeRelease();
	MotorSRV.SafeRelease();

	VelocityReadback.Reset();
	bGPUResourcesCreated = false;
}

// ============================================================
// Initialize
// ============================================================
void FWindGridGPU::Initialize(int32 InSizeX, int32 InSizeY, int32 InSizeZ, float InCellSize)
{
	if (bGPUResourcesCreated)
	{
		ReleaseGPUResources();
	}

	SizeX    = InSizeX;
	SizeY    = InSizeY;
	SizeZ    = InSizeZ;
	CellSize = InCellSize;

	const int32 Total = GetTotalCells();
	Solids.SetNumZeroed(Total);
	SolidsGPUData.SetNumZeroed(Total);

	{
		FScopeLock Lock(&ReadbackMutex);
		ReadbackVelocities.SetNumZeroed(Total);
		ReadbackTurbulences.SetNumZeroed(Total);
	}

	PendingMotors.Reserve(MaxMotors);

	ENQUEUE_RENDER_COMMAND(WindGPU_Initialize)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			using namespace WindGPUHelper;

			// Velocity textures (ping-pong)
			VelocityTexA  = CreateVelocityTex(RHICmdList, TEXT("WindVelA"), SizeX, SizeY, SizeZ);
			VelocityTexB  = CreateVelocityTex(RHICmdList, TEXT("WindVelB"), SizeX, SizeY, SizeZ);
			VelocityUAV_A = CreateUAV(RHICmdList, VelocityTexA);
			VelocityUAV_B = CreateUAV(RHICmdList, VelocityTexB);
			VelocitySRV_A = CreateSRV(RHICmdList, VelocityTexA);
			VelocitySRV_B = CreateSRV(RHICmdList, VelocityTexB);

			// Pressure textures (ping-pong for Jacobi)
			PressureTexA  = CreatePressureTex(RHICmdList, TEXT("WindPressA"), SizeX, SizeY, SizeZ);
			PressureTexB  = CreatePressureTex(RHICmdList, TEXT("WindPressB"), SizeX, SizeY, SizeZ);
			PressureUAV_A = CreateUAV(RHICmdList, PressureTexA);
			PressureUAV_B = CreateUAV(RHICmdList, PressureTexB);
			PressureSRV_A = CreateSRV(RHICmdList, PressureTexA);
			PressureSRV_B = CreateSRV(RHICmdList, PressureTexB);

			// Solids texture (R32_UINT, Dynamic = CPU-updated each frame)
			SolidsTex = CreateSolidsTex(RHICmdList, TEXT("WindSolids"), SizeX, SizeY, SizeZ);
			SolidsSRV = CreateSolidsSRV(RHICmdList, SolidsTex);

			// Motor StructuredBuffer (UE5.7 API: FRHIBufferCreateDesc)
			FRHIBufferCreateDesc BufDesc = FRHIBufferCreateDesc::CreateStructured(TEXT("WindMotorBuffer"), sizeof(FGPUMotorData) * MaxMotors, sizeof(FGPUMotorData)).AddUsage(BUF_Dynamic);
			MotorBuffer = RHICmdList.CreateBuffer(BufDesc);
			MotorSRV = RHICmdList.CreateShaderResourceView(MotorBuffer, FRHIViewDesc::CreateBufferSRV().SetTypeFromBuffer(MotorBuffer));

			VelocityReadback = MakeUnique<FRHIGPUTextureReadback>(TEXT("WindVelocityReadback"));

			bGPUResourcesCreated = true;
			bVelocityIsA = true;
			bPressureIsA = true;
		}
	);

	// Block only during initialization (not per-frame)
	FlushRenderingCommands();
}

// ============================================================
// Reset
// ============================================================
void FWindGridGPU::Reset()
{
	const int32 Total = GetTotalCells();
	FMemory::Memzero(Solids.GetData(), Total * sizeof(uint8));
	FMemory::Memzero(SolidsGPUData.GetData(), Total * sizeof(uint32));

	{
		FScopeLock Lock(&ReadbackMutex);
		FMemory::Memzero(ReadbackVelocities.GetData(), Total * sizeof(FVector));
		FMemory::Memzero(ReadbackTurbulences.GetData(), Total * sizeof(float));
	}
	PendingMotors.Reset();

	if (!bGPUResourcesCreated) return;

	ENQUEUE_RENDER_COMMAND(WindGPU_Reset)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.ClearUAVFloat(VelocityUAV_A, FVector4f(0.f, 0.f, 0.f, 0.f));
			RHICmdList.ClearUAVFloat(VelocityUAV_B, FVector4f(0.f, 0.f, 0.f, 0.f));
			RHICmdList.ClearUAVFloat(PressureUAV_A, FVector4f(0.f, 0.f, 0.f, 0.f));
			RHICmdList.ClearUAVFloat(PressureUAV_B, FVector4f(0.f, 0.f, 0.f, 0.f));
		}
	);
}

// ============================================================
// Obstacle Grid
// ============================================================
void FWindGridGPU::ClearSolids()
{
	FMemory::Memzero(Solids.GetData(), Solids.Num() * sizeof(uint8));
	FMemory::Memzero(SolidsGPUData.GetData(), SolidsGPUData.Num() * sizeof(uint32));
}

void FWindGridGPU::MarkSolid(int32 X, int32 Y, int32 Z)
{
	if (!IsInBounds(X, Y, Z)) return;
	const int32 Idx = CellIndex(X, Y, Z);
	Solids[Idx] = 1;
	SolidsGPUData[Idx] = 1;
}

bool FWindGridGPU::IsSolid(int32 X, int32 Y, int32 Z) const
{
	if (!IsInBounds(X, Y, Z)) return false;
	return Solids[CellIndex(X, Y, Z)] != 0;
}

// ============================================================
// InjectMotor — accumulate on game thread, flushed in Advect
// ============================================================
void FWindGridGPU::InjectMotor(const FWindMotorData& Motor, float DeltaTime)
{
	if (!Motor.bEnabled || PendingMotors.Num() >= MaxMotors) return;

	FGPUMotorData GPUMotor;
	// Explicit FVector (double) → FVector3f (float) casts
	GPUMotor.WorldPosition      = FVector3f(Motor.WorldPosition);
	GPUMotor.Strength           = Motor.Strength;
	GPUMotor.Direction          = FVector3f(Motor.Direction);
	GPUMotor.Radius             = Motor.Radius;
	GPUMotor.PreviousPosition   = FVector3f(Motor.PreviousPosition);
	GPUMotor.Height             = Motor.Height;
	GPUMotor.AngularVelocity    = FVector3f(Motor.AngularVelocity);
	GPUMotor.VortexAngularSpeed = Motor.VortexAngularSpeed;
	GPUMotor.InnerRadius        = Motor.InnerRadius;
	GPUMotor.TopRadius          = Motor.TopRadius;
	GPUMotor.ImpulseScale       = Motor.ImpulseScale;
	GPUMotor.MoveLength         = Motor.MoveLength;
	GPUMotor.Shape              = Motor.Shape;
	GPUMotor.EmissionType       = Motor.EmissionType;
	GPUMotor.bEnabled           = Motor.bEnabled;
	GPUMotor._pad               = 0;

	PendingMotors.Add(GPUMotor);
	PendingDeltaTime = DeltaTime;
}

// ============================================================
// DecayToAmbient — consume readback + upload solids + dispatch decay
// ============================================================
void FWindGridGPU::DecayToAmbient(FVector AmbientWind, float DecayRate, float DeltaTime)
{
	if (!bGPUResourcesCreated) return;

	// Try to consume last frame's async readback
	if (bReadbackPending)
	{
		FRHIGPUTextureReadback* ReadbackPtr = VelocityReadback.Get();
		const int32 SnapSizeX = SizeX;
		const int32 SnapSizeY = SizeY;
		const int32 SnapSizeZ = SizeZ;

		ENQUEUE_RENDER_COMMAND(WindGPU_ConsumeReadback)(
			[this, ReadbackPtr, SnapSizeX, SnapSizeY, SnapSizeZ](FRHICommandListImmediate& RHICmdList)
			{
				if (ReadbackPtr && ReadbackPtr->IsReady())
				{
					// RowPitchPixels and DepthPitchPixels account for GPU alignment padding
					int32 RowPitchPixels = 0;
					int32 DepthPitchPixels = 0;
					const FFloat16Color* Mapped = static_cast<const FFloat16Color*>(
						ReadbackPtr->Lock(RowPitchPixels, &DepthPitchPixels));

					if (Mapped)
					{
						FScopeLock Lock(&ReadbackMutex);
						for (int32 Z = 0; Z < SnapSizeZ; ++Z)
						{
							for (int32 Y = 0; Y < SnapSizeY; ++Y)
							{
								const FFloat16Color* Row = Mapped
									+ Z * DepthPitchPixels
									+ Y * RowPitchPixels;

								for (int32 X = 0; X < SnapSizeX; ++X)
								{
									const int32 CellIdx = X + Y * SnapSizeX + Z * SnapSizeX * SnapSizeY;
									ReadbackVelocities[CellIdx] = FVector(
										(float)Row[X].R,
										(float)Row[X].G,
										(float)Row[X].B
									);
									ReadbackTurbulences[CellIdx] = (float)Row[X].A;
								}
							}
						}
						ReadbackPtr->Unlock();
						bReadbackPending = false;
					}
				}
			}
		);
	}

	// Upload CPU solids to GPU texture
	TArray<uint32> SolidsSnapshot = SolidsGPUData;

	const FVector3f AmbientWind3f((float)AmbientWind.X, (float)AmbientWind.Y, (float)AmbientWind.Z);
	const float DecayFactor = FMath::Clamp(DecayRate * DeltaTime, 0.f, 1.f);
	const FIntVector GridSizeVec(SizeX, SizeY, SizeZ);
	const FIntVector Groups = GetDispatchGroups();

	ENQUEUE_RENDER_COMMAND(WindGPU_Decay)(
		[this, SolidsSnapshot = MoveTemp(SolidsSnapshot), AmbientWind3f, DecayFactor, GridSizeVec, Groups]
		(FRHICommandListImmediate& RHICmdList)
		{
			// Upload solids to GPU texture
			FUpdateTextureRegion3D Region;
			Region.SrcX   = Region.SrcY   = Region.SrcZ   = 0;
			Region.DestX  = Region.DestY  = Region.DestZ  = 0;
			Region.Width  = SizeX;
			Region.Height = SizeY;
			Region.Depth  = SizeZ;

			const uint32 RowPitchBytes   = SizeX * sizeof(uint32);
			const uint32 SlicePitchBytes = SizeX * SizeY * sizeof(uint32);

			RHICmdList.UpdateTexture3D(
				SolidsTex, 0, Region,
				RowPitchBytes, SlicePitchBytes,
				reinterpret_cast<const uint8*>(SolidsSnapshot.GetData())
			);
			// UpdateTexture3D handles its own state transition internally —
			// no explicit transition needed; SolidsTex is SRV-ready after the call.

			// Dispatch WindDecay.usf
			TShaderMapRef<FWindDecayCS> CS(GetGlobalShaderMap(ERHIFeatureLevel::SM5));

			FWindDecayCS::FParameters Params;
			Params.VelocityTex   = GetCurrentUAV();
			Params.SolidsTexture = SolidsSRV;
			Params.AmbientWind   = AmbientWind3f;
			Params.DecayFactor   = DecayFactor;
			Params.GridSize      = GridSizeVec;

			RHICmdList.Transition(FRHITransitionInfo(GetCurrentUAV(), ERHIAccess::SRVCompute, ERHIAccess::UAVCompute));
			FComputeShaderUtils::Dispatch(RHICmdList, CS, Params, Groups);
			RHICmdList.Transition(FRHITransitionInfo(GetCurrentUAV(), ERHIAccess::UAVCompute, ERHIAccess::SRVCompute));
		}
	);
}

// ============================================================
// Advect — flush motors + dispatch InjectMotor + Advect shaders
// ============================================================
void FWindGridGPU::Advect(float AdvectionForce, float DeltaTime, bool bForwardPass)
{
	if (!bGPUResourcesCreated) return;

	TArray<FGPUMotorData> MotorsCopy = MoveTemp(PendingMotors);
	PendingMotors.Reset();

	const float Factor = (DeltaTime > 0.f)
		? (AdvectionForce * DeltaTime / FMath::Max(CellSize, 1.f))
		: 0.f;

	const FVector3f Origin3f((float)WorldOrigin.X, (float)WorldOrigin.Y, (float)WorldOrigin.Z);
	const FIntVector GridSizeVec(SizeX, SizeY, SizeZ);
	const FIntVector Groups = GetDispatchGroups();
	const float InternalDT = (PendingDeltaTime > 0.f) ? PendingDeltaTime : DeltaTime;
	const float CellSizeCopy = CellSize;

	ENQUEUE_RENDER_COMMAND(WindGPU_InjectAndAdvect)(
		[this, MotorsCopy = MoveTemp(MotorsCopy), Factor, Origin3f, GridSizeVec, Groups, InternalDT, CellSizeCopy]
		(FRHICommandListImmediate& RHICmdList)
		{
			const int32 MotorCount = FMath::Min(MotorsCopy.Num(), MaxMotors);

			if (MotorCount > 0)
			{
				// Upload motor data (UE5.7: RHICmdList.LockBuffer / UnlockBuffer)
				const uint32 LockSize = sizeof(FGPUMotorData) * MotorCount;
				void* BufData = RHICmdList.LockBuffer(MotorBuffer, 0, LockSize, RLM_WriteOnly);
				FMemory::Memcpy(BufData, MotorsCopy.GetData(), LockSize);
				RHICmdList.UnlockBuffer(MotorBuffer);

				// Dispatch WindInjectMotor.usf
				TShaderMapRef<FWindInjectMotorCS> InjectCS(GetGlobalShaderMap(ERHIFeatureLevel::SM5));

				FWindInjectMotorCS::FParameters InjectParams;
				InjectParams.VelocityTex    = GetCurrentUAV();
				InjectParams.SolidsTexture  = SolidsSRV;
				InjectParams.Motors         = MotorSRV;
				InjectParams.MotorCount     = MotorCount;
				InjectParams.GridSize       = GridSizeVec;
				InjectParams.GridOrigin     = Origin3f;
				InjectParams.GridCellSize   = CellSizeCopy;
				InjectParams.DeltaTime      = InternalDT;

				RHICmdList.Transition(FRHITransitionInfo(GetCurrentUAV(), ERHIAccess::SRVCompute, ERHIAccess::UAVCompute));
				FComputeShaderUtils::Dispatch(RHICmdList, InjectCS, InjectParams, Groups);
				RHICmdList.Transition(FRHITransitionInfo(GetCurrentUAV(), ERHIAccess::UAVCompute, ERHIAccess::SRVCompute));
			}

			// Dispatch WindAdvect.usf (semi-Lagrangian reverse)
			if (Factor > 0.f)
			{
				TShaderMapRef<FWindAdvectCS> AdvectCS(GetGlobalShaderMap(ERHIFeatureLevel::SM5));

				FWindAdvectCS::FParameters AdvectParams;
				AdvectParams.VelocityIn    = GetCurrentSRV();
				AdvectParams.VelocityOut   = GetScratchUAV();
				AdvectParams.SolidsTexture = SolidsSRV;
				AdvectParams.LinearSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				AdvectParams.Factor        = Factor;
				AdvectParams.GridSize      = GridSizeVec;

				RHICmdList.Transition(FRHITransitionInfo(GetScratchUAV(), ERHIAccess::SRVCompute, ERHIAccess::UAVCompute));
				FComputeShaderUtils::Dispatch(RHICmdList, AdvectCS, AdvectParams, Groups);
				RHICmdList.Transition(FRHITransitionInfo(GetScratchUAV(), ERHIAccess::UAVCompute, ERHIAccess::SRVCompute));

				SwapVelocityBuffers();
			}
		}
	);
}

// ============================================================
// Diffuse — N-pass ping-pong Laplacian blur
// ============================================================
void FWindGridGPU::Diffuse(float DiffusionRate, float DeltaTime, int32 Iterations)
{
	if (!bGPUResourcesCreated || Iterations <= 0) return;

	const float Alpha = DiffusionRate * DeltaTime;
	const FIntVector GridSizeVec(SizeX, SizeY, SizeZ);
	const FIntVector Groups = GetDispatchGroups();

	ENQUEUE_RENDER_COMMAND(WindGPU_Diffuse)(
		[this, Alpha, GridSizeVec, Groups, Iterations](FRHICommandListImmediate& RHICmdList)
		{
			TShaderMapRef<FWindDiffuseCS> CS(GetGlobalShaderMap(ERHIFeatureLevel::SM5));

			for (int32 Pass = 0; Pass < Iterations; Pass++)
			{
				FWindDiffuseCS::FParameters Params;
				Params.VelocityIn    = GetCurrentSRV();
				Params.VelocityOut   = GetScratchUAV();
				Params.SolidsTexture = SolidsSRV;
				Params.Alpha         = Alpha;
				Params.GridSize      = GridSizeVec;

				RHICmdList.Transition(FRHITransitionInfo(GetScratchUAV(), ERHIAccess::SRVCompute, ERHIAccess::UAVCompute));
				FComputeShaderUtils::Dispatch(RHICmdList, CS, Params, Groups);
				RHICmdList.Transition(FRHITransitionInfo(GetScratchUAV(), ERHIAccess::UAVCompute, ERHIAccess::SRVCompute));

				SwapVelocityBuffers();
			}
		}
	);
}

// ============================================================
// ProjectPressure — Jacobi + gradient subtraction
// ============================================================
void FWindGridGPU::ProjectPressure(int32 Iterations)
{
	if (!bGPUResourcesCreated || Iterations <= 0) return;

	const FIntVector GridSizeVec(SizeX, SizeY, SizeZ);
	const FIntVector Groups = GetDispatchGroups();

	ENQUEUE_RENDER_COMMAND(WindGPU_Pressure)(
		[this, Iterations, GridSizeVec, Groups](FRHICommandListImmediate& RHICmdList)
		{
			TShaderMapRef<FWindPressureCS> CS(GetGlobalShaderMap(ERHIFeatureLevel::SM5));

			RHICmdList.ClearUAVFloat(PressureUAV_A, FVector4f(0.f, 0.f, 0.f, 0.f));
			RHICmdList.ClearUAVFloat(PressureUAV_B, FVector4f(0.f, 0.f, 0.f, 0.f));
			bPressureIsA = true;

			// Jacobi iterations (ping-pong pressure A ↔ B)
			for (int32 Iter = 0; Iter < Iterations; Iter++)
			{
				FWindPressureCS::FParameters Params;
				Params.PressureOut   = GetScratchPressureUAV();
				Params.PressureIn    = GetCurrentPressureSRV();
				Params.VelocityTex   = GetCurrentSRV();
				Params.SolidsTexture = SolidsSRV;
				Params.VelocityOut   = GetScratchUAV(); // not used in pass 0, but must be valid
				Params.GridSize      = GridSizeVec;
				Params.PassType      = 0;

				RHICmdList.Transition(FRHITransitionInfo(GetScratchPressureUAV(), ERHIAccess::SRVCompute, ERHIAccess::UAVCompute));
				FComputeShaderUtils::Dispatch(RHICmdList, CS, Params, Groups);
				RHICmdList.Transition(FRHITransitionInfo(GetScratchPressureUAV(), ERHIAccess::UAVCompute, ERHIAccess::SRVCompute));

				SwapPressureBuffers();
			}

			// Gradient subtraction — write divergence-free velocity to scratch
			{
				FWindPressureCS::FParameters GradParams;
				GradParams.PressureOut   = GetScratchPressureUAV(); // dummy write in pass 1
				GradParams.PressureIn    = GetCurrentPressureSRV();
				GradParams.VelocityTex   = GetCurrentSRV();
				GradParams.SolidsTexture = SolidsSRV;
				GradParams.VelocityOut   = GetScratchUAV();
				GradParams.GridSize      = GridSizeVec;
				GradParams.PassType      = 1;

				RHICmdList.Transition(FRHITransitionInfo(GetScratchUAV(), ERHIAccess::SRVCompute, ERHIAccess::UAVCompute));
				FComputeShaderUtils::Dispatch(RHICmdList, CS, GradParams, Groups);
				RHICmdList.Transition(FRHITransitionInfo(GetScratchUAV(), ERHIAccess::UAVCompute, ERHIAccess::SRVCompute));

				SwapVelocityBuffers();
			}
		}
	);
}

// ============================================================
// BoundaryFadeOut — smooth edges + kick async readback
// ============================================================
void FWindGridGPU::BoundaryFadeOut(int32 FadeCells)
{
	if (!bGPUResourcesCreated) return;

	const FIntVector GridSizeVec(SizeX, SizeY, SizeZ);
	const FIntVector Groups = GetDispatchGroups();

	// Mark pending on the game thread before enqueueing so DecayToAmbient
	// sees it as true even if the render command hasn't executed yet.
	if (VelocityReadback.IsValid())
	{
		bReadbackPending = true;
	}

	ENQUEUE_RENDER_COMMAND(WindGPU_BoundaryAndReadback)(
		[this, FadeCells, GridSizeVec, Groups](FRHICommandListImmediate& RHICmdList)
		{
			if (FadeCells > 0)
			{
				TShaderMapRef<FWindBoundaryCS> CS(GetGlobalShaderMap(ERHIFeatureLevel::SM5));

				FWindBoundaryCS::FParameters Params;
				Params.VelocityTex = GetCurrentUAV();
				Params.GridSize    = GridSizeVec;
				Params.FadeCells   = FadeCells;

				RHICmdList.Transition(FRHITransitionInfo(GetCurrentUAV(), ERHIAccess::SRVCompute, ERHIAccess::UAVCompute));
				FComputeShaderUtils::Dispatch(RHICmdList, CS, Params, Groups);
				RHICmdList.Transition(FRHITransitionInfo(GetCurrentUAV(), ERHIAccess::UAVCompute, ERHIAccess::SRVCompute));
			}

			// Kick async readback for CPU queries next frame
			if (VelocityReadback.IsValid())
			{
				RHICmdList.Transition(FRHITransitionInfo(GetCurrentTex(), ERHIAccess::SRVCompute, ERHIAccess::CopySrc));
				VelocityReadback->EnqueueCopy(RHICmdList, GetCurrentTex());
				RHICmdList.Transition(FRHITransitionInfo(GetCurrentTex(), ERHIAccess::CopySrc, ERHIAccess::SRVCompute));
			}
		}
	);
}

// ============================================================
// ShiftData — integer cell shift for infinite-scrolling grid
// ============================================================
void FWindGridGPU::ShiftData(FIntVector CellOffset, FVector AmbientWind)
{
	if (!bGPUResourcesCreated) return;
	if (CellOffset.X == 0 && CellOffset.Y == 0 && CellOffset.Z == 0) return;

	const FVector3f Ambient3f((float)AmbientWind.X, (float)AmbientWind.Y, (float)AmbientWind.Z);
	const FIntVector GridSizeVec(SizeX, SizeY, SizeZ);
	const FIntVector Groups = GetDispatchGroups();

	ENQUEUE_RENDER_COMMAND(WindGPU_Shift)(
		[this, CellOffset, Ambient3f, GridSizeVec, Groups](FRHICommandListImmediate& RHICmdList)
		{
			TShaderMapRef<FWindShiftCS> CS(GetGlobalShaderMap(ERHIFeatureLevel::SM5));

			FWindShiftCS::FParameters Params;
			Params.VelocityIn  = GetCurrentSRV();
			Params.VelocityOut = GetScratchUAV();
			Params.CellOffset  = CellOffset;
			Params.GridSize    = GridSizeVec;
			Params.AmbientWind = Ambient3f;

			RHICmdList.Transition(FRHITransitionInfo(GetScratchUAV(), ERHIAccess::SRVCompute, ERHIAccess::UAVCompute));
			FComputeShaderUtils::Dispatch(RHICmdList, CS, Params, Groups);
			RHICmdList.Transition(FRHITransitionInfo(GetScratchUAV(), ERHIAccess::UAVCompute, ERHIAccess::SRVCompute));

			SwapVelocityBuffers();
		}
	);
}

// ============================================================
// CPU Query — trilinear interpolation from readback buffer
// ============================================================
FVector FWindGridGPU::SampleVelocityAt(const FVector& WorldPos) const
{
	const FVector Local = (WorldPos - WorldOrigin) / CellSize - FVector(0.5);
	return SampleVelocityAtLocal((float)Local.X, (float)Local.Y, (float)Local.Z);
}

FVector FWindGridGPU::SampleVelocityAtLocal(float Lx, float Ly, float Lz) const
{
	const int32 X0 = FMath::FloorToInt(Lx);
	const int32 Y0 = FMath::FloorToInt(Ly);
	const int32 Z0 = FMath::FloorToInt(Lz);

	const float Fx = Lx - X0;
	const float Fy = Ly - Y0;
	const float Fz = Lz - Z0;

	FVector Result = FVector::ZeroVector;

	FScopeLock Lock(&ReadbackMutex);
	for (int32 Dz = 0; Dz <= 1; Dz++)
	{
		for (int32 Dy = 0; Dy <= 1; Dy++)
		{
			for (int32 Dx = 0; Dx <= 1; Dx++)
			{
				const int32 Cx = FMath::Clamp(X0 + Dx, 0, SizeX - 1);
				const int32 Cy = FMath::Clamp(Y0 + Dy, 0, SizeY - 1);
				const int32 Cz = FMath::Clamp(Z0 + Dz, 0, SizeZ - 1);

				const float Wx = Dx ? Fx : (1.f - Fx);
				const float Wy = Dy ? Fy : (1.f - Fy);
				const float Wz = Dz ? Fz : (1.f - Fz);

				Result += ReadbackVelocities[CellIndex(Cx, Cy, Cz)] * Wx * Wy * Wz;
			}
		}
	}
	return Result;
}

float FWindGridGPU::SampleTurbulenceAt(const FVector& WorldPos) const
{
	const int32 Idx = WorldToIndex(WorldPos);
	if (Idx < 0) return 0.f;
	FScopeLock Lock(&ReadbackMutex);
	return ReadbackTurbulences[Idx];
}

const TArray<FVector>& FWindGridGPU::GetVelocities() const
{
	return ReadbackVelocities;
}

const TArray<float>& FWindGridGPU::GetTurbulences() const
{
	return ReadbackTurbulences;
}

// ============================================================
// Coordinate Helpers
// ============================================================
int32 FWindGridGPU::CellIndex(int32 X, int32 Y, int32 Z) const
{
	return X + Y * SizeX + Z * SizeX * SizeY;
}

bool FWindGridGPU::IsInBounds(int32 X, int32 Y, int32 Z) const
{
	return X >= 0 && X < SizeX && Y >= 0 && Y < SizeY && Z >= 0 && Z < SizeZ;
}

FIntVector FWindGridGPU::WorldToCell(const FVector& WorldPos) const
{
	const FVector Local = WorldPos - WorldOrigin;
	return FIntVector(
		FMath::FloorToInt((float)(Local.X / CellSize)),
		FMath::FloorToInt((float)(Local.Y / CellSize)),
		FMath::FloorToInt((float)(Local.Z / CellSize))
	);
}

int32 FWindGridGPU::WorldToIndex(const FVector& WorldPos) const
{
	const FIntVector Cell = WorldToCell(WorldPos);
	if (!IsInBounds(Cell.X, Cell.Y, Cell.Z)) return -1;
	return CellIndex(Cell.X, Cell.Y, Cell.Z);
}

FVector FWindGridGPU::CellToWorld(int32 X, int32 Y, int32 Z) const
{
	return WorldOrigin + FVector(
		(X + 0.5f) * CellSize,
		(Y + 0.5f) * CellSize,
		(Z + 0.5f) * CellSize
	);
}
