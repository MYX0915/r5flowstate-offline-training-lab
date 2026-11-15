//=============================================================================//
// Purpose: Offline training-only, held-RMB steering to real skeletal points.
//=============================================================================//
#include "core/stdafx.h"
#include "training_aim.h"
#include "training_aim_math.h"
#include "training_weapon.h"
#include "local_entity_probe.h"
#include "net_bridge_internal.h"
#include "demo_bridge.h"
#include "game/client/classvar_natives.h"
#include "game/client/cliententitylist.h"
#include "game/client/portal/c_prop_portal.h"
#include "engine/enginetrace.h"
#include "public/bspflags.h"
#include "tier0/commandline.h"
#include "windows/id3dx.h"
#include "windows/input.h"
#include "imgui/imgui.h"
#include "vscript/vsquirrel_s21.h"
#include "vscript/languages/squirrel_re/vsquirrel.h"
#include "vscript/languages/squirrel_re/include/squirrel.h"
#include "game/shared/vscript_gamedll_defs.h"
#include <mutex>

static constexpr int kFlags = FCVAR_RELEASE | FCVAR_ACCESSIBLE_FROM_THREADS;
ConVar local_aim_enable("local_aim_enable","0",kFlags,"Enable offline training aim assistance while holding RMB.");
ConVar local_aim_part("local_aim_part","0",kFlags,"0 head, 1 neck, 2 chest, 3 hands, 4 knees.",true,0,true,4);
ConVar local_aim_radius("local_aim_radius","180",kFlags,"Centered selection circle radius in pixels.",true,20,true,600);
ConVar local_aim_speed("local_aim_speed","20",kFlags,"Exponential steering speed per second.",true,1,true,60);
ConVar local_aim_npc("local_aim_npc","1",kFlags,"Allow NPC training targets.");
ConVar local_aim_players("local_aim_players","1",kFlags,"Allow other player entities in the owned offline session.");
ConVar local_aim_circle("local_aim_circle","1",kFlags,"Draw the centered selection circle.");
static ConVar local_aim_trace("local_aim_trace","0",FCVAR_DEVELOPMENTONLY,
	"Log offline aim input, projected error and actual view angles at five samples per second.");

