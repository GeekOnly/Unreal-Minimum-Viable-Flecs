#include "Wind3DInteractiveCPUModule.h"
#include "IWindSolver.h"
#include "WindGridCPU.h"

IMPLEMENT_MODULE(FWind3DInteractiveCPUModule, Wind3DInteractiveCPU)

void FWind3DInteractiveCPUModule::StartupModule()
{
	// Register CPU solver factory so Core module can create FWindGridCPU
	// without knowing the concrete type (avoids circular dependency)
	FWindSolverFactory::CreateCPU.BindStatic([]() -> TUniquePtr<IWindSolver>
	{
		return MakeUnique<FWindGridCPU>();
	});
}

void FWind3DInteractiveCPUModule::ShutdownModule()
{
	FWindSolverFactory::CreateCPU.Unbind();
}
