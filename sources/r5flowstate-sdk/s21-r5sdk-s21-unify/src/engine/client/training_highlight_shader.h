#pragma once
#if defined(CLIENT_DLL) && defined(SDK_WIP)
#include "thirdparty/detours/include/idetour.h"
bool TrainingHighlight_ShaderReady(void);
class VTrainingHighlightShader : public IDetour
{
	virtual void GetAdr(void) const;
	virtual void GetFun(void) const;
	virtual void GetVar(void) const { }
	virtual void GetCon(void) const { }
	virtual void Detour(bool attach) const;
};
#endif // CLIENT_DLL && SDK_WIP
