#pragma once
#if defined(CLIENT_DLL) && defined(SDK_WIP)
#include "thirdparty/detours/include/idetour.h"
#include "tier1/convar.h"
class CSquirrelVM;
extern ConVar local_model_highlight;
void TrainingHighlight_RegisterNatives(CSquirrelVM* vm);
void TrainingHighlight_Frame(void);
class VTrainingHighlight : public IDetour
{
	virtual void GetAdr(void) const;
	virtual void GetFun(void) const;
	virtual void GetVar(void) const { }
	virtual void GetCon(void) const { }
	virtual void Detour(bool attach) const;
};
#endif // CLIENT_DLL && SDK_WIP
