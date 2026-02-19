#include "WindTextureManager.h"
#include "IWindSolver.h"
#include "Wind3DInteractiveModule.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Materials/MaterialInstanceDynamic.h"

FWindTextureManager::FWindTextureManager()
{
}

FWindTextureManager::~FWindTextureManager()
{
	Shutdown();
}

void FWindTextureManager::Initialize(UWorld* World, int32 SizeX, int32 SizeY, int32 SizeZ)
{
	if (bInitialized) Shutdown();

	GridSizeX = SizeX;
	GridSizeY = SizeY;
	GridSizeZ = SizeZ;
	AtlasWidth = SizeX * SizeZ;
	AtlasHeight = SizeY;
	CachedWorld = World;

	StagingBuffer.SetNumZeroed(AtlasWidth * AtlasHeight);

	CreateAtlasTexture();
	CreateMPC();

	bInitialized = true;

	UE_LOG(LogWind3D, Log, TEXT("WindTextureManager initialized: Atlas %dx%d (Grid %dx%dx%d)"),
		AtlasWidth, AtlasHeight, GridSizeX, GridSizeY, GridSizeZ);
}

void FWindTextureManager::Shutdown()
{
	if (!bInitialized) return;

	if (WindAtlasTexture)
	{
		WindAtlasTexture->RemoveFromRoot();
		WindAtlasTexture = nullptr;
	}

	if (WindMPC)
	{
		WindMPC->RemoveFromRoot();
		WindMPC = nullptr;
	}

	StagingBuffer.Empty();
	bInitialized = false;

	UE_LOG(LogWind3D, Log, TEXT("WindTextureManager shut down."));
}

void FWindTextureManager::CreateAtlasTexture()
{
	WindAtlasTexture = UTexture2D::CreateTransient(AtlasWidth, AtlasHeight, PF_FloatRGBA);
	if (!WindAtlasTexture)
	{
		UE_LOG(LogWind3D, Error, TEXT("Failed to create wind atlas texture!"));
		return;
	}

	WindAtlasTexture->AddToRoot();
	WindAtlasTexture->Filter = TF_Bilinear;
	WindAtlasTexture->AddressX = TA_Clamp;
	WindAtlasTexture->AddressY = TA_Clamp;
	WindAtlasTexture->SRGB = 0;
	WindAtlasTexture->CompressionSettings = TC_HDR;
	WindAtlasTexture->MipGenSettings = TMGS_NoMipmaps;

	// Initialize mip data with zeros
	FTexture2DMipMap& Mip = WindAtlasTexture->GetPlatformData()->Mips[0];
	void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memzero(TextureData, AtlasWidth * AtlasHeight * sizeof(FFloat16Color));
	Mip.BulkData.Unlock();

	WindAtlasTexture->UpdateResource();
}

void FWindTextureManager::CreateMPC()
{
	WindMPC = NewObject<UMaterialParameterCollection>(
		GetTransientPackage(),
		FName(TEXT("Wind3D_MPC")),
		RF_Transient);

	if (!WindMPC)
	{
		UE_LOG(LogWind3D, Error, TEXT("Failed to create wind MPC!"));
		return;
	}

	WindMPC->AddToRoot();

	// Vector parameters
	auto AddVectorParam = [this](const FName& Name, const FLinearColor& Default)
	{
		FCollectionVectorParameter Param;
		Param.ParameterName = Name;
		Param.DefaultValue = Default;
		WindMPC->VectorParameters.Add(Param);
	};

	AddVectorParam(FName(TEXT("WindVolumeOrigin")), FLinearColor(0, 0, 0, 0));
	AddVectorParam(FName(TEXT("WindVolumeSize")), FLinearColor(3200, 3200, 1600, 0));
	AddVectorParam(FName(TEXT("WindGridCells")), FLinearColor(16, 16, 8, 0));
	AddVectorParam(FName(TEXT("WindAmbient")), FLinearColor(0, 0, 0, 0));

	// Scalar parameters
	auto AddScalarParam = [this](const FName& Name, float Default)
	{
		FCollectionScalarParameter Param;
		Param.ParameterName = Name;
		Param.DefaultValue = Default;
		WindMPC->ScalarParameters.Add(Param);
	};

	AddScalarParam(FName(TEXT("WindOverallPower")), 1.0f);
	AddScalarParam(FName(TEXT("WindMaxSpeed")), MaxWindSpeed);
}

