#include "WindMaterialBuilder.h"
#include "WindSubsystem.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameterVolume.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionAppendVector.h"

#include "Engine/VolumeTexture.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogWindMaterialBuilder, Log, All);

// ---------------------------------------------------------------------------
// Console command
// ---------------------------------------------------------------------------
static FAutoConsoleCommand GWindCreateMaterialCmd(
	TEXT("Wind3D.CreateMaterial"),
	TEXT("Creates a wind grass material. Usage: Wind3D.CreateMaterial [/Game/Wind/M_WindGrass_Auto]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		FString Path = TEXT("/Game/Wind/M_WindGrass_Auto");
		if (Args.Num() > 0 && !Args[0].IsEmpty())
		{
			Path = Args[0];
		}

		UMaterial* Mat = UWindMaterialBuilder::CreateWindMaterial(Path);
		if (Mat)
		{
			UE_LOG(LogWindMaterialBuilder, Log, TEXT("Wind material created at: %s"), *Path);
		}
		else
		{
			UE_LOG(LogWindMaterialBuilder, Error, TEXT("Failed to create wind material at: %s"), *Path);
		}
	})
);

// ---------------------------------------------------------------------------
// Helper: create a material expression and add it to the material
// ---------------------------------------------------------------------------
template<typename T>
static T* AddExpression(UMaterial* Mat, int32 PosX = 0, int32 PosY = 0)
{
	T* Expr = NewObject<T>(Mat);
	Expr->MaterialExpressionEditorX = PosX;
	Expr->MaterialExpressionEditorY = PosY;
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
	Mat->GetEditorOnlyData()->ExpressionCollection.AddExpression(Expr);
#else
	Mat->Expressions.Add(Expr);
#endif
	return Expr;
}

