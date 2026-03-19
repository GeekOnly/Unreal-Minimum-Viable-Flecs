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
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialParameterCollection.h"

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

static FAutoConsoleCommand GWindCreateDebugMaterialCmd(
	TEXT("Wind3D.CreateDebugMaterial"),
	TEXT("Creates a debug wind material. Usage: Wind3D.CreateDebugMaterial [/Game/Wind/M_WindDebug_Auto]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		FString Path = TEXT("/Game/Wind/M_WindDebug_Auto");
		if (Args.Num() > 0 && !Args[0].IsEmpty())
		{
			Path = Args[0];
		}

		UMaterial* Mat = UWindMaterialBuilder::CreateDebugMaterial(Path);
		if (Mat)
		{
			UE_LOG(LogWindMaterialBuilder, Log, TEXT("Debug Wind material created at: %s"), *Path);
		}
		else
		{
			UE_LOG(LogWindMaterialBuilder, Error, TEXT("Failed to create Debug Wind material at: %s"), *Path);
		}
	})
);

// ---------------------------------------------------------------------------
// Helper: create or load persistent MPC asset for wind parameters
// ---------------------------------------------------------------------------
static UMaterialParameterCollection* CreateOrLoadWindMPC()
{
	const FString MPCPath = TEXT("/Game/Wind/MPC_Wind3D");
	const FString AssetPath = MPCPath + TEXT(".MPC_Wind3D");

	// Try to load existing
	UMaterialParameterCollection* MPC = LoadObject<UMaterialParameterCollection>(nullptr, *AssetPath);
	if (MPC) return MPC;

	// Create new
	UPackage* Pkg = CreatePackage(*MPCPath);
	if (!Pkg) return nullptr;

	MPC = NewObject<UMaterialParameterCollection>(Pkg, TEXT("MPC_Wind3D"), RF_Public | RF_Standalone);
	if (!MPC) return nullptr;

	// Vector parameters (must match WindTextureManager::UpdateMPCParams)
	{
		FCollectionVectorParameter P;
		P.ParameterName = FName(TEXT("WindVolumeOrigin"));
		P.DefaultValue = FLinearColor(0, 0, 0, 0);
		MPC->VectorParameters.Add(P);
	}
	{
		FCollectionVectorParameter P;
		P.ParameterName = FName(TEXT("WindVolumeSize"));
		P.DefaultValue = FLinearColor(6400, 6400, 3200, 0);
		MPC->VectorParameters.Add(P);
	}
	{
		FCollectionVectorParameter P;
		P.ParameterName = FName(TEXT("WindAmbient"));
		P.DefaultValue = FLinearColor(0, 0, 0, 0);
		MPC->VectorParameters.Add(P);
	}

	// Scalar parameters
	{
		FCollectionScalarParameter P;
		P.ParameterName = FName(TEXT("WindOverallPower"));
		P.DefaultValue = 1.0f;
		MPC->ScalarParameters.Add(P);
	}
	{
		FCollectionScalarParameter P;
		P.ParameterName = FName(TEXT("WindMaxSpeed"));
		P.DefaultValue = 2000.f;
		MPC->ScalarParameters.Add(P);
	}

	// Save
	Pkg->MarkPackageDirty();
	const FString Filename = FPackageName::LongPackageNameToFilename(MPCPath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Pkg, MPC, *Filename, SaveArgs);
	FAssetRegistryModule::AssetCreated(MPC);

	UE_LOG(LogWindMaterialBuilder, Log, TEXT("Created persistent wind MPC: %s"), *MPCPath);
	return MPC;
}

// ---------------------------------------------------------------------------
// Helper: create a CollectionParameter expression with correct ParameterId
// ---------------------------------------------------------------------------
static UMaterialExpressionCollectionParameter* AddMPCParam(
	UMaterial* Mat, UMaterialParameterCollection* MPC, FName ParamName, int32 PosX, int32 PosY)
{
	auto* Expr = NewObject<UMaterialExpressionCollectionParameter>(Mat);
	Expr->MaterialExpressionEditorX = PosX;
	Expr->MaterialExpressionEditorY = PosY;
	Mat->GetEditorOnlyData()->ExpressionCollection.AddExpression(Expr);

	Expr->Collection = MPC;
	Expr->ParameterName = ParamName;

	// Match ParameterId from the MPC's parameter list
	for (const FCollectionVectorParameter& P : MPC->VectorParameters)
	{
		if (P.ParameterName == ParamName) { Expr->ParameterId = P.Id; return Expr; }
	}
	for (const FCollectionScalarParameter& P : MPC->ScalarParameters)
	{
		if (P.ParameterName == ParamName) { Expr->ParameterId = P.Id; return Expr; }
	}

	UE_LOG(LogWindMaterialBuilder, Warning, TEXT("MPC parameter '%s' not found — material may not compile"), *ParamName.ToString());
	return Expr;
}

