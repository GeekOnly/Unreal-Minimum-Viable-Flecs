#include "WindSubsystem.h"
#include "WindCascade.h"
#include "WindComponents.h"
#include "Wind3DInteractiveModule.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"

static TAutoConsoleVariable<int32> CVarShowWindDebug(
	TEXT("Wind3D.ShowDebug"),
	0,
	TEXT("Visualize wind 3D grid with debug arrows.\n")
	TEXT("0: Off\n")
	TEXT("1: On"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarShowWindTrailDebug(
	TEXT("Wind3D.ShowTrailDebug"),
	1,
	TEXT("Visualize Moving motor trail segments when Wind3D.ShowDebug is enabled.\n")
	TEXT("0: Off\n")
	TEXT("1: On"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarShowWindObstacleNormals(
	TEXT("Wind3D.ShowObstacleNormals"),
	1,
	TEXT("Visualize obstacle normals used for wind deflection when Wind3D.ShowDebug is enabled.\n")
	TEXT("0: Off\n")
	TEXT("1: On"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarUseGPUWind(
	TEXT("Wind3D.UseGPU"),
	0,
	TEXT("Select wind simulation solver.\n")
	TEXT("0: CPU (default, always available)\n")
	TEXT("1: GPU Compute Shaders (requires Wind3DInteractiveGPU module)"),
	ECVF_Default
);

static TAutoConsoleVariable<int32> CVarFoliagePreset(
	TEXT("Wind3D.FoliagePreset"),
	1,
	TEXT("Foliage bend response preset.\n")
	TEXT("0: Custom (use Wind3D.FoliageAttackScale / ReleaseScale / LeanScale)\n")
	TEXT("1: Natural (lean slower, recover faster)\n")
	TEXT("2: Storm (lean faster, recover slower)"),
	ECVF_Default
);

static TAutoConsoleVariable<float> CVarFoliageAttackScale(
	TEXT("Wind3D.FoliageAttackScale"),
	1.0f,
	TEXT("Runtime scale for foliage lean-in speed (attack). Used directly when Wind3D.FoliagePreset=0."),
	ECVF_Default
);

static TAutoConsoleVariable<float> CVarFoliageReleaseScale(
	TEXT("Wind3D.FoliageReleaseScale"),
	1.0f,
	TEXT("Runtime scale for foliage recover speed (release). Used directly when Wind3D.FoliagePreset=0."),
	ECVF_Default
);

static TAutoConsoleVariable<float> CVarFoliageLeanScale(
	TEXT("Wind3D.FoliageLeanScale"),
	1.0f,
	TEXT("Runtime scale for foliage lean amount. Used directly when Wind3D.FoliagePreset=0."),
	ECVF_Default
);

// ---- Helper: create solver based on current CVar ----
static TUniquePtr<IWindSolver> CreateSolver()
{
	if (CVarUseGPUWind.GetValueOnGameThread() != 0 && FWindSolverFactory::CreateGPU.IsBound())
	{
		UE_LOG(LogWind3D, Log, TEXT("Wind3D: Using GPU compute solver"));
		return FWindSolverFactory::CreateGPU.Execute();
	}
	check(FWindSolverFactory::CreateCPU.IsBound());
	UE_LOG(LogWind3D, Log, TEXT("Wind3D: Using CPU solver"));
	return FWindSolverFactory::CreateCPU.Execute();
}

// ... (Top of file remains)

void UWindSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    // Note: UTickableWorldSubsystem automatically registers for Tick.
    // We don't need manual Ticker delegates anymore.

	char Name[] = {"Wind3D Interactive"};
	char* Argv = Name;
	ECSWorld = MakeUnique<flecs::world>(1, &Argv);

	// Flecs Explorer: uncomment when needed (requires available port 27750)
	// GetEcsWorld()->import<flecs::monitor>();
	// GetEcsWorld()->set<flecs::Rest>({});

	// Create solver via factory (GPU if Wind3D.UseGPU 1, otherwise CPU)
	Solver = CreateSolver();
	// Default grid matches WindFieldSetupActor defaults (16x16x8)
	Solver->Initialize(16, 16, 8, 200.f);
	
	// Initialize texture manager with grid dimensions
	TextureManager.Initialize(GetWorld(), Solver->GetSizeX(), Solver->GetSizeY(), Solver->GetSizeZ());
	bTextureManagerInitialized = true;

	RegisterComponents();
	RegisterSystems();

	UE_LOG(LogWind3D, Log, TEXT("UWindSubsystem initialized (World: %s). Grid: %dx%dx%d"),
        GetWorld() ? *GetWorld()->GetName() : TEXT("Unknown"),
		Solver->GetSizeX(), Solver->GetSizeY(), Solver->GetSizeZ());

	Super::Initialize(Collection);
}

void UWindSubsystem::Deinitialize()
{
	TextureManager.Shutdown();
	bTextureManagerInitialized = false;

	if (Cascade.IsValid())
	{
		Cascade->Shutdown();
		Cascade.Reset();
	}

	ECSWorld.Reset();

	UE_LOG(LogWind3D, Log, TEXT("UWindSubsystem deinitialized."));
	Super::Deinitialize();
}

bool UWindSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}


void UWindSubsystem::RegisterComponents()
{
	if (!ECSWorld) return;

	ECSWorld->component<FWindMotorData>();
	ECSWorld->component<FWindGridComponent>();
	ECSWorld->component<FFoliageWorldPosition>();
	ECSWorld->component<FWindVelocityAtEntity>();
	ECSWorld->component<FFoliageInstanceData>();
	ECSWorld->component<FWindReceiver>();
	ECSWorld->component<FFoliageTag>();
}

void UWindSubsystem::RegisterSystems()
{
	if (!ECSWorld) return;

	// Establish Grid Singleton
	flecs::entity GridEntity = ECSWorld->entity("WindGridSingleton");
	GridEntity.set<FWindGridComponent>({Solver.Get()});

	// System: Inject Wind Motors into Grid (runs every frame)
	ECSWorld->system<const FWindMotorData>("InjectWindMotors")
		.iter([this](flecs::iter& It, const FWindMotorData* Motors)
		{
			const float DT = It.delta_time();
			const bool bCasc = bUseCascade && Cascade.IsValid() && Cascade->IsInitialized();
			for (auto I : It)
			{
				if (bCasc)
				{
					Cascade->InjectMotor(Motors[I], DT);
				}
				else
				{
					Solver->InjectMotor(Motors[I], DT);
				}
			}
		});

	// System: Advection via Flecs Singleton
	ECSWorld->system<const FWindGridComponent>("WindAdvectionSystem")
		.iter([this](flecs::iter& It, const FWindGridComponent* GridComp)
		{
			if (!GridComp) return;
			const float DT = It.delta_time();
			const bool bCasc = bUseCascade && Cascade.IsValid() && Cascade->IsInitialized();
			if (bCasc)
			{
				Cascade->Advect(AdvectionForce, DT, bForwardAdvection);
			}
			else
			{
				for (auto I : It)
				{
					if (GridComp[I].SolverPtr)
					{
						GridComp[I].SolverPtr->Advect(AdvectionForce, DT, bForwardAdvection);
					}
				}
			}
		});

	// System: Sample wind velocity at foliage world positions
	ECSWorld->system<const FFoliageWorldPosition, FWindVelocityAtEntity>("SampleWindForFoliage")
		.iter([this](flecs::iter& It, const FFoliageWorldPosition* Pos, FWindVelocityAtEntity* WindAt)
		{
			const bool bCasc = bUseCascade && Cascade.IsValid() && Cascade->IsInitialized();
			for (auto I : It)
			{
				if (bCasc)
				{
					WindAt[I].Velocity = Cascade->SampleVelocityAt(Pos[I].Location);
					WindAt[I].Turbulence = Cascade->SampleTurbulenceAt(Pos[I].Location);
				}
				else
				{
					WindAt[I].Velocity = Solver->SampleVelocityAt(Pos[I].Location);
					WindAt[I].Turbulence = Solver->SampleTurbulenceAt(Pos[I].Location);
				}
			}
		});

	// System: Spring-damper foliage response — writes CustomPrimitiveData to HISM instances
	ECSWorld->system<FWindReceiver, const FWindVelocityAtEntity, const FFoliageInstanceData>("FoliageSpringResponse")
		.iter([this](flecs::iter& It, FWindReceiver* Recv, const FWindVelocityAtEntity* WindAt, const FFoliageInstanceData* Foliage)
		{
			const float DT = It.delta_time();

			const int32 Preset = CVarFoliagePreset.GetValueOnGameThread();
			float AttackScale = FMath::Max(CVarFoliageAttackScale.GetValueOnGameThread(), 0.05f);
			float ReleaseScale = FMath::Max(CVarFoliageReleaseScale.GetValueOnGameThread(), 0.05f);
			float LeanScale = FMath::Max(CVarFoliageLeanScale.GetValueOnGameThread(), 0.05f);

			if (Preset == 1) // Natural
			{
				AttackScale = 0.65f;
				ReleaseScale = 1.35f;
				LeanScale = 0.95f;
			}
			else if (Preset == 2) // Storm
			{
				AttackScale = 2.2f;
				ReleaseScale = 0.45f;
				LeanScale = 1.55f;
			}

			for (auto I : It)
			{
				// Drive foliage toward a bend target from wind speed (attack/release),
				// which creates sustained leaning instead of springy jitter.
				const float WindSpeed = WindAt[I].Velocity.Size();
				const float WindNorm = FMath::Clamp(WindSpeed / FWindTextureManager::MaxWindSpeed, 0.f, 1.f);

				const float DynamicLean = FMath::Lerp(1.f, 1.25f, WindNorm) * LeanScale;
				const float TargetBend = FMath::Pow(WindNorm, 0.65f) * Recv[I].Sensitivity * 1.1f * DynamicLean;
				const float MaxBend = FMath::Max(Recv[I].Sensitivity * 1.8f * LeanScale, 0.2f);
				const float ClampedTarget = FMath::Clamp(TargetBend, 0.f, MaxBend);

				const float AttackRate = FMath::Max(Recv[I].StiffnessK * 0.9f * AttackScale, 0.25f);
				const float ReleaseRate = FMath::Max(Recv[I].DampingC * 0.8f * ReleaseScale, 0.2f);
				const float BlendRate = (ClampedTarget >= Recv[I].DisplacementCurrent) ? AttackRate : ReleaseRate;
				const float Alpha = 1.f - FMath::Exp(-BlendRate * DT);

				const float PrevDisplacement = Recv[I].DisplacementCurrent;
				Recv[I].DisplacementCurrent = FMath::Lerp(Recv[I].DisplacementCurrent, ClampedTarget, Alpha);
				Recv[I].DisplacementVelocity = (DT > SMALL_NUMBER)
					? (Recv[I].DisplacementCurrent - PrevDisplacement) / DT
					: 0.f;

				const float OutputDisplacement = Recv[I].DisplacementCurrent;
				const float OutputTurbulence = WindAt[I].Turbulence * FMath::Lerp(1.f, 0.35f, WindNorm);

				auto* HISM = static_cast<UHierarchicalInstancedStaticMeshComponent*>(Foliage[I].HISMComponentPtr);
				if (HISM && Foliage[I].InstanceIndex >= 0)
				{
					HISM->SetCustomDataValue(Foliage[I].InstanceIndex, Foliage[I].CPDSlotDisplace, OutputDisplacement);
					HISM->SetCustomDataValue(Foliage[I].InstanceIndex, Foliage[I].CPDSlotTurb, OutputTurbulence);
					DirtyHISMs.Add(HISM);
				}
			}
		});
}

void UWindSubsystem::Tick(float DeltaTime)
{
	static float TickTimer = 0.f;
	TickTimer += DeltaTime;
	if (TickTimer > 5.0f)
	{
		TickTimer = 0.f;
		const int32 CVarVal = CVarShowWindDebug.GetValueOnGameThread();
		const UWorld* World = GetWorld();
		UE_LOG(LogWind3D, Log, TEXT("WindSubsystem Tick Heartbeat: CVar=%d, World=%s, Time=%f"),
			CVarVal, World ? *World->GetName() : TEXT("None"), World ? World->GetTimeSeconds() : 0.f);
	}

	if (ECSWorld)
	{
		DirtyHISMs.Reset();

		const bool bCascadeActive = bUseCascade && Cascade.IsValid() && Cascade->IsInitialized();

		// === GoW Wind Simulation Pipeline ===

		// 0. World Wind — rotate ambient direction with noise
		UpdateWorldWind(DeltaTime);

		// 1. Grid Offset — shift data when center actor moves
		UpdateGridOffset();

		// 1.5 Occupancy — mark cells blocked by collision
		UpdateOccupancy();

		// 2. Decay toward ambient (now with rotating direction)
		if (bCascadeActive)
		{
			Cascade->DecayToAmbient(AmbientWind, DecayRate, DeltaTime);
		}
		else
		{
			Solver->DecayToAmbient(AmbientWind, DecayRate, DeltaTime);
		}

		// 2.5 Ensure Flecs Singleton has valid pointer (in case SetupWindGrid re-created Solver)
		flecs::entity GridEntity = ECSWorld->entity("WindGridSingleton");
		GridEntity.set<FWindGridComponent>({&GetSolver()});

		// 3. Motor Injection & Advection (ECS pipeline)
		ECSWorld->progress(DeltaTime);

		// 4. Diffusion — 3D blur to spread wind naturally (multi-pass)
		if (bCascadeActive)
		{
			Cascade->Diffuse(DiffusionRate, DeltaTime, DiffusionIterations);
		}
		else
		{
			Solver->Diffuse(DiffusionRate, DeltaTime, DiffusionIterations);
		}

		// 5. Advection is now handled inside Flecs progress() above.

		// 5.5. Pressure projection — make velocity divergence-free
		if (PressureIterations > 0)
		{
			if (bCascadeActive)
			{
				Cascade->ProjectPressure(PressureIterations);
			}
			else
			{
				Solver->ProjectPressure(PressureIterations);
			}
		}

		// 5.75. Boundary fade-out — prevent hard cutoff at grid edges
		if (BoundaryFadeCells > 0)
		{
			if (bCascadeActive)
			{
				Cascade->BoundaryFadeOut(BoundaryFadeCells);
			}
			else
			{
				Solver->BoundaryFadeOut(BoundaryFadeCells);
			}
		}

		// 6. Material Integration — update texture + MPC (uses finest cascade)
		if (bEnableMaterialIntegration)
		{
			const IWindSolver& PrimarySolver = GetSolver();
			if (!bTextureManagerInitialized)
			{
				TextureManager.Initialize(GetWorld(), PrimarySolver.GetSizeX(), PrimarySolver.GetSizeY(), PrimarySolver.GetSizeZ());
				bTextureManagerInitialized = true;
			}
			TextureManager.SetVizZSlice(WindVizZSlice);
			TextureManager.UpdateFromGrid(PrimarySolver, AmbientWind, OverallPower);
		}

		// Batch mark render state dirty for all affected HISMs
		for (auto* HISM : DirtyHISMs)
		{
			if (HISM) HISM->MarkRenderStateDirty();
		}
	}

	if (CVarShowWindDebug.GetValueOnGameThread() > 0)
	{
		DrawDebugWind();
	}
}

TStatId UWindSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWindSubsystem, STATGROUP_Tickables);
}

IWindSolver& UWindSubsystem::GetSolver() const
{
	if (Cascade.IsValid() && Cascade->IsInitialized())
	{
		return Cascade->GetFinestSolver();
	}
	check(Solver.IsValid());
	return *Solver;
}

void UWindSubsystem::SetupWindGrid(FVector Origin, float CellSize, int32 InSizeX, int32 InSizeY, int32 InSizeZ)
{
    UE_LOG(LogWind3D, Log, TEXT("SetupWindGrid Called: Origin=%s, CellSize=%f, Size=(%d, %d, %d)"),
        *Origin.ToString(), CellSize, InSizeX, InSizeY, InSizeZ);

	if (bUseCascade)
	{
		// Build cascade configs from the single-grid params as base
		TArray<FWindCascadeConfig> Configs;
		const float BaseCellSize = FMath::Max(CellSize, 10.f);

		for (int32 i = 0; i < NumCascadeLevels; i++)
		{
			FWindCascadeConfig Cfg;
			const float Scale = FMath::Pow(2.f, static_cast<float>(i));
			Cfg.CellSize = BaseCellSize * Scale;
			// Finer levels get more cells, coarser levels keep base size
			const float InvScale = 1.f / Scale;
			Cfg.SizeX = FMath::Clamp(FMath::CeilToInt(InSizeX * 2.f * InvScale), 8, 128);
			Cfg.SizeY = FMath::Clamp(FMath::CeilToInt(InSizeY * 2.f * InvScale), 8, 128);
			Cfg.SizeZ = FMath::Clamp(InSizeZ, 4, 64);
			Cfg.BlendCells = FMath::Max(2, 4 - i);
			Configs.Add(Cfg);
		}

		SetupWindCascade(Configs);

		// Set origin centered
		const FVector HalfExtent = FVector(
			InSizeX * CellSize * 0.5f,
			InSizeY * CellSize * 0.5f,
			InSizeZ * CellSize * 0.5f);
		Cascade->SetWorldOriginCentered(Origin + HalfExtent);
		return;
	}

	// Single-grid mode (legacy)
	Solver = CreateSolver();
	Solver->SetWorldOrigin(Origin);
	Solver->Initialize(
		FMath::Clamp(InSizeX, 1, 128),
		FMath::Clamp(InSizeY, 1, 128),
		FMath::Clamp(InSizeZ, 1, 64),
		FMath::Max(CellSize, 10.f));

	// Reinitialize texture manager for new grid dimensions
	if (bTextureManagerInitialized)
	{
		TextureManager.Shutdown();
		bTextureManagerInitialized = false;
	}
	TextureManager.Initialize(GetWorld(), Solver->GetSizeX(), Solver->GetSizeY(), Solver->GetSizeZ());
	bTextureManagerInitialized = true;
}

void UWindSubsystem::SetupWindCascade(const TArray<FWindCascadeConfig>& Configs)
{
	const bool bUseGPU = CVarUseGPUWind.GetValueOnGameThread() != 0;

	if (!Cascade.IsValid())
	{
		Cascade = MakeUnique<FWindCascade>();
	}
	Cascade->Initialize(Configs, bUseGPU);

	// Texture manager uses finest cascade for material integration
	const IWindSolver& Finest = Cascade->GetFinestSolver();
	if (bTextureManagerInitialized)
	{
		TextureManager.Shutdown();
		bTextureManagerInitialized = false;
	}
	TextureManager.Initialize(GetWorld(), Finest.GetSizeX(), Finest.GetSizeY(), Finest.GetSizeZ());
	bTextureManagerInitialized = true;

	UE_LOG(LogWind3D, Log, TEXT("SetupWindCascade: %d levels, finest=%dx%dx%d @ %.0f cm"),
		Configs.Num(), Finest.GetSizeX(), Finest.GetSizeY(), Finest.GetSizeZ(), Finest.GetCellSize());
}

void UWindSubsystem::SetAmbientWind(FVector AmbientVelocity)
{
	AmbientWind = AmbientVelocity;
}

void UWindSubsystem::UpdateWorldWind(float DeltaTime)
{
	if (!bEnableWorldWind) return;

	WorldWindTime += DeltaTime;

	// Base rotation around Z-axis
	const float BaseAngleDeg = WorldWindRotationSpeed * WorldWindTime;

	// Perlin Noise for Direction (more organic than sine waves)
	// Sample noise at (Time * Freq, 0, 0)
	const float NoiseTime = WorldWindTime * WorldWindNoiseFrequency;
	const float DirNoise = FMath::PerlinNoise3D(FVector(NoiseTime, 0.f, 0.f));
	
	// Map noise [-1, 1] to angle deviation
	const float NoiseAngle = DirNoise * WorldWindNoiseAmplitude;

	const float FinalAngleDeg = BaseAngleDeg + NoiseAngle;
	const float AngleRad = FMath::DegreesToRadians(FinalAngleDeg);

	// Perlin Noise for Speed Gusts
	// Offset sample position to uncorrelate from direction noise
	const float GustNoise = FMath::PerlinNoise3D(FVector(NoiseTime * GustFrequency, 100.f, 0.f));
	
	// Map noise [-1, 1] to speed variation
	// Gusts are usually additive (0 to 1 range effectively)
	const float SpeedVariation = (GustNoise + 0.2f) * GustSpeed; // +0.2 bias to make gusts more frequent than lulls
	const float FinalSpeed = FMath::Max(WorldWindSpeed + SpeedVariation, 0.f);

	// Compute direction on XY plane (rotating around Z)
	AmbientWind = FVector(
		FMath::Cos(AngleRad) * FinalSpeed,
		FMath::Sin(AngleRad) * FinalSpeed,
		0.f
	);
}

void UWindSubsystem::SetGridCenterActor(AActor* Actor)
{
	GridCenterActor = Actor;
	bGridCenterInitialized = false;
	if (Actor)
	{
		UE_LOG(LogWind3D, Log, TEXT("Grid center actor set to: %s"), *Actor->GetName());
	}
}

void UWindSubsystem::UpdateGridOffset()
{
	if (!GridCenterActor.IsValid()) return;
	if (!Solver.IsValid() && !(bUseCascade && Cascade.IsValid() && Cascade->IsInitialized())) return;

	const FVector CurrentCenter = GridCenterActor->GetActorLocation();

	auto CenterSolverOnActor = [](IWindSolver& InSolver, const FVector& Center)
	{
		const float Cell = InSolver.GetCellSize();
		const FVector HalfExtent(
			InSolver.GetSizeX() * Cell * 0.5f,
			InSolver.GetSizeY() * Cell * 0.5f,
			InSolver.GetSizeZ() * Cell * 0.5f);
		InSolver.SetWorldOrigin(Center - HalfExtent);
	};

	if (!bGridCenterInitialized)
	{
		PreviousGridCenter = CurrentCenter;
		bGridCenterInitialized = true;

		// Initialize cascade origins centered on player
		if (bUseCascade && Cascade.IsValid() && Cascade->IsInitialized())
		{
			Cascade->SetWorldOriginCentered(CurrentCenter);
		}
		else
		{
			CenterSolverOnActor(*Solver, CurrentCenter);
		}
		return;
	}

	const FVector Delta = CurrentCenter - PreviousGridCenter;

	if (bUseCascade && Cascade.IsValid() && Cascade->IsInitialized())
	{
		// Cascade handles per-level shifting internally
		Cascade->ShiftAllGrids(Delta, AmbientWind);
		PreviousGridCenter = CurrentCenter;
		return;
	}

	// Single-grid mode (legacy)
	const float CS = Solver->GetCellSize();
	const FIntVector CellOffset(
		FMath::FloorToInt(Delta.X / CS),
		FMath::FloorToInt(Delta.Y / CS),
		FMath::FloorToInt(Delta.Z / CS)
	);

	if (CellOffset.X == 0 && CellOffset.Y == 0 && CellOffset.Z == 0) return;

	Solver->ShiftData(FIntVector(-CellOffset.X, -CellOffset.Y, -CellOffset.Z), AmbientWind);

	const FVector OriginShift(
		CellOffset.X * CS,
		CellOffset.Y * CS,
		CellOffset.Z * CS
	);
	Solver->SetWorldOrigin(Solver->GetWorldOrigin() + OriginShift);

	PreviousGridCenter += OriginShift;
}

void UWindSubsystem::UpdateOccupancy()
{
	if (!bEnableObstacles) return;

	// Amortize: only update every N frames
	if (++ObstacleFrameCounter < ObstacleUpdateInterval) return;
	ObstacleFrameCounter = 0;

	UWorld* World = GetWorld();
	if (!World) return;

	// Helper lambda: rasterize obstacles into a single solver
	auto RasterizeObstacles = [&](IWindSolver& S)
	{
		S.ClearSolids();

		const FVector GridMin = S.GetWorldOrigin();
		const float CS = S.GetCellSize();
		const FVector GridExtent = FVector(
			S.GetSizeX() * CS,
			S.GetSizeY() * CS,
			S.GetSizeZ() * CS);
		const FVector GridCenter = GridMin + GridExtent * 0.5f;

		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(WindOccupancy), false);
		World->OverlapMultiByChannel(
			Overlaps, GridCenter, FQuat::Identity, ObstacleChannel,
			FCollisionShape::MakeBox(GridExtent * 0.5f), Params);

		for (const FOverlapResult& Result : Overlaps)
		{
			UPrimitiveComponent* Comp = Result.GetComponent();
			if (!Comp) continue;

			const FBox Bounds = Comp->Bounds.GetBox();
			FIntVector MinCell = S.WorldToCell(Bounds.Min);
			FIntVector MaxCell = S.WorldToCell(Bounds.Max);

			MinCell.X = FMath::Clamp(MinCell.X, 0, S.GetSizeX() - 1);
			MinCell.Y = FMath::Clamp(MinCell.Y, 0, S.GetSizeY() - 1);
			MinCell.Z = FMath::Clamp(MinCell.Z, 0, S.GetSizeZ() - 1);
			MaxCell.X = FMath::Clamp(MaxCell.X, 0, S.GetSizeX() - 1);
			MaxCell.Y = FMath::Clamp(MaxCell.Y, 0, S.GetSizeY() - 1);
			MaxCell.Z = FMath::Clamp(MaxCell.Z, 0, S.GetSizeZ() - 1);

			for (int32 Z = MinCell.Z; Z <= MaxCell.Z; Z++)
				for (int32 Y = MinCell.Y; Y <= MaxCell.Y; Y++)
					for (int32 X = MinCell.X; X <= MaxCell.X; X++)
						S.MarkSolid(X, Y, Z);
		}
	};

	if (bUseCascade && Cascade.IsValid() && Cascade->IsInitialized())
	{
		for (int32 i = 0; i < Cascade->GetNumLevels(); i++)
		{
			RasterizeObstacles(*Cascade->GetLevel(i).Solver);
		}
	}
	else
	{
		RasterizeObstacles(*Solver);
	}
}

FWindEntityHandle UWindSubsystem::RegisterWindMotor(
	FVector Position,
	FVector Direction,
	float Strength,
	float Radius,
	EWindMotorShape Shape,
	EWindEmissionType EmissionType)
{
	if (!ECSWorld) return FWindEntityHandle();

	const FWindMotorData InitData = {
		.WorldPosition = Position,
		.Direction = Direction.GetSafeNormal(),
		.Strength = Strength,
		.Radius = Radius,
		.InnerRadius = 0.f,
		.Height = 1000.f,
		.VortexAngularSpeed = 1.f,
		.PreviousPosition = Position, // Initialize to current pos — avoids ZeroVector capsule bug on first frame
		.Shape = static_cast<uint8>(Shape),
		.EmissionType = static_cast<uint8>(EmissionType),
		.bEnabled = 1
	};

	flecs::entity Entity;

	// Reuse pooled entity if available
	{
		FScopeLock Lock(&MotorPoolMutex);
		if (MotorPool.Num() > 0)
		{
			Entity = MotorPool.Pop(EAllowShrinking::No);
		}
	}

	if (Entity.is_valid())
	{
		Entity.set<FWindMotorData>(InitData);
		UE_LOG(LogWind3D, Verbose, TEXT("Reused pooled Wind Motor Entity %llu"), Entity.id());
	}
	else
	{
		Entity = ECSWorld->entity().set<FWindMotorData>(InitData);
		UE_LOG(LogWind3D, Verbose, TEXT("Created new Wind Motor Entity %llu"), Entity.id());
	}

	return FWindEntityHandle(Entity.id());
}

void UWindSubsystem::UpdateWindMotor(
	FWindEntityHandle Handle,
	FVector Position,
	FVector Direction,
	float Strength,
	float Radius,
	EWindMotorShape Shape,
	EWindEmissionType EmissionType,
	float Height,
	float InnerRadius,
	float VortexAngularSpeed,
	bool bEnabled,
	float TopRadius,
	float MoveLength,
	float ImpulseScale,
	FVector AngularVelocity)
{
	if (!ECSWorld || !Handle.IsValid()) return;

	flecs::entity Entity = ECSWorld->entity(Handle.EntityId);
	if (Entity.is_valid())
	{
		FWindMotorData* Data = Entity.get_mut<FWindMotorData>();
		if (Data)
		{
			// Store previous position before updating (for Moving motor)
			Data->PreviousPosition = Data->WorldPosition;

			Data->WorldPosition = Position;
			Data->Direction = Direction.GetSafeNormal();
			Data->Strength = Strength;
			Data->Radius = Radius;
			Data->InnerRadius = InnerRadius;
			Data->Height = Height;
			Data->VortexAngularSpeed = VortexAngularSpeed;
			Data->TopRadius = TopRadius;
			Data->MoveLength = MoveLength;
			Data->ImpulseScale = ImpulseScale;
			Data->AngularVelocity = AngularVelocity;
			Data->Shape = static_cast<uint8>(Shape);
			Data->EmissionType = static_cast<uint8>(EmissionType);
			Data->bEnabled = bEnabled ? 1 : 0;
		}
	}
}

void UWindSubsystem::UnregisterWindMotor(FWindEntityHandle Handle)
{
	if (!ECSWorld || !Handle.IsValid()) return;

	flecs::entity Entity = ECSWorld->entity(Handle.EntityId);
	if (Entity.is_valid())
	{
		// Pool instead of destroy: disable and stash for reuse
		FWindMotorData* Data = Entity.get_mut<FWindMotorData>();
		if (Data)
		{
			Data->bEnabled = 0;
			Data->Strength = 0.f;
		}
		Entity.remove<FWindMotorData>();
		{
			FScopeLock Lock(&MotorPoolMutex);
			if (MotorPool.Num() < MaxMotorPoolSize)
			{
				MotorPool.Add(Entity);
			}
			else
			{
				Entity.destruct();
			}
		}
		UE_LOG(LogWind3D, Verbose, TEXT("Pooled Wind Motor Entity %llu (pool size: %d)"), Handle.EntityId, MotorPool.Num());
	}
}

FWindEntityHandle UWindSubsystem::RegisterFoliageInstance(
	UHierarchicalInstancedStaticMeshComponent* HISM,
	int32 InstanceIndex,
	FVector WorldLocation,
	float Sensitivity,
	float Stiffness,
	float Damping,
	int32 CPDSlotDisplace,
	int32 CPDSlotTurbulence)
{
	if (!ECSWorld || !HISM) return FWindEntityHandle();

	flecs::entity E = ECSWorld->entity()
		.set<FFoliageWorldPosition>({WorldLocation})
		.set<FWindReceiver>({Sensitivity, 0.f, 0.f, Stiffness, Damping})
		.set<FWindVelocityAtEntity>({FVector::ZeroVector, 0.f})
		.set<FFoliageInstanceData>({HISM, InstanceIndex, CPDSlotDisplace, CPDSlotTurbulence})
		.add<FFoliageTag>();

	return FWindEntityHandle(E.id());
}

FVector UWindSubsystem::QueryWindAtPosition(FVector WorldPosition) const
{
	if (bUseCascade && Cascade.IsValid() && Cascade->IsInitialized())
	{
		return Cascade->SampleVelocityAt(WorldPosition) * OverallPower;
	}
	return Solver->SampleVelocityAt(WorldPosition) * OverallPower;
}

void UWindSubsystem::DrawDebugWind()
{
	UWorld* World = GetWorld();
	if (!World) return;

	const bool bDrawTrailDebug = CVarShowWindTrailDebug.GetValueOnGameThread() != 0;
	const bool bDrawObstacleNormals = CVarShowWindObstacleNormals.GetValueOnGameThread() != 0;

	// Color per cascade level for bounding box
	static const FColor CascadeColors[] = { FColor::Cyan, FColor::Green, FColor::Yellow, FColor::Orange };

	auto DrawSolverDebug = [this, World, bDrawObstacleNormals](const IWindSolver& S, const FColor& BoxColor, float BoxThickness)
	{
		const TArray<FVector>& Vels = S.GetVelocities();
		const int32 TotalCells = Vels.Num();
		if (TotalCells == 0) return;

		const FVector GridMin = S.GetWorldOrigin();
		const FVector GridMax = GridMin + FVector(
			S.GetSizeX() * S.GetCellSize(),
			S.GetSizeY() * S.GetCellSize(),
			S.GetSizeZ() * S.GetCellSize());
		const FVector GridCenter = (GridMin + GridMax) * 0.5f;
		const FVector GridExtent = (GridMax - GridMin) * 0.5f;

		DrawDebugBox(GetWorld(), GridCenter, GridExtent, BoxColor, false, 0.f, 0, BoxThickness);
		DrawDebugPoint(GetWorld(), S.GetWorldOrigin(), 20.f, FColor::Magenta, false, 0.f, 0);

		const float AmbientSpeed = AmbientWind.Size();
		const float ExcessThreshold = FMath::Max(AmbientSpeed * 0.3f, 30.f);
		const int32 Step = FMath::Max(1, FMath::CeilToInt(FMath::Pow((float)TotalCells / 2000.f, 1.f / 3.f)));

		const auto IsSolidSafe = [&S](int32 X, int32 Y, int32 Z) -> bool
		{
			return S.IsInBounds(X, Y, Z) && S.IsSolid(X, Y, Z);
		};

		for (int32 Z = 0; Z < S.GetSizeZ(); Z += Step)
		{
			for (int32 Y = 0; Y < S.GetSizeY(); Y += Step)
			{
				for (int32 X = 0; X < S.GetSizeX(); X += Step)
				{
					const int32 Idx = S.CellIndex(X, Y, Z);
					const FVector CellCenter = S.CellToWorld(X, Y, Z);

					if (S.IsSolid(X, Y, Z))
					{
						DrawDebugPoint(GetWorld(), CellCenter, 8.f, FColor::Red, false, 0.f, 0);
						continue;
					}

					DrawDebugPoint(GetWorld(), CellCenter, 4.f, FColor(40, 40, 40), false, 0.f, 0);

					if (bDrawObstacleNormals)
					{
						const FVector SolidNormal(
							(IsSolidSafe(X - 1, Y, Z) ? 1.f : 0.f) - (IsSolidSafe(X + 1, Y, Z) ? 1.f : 0.f),
							(IsSolidSafe(X, Y - 1, Z) ? 1.f : 0.f) - (IsSolidSafe(X, Y + 1, Z) ? 1.f : 0.f),
							(IsSolidSafe(X, Y, Z - 1) ? 1.f : 0.f) - (IsSolidSafe(X, Y, Z + 1) ? 1.f : 0.f)
						);

						if (!SolidNormal.IsNearlyZero())
						{
							const FVector N = SolidNormal.GetSafeNormal();
							const float NormalLen = S.GetCellSize() * 0.35f;
							DrawDebugDirectionalArrow(World, CellCenter,
								CellCenter + N * NormalLen,
								14.f, FColor(255, 120, 0), false, 0.f, 0, 2.5f);
						}
					}

					const FVector& Velocity = Vels[Idx];
					const float Speed = Velocity.Size();
					if (Speed < 1.f) continue;

					const FVector VelNorm = Velocity.GetSafeNormal();
					const float BaseLen = S.GetCellSize() * 0.3f;
					DrawDebugDirectionalArrow(GetWorld(), CellCenter,
						CellCenter + VelNorm * BaseLen,
						15.f, FColor(50, 80, 50), false, 0.f, 0, 1.5f);

					const float DeviationSpeed = (Velocity - AmbientWind).Size();
					if (DeviationSpeed > ExcessThreshold)
					{
						const float ArrowLen = FMath::Clamp(DeviationSpeed * 0.25f, 20.f, S.GetCellSize() * 0.9f);

						FColor Color = FColor::Cyan;
						if (DeviationSpeed > 200.f) Color = FColor::Yellow;
						if (DeviationSpeed > 500.f) Color = FColor(255, 140, 0);
						if (DeviationSpeed > 900.f) Color = FColor::Red;

						DrawDebugDirectionalArrow(GetWorld(), CellCenter,
							CellCenter + VelNorm * ArrowLen,
							50.f, Color, false, 0.f, 0, 5.f);
					}
				}
			}
		}
	};

	if (bUseCascade && Cascade.IsValid() && Cascade->IsInitialized())
	{
		for (int32 i = 0; i < Cascade->GetNumLevels(); i++)
		{
			const FColor& BoxCol = CascadeColors[FMath::Min(i, 3)];
			DrawSolverDebug(*Cascade->GetLevel(i).Solver, BoxCol, 5.f - i * 1.f);
		}
	}
	else
	{
		DrawSolverDebug(*Solver, FColor::Cyan, 5.f);
	}

	if (bDrawTrailDebug && ECSWorld)
	{
		ECSWorld->each<FWindMotorData>([World](const FWindMotorData& Motor)
		{
			if (!Motor.bEnabled) return;

			const EWindEmissionType EmissionType = static_cast<EWindEmissionType>(Motor.EmissionType);
			if (EmissionType != EWindEmissionType::Moving) return;

			const FVector MoveDelta = Motor.WorldPosition - Motor.PreviousPosition;
			const float MoveDistance = MoveDelta.Size();
			const FVector MoveDir = MoveDistance > SMALL_NUMBER
				? MoveDelta / MoveDistance
				: (Motor.Direction.IsNearlyZero() ? FVector::ForwardVector : Motor.Direction.GetSafeNormal());
			const float TrailLength = FMath::Max(Motor.MoveLength, MoveDistance);
			const FVector TrailEnd = Motor.WorldPosition;
			const FVector TrailStart = TrailEnd - MoveDir * TrailLength;

			DrawDebugLine(World, TrailStart, TrailEnd, FColor(170, 0, 255), false, 0.f, 0, 4.f);

			const float MarkerRadius = FMath::Clamp(Motor.Radius * 0.1f, 8.f, 60.f);
			DrawDebugSphere(World, TrailStart, MarkerRadius, 10, FColor::Purple, false, 0.f, 0, 1.5f);
			DrawDebugSphere(World, TrailEnd, MarkerRadius, 10, FColor::Cyan, false, 0.f, 0, 1.5f);

			DrawDebugDirectionalArrow(World, TrailEnd,
				TrailEnd + MoveDir * FMath::Max(Motor.Radius * 0.75f, 20.f),
				18.f, FColor::Cyan, false, 0.f, 0, 2.5f);
		});
	}

	static int32 LogCounter = 0;
	if (LogCounter++ % 300 == 0)
	{
		const FVector GridMin = GetSolver().GetWorldOrigin();
		const FVector GridMax = GridMin + FVector(
			GetSolver().GetSizeX() * GetSolver().GetCellSize(),
			GetSolver().GetSizeY() * GetSolver().GetCellSize(),
			GetSolver().GetSizeZ() * GetSolver().GetCellSize());
		UE_LOG(LogWind3D, Log, TEXT("DrawDebugWind: Primary bounds Min=%s, Max=%s, Cascade=%d"),
			*GridMin.ToString(), *GridMax.ToString(),
			bUseCascade && Cascade.IsValid() ? Cascade->GetNumLevels() : 0);
	}
}

// --- Audio Integration ---

float UWindSubsystem::GetWindIntensityAtPosition(FVector WorldPosition) const
{
	FVector Vel;
	if (bUseCascade && Cascade.IsValid() && Cascade->IsInitialized())
	{
		Vel = Cascade->SampleVelocityAt(WorldPosition);
	}
	else
	{
		Vel = Solver->SampleVelocityAt(WorldPosition);
	}
	return FMath::Clamp(Vel.Size() / FWindTextureManager::MaxWindSpeed, 0.f, 1.f);
}

float UWindSubsystem::GetWindTurbulenceAtPosition(FVector WorldPosition) const
{
	if (bUseCascade && Cascade.IsValid() && Cascade->IsInitialized())
	{
		return Cascade->SampleTurbulenceAt(WorldPosition);
	}
	return Solver->SampleTurbulenceAt(WorldPosition);
}

// --- Material Integration ---

UVolumeTexture* UWindSubsystem::GetWindVolumeTexture() const
{
	return TextureManager.GetWindVolumeTexture();
}

UTexture2D* UWindSubsystem::GetWindVizTexture() const
{
	return TextureManager.GetWindVizTexture();
}

UMaterialParameterCollection* UWindSubsystem::GetWindMPC() const
{
	return TextureManager.GetWindMPC();
}

void UWindSubsystem::BindWindToMaterial(UMaterialInstanceDynamic* MID, FName TextureParamName)
{
	TextureManager.BindToMaterial(MID, TextureParamName);
}

UMaterialInstanceDynamic* UWindSubsystem::ApplyWindToComponent(
	UPrimitiveComponent* Component,
	UMaterialInterface* WindMaterial,
	int32 MaterialIndex)
{
	if (!Component || !WindMaterial) return nullptr;
	if (MaterialIndex >= Component->GetNumMaterials()) return nullptr;

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(WindMaterial, Component);
	if (!MID) return nullptr;

	Component->SetMaterial(MaterialIndex, MID);
	BindWindToMaterial(MID, FName(TEXT("WindVolume")));

	UE_LOG(LogWind3D, Log, TEXT("ApplyWindToComponent: %s slot %d -> MID bound"),
		*Component->GetName(), MaterialIndex);

	return MID;
}
