#include "WindFieldSetupActor.h"
#include "WindSubsystem.h"
#include "Components/BoxComponent.h"
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

        WindSys->SetupWindGrid(Origin, CellSize, GridSizeX, GridSizeY, GridSizeZ);
        WindSys->SetAmbientWind(AmbientWind);

        if (bAutoStartDebug)
        {
            // Set console variable
            UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("Wind3D.ShowDebug 1"));
        }
    }
}