void FWindTextureManager::EncodeGridToStagingBuffer(const IWindSolver& Grid)
{
	const float InvMaxSpeed = 1.0f / MaxWindSpeed;
	const TArray<FVector>& Velocities = Grid.GetVelocities();
	const TArray<float>& Turbulences = Grid.GetTurbulences();

	for (int32 Z = 0; Z < GridSizeZ; Z++)
	{
		for (int32 Y = 0; Y < GridSizeY; Y++)
		{
			for (int32 X = 0; X < GridSizeX; X++)
			{
				const int32 GridIdx = Grid.CellIndex(X, Y, Z);
				const FVector& Vel = Velocities[GridIdx];
				const float Turb = Turbulences.IsValidIndex(GridIdx) ? Turbulences[GridIdx] : 0.f;

				// Atlas pixel: Z slices tiled horizontally
				const int32 AtlasX = Z * GridSizeX + X;
				const int32 AtlasY = Y;
				const int32 AtlasIdx = AtlasY * AtlasWidth + AtlasX;

				// Encode velocity: [-MaxSpeed, +MaxSpeed] -> [0, 1]
				FFloat16Color& Pixel = StagingBuffer[AtlasIdx];
				Pixel.R = FFloat16(FMath::Clamp((Vel.X * InvMaxSpeed + 1.0f) * 0.5f, 0.0f, 1.0f));
				Pixel.G = FFloat16(FMath::Clamp((Vel.Y * InvMaxSpeed + 1.0f) * 0.5f, 0.0f, 1.0f));
				Pixel.B = FFloat16(FMath::Clamp((Vel.Z * InvMaxSpeed + 1.0f) * 0.5f, 0.0f, 1.0f));
				Pixel.A = FFloat16(FMath::Clamp(Turb, 0.0f, 1.0f));
			}
		}
	}
}

void FWindTextureManager::UploadToGPU()
{
	if (!WindAtlasTexture) return;

	// For this tiny texture (e.g. 128x16 = 16 KB), BulkData lock + UpdateResource is simple and reliable.
	FTexturePlatformData* PlatformData = WindAtlasTexture->GetPlatformData();
	if (!PlatformData || PlatformData->Mips.Num() == 0) return;

	FTexture2DMipMap& Mip = PlatformData->Mips[0];
	void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	if (TextureData)
	{
		FMemory::Memcpy(TextureData, StagingBuffer.GetData(),
			StagingBuffer.Num() * sizeof(FFloat16Color));
	}
	Mip.BulkData.Unlock();

	WindAtlasTexture->UpdateResource();
}

void FWindTextureManager::UpdateMPCParams(
	const IWindSolver& Grid,
	const FVector& AmbientWind,
	float OverallPower)
{
	UWorld* World = CachedWorld.Get();
	if (!World || !WindMPC) return;

	UMaterialParameterCollectionInstance* Inst =
		World->GetParameterCollectionInstance(WindMPC);
	if (!Inst) return;

	const FVector GridOrigin = Grid.GetWorldOrigin();
	const FVector VolumeSize(
		Grid.GetSizeX() * Grid.GetCellSize(),
		Grid.GetSizeY() * Grid.GetCellSize(),
		Grid.GetSizeZ() * Grid.GetCellSize());

	Inst->SetVectorParameterValue(
		FName(TEXT("WindVolumeOrigin")),
		FLinearColor(GridOrigin.X, GridOrigin.Y, GridOrigin.Z, 0));

	Inst->SetVectorParameterValue(
		FName(TEXT("WindVolumeSize")),
		FLinearColor(VolumeSize.X, VolumeSize.Y, VolumeSize.Z, 0));

	Inst->SetVectorParameterValue(
		FName(TEXT("WindGridCells")),
		FLinearColor(Grid.GetSizeX(), Grid.GetSizeY(), Grid.GetSizeZ(), 0));

	Inst->SetVectorParameterValue(
		FName(TEXT("WindAmbient")),
		FLinearColor(AmbientWind.X, AmbientWind.Y, AmbientWind.Z, 0));

	Inst->SetScalarParameterValue(FName(TEXT("WindOverallPower")), OverallPower);
}

void FWindTextureManager::UpdateFromGrid(
	const IWindSolver& Grid,
	const FVector& AmbientWind,
	float OverallPower)
{
	if (!bInitialized) return;

	// Check if grid dimensions changed (e.g. SetupWindGrid called with new size)
	if (Grid.GetSizeX() != GridSizeX || Grid.GetSizeY() != GridSizeY || Grid.GetSizeZ() != GridSizeZ)
	{
		UE_LOG(LogWind3D, Log, TEXT("Grid dimensions changed, reinitializing TextureManager."));
		UWorld* World = CachedWorld.Get();
		Shutdown();
		if (World)
		{
			Initialize(World, Grid.GetSizeX(), Grid.GetSizeY(), Grid.GetSizeZ());
		}
		return;
	}

	EncodeGridToStagingBuffer(Grid);
	UpdateMPCParams(Grid, AmbientWind, OverallPower);
	UploadToGPU();
}

void FWindTextureManager::BindToMaterial(UMaterialInstanceDynamic* MID, FName TextureParamName) const
{
	if (MID && WindAtlasTexture)
	{
		MID->SetTextureParameterValue(TextureParamName, WindAtlasTexture);
	}
}