// ---------------------------------------------------------------------------
// CreateWindMaterial
// ---------------------------------------------------------------------------
UMaterial* UWindMaterialBuilder::CreateWindMaterial(const FString& SavePath)
{
	// ---- 1. Create Package & Material ----
	const FString PackagePath = SavePath;
	const FString MaterialName = FPackageName::GetShortName(PackagePath);

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		UE_LOG(LogWindMaterialBuilder, Error, TEXT("Failed to create package: %s"), *PackagePath);
		return nullptr;
	}

	UMaterial* Mat = NewObject<UMaterial>(Package, *MaterialName, RF_Public | RF_Standalone);
	if (!Mat)
	{
		UE_LOG(LogWindMaterialBuilder, Error, TEXT("Failed to create material object"));
		return nullptr;
	}

	// Material settings
	Mat->TwoSided = false;
	Mat->bUsedWithInstancedStaticMeshes = true;

	// ---- 2. Create Expression Nodes ----

	// -- WorldPosition --
	auto* WorldPos = AddExpression<UMaterialExpressionWorldPosition>(Mat, -1600, 100);

	// -- VolumeOrigin (VectorParameter) --
	auto* OriginParam = AddExpression<UMaterialExpressionVectorParameter>(Mat, -1600, 300);
	OriginParam->ParameterName = TEXT("VolumeOrigin");
	OriginParam->DefaultValue = FLinearColor(0, 0, 0, 0);

	// -- VolumeSize (VectorParameter) --
	auto* SizeParam = AddExpression<UMaterialExpressionVectorParameter>(Mat, -1600, 500);
	SizeParam->ParameterName = TEXT("VolumeSize");
	SizeParam->DefaultValue = FLinearColor(6400, 6400, 3200, 0);

	// -- Time --
	auto* TimeNode = AddExpression<UMaterialExpressionTime>(Mat, -1600, 700);

	// -- VertexColor --
	auto* VertColor = AddExpression<UMaterialExpressionVertexColor>(Mat, -1600, 900);

	// -- Custom Node: Compute UVW --
	auto* UVWNode = AddExpression<UMaterialExpressionCustom>(Mat, -1100, 400);
	UVWNode->Description = TEXT("ComputeUVW");
	UVWNode->OutputType = CMOT_Float3;
	UVWNode->Code = TEXT(
		"float3 uvw = (WorldPos - VolumeOrigin.xyz) / max(VolumeSize.xyz, 0.001f);\n"
		"return saturate(uvw);"
	);
	// Inputs: WorldPos, VolumeOrigin, VolumeSize
	UVWNode->Inputs.Empty();
	{
		FCustomInput In;
		In.InputName = TEXT("WorldPos");
		In.Input.Connect(0, WorldPos);
		UVWNode->Inputs.Add(In);
	}
	{
		FCustomInput In;
		In.InputName = TEXT("VolumeOrigin");
		In.Input.Connect(0, OriginParam);
		UVWNode->Inputs.Add(In);
	}
	{
		FCustomInput In;
		In.InputName = TEXT("VolumeSize");
		In.Input.Connect(0, SizeParam);
		UVWNode->Inputs.Add(In);
	}

	// -- TextureSampleParameterVolume: WindVolume --
	auto* VolTexSample = AddExpression<UMaterialExpressionTextureSampleParameterVolume>(Mat, -700, 300);
	VolTexSample->ParameterName = TEXT("WindVolume");
	VolTexSample->SamplerType = SAMPLERTYPE_LinearColor;

	// Try to assign the default neutral texture
	UVolumeTexture* DefaultVolTex = LoadObject<UVolumeTexture>(
		nullptr, TEXT("/Game/Wind/VT_Wind_Neutral.VT_Wind_Neutral"));
	
	if (!DefaultVolTex)
	{
		// Auto-create if missing
		DefaultVolTex = CreateNeutralWindTexture(TEXT("/Game/Wind/VT_Wind_Neutral"));
	}

	if (DefaultVolTex)
	{
		VolTexSample->Texture = DefaultVolTex;
	}

	// Connect UVW -> TextureSample UVs
	VolTexSample->Coordinates.Connect(0, UVWNode);

	// -- Custom Node: Wind Displacement (WPO) --
	auto* WindWPO = AddExpression<UMaterialExpressionCustom>(Mat, -200, 300);
	WindWPO->Description = TEXT("WindDisplacement");
	WindWPO->OutputType = CMOT_Float3;
	WindWPO->Code = TEXT(
		"// Compute UVW for bounds check\n"
		"float3 UVW = (WorldPos - VolumeOrigin.xyz) / max(VolumeSize.xyz, 0.001f);\n"
		"\n"
		"// Out of bounds check\n"
		"if (any(UVW < -0.05f) || any(UVW > 1.05f))\n"
		"    return float3(0, 0, 0);\n"
		"\n"
		"// Edge fade\n"
		"float3 Edge = min(saturate(UVW), 1.0f - saturate(UVW));\n"
		"float Fade = saturate(min(min(Edge.x, Edge.y), Edge.z) * 10.0f);\n"
		"\n"
		"// Decode velocity from texture: [0,1] -> [-MaxSpeed, +MaxSpeed]\n"
		"float3 Velocity = (Texel.rgb * 2.0f - 1.0f) * 2000.0f * Fade;\n"
		"float Turbulence = Texel.a * Fade;\n"
		"float Speed = length(Velocity);\n"
		"\n"
		"if (Speed < 1.0f) return float3(0, 0, 0);\n"
		"\n"
		"// Flexibility: VertexColor R channel (0=root, 1=tip)\n"
		"float Flexibility = Flex;\n"
		"\n"
		"// Main bend\n"
		"float3 Dir = normalize(Velocity);\n"
		"float BendAmount = min(Speed, 1000.0f) * 0.5f * Flexibility;\n"
		"float3 MainBend = Dir * BendAmount;\n"
		"\n"
		"// Shiver (micro-animation)\n"
		"float Phase = dot(WorldPos, float3(0.1, 0.1, 0.1));\n"
		"float Shiver = sin(Time * 10.0f + Phase) * Turbulence * 0.2f * Speed * Flexibility;\n"
		"\n"
		"return MainBend + Dir * Shiver;"
	);

	// Inputs for WindWPO
	WindWPO->Inputs.Empty();
	{
		FCustomInput In;
		In.InputName = TEXT("WorldPos");
		In.Input.Connect(0, WorldPos);
		WindWPO->Inputs.Add(In);
	}
	{
		FCustomInput In;
		In.InputName = TEXT("VolumeOrigin");
		In.Input.Connect(0, OriginParam);
		WindWPO->Inputs.Add(In);
	}
	{
		FCustomInput In;
		In.InputName = TEXT("VolumeSize");
		In.Input.Connect(0, SizeParam);
		WindWPO->Inputs.Add(In);
	}
	{
		// Texel from volume texture sample
		// Output 0 is likely RGB (float3) based on the error "swizzle 'a' out of bounds"
		// We explicitly append Alpha (Output 4) to ensure float4.
		auto* AppendAlpha = AddExpression<UMaterialExpressionAppendVector>(Mat, -500, 300);
		AppendAlpha->A.Connect(0, VolTexSample); // RGB
		AppendAlpha->B.Connect(4, VolTexSample); // Alpha

		FCustomInput In;
		In.InputName = TEXT("Texel");
		In.Input.Connect(0, AppendAlpha);
		WindWPO->Inputs.Add(In);
	}
	{
		FCustomInput In;
		In.InputName = TEXT("Time");
		In.Input.Connect(0, TimeNode);
		WindWPO->Inputs.Add(In);
	}
	{
		// VertexColor R channel as flexibility
		FCustomInput In;
		In.InputName = TEXT("Flex");
		In.Input.Connect(1, VertColor); // Output index 1 = R channel
		WindWPO->Inputs.Add(In);
	}

	// ---- 3. Connect to Material Outputs ----

	// World Position Offset <- WindWPO
	Mat->GetEditorOnlyData()->WorldPositionOffset.Connect(0, WindWPO);

	// Base Color <- simple green constant
	auto* BaseColor = AddExpression<UMaterialExpressionConstant3Vector>(Mat, -200, 0);
	BaseColor->Constant = FLinearColor(0.15f, 0.45f, 0.05f);
	Mat->GetEditorOnlyData()->BaseColor.Connect(0, BaseColor);

	// ---- 4. Compile, Save & Register ----
	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();

	// Mark package dirty and save
	Package->MarkPackageDirty();

	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		PackagePath, FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;

	bool bSaved = UPackage::SavePackage(Package, Mat, *PackageFilename, SaveArgs);

	if (bSaved)
	{
		// Register with asset registry
		FAssetRegistryModule::AssetCreated(Mat);
		UE_LOG(LogWindMaterialBuilder, Log,
			TEXT("Wind material saved: %s (%s)"), *PackagePath, *PackageFilename);
	}
	else
	{
		UE_LOG(LogWindMaterialBuilder, Warning,
			TEXT("Wind material created in memory but save failed: %s"), *PackageFilename);
	}

	return Mat;
}

