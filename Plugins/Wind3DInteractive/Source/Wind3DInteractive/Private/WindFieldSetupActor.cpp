#include "WindFieldSetupActor.h"
#include "WindSubsystem.h"
#include "Wind3DInteractiveModule.h"
#include "Components/BoxComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "EngineUtils.h"
#include "Kismet/KismetSystemLibrary.h"

AWindFieldSetupActor::AWindFieldSetupActor()
{
	PrimaryActorTick.bCanEverTick = false;

    PreviewBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PreviewBox"));
    RootComponent = PreviewBox;
    
    // Default box visualization
    PreviewBox->SetBoxExtent(FVector(1600.f, 1600.f, 800.f));
    PreviewBox->SetLineThickness(5.f);
    PreviewBox->ShapeColor = FColor::Cyan;
    PreviewBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWindFieldSetupActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    UpdatePreviewBox();
}

void AWindFieldSetupActor::UpdatePreviewBox()
{
    if (PreviewBox)
    {
        const float X = GridSizeX * CellSize * 0.5f;
        const float Y = GridSizeY * CellSize * 0.5f;
        const float Z = GridSizeZ * CellSize * 0.5f;
        PreviewBox->SetBoxExtent(FVector(X, Y, Z));
        
        // Offset box so pivot is at bottom-center or corner?
        // Let's keep pivot at center for now as it's easier to place
    }
}

void AWindFieldSetupActor::BeginPlay()
{
	Super::BeginPlay();

    UWorld* World = GetWorld();
    if (!World) return;

    UWindSubsystem* WindSys = World->GetSubsystem<UWindSubsystem>();
    if (WindSys)
    {
        // Calculate Origin based on Actor Location being the CENTER of the grid
        const FVector Extent = PreviewBox->GetScaledBoxExtent();
        const FVector Origin = GetActorLocation() - Extent;

        // Cascade settings (must be set BEFORE SetupWindGrid)
        WindSys->bUseCascade = bUseCascade;
        WindSys->NumCascadeLevels = NumCascadeLevels;

        WindSys->SetupWindGrid(Origin, CellSize, GridSizeX, GridSizeY, GridSizeZ);
        WindSys->SetAmbientWind(AmbientWind);

        // Pass simulation parameters
        WindSys->DiffusionRate = DiffusionRate;
        WindSys->AdvectionForce = AdvectionForce;
        WindSys->DecayRate = DecayRate;
        WindSys->DiffusionIterations = DiffusionIterations;
        WindSys->bForwardAdvection = bForwardAdvection;
        WindSys->BoundaryFadeCells = BoundaryFadeCells;
        WindSys->PressureIterations = PressureIterations;

        // World wind (rotating ambient)
        WindSys->bEnableWorldWind = bEnableWorldWind;
        WindSys->WorldWindSpeed = WorldWindSpeed;
        WindSys->WorldWindRotationSpeed = WorldWindRotationSpeed;
        WindSys->WorldWindNoiseAmplitude = WorldWindNoiseAmplitude;
        WindSys->WorldWindNoiseFrequency = WorldWindNoiseFrequency;
        WindSys->WorldWindSpeedNoiseAmplitude = WorldWindSpeedNoiseAmplitude;

        // Material integration
        WindSys->OverallPower = OverallPower;
        WindSys->bEnableMaterialIntegration = bEnableMaterialIntegration;

        // Obstacle detection
        WindSys->bEnableObstacles = bEnableObstacles;
        WindSys->ObstacleUpdateInterval = ObstacleUpdateInterval;
        WindSys->ObstacleChannel = ObstacleChannel;

        // Set grid center actor (for follow-player)
        if (AActor* Center = CenterActor.Get())
        {
            WindSys->SetGridCenterActor(Center);
        }

        if (bAutoStartDebug)
        {
            UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("Wind3D.ShowDebug 1"));
        }

        if (bAutoRegisterFoliage)
        {
            AutoRegisterFoliage(WindSys);
        }
    }
}

bool AWindFieldSetupActor::ShouldRegisterFoliageComponent(const UHierarchicalInstancedStaticMeshComponent* HISM) const
{
    if (!HISM) return false;
    if (HISM->GetInstanceCount() <= 0) return false;

    if (bRegisterStaticOnly && HISM->Mobility != EComponentMobility::Static)
    {
        return false;
    }

    if (!RequiredFoliageComponentTag.IsNone() && !HISM->ComponentHasTag(RequiredFoliageComponentTag))
    {
        return false;
    }

    return true;
}

