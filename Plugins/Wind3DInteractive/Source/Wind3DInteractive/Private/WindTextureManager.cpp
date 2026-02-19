#include "WindTextureManager.h"
#include "IWindSolver.h"
#include "Wind3DInteractiveModule.h"
#include "Engine/VolumeTexture.h"
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
	CachedWorld = World;

	const int32 TotalVoxels = SizeX * SizeY * SizeZ;
	// Initialize with neutral wind (0.5, 0.5, 0.5, 0.0)
	StagingBuffer.Init(FFloat16Color(FLinearColor(0.5f, 0.5f, 0.5f, 0.0f)), TotalVoxels);

	// Viz: BGRA 8-bit — B=128, G=128, R=0, A=255 per pixel (no wind, fully opaque)
	const int32 VizPixels = SizeX * SizeY;
	VizStagingBuffer.SetNumUninitialized(VizPixels * 4);
	for (int32 i = 0; i < VizPixels; i++)
	{
		VizStagingBuffer[i * 4 + 0] = 128; // B = neutral Y
		VizStagingBuffer[i * 4 + 1] = 128; // G = neutral X
		VizStagingBuffer[i * 4 + 2] = 0;   // R = no speed
		VizStagingBuffer[i * 4 + 3] = 255; // A = opaque
	}

	CreateVolumeTexture();
	CreateVizTexture();
	CreateMPC();

	bInitialized = true;

	UE_LOG(LogWind3D, Log, TEXT("WindTextureManager initialized: VolumeTexture %dx%dx%d (%d voxels)"),
		GridSizeX, GridSizeY, GridSizeZ, TotalVoxels);
}

void FWindTextureManager::Shutdown()
{
	if (!bInitialized) return;

	if (WindVolumeTexture)
	{
		WindVolumeTexture->RemoveFromRoot();
		WindVolumeTexture = nullptr;
	}

	if (WindVizTexture)
	{
		WindVizTexture->RemoveFromRoot();
		WindVizTexture = nullptr;
	}

	if (WindMPC)
	{
		WindMPC->RemoveFromRoot();
		WindMPC = nullptr;
	}

	StagingBuffer.Empty();
	VizStagingBuffer.Empty();
	bInitialized = false;

	UE_LOG(LogWind3D, Log, TEXT("WindTextureManager shut down."));
}

