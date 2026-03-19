#include "NiagaraDataInterfaceWind3D.h"
#include "WindSubsystem.h"
#include "WindTextureManager.h"
#include "NiagaraTypes.h"
#include "NiagaraComponent.h"

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceWind3D"

static const FName SampleWindVelocityName(TEXT("SampleWindVelocity"));
static const FName SampleWindIntensityName(TEXT("SampleWindIntensity"));

UNiagaraDataInterfaceWind3D::UNiagaraDataInterfaceWind3D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// CPU-only DI – base class handles proxy automatically
}

void UNiagaraDataInterfaceWind3D::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	// SampleWindVelocity(float3 WorldPos) -> float3 Velocity
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = SampleWindVelocityName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Wind3D")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("WorldPosition")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.SetDescription(LOCTEXT("SampleWindVelocityDesc", "Sample wind velocity at a world position from the Wind3D grid."));
		OutFunctions.Add(Sig);
	}

	// SampleWindIntensity(float3 WorldPos) -> float Intensity, float Turbulence
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = SampleWindIntensityName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Wind3D")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("WorldPosition")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Intensity")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Turbulence")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.SetDescription(LOCTEXT("SampleWindIntensityDesc", "Sample normalized wind intensity and turbulence at a world position."));
		OutFunctions.Add(Sig);
	}
}

void UNiagaraDataInterfaceWind3D::GetVMExternalFunction(
	const FVMExternalFunctionBindingInfo& BindingInfo,
	void* InstanceData,
	FVMExternalFunction& OutFunc)
{
	if (BindingInfo.Name == SampleWindVelocityName)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceWind3D::SampleWindVelocity);
	}
	else if (BindingInfo.Name == SampleWindIntensityName)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceWind3D::SampleWindIntensity);
	}
}

void UNiagaraDataInterfaceWind3D::SampleWindVelocity(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<float> InPosX(Context);
	VectorVM::FExternalFuncInputHandler<float> InPosY(Context);
	VectorVM::FExternalFuncInputHandler<float> InPosZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVelX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVelY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVelZ(Context);

	UWindSubsystem* WindSys = GetWindSubsystem();

	for (int32 I = 0; I < Context.GetNumInstances(); ++I)
	{
		const FVector WorldPos(InPosX.GetAndAdvance(), InPosY.GetAndAdvance(), InPosZ.GetAndAdvance());
		FVector Vel = FVector::ZeroVector;
		if (WindSys)
		{
			Vel = WindSys->QueryWindAtPosition(WorldPos);
		}
		*OutVelX.GetDestAndAdvance() = (float)Vel.X;
		*OutVelY.GetDestAndAdvance() = (float)Vel.Y;
		*OutVelZ.GetDestAndAdvance() = (float)Vel.Z;
	}
}

void UNiagaraDataInterfaceWind3D::SampleWindIntensity(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<float> InPosX(Context);
	VectorVM::FExternalFuncInputHandler<float> InPosY(Context);
	VectorVM::FExternalFuncInputHandler<float> InPosZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutIntensity(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutTurbulence(Context);

	UWindSubsystem* WindSys = GetWindSubsystem();

	for (int32 I = 0; I < Context.GetNumInstances(); ++I)
	{
		const FVector WorldPos(InPosX.GetAndAdvance(), InPosY.GetAndAdvance(), InPosZ.GetAndAdvance());
		float Intensity = 0.f;
		float Turbulence = 0.f;
		if (WindSys)
		{
			Intensity = WindSys->GetWindIntensityAtPosition(WorldPos);
			Turbulence = WindSys->GetWindTurbulenceAtPosition(WorldPos);
		}
		*OutIntensity.GetDestAndAdvance() = Intensity;
		*OutTurbulence.GetDestAndAdvance() = Turbulence;
	}
}

bool UNiagaraDataInterfaceWind3D::Equals(const UNiagaraDataInterface* Other) const
{
	return Other && Other->IsA(GetClass());
}

bool UNiagaraDataInterfaceWind3D::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination)) return false;
	return true;
}

UWindSubsystem* UNiagaraDataInterfaceWind3D::GetWindSubsystem() const
{
	UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
	if (!World) return nullptr;
	return World->GetSubsystem<UWindSubsystem>();
}

#undef LOCTEXT_NAMESPACE
