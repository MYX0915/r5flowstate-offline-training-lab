//=============================================================================//
// Purpose: Owned offline weapon training options; client prediction half.
//=============================================================================//
#include "core/stdafx.h"
#include "training_weapon.h"
#include "game/client/classvar_natives.h"
#include "game/client/pred_authority.h"
#include "tier0/commandline.h"
#include <cmath>

static constexpr int kTrainingWeaponFlags=FCVAR_RELEASE|FCVAR_USERINFO|FCVAR_ACCESSIBLE_FROM_THREADS;
ConVar local_no_recoil("local_no_recoil","0",kTrainingWeaponFlags,"Suppress weapon recoil in the owned offline session.");
ConVar local_bullet_assist("local_bullet_assist","0",kTrainingWeaponFlags,"Correct offline shots toward visible NPC/player targets.");
ConVar local_bullet_angle("local_bullet_angle","10",kTrainingWeaponFlags,"Offline correction half-angle; 180 enables nearest visible target in all directions.",true,0,true,180);

static uint64_t s_clearCalls=0,s_nonzeroClears=0;
static float s_peakBeforeClear=0;
static bool s_recoilReady=false;
static void TrainingWeapon_Dump(const CCommand&)
{
	Msg(eDLL_T::CLIENT,"[TRAINING-WEAPON] recoil=%d ready=%d clears=%llu nonzero=%llu peak=%.6f bullet=%d cone=%.2f\n",
		local_no_recoil.GetBool(),s_recoilReady,static_cast<unsigned long long>(s_clearCalls),static_cast<unsigned long long>(s_nonzeroClears),
		s_peakBeforeClear,local_bullet_assist.GetBool(),local_bullet_angle.GetFloat());
}
static ConCommand s_weaponDump("local_weapon_training_dump",TrainingWeapon_Dump,"Read offline weapon training diagnostics.",FCVAR_DEVELOPMENTONLY);

void TrainingWeapon_ClearRecoil(void)
{
	if (!local_no_recoil.GetBool() || !CommandLine()->CheckParm("-offline")) return;
	void* const player=const_cast<void*>(ClassVar_LocalPlayer());
	if (!player) return;
	using GetMapFn=void*(__fastcall*)(void*);
	void* const map=reinterpret_cast<GetMapFn>((*static_cast<void***>(player))[VIDX_GetPredDescMap])(player);
	if (!map) return;
	static void* previousMap=nullptr;
	static int offsets[6]={-1,-1,-1,-1,-1,-1};
	static const char* names[6]={"m_currentFrameLocalPlayer.m_vecPunchBase_Angle","m_currentFrameLocalPlayer.m_vecPunchBase_AngleVel",
		"m_currentFrameLocalPlayer.m_vecPunchWeapon_Angle","m_currentFrameLocalPlayer.m_vecPunchWeapon_AngleVel.x",
		"m_currentFrameLocalPlayer.m_vecPunchWeapon_AngleVel.y","m_currentFrameLocalPlayer.m_vecPunchWeapon_AngleVel.z"};
	if (map!=previousMap)
	{
		const uintptr_t optimized=*reinterpret_cast<uintptr_t*>(static_cast<char*>(map)+DMAP_OPTIMIZED);
		if (!optimized) return;
		const uintptr_t info=optimized+OPT_INFO_STRIDE*PC_NETWORKED_ONLY;
		const uintptr_t fields=*reinterpret_cast<uintptr_t*>(info+INFO_FLAT_FIELDS);
		const int count=*reinterpret_cast<int*>(info+INFO_FLAT_COUNT);
		if (!fields || count<1 || count>4096) return;
		for (int& offset:offsets) offset=-1;
		for (int n=0;n<count;++n)
		{
			const uintptr_t field=fields+n*TD_STRIDE;
			const char* const name=*reinterpret_cast<const char**>(field+TD_FIELDNAME);
			if (!name) continue;
			for (int i=0;i<6;++i)
				if (!strcmp(name,names[i])) offsets[i]=*reinterpret_cast<const int*>(field+TD_FLATOFFSET0);
		}
		previousMap=map;
		Msg(eDLL_T::CLIENT,"[TRAINING-RECOIL] live offsets=%d,%d,%d,%d,%d,%d\n",offsets[0],offsets[1],offsets[2],offsets[3],offsets[4],offsets[5]);
	}
	for (int offset:offsets) if (offset<1 || offset>0x20000) {s_recoilReady=false;return;}
	s_recoilReady=true;
	float peak=0;
	for (int i=0;i<6;++i)
	{
		float* const field=reinterpret_cast<float*>(static_cast<char*>(player)+offsets[i]);
		const int count=i<3 ? 3 : 1;
		for (int n=0;n<count;++n)
		{
			if (!std::isfinite(field[n])) return;
			peak=Max(peak,fabsf(field[n]));
		}
	}
	++s_clearCalls;if (peak>0.000001f) ++s_nonzeroClears;
	s_peakBeforeClear=Max(s_peakBeforeClear,peak);
	for (int i=0;i<6;++i) memset(static_cast<char*>(player)+offsets[i],0,(i<3 ? 3 : 1)*sizeof(float));
}