void FWindTextureManager::CreateVolumeTexture()
{
	WindVolumeTexture = UVolumeTexture::CreateTransient(GridSizeX, GridSizeY, GridSizeZ, PF_FloatRGBA);
	if (!WindVolumeTexture)
	{
		UE_LOG(LogWind3D, Error, TEXT("Failed to create wind volume texture!"));
		return;
	}

	WindVolumeTexture->AddToRoot();
	WindVolumeTexture->Filter = TF_Bilinear;
	WindVolumeTexture->SRGB = 0;
	WindVolumeTexture->CompressionSettings = TC_HDR;
	WindVolumeTexture->MipGenSettings = TMGS_NoMipmaps;

	// Initialize mip data with zeros
	FTexturePlatformData* PlatformData = WindVolumeTexture->GetPlatformData();
	if (PlatformData && PlatformData->Mips.Num() > 0)
	{
		FTexture2DMipMap& Mip = PlatformData->Mips[0];
		void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
		
		if (TextureData)
		{
			// Initialize mip data with neutral wind (0.5, 0.5, 0.5, 0.0) -> Velocity (0, 0, 0)
			FFloat16Color* DataPtr = (FFloat16Color*)TextureData;
			const int32 NumVoxels = GridSizeX * GridSizeY * GridSizeZ;
			const FFloat16Color NeutralColor(FLinearColor(0.5f, 0.5f, 0.5f, 0.0f));
			
			for (int32 i = 0; i < NumVoxels; ++i)
			{
				DataPtr[i] = NeutralColor;
			}
		}
		Mip.BulkData.Unlock();
	}

	WindVolumeTexture->UpdateResource();
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
	AddVectorParam(FName(TEXT("WindVolumeSize")), FLinearColor(0, 0, 0, 0)); // Updated in Tick
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

void FWindTextureManager::CreateVizTexture()
{
	WindVizTexture = UTexture2D::CreateTransient(GridSizeX, GridSizeY, PF_B8G8R8A8, TEXT("WindVizTexture"));
	if (!WindVizTexture)
	{
		UE_LOG(LogWind3D, Error, TEXT("Failed to create wind viz texture!"));
		return;
	}

	WindVizTexture->AddToRoot();
	WindVizTexture->Filter = TF_Nearest; // pixel-perfect for grid cells
	WindVizTexture->SRGB = 0;
	WindVizTexture->MipGenSettings = TMGS_NoMipmaps;

	// Upload initial grey frame
	FTexture2DMipMap& Mip = WindVizTexture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	if (Data)
	{
		FMemory::Memcpy(Data, VizStagingBuffer.GetData(), VizStagingBuffer.Num());
	}
	Mip.BulkData.Unlock();
	WindVizTexture->UpdateResource();

	UE_LOG(LogWind3D, Log, TEXT("WindVizTexture created: %dx%d (PF_B8G8R8A8)"), GridSizeX, GridSizeY);
}

void FWindTextureManager::UpdateVizSlice(const IWindSolver& Grid)
{
	if (!WindVizTexture) return;

	const int32 ZSlice = (VizZSlice < 0)
		? (GridSizeZ / 2)
		: FMath::Clamp(VizZSlice, 0, GridSizeZ - 1);

	const float InvMaxSpeed = 1.0f / MaxWindSpeed;
	const TArray<FVector>& Velocities = Grid.GetVelocities();
	const TArray<float>& Turbulences = Grid.GetTurbulences();

	for (int32 Y = 0; Y < GridSizeY; Y++)
	{
		for (int32 X = 0; X < GridSizeX; X++)
		{
			const int32 GridIdx = Grid.CellIndex(X, Y, ZSlice);
			const FVector& Vel = Velocities[GridIdx];
			const float Turb = Turbulences.IsValidIndex(GridIdx) ? Turbulences[GridIdx] : 0.f;

			// R = speed magnitude (0=no wind, 255=MaxWindSpeed)
			const uint8 SpeedByte = (uint8)(FMath::Clamp(Vel.Size() * InvMaxSpeed, 0.f, 1.f) * 255.f);
			// G = X velocity direction (0=max -X, 128=zero, 255=max +X)
			const uint8 VelXByte  = (uint8)(FMath::Clamp((Vel.X * InvMaxSpeed + 1.f) * 0.5f, 0.f, 1.f) * 255.f);
			// B = Y velocity direction (0=max -Y, 128=zero, 255=max +Y)
			const uint8 VelYByte  = (uint8)(FMath::Clamp((Vel.Y * InvMaxSpeed + 1.f) * 0.5f, 0.f, 1.f) * 255.f);

			// PF_B8G8R8A8 memory order: B, G, R, A
			const int32 PixIdx = (Y * GridSizeX + X) * 4;
			VizStagingBuffer[PixIdx + 0] = VelYByte;  // B
			VizStagingBuffer[PixIdx + 1] = VelXByte;  // G
			VizStagingBuffer[PixIdx + 2] = SpeedByte; // R
			VizStagingBuffer[PixIdx + 3] = 255;        // A = fully opaque
		}
	}

	FTexture2DMipMap& Mip = WindVizTexture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	if (Data)
	{
		FMemory::Memcpy(Data, VizStagingBuffer.GetData(), VizStagingBuffer.Num());
	}
	Mip.BulkData.Unlock();
	WindVizTexture->UpdateResource();
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

				// Volume texture: flat 3D index = Z * SizeX * SizeY + Y * SizeX + X
				const int32 TexIdx = Z * GridSizeX * GridSizeY + Y * GridSizeX + X;

				// Encode velocity: [-MaxSpeed, +MaxSpeed] -> [0, 1]
				FFloat16Color& Pixel = StagingBuffer[TexIdx];
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
	if (!WindVolumeTexture) return;

	FTexturePlatformData* PlatformData = WindVolumeTexture->GetPlatformData();
	if (!PlatformData || PlatformData->Mips.Num() == 0) return;

	FTexture2DMipMap& Mip = PlatformData->Mips[0];
	void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	if (TextureData)
	{
		FMemory::Memcpy(TextureData, StagingBuffer.GetData(),
			StagingBuffer.Num() * sizeof(FFloat16Color));
	}
	Mip.BulkData.Unlock();

	WindVolumeTexture->UpdateResource();

	// Debug: log a sample voxel every ~5 seconds
	static int32 UploadLogCounter = 0;
	if (UploadLogCounter++ % 300 == 0 && StagingBuffer.Num() > 0)
	{
		// Sample center voxel
		const int32 CenterIdx = (GridSizeZ / 2) * GridSizeX * GridSizeY + (GridSizeY / 2) * GridSizeX + (GridSizeX / 2);
		if (StagingBuffer.IsValidIndex(CenterIdx))
		{
			const FFloat16Color& C = StagingBuffer[CenterIdx];
			UE_LOG(LogWind3D, Log, TEXT("UploadToGPU: CenterVoxel[%d] = (%.3f, %.3f, %.3f, %.3f) Texture=%s"),
				CenterIdx, C.R.GetFloat(), C.G.GetFloat(), C.B.GetFloat(), C.A.GetFloat(),
				*GetNameSafe(WindVolumeTexture));
		}
	}
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
	UpdateBoundMIDs(Grid);
	UpdateVizSlice(Grid);
}

void FWindTextureManager::BindToMaterial(UMaterialInstanceDynamic* MID, FName TextureParamName)
{
	if (MID && WindVolumeTexture)
	{
		MID->SetTextureParameterValue(TextureParamName, WindVolumeTexture);

		// Track this MID for per-frame VolumeOrigin/VolumeSize updates
		// Avoid duplicates
		bool bAlreadyTracked = false;
		for (const auto& Weak : BoundMIDs)
		{
			if (Weak.Get() == MID)
			{
				bAlreadyTracked = true;
				break;
			}
		}
		if (!bAlreadyTracked)
		{
			BoundMIDs.Add(MID);
			UE_LOG(LogWind3D, Log, TEXT("Tracking MID for wind parameter updates (total: %d)"), BoundMIDs.Num());
		}
	}
}

void FWindTextureManager::UpdateBoundMIDs(const IWindSolver& Grid)
{
	const FVector GridOrigin = Grid.GetWorldOrigin();
	const FVector VolumeSize(
		Grid.GetSizeX() * Grid.GetCellSize(),
		Grid.GetSizeY() * Grid.GetCellSize(),
		Grid.GetSizeZ() * Grid.GetCellSize());

	const FLinearColor OriginColor(GridOrigin.X, GridOrigin.Y, GridOrigin.Z, 0.f);
	const FLinearColor SizeColor(VolumeSize.X, VolumeSize.Y, VolumeSize.Z, 0.f);

	// Update all tracked MIDs, remove stale ones
	for (int32 i = BoundMIDs.Num() - 1; i >= 0; --i)
	{
		UMaterialInstanceDynamic* MID = BoundMIDs[i].Get();
		if (!MID)
		{
			BoundMIDs.RemoveAtSwap(i);
			continue;
		}

		MID->SetVectorParameterValue(FName(TEXT("VolumeOrigin")), OriginColor);
		MID->SetVectorParameterValue(FName(TEXT("VolumeSize")), SizeColor);
	}
}