// ---------------------------------------------------------------------------
// CreateNeutralWindTexture
// ---------------------------------------------------------------------------
UVolumeTexture* UWindMaterialBuilder::CreateNeutralWindTexture(const FString& PackagePath)
{
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		PackagePath, FPackageName::GetAssetPackageExtension());

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package) return nullptr;

	UVolumeTexture* Tex = NewObject<UVolumeTexture>(Package, *FPackageName::GetShortName(PackagePath), RF_Public | RF_Standalone);
	if (!Tex) return nullptr;

	// Texture Settings
	Tex->SRGB = 0;
	Tex->CompressionSettings = TC_HDR;
	Tex->Filter = TF_Bilinear;
	Tex->MipGenSettings = TMGS_NoMipmaps;

	// Fill Data
	const int32 SizeX = 32;
	const int32 SizeY = 32;
	const int32 SizeZ = 16;
	const int32 TotalVoxels = SizeX * SizeY * SizeZ;

	TArray<FFloat16Color> Data;
	Data.SetNumUninitialized(TotalVoxels);
	
	const FFloat16Color Neutral(FLinearColor(0.5f, 0.5f, 0.5f, 0.0f)); // Velocity 0
	for (auto& Pixel : Data)
	{
		Pixel = Neutral;
	}

	// Initialize Source
	Tex->Source.Init(SizeX, SizeY, SizeZ, 1, TSF_RGBA16F, (const uint8*)Data.GetData());
	Tex->UpdateResource();

	// Save
	Package->MarkPackageDirty();
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	
	if (UPackage::SavePackage(Package, Tex, *PackageFilename, SaveArgs))
	{
		FAssetRegistryModule::AssetCreated(Tex);
		UE_LOG(LogWindMaterialBuilder, Log, TEXT("Created neutral wind texture: %s"), *PackagePath);
	}
	
	return Tex;
}

// ---------------------------------------------------------------------------
// ApplyWindToComponent
// ---------------------------------------------------------------------------
UMaterialInstanceDynamic* UWindMaterialBuilder::ApplyWindToComponent(
	UPrimitiveComponent* Component,
	UMaterialInterface* WindMaterial,
	int32 MaterialIndex)
{
	if (!Component || !WindMaterial)
	{
		UE_LOG(LogWindMaterialBuilder, Error, TEXT("ApplyWindToComponent: null Component or WindMaterial"));
		return nullptr;
	}

	UWorld* World = Component->GetWorld();
	if (!World)
	{
		UE_LOG(LogWindMaterialBuilder, Error, TEXT("ApplyWindToComponent: no world"));
		return nullptr;
	}

	// Create MID
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(WindMaterial, Component);
	if (!MID)
	{
		UE_LOG(LogWindMaterialBuilder, Error, TEXT("ApplyWindToComponent: failed to create MID"));
		return nullptr;
	}

	// Apply to component
	Component->SetMaterial(MaterialIndex, MID);

	// Bind wind texture via subsystem
	UWindSubsystem* WindSub = World->GetSubsystem<UWindSubsystem>();
	if (WindSub)
	{
		WindSub->BindWindToMaterial(MID, FName(TEXT("WindVolume")));
		UE_LOG(LogWindMaterialBuilder, Log, TEXT("Wind bound to component: %s (slot %d)"),
			*Component->GetName(), MaterialIndex);
	}
	else
	{
		UE_LOG(LogWindMaterialBuilder, Warning,
			TEXT("WindSubsystem not found — texture will use default. Call BindWindToMaterial manually at runtime."));
	}

	return MID;
}

void UWindMaterialBuilder::RegisterConsoleCommands()
{
	// Console command is auto-registered via FAutoConsoleCommand above
	UE_LOG(LogWindMaterialBuilder, Log, TEXT("Wind3D.CreateMaterial console command available."));
}