bool AWindFieldSetupActor::IsInsideRegistrationRadius(const FVector& WorldLocation) const
{
    if (AutoRegisterRadius <= 0.f)
    {
        return true;
    }

    return FVector::DistSquared(WorldLocation, GetActorLocation()) <= FMath::Square(AutoRegisterRadius);
}

void AWindFieldSetupActor::AutoRegisterFoliage(UWindSubsystem* WindSys)
{
    if (!WindSys) return;

    UWorld* World = GetWorld();
    if (!World) return;

    TArray<UHierarchicalInstancedStaticMeshComponent*> CandidateComponents;
    if (FoliageSourceActors.Num() > 0)
    {
        for (const TSoftObjectPtr<AActor>& SourceActor : FoliageSourceActors)
        {
            AActor* Actor = SourceActor.Get();
            if (!Actor || Actor->GetWorld() != World)
            {
                continue;
            }

            TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> ActorHISMs(Actor);
            CandidateComponents.Append(ActorHISMs);
        }
    }
    else
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (!Actor || Actor == this)
            {
                continue;
            }

            TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> ActorHISMs(Actor);
            CandidateComponents.Append(ActorHISMs);
        }
    }

    TSet<UHierarchicalInstancedStaticMeshComponent*> UniqueComponents;
    int32 RegisteredCount = 0;
    int32 ScannedCount = 0;
    int32 SkippedFilter = 0;
    int32 SkippedRadius = 0;
    int32 SkippedCap = 0;
    bool bCapReached = false;

    for (UHierarchicalInstancedStaticMeshComponent* HISM : CandidateComponents)
    {
        if (!HISM || UniqueComponents.Contains(HISM))
        {
            continue;
        }
        UniqueComponents.Add(HISM);

        if (!ShouldRegisterFoliageComponent(HISM))
        {
            SkippedFilter += FMath::Max(HISM->GetInstanceCount(), 0);
            continue;
        }

        const int32 MinRequiredCustomData = FMath::Max(FoliageCPDSlotDisplace, FoliageCPDSlotTurbulence) + 1;
        if (HISM->NumCustomDataFloats < MinRequiredCustomData)
        {
            HISM->NumCustomDataFloats = MinRequiredCustomData;
            HISM->MarkRenderStateDirty();
        }

        const int32 InstanceCount = HISM->GetInstanceCount();
        for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
        {
            if (MaxAutoRegisterInstances > 0 && RegisteredCount >= MaxAutoRegisterInstances)
            {
                SkippedCap += (InstanceCount - InstanceIndex);
                bCapReached = true;
                break;
            }

            FTransform InstanceTransform;
            if (!HISM->GetInstanceTransform(InstanceIndex, InstanceTransform, true))
            {
                continue;
            }

            ++ScannedCount;
            const FVector WorldLocation = InstanceTransform.GetLocation();
            if (!IsInsideRegistrationRadius(WorldLocation))
            {
                ++SkippedRadius;
                continue;
            }

            const FWindEntityHandle Handle = WindSys->RegisterFoliageInstance(
                HISM,
                InstanceIndex,
                WorldLocation,
                FoliageSensitivity,
                FoliageStiffness,
                FoliageDamping,
                FoliageCPDSlotDisplace,
                FoliageCPDSlotTurbulence,
                FoliageMass,
                FoliageSpringConstant,
                FoliageDampingCoefficient,
                FoliageMaxVelocity,
                FoliageMaxAcceleration,
                FoliageRestDisplacement,
                FoliageWindFilterAlpha
            );

            if (Handle.IsValid())
            {
                ++RegisteredCount;
            }
        }

        if (bCapReached)
        {
            break;
        }
    }

    UE_LOG(LogWind3D, Log,
        TEXT("AutoRegisterFoliage: Registered=%d, Components=%d, Scanned=%d, Skipped(Filter=%d, Radius=%d, Cap=%d)"),
        RegisteredCount,
        UniqueComponents.Num(),
        ScannedCount,
        SkippedFilter,
        SkippedRadius,
        SkippedCap);
}