namespace
{
	using LookupFn = int(__fastcall*)(const void*,const char*);
	using OriginFn = Vector3D*(__fastcall*)(void*,Vector3D*,int);
	using CreateMoveFn = void(__fastcall*)(void*,int,float,bool);
	using GetCmdFn = void*(__fastcall*)(void*,int,int);
	CreateMoveFn s_createMove=nullptr;
	void* s_inputObject=nullptr;
	LookupFn s_lookup = nullptr;
	OriginFn s_origin = nullptr;
	std::mutex s_mutex;
	TrainingAimStatus_t s_status;
	uint64_t s_writes = 0;
	TrainingActor_t s_actors[512];
	struct ScriptPoint_t { TrainingActor_t actor; Vector3D point; int bone; bool visible; };
	ScriptPoint_t s_points[512];
	int s_pointCount=0, s_actorCount=0, s_pointsPart=-1;
	double s_pointsAt=0;
	bool s_scriptStarted=false;
	int s_viewProbeFrames=0;
	float s_viewProbeExpected[3]={};
	bool s_viewProbePending=false;
	void SteerCommand(void* command,float dt);
	void ApplyCommandAngles(void* command,const float* angles)
	{
		// Verified S21: usercmd angles +0x0C; CInput user +0x18, previous angles +0xBC.
		memcpy(static_cast<char*>(command)+0x0C,angles,12);
		memcpy(static_cast<char*>(s_inputObject)+0xD4,angles,12);
		DemoPlay_SetEngineViewAngles(angles);
	}
	void __fastcall CreateMove(void* object,int sequence,float dt,bool active)
	{
		TrainingWeapon_ClearRecoil();
		s_createMove(object,sequence,dt,active);
		TrainingWeapon_ClearRecoil();
		if (!active || !CommandLine()->CheckParm("-offline") || !s_inputObject || !TrainingAim_SessionReady()) return;
		void** const table=*static_cast<void***>(s_inputObject);
		if (!table || !table[9]) return;
		// S21 wrapper slot 28 returns packet+0x220 (a number), not a command pointer.
		void* const command=reinterpret_cast<GetCmdFn>(table[9])(s_inputObject,-1,sequence);
		if (reinterpret_cast<uintptr_t>(command)<0x100000000ULL || reinterpret_cast<uintptr_t>(command)>0x7fffffffffffULL) return;
		const uint32_t* const words=static_cast<const uint32_t*>(command);
		if (words[0]!=static_cast<uint32_t>(sequence)) return;
		if (s_viewProbePending && (GetAsyncKeyState(VK_RBUTTON)&0x8000)==0)
		{
			ApplyCommandAngles(command,s_viewProbeExpected);
			s_viewProbePending=false;s_viewProbeFrames=10;
		}
		else SteerCommand(command,dt);
		if (!local_aim_trace.GetBool()) return;
		static int lastLogged=-1000;
		if (sequence-lastLogged<160) return;
		lastLogged=sequence;
		float view[3]={};DemoPlay_GetEngineViewAngles(view);
		Msg(eDLL_T::CLIENT,"[TRAINING-CMD] sequence=%d dt=%.6f raw=%08X %08X %08X %08X %08X %08X %08X %08X engine=(%.4f %.4f)\n",
			sequence,dt,words[0],words[1],words[2],words[3],words[4],words[5],words[6],words[7],view[0],view[1]);
	}
	const char* const s_sampleScript=R"sq(
thread (void function(){
 array<array<string> > names=[["def_c_head"],["def_c_neckB","def_c_neckA"],["def_c_spineC","def_c_spineB"],["def_l_wrist","def_r_wrist"],["def_l_knee","def_r_knee"]];
 array<entity> targets;
 float nextScan=0;
 while(IsValid(GetLocalClientPlayer())){
  TrainingAim_ResetPoints();
  if(GetConVarBool("local_aim_enable") && TrainingAim_SessionReady()){
   entity viewer=GetLocalClientPlayer();
   if(Time()>=nextScan){targets=GetNPCArray();targets.extend(GetPlayerArray());nextScan=Time()+0.25;}
   int part=GetConVarInt("local_aim_part");
   foreach(entity n in targets){
    if(!IsValid(n)||n==viewer||n.GetHealth()<=0)continue;
    foreach(string name in names[part]){
     int bone=n.LookupBone(name);if(bone<0)continue;
     vector point=n.GetBoneOrigin(bone);
     TraceResults tr=TraceLine(viewer.EyePosition(),point,viewer,TRACE_MASK_SHOT,TRACE_COLLISION_GROUP_NONE);
     TrainingAim_SubmitPoint(n.GetEntIndex(),bone,point,tr.hitEnt==n||tr.fraction>0.99);
    }
   }
  }
  WaitFrame();
 }
})();
)sq";

	SQRESULT ResetPoints(HSQUIRRELVM vm)
	{
		s_pointCount=0;s_actorCount=LocalEntityProbe_AimActors(s_actors,512);s_pointsAt=Plat_FloatTime();s_pointsPart=local_aim_part.GetInt();
		SCRIPT_CHECK_AND_RETURN(vm,SQ_OK);
	}
	SQRESULT SessionReady(HSQUIRRELVM vm)
	{
		sq_pushbool(vm,TrainingAim_SessionReady()?SQTrue:SQFalse);SCRIPT_CHECK_AND_RETURN(vm,SQ_OK);
	}
	SQRESULT SubmitPoint(HSQUIRRELVM vm)
	{
		SQInteger index=0,bone=0;SQBool visible=SQFalse;const SQVector3D* point=nullptr;
		if(SQ_FAILED(sq_getinteger(vm,2,&index))||SQ_FAILED(sq_getinteger(vm,3,&bone))
			||SQ_FAILED(sq_getvector(vm,4,&point))||SQ_FAILED(sq_getbool(vm,5,&visible))||!point||s_pointCount>=512)
			SCRIPT_CHECK_AND_RETURN(vm,SQ_OK);
		for(int i=0;i<s_actorCount;++i)
		{
			if((s_actors[i].handle&0xffff)!=index)continue;
			if(!std::isfinite(point->x)||!std::isfinite(point->y)||!std::isfinite(point->z))break;
			s_points[s_pointCount++]={s_actors[i],Vector3D(point->x,point->y,point->z),static_cast<int>(bone),visible!=SQFalse};break;
		}
		SCRIPT_CHECK_AND_RETURN(vm,SQ_OK);
	}
	const char* s_names[5][3] = {
		{"def_c_head",nullptr,nullptr}, {"def_c_neckB","def_c_neckA",nullptr},
		{"def_c_spineC","def_c_spineB",nullptr}, {"def_l_wrist","def_r_wrist",nullptr},
		{"def_l_knee","def_r_knee",nullptr}};

	bool BonePoint(void* entity, const char* name, Vector3D& out, int& index)
	{
		// These fields are used by this image's own LookupBone/GetBoneOrigin wrappers.
		const void* const hdr = *reinterpret_cast<const void* const*>(static_cast<const char*>(entity)+0x1000);
		if (!hdr) return false;
		const char* const studio = *reinterpret_cast<const char* const*>(static_cast<const char*>(hdr)+8);
		if (!studio) return false;
		const int count = *reinterpret_cast<const int16_t*>(studio+0x74);
		if (count <= 0 || count > 512) return false;
		index = s_lookup(hdr,name);
		if (index < 0 || index >= count) return false;
		if (!s_origin(entity,&out,index)) return false;
		return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z)
			&& (out-Portal_AbsOrigin(entity)).LengthSqr() < 1024.0f*1024.0f;
	}

	bool Matrix(const TrainingCamera_t& camera, float width,float height,float out[16])
	{
		if (width <= 0 || height <= 0) return false;
		Vector3D forward,right,up;
		AngleVectors(QAngle(camera.angles[0],camera.angles[1],camera.angles[2]),&forward,&right,&up);
		const float ty=tanf(camera.fov*0.00872664626f)*0.75f;
		return EntityProbe_BasisMatrix(camera.origin,&forward.x,&right.x,&up.x,ty*width/height,ty,out);
	}

	void Select(const void* local, const TrainingCamera_t& camera, int width,int height,TrainingAimStatus_t& state)
	{
		float matrix[16]; if (!Matrix(camera,static_cast<float>(width),static_cast<float>(height),matrix)) return;
		const int rect[4]={0,0,width,height};
		if(Plat_FloatTime()-s_pointsAt>0.2 || s_pointsPart!=local_aim_part.GetInt())return;
		float best=local_aim_radius.GetFloat()*local_aim_radius.GetFloat();
		for (int i=0;i<s_pointCount;++i)
		{
			const ScriptPoint_t& sample=s_points[i];
			const TrainingActor_t& actor=sample.actor;
			if (!TrainingAim_Allowed(actor.kind,false,local_aim_npc.GetBool(),local_aim_players.GetBool())) continue;
			void* const entity=ClientEntityList_EntityAt(actor.handle&0xffff,actor.handle>>16);
			if (!entity || entity==local || actor.health<0 || actor.health>0x10000) continue;
			if (*reinterpret_cast<const int*>(static_cast<const char*>(entity)+actor.health)<=0) continue;
			++state.considered;
				const Vector3D& point=sample.point;float screen[2];
				if (!EntityProbe_Project(matrix,&point.x,rect,screen)
					|| !TrainingAim_InCircle(screen[0],screen[1],static_cast<float>(width),static_cast<float>(height),local_aim_radius.GetFloat())) continue;
				const float dx=screen[0]-width*0.5f,dy=screen[1]-height*0.5f,score=dx*dx+dy*dy;
				if (score>best) continue;
				if (!sample.visible) {++state.blocked;continue;}
				best=score; state.handle=actor.handle; state.bone=sample.bone;
				memcpy(state.point,&point.x,12); memcpy(state.screen,screen,8); state.error=sqrtf(score);
		}
	}

	void SteerCommand(void* command,float dt)
	{
		const HWND window=DirectX_GetMainWindow();
		if (!TrainingAim_ShouldSteer(local_aim_enable.GetBool(),true,window && GetForegroundWindow()==window,
			g_bBlockInput.load(),(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0,true)) return;
		const void* const local=ClassVar_LocalPlayer();RECT rect={};TrainingCamera_t camera={};
		if (!local || DemoPlay_LocalObserverMode()>0 || !LocalEntityProbe_Camera(camera) || !GetClientRect(window,&rect)) return;
		TrainingAimStatus_t state;Select(local,camera,rect.right,rect.bottom,state);
		if (state.handle==UINT32_MAX) return;
		float current[3],target[3],next[3];
		memcpy(current,static_cast<const char*>(command)+0x0C,12);
		if (!std::isfinite(current[0]) || !std::isfinite(current[1]) || !std::isfinite(current[2])
			|| !TrainingAim_Angles(camera.origin,state.point,target)) return;
		TrainingAim_Smooth(current,target,dt,local_aim_speed.GetFloat(),next);
		if (!TrainingAim_ShouldSteer(local_aim_enable.GetBool(),true,GetForegroundWindow()==window,g_bBlockInput.load(),
			(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0,true)) return;
		ApplyCommandAngles(command,next);++s_writes;
	}

	void Dump_f(const CCommand&)
	{
		const TrainingAimStatus_t state=TrainingAim_Status();
		Msg(eDLL_T::CLIENT,"[TRAINING-AIM] state=%s held=%d part=%d target=%u bone=%d errorPx=%.2f writes=%llu considered=%d missing=%d blocked=%d point=(%.2f %.2f %.2f)\n",
			TrainingAim_StateName(state.state),state.held,local_aim_part.GetInt(),state.handle,state.bone,state.error,
			static_cast<unsigned long long>(state.writes),state.considered,state.missing,state.blocked,state.point[0],state.point[1],state.point[2]);
	}
	ConCommand s_dump("local_aim_dump",Dump_f,"Dump the offline training aim state.",FCVAR_DEVELOPMENTONLY);

	void ViewProbe_f(const CCommand&)
	{
		if (!CommandLine()->CheckParm("-offline") || !TrainingAim_SessionReady()
			|| (GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0)
		{
			Warning(eDLL_T::CLIENT,"[TRAINING-VIEW-PROBE] requires offline session and released RMB\n");
			return;
		}
		if (!DemoPlay_GetEngineViewAngles(s_viewProbeExpected)) return;
		s_viewProbeExpected[1]=TrainingAim_Wrap(s_viewProbeExpected[1]+5.0f);
		s_viewProbePending=true;
		Msg(eDLL_T::CLIENT,"[TRAINING-VIEW-PROBE] requested=(%.4f %.4f)\n",s_viewProbeExpected[0],s_viewProbeExpected[1]);
	}
	ConCommand s_viewProbe("local_aim_view_probe",ViewProbe_f,"Offline diagnostic: add five yaw degrees and log the next ten rendered-camera samples.",FCVAR_DEVELOPMENTONLY);

	void Bones_f(const CCommand& args)
	{
		if (!s_lookup || !s_origin) { Warning(eDLL_T::CLIENT,"[TRAINING-BONES] reader unavailable\n"); return; }
		const int count=LocalEntityProbe_AimActors(s_actors,512);
		void* entity=nullptr; uint32_t handle=UINT32_MAX;
		for (int i=0;i<count;++i)
		{
			if (s_actors[i].kind!=EntityProbeKind_t::Npc) continue;
			if (args.ArgC()>1 && (s_actors[i].handle&0xffff)!=static_cast<uint32_t>(atoi(args.Arg(1)))) continue;
			entity=ClientEntityList_EntityAt(s_actors[i].handle&0xffff,s_actors[i].handle>>16);
			handle=s_actors[i].handle; if (entity) break;
		}
		if (!entity) { Warning(eDLL_T::CLIENT,"[TRAINING-BONES] no matching live NPC\n"); return; }
		for (int part=0;part<5;++part) for (int n=0;n<3 && s_names[part][n];++n)
		{
			Vector3D point(0,0,0); int bone=-1; const bool valid=BonePoint(entity,s_names[part][n],point,bone);
			const Vector3D origin=Portal_AbsOrigin(entity);
			Msg(eDLL_T::CLIENT,"[TRAINING-BONES] handle=%u entity=%p part=%d name=%s bone=%d valid=%d position=(%.2f %.2f %.2f) origin=(%.2f %.2f %.2f)\n",
				handle,entity,part,s_names[part][n],bone,valid,point.x,point.y,point.z,origin.x,origin.y,origin.z);
		}
	}
	ConCommand s_bones("local_aim_bones",Bones_f,"Read actual NPC bone indices and positions.",FCVAR_DEVELOPMENTONLY);
}

bool TrainingAim_Enabled(void) { return local_aim_enable.GetBool(); }
bool TrainingAim_SessionReady(void)
{
	const uintptr_t signon=NetObs_ClientStateSignonAddr(),inGame=NetObs_InGameFlagAddr();
	return signon && inGame && *reinterpret_cast<const int*>(signon)>=8
		&& *reinterpret_cast<const unsigned char*>(inGame)!=0;
}
void TrainingAim_RegisterNatives(CSquirrelVM* vm)
{
	s_scriptStarted=false;s_pointCount=0;
	if(Script_RegisterFuncTC_S21(vm,"TrainingAim_SessionReady",reinterpret_cast<void*>(SessionReady),"bool","")==SQ_ERROR)
		Warning(eDLL_T::CLIENT,"[TRAINING-AIM] session readiness registration failed\n");
	if(Script_RegisterFuncTC_S21(vm,"TrainingAim_ResetPoints",reinterpret_cast<void*>(ResetPoints),"void","")==SQ_ERROR
		||Script_RegisterFuncTC_S21(vm,"TrainingAim_SubmitPoint",reinterpret_cast<void*>(SubmitPoint),"void","int index, int bone, vector point, bool visible")==SQ_ERROR)
		Warning(eDLL_T::CLIENT,"[TRAINING-AIM] script sampler registration failed\n");
}
TrainingAimStatus_t TrainingAim_Status(void) { std::lock_guard<std::mutex> lock(s_mutex); return s_status; }
const char* TrainingAim_StateName(int state)
{
	static const char* names[]={u8"已关闭",u8"需要离线训练场",u8"游戏未获得焦点",u8"菜单已打开",u8"没有有效视角",u8"骨骼接口不可用",u8"范围内没有可见目标",u8"按住鼠标右键瞄准",u8"正在瞄准",u8"视角接口不可用"};
	return state>=0 && state<10 ? names[state] : u8"未知";
}

void TrainingAim_Frame(float dt)
{
	TrainingAimStatus_t state; TrainingCamera_t camera;
	const void* const local=ClassVar_LocalPlayer();
	if(!local)s_scriptStarted=false;
	if(local && TrainingAim_SessionReady() && !s_scriptStarted && CommandLine()->CheckParm("-offline"))
	{
		s_scriptStarted=true;
		Script_Execute_S21(s_sampleScript,SQCONTEXT::CLIENT);
	}
	const HWND window=DirectX_GetMainWindow(); RECT rect={};
	state.held=(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0;
	if (!local_aim_enable.GetBool()) state.state=0;
	else if (!CommandLine()->CheckParm("-offline")) state.state=1;
	else if (!window || GetForegroundWindow()!=window) state.state=2;
	else if (g_bBlockInput.load()) state.state=3;
	else if (!local || !TrainingAim_SessionReady() || DemoPlay_LocalObserverMode()>0 || !LocalEntityProbe_Camera(camera) || !GetClientRect(window,&rect)) state.state=4;
	else if (!s_lookup || !s_origin) state.state=5;
	else
	{
		Select(local,camera,rect.right,rect.bottom,state);
		state.state=state.handle==UINT32_MAX ? 6 : state.held ? 8 : 7;
	}
	state.writes=s_writes;
	if (s_viewProbeFrames>0)
	{
		float view[3]={};TrainingCamera_t observed;
		const bool engineValid=DemoPlay_GetEngineViewAngles(view);
		const bool cameraValid=LocalEntityProbe_Camera(observed);
		Msg(eDLL_T::CLIENT,"[TRAINING-VIEW-PROBE] remaining=%d engineValid=%d engine=(%.4f %.4f) cameraValid=%d camera=(%.4f %.4f) expected=(%.4f %.4f) dt=%.6f\n",
			s_viewProbeFrames--,engineValid,view[0],view[1],cameraValid,observed.angles[0],observed.angles[1],s_viewProbeExpected[0],s_viewProbeExpected[1],dt);
	}
	static double nextTrace=0;
	const double now=Plat_FloatTime();
	if (local_aim_trace.GetBool() && CommandLine()->CheckParm("-offline") && now>=nextTrace)
	{
		nextTrace=now+0.2;
		float view[3]={};
		const bool valid=DemoPlay_GetEngineViewAngles(view);
		Msg(eDLL_T::CLIENT,"[TRAINING-AIM-TRACE] t=%.3f state=%d held=%d part=%d target=%u bone=%d errorPx=%.3f writes=%llu viewValid=%d view=(%.4f %.4f %.4f)\n",
			now,state.state,state.held,local_aim_part.GetInt(),state.handle,state.bone,state.error,
			static_cast<unsigned long long>(state.writes),valid,view[0],view[1],view[2]);
	}
	std::lock_guard<std::mutex> lock(s_mutex); s_status=state;
}

void TrainingAim_Draw(void)
{
	if(!TrainingAim_SessionReady())return;
	const ImVec2 size=ImGui::GetIO().DisplaySize;
	ImDrawList* const draw=ImGui::GetForegroundDrawList();
	if (local_aim_circle.GetBool())
		draw->AddCircle(ImVec2(size.x*0.5f,size.y*0.5f),local_aim_radius.GetFloat(),local_aim_enable.GetBool()?IM_COL32(80,210,255,210):IM_COL32(180,190,200,180),96,1.5f);
	const TrainingAimStatus_t state=TrainingAim_Status();
	if (local_aim_enable.GetBool() && state.handle!=UINT32_MAX)
		draw->AddCircleFilled(ImVec2(state.screen[0],state.screen[1]),4,IM_COL32(255,100,80,255));
}

void VTrainingAim::GetAdr(void) const { LogFunAdr("Studio_LookupBone",s_lookup); LogFunAdr("Animating_GetBoneOrigin",s_origin); }
void VTrainingAim::GetFun(void) const
{
	Module_FindPattern(g_GameDll,"48 89 5C 24 08 57 48 83 EC 30 8B FA 0F 29 74 24 20 33 D2 41 0F B6 D9 0F 28 F2 44 8D 42 01").GetPtr(s_createMove);
	const CMemory commandWrapper=Module_FindPattern(g_GameDll,"48 83 EC 28 48 8B 05 ?? ?? ?? ?? 48 8D 0D ?? ?? ?? ?? 44 8B C2 BA FF FF FF FF FF 50 48 48 85 C0");
	if (commandWrapper) s_inputObject=commandWrapper.Offset(11).ResolveRelativeAddress(3,7).RCast<void*>();
	if (!s_createMove || !s_inputObject) Warning(eDLL_T::CLIENT,"[TRAINING-CMD] input command surface unresolved\n");
	Module_FindPattern(g_GameDll,"48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 20 4C 8B 49 08 4C 8B FA 33 FF").GetPtr(s_lookup);
	// The short script-wrapper call pattern also matches unrelated position getters.
	// Match the bone-matrix translation extraction, then use x64 unwind metadata.
	const CMemory body=Module_FindPattern(g_GameDll,"4C 8D 44 24 20 0F B7 D7 48 8B CB E8 ?? ?? ?? ?? F3 0F 10 44 24 2C 48 8B C6 F3 0F 10 4C 24 3C");
	DWORD64 imageBase=0;
	const PRUNTIME_FUNCTION entry=body ? RtlLookupFunctionEntry(body.GetPtr(),&imageBase,nullptr) : nullptr;
	if (entry) s_origin=reinterpret_cast<OriginFn>(imageBase+entry->BeginAddress);
	if (!s_lookup || !s_origin) Warning(eDLL_T::CLIENT,"[TRAINING-AIM] bone API unresolved -- aim assistance unavailable\n");
}
void VTrainingAim::Detour(bool attach) const
{
	if (s_createMove && s_inputObject) DetourSetup(&s_createMove,&CreateMove,attach);
}