// ---------------------------------------------------------------------------
// Helper: create a material expression and add it to the material
// ---------------------------------------------------------------------------
template<typename T>
static T* AddExpression(UMaterial* Mat, int32 PosX = 0, int32 PosY = 0)
{
	T* Expr = NewObject<T>(Mat);
	Expr->MaterialExpressionEditorX = PosX;
	Expr->MaterialExpressionEditorY = PosY;
	Mat->GetEditorOnlyData()->ExpressionCollection.AddExpression(Expr);
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

	// -- Create/Load persistent MPC --
	UMaterialParameterCollection* WindMPC = CreateOrLoadWindMPC();
	if (!WindMPC)
	{
		UE_LOG(LogWindMaterialBuilder, Error, TEXT("Failed to create/load wind MPC"));
		return nullptr;
	}

	// -- WorldPosition --
	auto* WorldPos = AddExpression<UMaterialExpressionWorldPosition>(Mat, -1600, 100);

	// -- VolumeOrigin (from MPC — auto-updated every frame) --
	auto* OriginParam = AddMPCParam(Mat, WindMPC, FName(TEXT("WindVolumeOrigin")), -1600, 300);

	// -- VolumeSize (from MPC) --
	auto* SizeParam = AddMPCParam(Mat, WindMPC, FName(TEXT("WindVolumeSize")), -1600, 500);

	// -- Time --
	auto* TimeNode = AddExpression<UMaterialExpressionTime>(Mat, -1600, 700);

	// -- MaxWindSpeed (from MPC) --
	auto* MaxSpeedParam = AddMPCParam(Mat, WindMPC, FName(TEXT("WindMaxSpeed")), -1600, 800);

	// -- VertexColor --
	auto* VertColor = AddExpression<UMaterialExpressionVertexColor>(Mat, -1600, 900);

	// -- TextureCoordinate (UV.Y fallback for flexibility when no vertex color) --
	auto* TexCoordNode = AddExpression<UMaterialExpressionTextureCoordinate>(Mat, -1600, 1000);

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
		"// ============================================================\n"
		"// Wind Grass WPO — GoW / Ghost of Tsushima style\n"
		"// ============================================================\n"
		"\n"
		"// Flexibility: UV.Y gradient (0=root, 1=tip)\n"
		"float FlexGrad = 1.0 - TexCoordY; // Most grass: UV.Y=1 at root, 0 at tip\n"
		"float FlexGrad2 = FlexGrad * FlexGrad; // Quadratic — roots stay firmly planted\n"
		"\n"
		"// --- Per-blade hash ---\n"
		"float2 BladeXY = floor(WorldPos.xy / 80.0);\n"
		"float Hash = frac(sin(dot(BladeXY, float2(127.1, 311.7))) * 43758.5453);\n"
		"\n"
		"// ============================================================\n"
		"// DOMINO WAVE — visible wavefront sweeps across the grass field\n"
		"// Like wheat fields in the wind or Ghost of Tsushima\n"
		"// ============================================================\n"
		"\n"
		"// Wave direction rotates slowly for organic feel\n"
		"float Angle1 = Time * 0.15;\n"
		"float2 WaveDir1 = float2(cos(Angle1), sin(Angle1));\n"
		"float Dist1 = dot(WorldPos.xy, WaveDir1);\n"
		"\n"
		"// Domino pulse: sharp rise, hold, soft release\n"
		"// frac = repeating 0→1 sawtooth, smoothstep shapes into a pulse\n"
		"float P1 = frac(Time * 0.25 - Dist1 * 0.0008 + Hash * 0.05);\n"
		"float Domino1 = smoothstep(0.0, 0.12, P1) * smoothstep(0.45, 0.12, P1);\n"
		"\n"
		"// Second wave at different angle and speed\n"
		"float Angle2 = Time * 0.1 + 2.1;\n"
		"float2 WaveDir2 = float2(cos(Angle2), sin(Angle2));\n"
		"float Dist2 = dot(WorldPos.xy, WaveDir2);\n"
		"float P2 = frac(Time * 0.15 - Dist2 * 0.0006 + Hash * 0.03);\n"
		"float Domino2 = smoothstep(0.0, 0.15, P2) * smoothstep(0.5, 0.15, P2);\n"
		"\n"
		"// Domino bend — pushes grass in the wave direction\n"
		"float DominoStr = Domino1 * 18.0 + Domino2 * 10.0;\n"
		"float2 DominoDir = normalize(WaveDir1 * Domino1 + WaveDir2 * Domino2 + 0.001);\n"
		"float DominoX = DominoDir.x * DominoStr * FlexGrad2;\n"
		"float DominoY = DominoDir.y * DominoStr * FlexGrad2;\n"
		"\n"
		"// Gentle idle sway underneath\n"
		"float IdlePhase = Time * 1.0 + WorldPos.x * 0.003 + Hash * 6.28;\n"
		"float IdleX = sin(IdlePhase) * 5.0 * FlexGrad2;\n"
		"float IdleY = sin(IdlePhase * 0.7 + 1.57) * 3.0 * FlexGrad2;\n"
		"\n"
		"// Micro-shiver on tips\n"
		"float Shiver = sin(Time * 6.0 + Hash * 80.0) * 1.5 * FlexGrad;\n"
		"\n"
		"float3 Result = float3(\n"
		"    DominoX + IdleX + Shiver,\n"
		"    DominoY + IdleY,\n"
		"    -DominoStr * 0.15 * FlexGrad2  // Slight downward dip during domino bend\n"
		");\n"
		"\n"
		"// ============================================================\n"
		"// Wind motor displacement (from 3D volume texture)\n"
		"// ============================================================\n"
		"float3 UVW = (WorldPos - VolumeOrigin.xyz) / max(VolumeSize.xyz, 0.001f);\n"
		"\n"
		"if (!(any(UVW < -0.05f) || any(UVW > 1.05f)))\n"
		"{\n"
		"    float3 Edge = min(saturate(UVW), 1.0f - saturate(UVW));\n"
		"    float Fade = saturate(min(min(Edge.x, Edge.y), Edge.z) * 10.0f);\n"
		"\n"
		"    float3 Velocity = (Texel.rgb * 2.0f - 1.0f) * MaxWindSpeed * Fade;\n"
		"    float Speed = length(Velocity);\n"
		"\n"
		"    if (Speed > 5.0f)\n"
		"    {\n"
		"        float3 Dir = normalize(Velocity);\n"
		"        float BendAmount = min(Speed, 600.0f) * 0.06 * FlexGrad2;\n"
		"        Result += Dir * BendAmount;\n"
		"\n"
		"        // Motor shiver\n"
		"        float WindShiver = sin(Time * 8.0 + Hash * 80.0) * Speed * 0.004 * FlexGrad;\n"
		"        Result += Dir * WindShiver;\n"
		"    }\n"
		"}\n"
		"\n"
		"return Result;"
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
	{
		// UV.Y as fallback flexibility (V=0 at root, V=1 at tip)
		auto* MaskV = AddExpression<UMaterialExpressionComponentMask>(Mat, -1400, 1000);
		MaskV->R = 0; MaskV->G = 1; MaskV->B = 0; MaskV->A = 0;
		MaskV->Input.Connect(0, TexCoordNode);

		FCustomInput In;
		In.InputName = TEXT("TexCoordY");
		In.Input.Connect(0, MaskV);
		WindWPO->Inputs.Add(In);
	}
	{
		FCustomInput In;
		In.InputName = TEXT("MaxWindSpeed");
		In.Input.Connect(0, MaxSpeedParam);
		WindWPO->Inputs.Add(In);
	}

	// ---- 3. Connect to Material Outputs ----

	// World Position Offset <- WindWPO
	Mat->GetEditorOnlyData()->WorldPositionOffset.Connect(0, WindWPO);

	// Base Color <- Debug: shows wind data in color
	// Green = no wind, Yellow/Red = wind speed, Blue = outside volume
	auto* DebugBaseColor = AddExpression<UMaterialExpressionCustom>(Mat, -200, 0);
	DebugBaseColor->Description = TEXT("WindDebugColor");
	DebugBaseColor->OutputType = CMOT_Float3;
	DebugBaseColor->Code = TEXT(
		"float3 UVW = (WorldPos - VolumeOrigin.xyz) / max(VolumeSize.xyz, 0.001f);\n"
		"bool bInside = !(any(UVW < -0.05f) || any(UVW > 1.05f));\n"
		"if (!bInside) return float3(0.1, 0.1, 0.4); // Blue = outside volume\n"
		"float3 Vel = (Texel.rgb * 2.0f - 1.0f) * MaxWindSpeed;\n"
		"float Speed = length(Vel);\n"
		"float N = saturate(Speed / 500.0f);\n"
		"// Green(0) -> Yellow(0.5) -> Red(1.0)\n"
		"return lerp(float3(0.1, 0.5, 0.05), float3(1, 0.2, 0), N);"
	);
	DebugBaseColor->Inputs.Empty();
	{
		FCustomInput In; In.InputName = TEXT("WorldPos");
		In.Input.Connect(0, WorldPos); DebugBaseColor->Inputs.Add(In);
	}
	{
		FCustomInput In; In.InputName = TEXT("VolumeOrigin");
		In.Input.Connect(0, OriginParam); DebugBaseColor->Inputs.Add(In);
	}
	{
		FCustomInput In; In.InputName = TEXT("VolumeSize");
		In.Input.Connect(0, SizeParam); DebugBaseColor->Inputs.Add(In);
	}
	{
		auto* AppendAlpha2 = AddExpression<UMaterialExpressionAppendVector>(Mat, -500, 0);
		AppendAlpha2->A.Connect(0, VolTexSample);
		AppendAlpha2->B.Connect(4, VolTexSample);
		FCustomInput In; In.InputName = TEXT("Texel");
		In.Input.Connect(0, AppendAlpha2); DebugBaseColor->Inputs.Add(In);
	}
	{
		FCustomInput In; In.InputName = TEXT("MaxWindSpeed");
		In.Input.Connect(0, MaxSpeedParam); DebugBaseColor->Inputs.Add(In);
	}
	Mat->GetEditorOnlyData()->BaseColor.Connect(0, DebugBaseColor);

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
// CreateDebugMaterial
// ---------------------------------------------------------------------------
UMaterial* UWindMaterialBuilder::CreateDebugMaterial(const FString& SavePath)
{
	// ---- 1. Create Package & Material ----
	const FString PackagePath = SavePath;
	const FString MaterialName = FPackageName::GetShortName(PackagePath);

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package) return nullptr;

	UMaterial* Mat = NewObject<UMaterial>(Package, *MaterialName, RF_Public | RF_Standalone);
	if (!Mat) return nullptr;

	Mat->TwoSided = false;
	Mat->bUsedWithInstancedStaticMeshes = true;
	// Set to Unlit for clearer visualization of pure color
	Mat->SetShadingModel(MSM_Unlit);

	// ---- 2. Create Expression Nodes ----

	// -- Create/Load persistent MPC --
	UMaterialParameterCollection* WindMPC = CreateOrLoadWindMPC();
	if (!WindMPC)
	{
		UE_LOG(LogWindMaterialBuilder, Error, TEXT("Failed to create/load wind MPC for debug material"));
		return nullptr;
	}

	// -- WorldPosition --
	auto* WorldPos = AddExpression<UMaterialExpressionWorldPosition>(Mat, -1200, 100);

	// -- VolumeOrigin (from MPC) --
	auto* OriginParam = AddMPCParam(Mat, WindMPC, FName(TEXT("WindVolumeOrigin")), -1200, 300);

	// -- VolumeSize (from MPC) --
	auto* SizeParam = AddMPCParam(Mat, WindMPC, FName(TEXT("WindVolumeSize")), -1200, 500);

	// -- MaxWindSpeed (from MPC) --
	auto* MaxSpeedParam = AddMPCParam(Mat, WindMPC, FName(TEXT("WindMaxSpeed")), -1200, 600);

	// -- TextureSampleParameterVolume: WindVolume --
	auto* VolTexSample = AddExpression<UMaterialExpressionTextureSampleParameterVolume>(Mat, -800, 300);
	VolTexSample->ParameterName = TEXT("WindVolume");
	VolTexSample->SamplerType = SAMPLERTYPE_LinearColor;

	// Try to load default neutral texture
	UVolumeTexture* DefaultVolTex = LoadObject<UVolumeTexture>(
		nullptr, TEXT("/Game/Wind/VT_Wind_Neutral.VT_Wind_Neutral"));
	if (!DefaultVolTex) DefaultVolTex = CreateNeutralWindTexture(TEXT("/Game/Wind/VT_Wind_Neutral"));
	if (DefaultVolTex) VolTexSample->Texture = DefaultVolTex;

	// -- Custom Node: Debug Visualization --
	auto* DebugNode = AddExpression<UMaterialExpressionCustom>(Mat, -400, 200);
	DebugNode->Description = TEXT("DebugWindViz");
	DebugNode->OutputType = CMOT_Float3;
	DebugNode->Code = TEXT(
		"// Normalize World Pos to UVW [0..1]\n"
		"float3 UVW = (WorldPos - VolumeOrigin.xyz) / max(VolumeSize.xyz, 1.0f);\n"
		"\n"
		"// Decode velocity (assuming texture stores [0..1] mapped to [-Max, +Max])\n"
		"float3 Velocity = (Texel.rgb * 2.0f - 1.0f) * MaxWindSpeed;\n"
		"float Speed = length(Velocity);\n"
		"float NormSpeed = saturate(Speed / 400.0f);\n"
		"\n"
		"// Visualize Speed: Green (0) -> Red (Max)\n"
		"float3 Color = lerp(float3(0,1,0), float3(1,0,0), NormSpeed);\n"
		"\n"
		"// Gray out if outside volume bounds\n"
		"if (any(UVW < 0.0f) || any(UVW > 1.0f))\n"
		"{\n"
		"    // Check pattern for out of bounds\n"
		"    float3 P = frac(WorldPos / 100.0f);\n"
		"    if (P.x < 0.5f ^ P.y < 0.5f ^ P.z < 0.5f) return float3(0.2, 0.2, 0.2);\n"
		"    return float3(0.1, 0.1, 0.1);\n"
		"}\n"
		"\n"
		"return Color;"
	);

	// Connect Inputs
	DebugNode->Inputs.Empty();
	{
		FCustomInput In;
		In.InputName = TEXT("WorldPos");
		In.Input.Connect(0, WorldPos);
		DebugNode->Inputs.Add(In);
	}
	{
		FCustomInput In;
		In.InputName = TEXT("VolumeOrigin");
		In.Input.Connect(0, OriginParam);
		DebugNode->Inputs.Add(In);
	}
	{
		FCustomInput In;
		In.InputName = TEXT("VolumeSize");
		In.Input.Connect(0, SizeParam);
		DebugNode->Inputs.Add(In);
	}
	{
		FCustomInput In;
		In.InputName = TEXT("Texel");
		In.Input.Connect(0, VolTexSample);
		DebugNode->Inputs.Add(In);
	}
	{
		FCustomInput In;
		In.InputName = TEXT("MaxWindSpeed");
		In.Input.Connect(0, MaxSpeedParam);
		DebugNode->Inputs.Add(In);
	}

	// UVW Calculation for Texture Sample
	auto* UVWCalc = AddExpression<UMaterialExpressionCustom>(Mat, -1000, 300);
	UVWCalc->Description = TEXT("CalcUVW");
	UVWCalc->OutputType = CMOT_Float3;
	UVWCalc->Code = TEXT("return saturate((WorldPos - VolumeOrigin.xyz) / max(VolumeSize.xyz, 1.0f));");
	UVWCalc->Inputs.Empty();
	{
		FCustomInput In; In.InputName = TEXT("WorldPos"); In.Input.Connect(0, WorldPos); UVWCalc->Inputs.Add(In);
	}
	{
		FCustomInput In; In.InputName = TEXT("VolumeOrigin"); In.Input.Connect(0, OriginParam); UVWCalc->Inputs.Add(In);
	}
	{
		FCustomInput In; In.InputName = TEXT("VolumeSize"); In.Input.Connect(0, SizeParam); UVWCalc->Inputs.Add(In);
	}
	VolTexSample->Coordinates.Connect(0, UVWCalc);

	// Connect to Emissive Color
	Mat->GetEditorOnlyData()->EmissiveColor.Connect(0, DebugNode);

	// ---- 3. Compile, Save & Register ----
	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();
	Package->MarkPackageDirty();

	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		PackagePath, FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;

	if (UPackage::SavePackage(Package, Mat, *PackageFilename, SaveArgs))
	{
		FAssetRegistryModule::AssetCreated(Mat);
		UE_LOG(LogWindMaterialBuilder, Log, TEXT("Debug Wind material saved: %s"), *PackagePath);
	}
	else
	{
		UE_LOG(LogWindMaterialBuilder, Error, TEXT("Failed to save Debug Wind material: %s"), *PackagePath);
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
	const int32 SizeX = 16;
	const int32 SizeY = 16;
	const int32 SizeZ = 8;
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

	if (MaterialIndex >= Component->GetNumMaterials()) return nullptr;

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
