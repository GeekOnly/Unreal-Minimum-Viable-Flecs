#pragma once

#include "CoreMinimal.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceWind3D.generated.h"

class UWindSubsystem;

/**
 * Niagara Data Interface for Wind3DInteractive.
 * Exposes SampleWindVelocity(float3 WorldPos) to Niagara CPU/GPU scripts.
 *
 * Usage in Niagara:
 *   1. Add a "Wind3D" Data Interface to your emitter
 *   2. Call SampleWindVelocity in a Particle Update script
 *   3. Apply the returned velocity as force or direct velocity
 */
UCLASS(EditInlineNew, Category = "Wind", meta = (DisplayName = "Wind 3D Interactive"))
class WIND3DINTERACTIVE_API UNiagaraDataInterfaceWind3D : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

public:
	// UNiagaraDataInterface overrides
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc) override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return Target == ENiagaraSimTarget::CPUSim; }
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

	// VM function implementations
	void SampleWindVelocity(FVectorVMExternalFunctionContext& Context);
	void SampleWindIntensity(FVectorVMExternalFunctionContext& Context);

private:
	UWindSubsystem* GetWindSubsystem() const;
};
