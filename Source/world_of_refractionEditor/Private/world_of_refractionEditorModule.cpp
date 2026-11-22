// world_of_refractionEditorModule.cpp
// B-573: the editor-only module. It registers the agent play toolset with unreal-mcp.

#include "AgentPlay/AgentPlayToolset.h"
#include "Modules/ModuleManager.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

class FWorldOfRefractionEditorModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		UToolsetRegistry::RegisterToolsetClass(UWoRAgentPlayToolset::StaticClass());
	}

	virtual void ShutdownModule() override
	{
		if (UObjectInitialized())
		{
			UWoRAgentPlayToolset::CancelSequence(TEXT("the editor module unloaded"));
			UToolsetRegistry::UnregisterToolsetClass(UWoRAgentPlayToolset::StaticClass());
		}
	}
};

IMPLEMENT_MODULE(FWorldOfRefractionEditorModule, world_of_refractionEditor)
