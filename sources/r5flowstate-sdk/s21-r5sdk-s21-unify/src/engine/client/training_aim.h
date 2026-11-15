#pragma once
#if defined(CLIENT_DLL) && defined(SDK_WIP)
#include "tier1/convar.h"
#include "thirdparty/detours/include/idetour.h"
#include <cstdint>

extern ConVar local_aim_enable, local_aim_part, local_aim_radius, local_aim_speed;
extern ConVar local_aim_npc, local_aim_players, local_aim_circle;
class CSquirrelVM;
void TrainingAim_RegisterNatives(CSquirrelVM* vm);
struct TrainingAimStatus_t
{
	int state = 0, considered = 0, missing = 0, blocked = 0, bone = -1;
	uint32_t handle = UINT32_MAX;
	uint64_t writes = 0;
	float point[3] = {}, screen[2] = {}, error = 0;
	bool held = false;
};
bool TrainingAim_Enabled(void);
bool TrainingAim_SessionReady(void);
void TrainingAim_Frame(float dt);
void TrainingAim_Draw(void);
TrainingAimStatus_t TrainingAim_Status(void);
const char* TrainingAim_StateName(int state);

class VTrainingAim : public IDetour
{
	virtual void GetAdr(void) const;
	virtual void GetFun(void) const;
	virtual void GetVar(void) const { }
	virtual void GetCon(void) const { }
	virtual void Detour(bool attach) const;
};
#endif // CLIENT_DLL && SDK_WIP
