//=============================================================================//
// Purpose: One native model context with depth-tested red fill and green outline.
//=============================================================================//
#include "core/stdafx.h"
#include "training_highlight.h"
#include "training_aim.h"
#include "training_highlight_script.h"
#include "training_highlight_shader.h"
#include "game/client/classvar_natives.h"
#include "tier0/commandline.h"
#include "vscript/vsquirrel_s21.h"
#include "vscript/languages/squirrel_re/vsquirrel.h"
#include "vscript/languages/squirrel_re/include/squirrel.h"

ConVar local_model_highlight("local_model_highlight","0",FCVAR_RELEASE|FCVAR_ACCESSIBLE_FROM_THREADS,
	"Offline training model outline and depth-tested fill.");
namespace
{
	unsigned char** s_table=nullptr;
	struct Saved_t { int id=-1; unsigned char bytes[52]={}; };
	Saved_t s_saved;
	unsigned int s_savedGeneration=0;
	bool s_scriptStarted=false;
	void Restore(void)
	{
		if(s_table && *s_table && s_savedGeneration==g_nS21ClientVMGeneration)
		{
			const int count=*reinterpret_cast<const int*>(reinterpret_cast<const char*>(s_table)+0x18);
			if(s_saved.id>=0 && s_saved.id<count)memcpy(*s_table+s_saved.id*52,s_saved.bytes,52);
		}
		s_saved.id=-1;
	}
	SQRESULT Configure(HSQUIRRELVM vm)
	{
		SQInteger context=-1;
		sq_getinteger(vm,2,&context);
		// The same native getter reads count at table-pointer storage + 0x18.
		const int count=s_table ? *reinterpret_cast<const int*>(reinterpret_cast<const char*>(s_table)+0x18) : 0;
		const bool valid=CommandLine()->CheckParm("-offline") && TrainingHighlight_ShaderReady() && s_table && *s_table
			&&context>=0&&context<count&&context<255;
		if(valid)
		{
			Restore();s_savedGeneration=g_nS21ClientVMGeneration;
			unsigned char* const entry=*s_table+context*52;
			s_saved.id=static_cast<int>(context);memcpy(s_saved.bytes,entry,52);
			uint32_t bits;memcpy(&bits,entry,4);
			// Function 2 is the verified depth branch; function 169 supplies the outer contour.
			bits=(bits&0x3fff0000u)|2u|(169u<<8)|0x40000000u;
			memcpy(entry,&bits,4);
			const float params[6]={0,1,0,1,0,0};memcpy(entry+4,params,sizeof(params));
			const int drawMode=0;memcpy(entry+48,&drawMode,sizeof(drawMode));
			Msg(eDLL_T::CLIENT,"[TRAINING-HIGHLIGHT] configured single context=%d count=%d\n",static_cast<int>(context),count);
		}
		else
		{
			static double nextWarning=0;const double now=Plat_FloatTime();
			if(now>=nextWarning){nextWarning=now+5;Warning(eDLL_T::CLIENT,"[TRAINING-HIGHLIGHT] waiting for supported renderer/context\n");}
		}
		sq_pushbool(vm,valid?SQTrue:SQFalse);SCRIPT_CHECK_AND_RETURN(vm,SQ_OK);
	}
	SQRESULT RestoreScript(HSQUIRRELVM vm) { Restore();SCRIPT_CHECK_AND_RETURN(vm,SQ_OK); }
	SQRESULT OriginalParam(HSQUIRRELVM vm)
	{
		SQInteger context=-1,parameter=-1;sq_getinteger(vm,2,&context);sq_getinteger(vm,3,&parameter);
		SQVector3D value={};
		const int count=s_table ? *reinterpret_cast<const int*>(reinterpret_cast<const char*>(s_table)+0x18) : 0;
		if(s_table&&*s_table&&context>=0&&context<count&&parameter>=0&&parameter<2)
		{
			const unsigned char* entry=*s_table+context*52;
			if(s_saved.id==context&&s_savedGeneration==g_nS21ClientVMGeneration)entry=s_saved.bytes;
			memcpy(&value,entry+4+parameter*12,12);
		}
		sq_pushvector(vm,&value);SCRIPT_CHECK_AND_RETURN(vm,SQ_OK);
	}
}
void TrainingHighlight_RegisterNatives(CSquirrelVM* vm)
{
	s_scriptStarted=false;
	if(Script_RegisterFuncTC_S21(vm,"TrainingHighlight_Configure",reinterpret_cast<void*>(Configure),"bool","int context")==SQ_ERROR
		||Script_RegisterFuncTC_S21(vm,"TrainingHighlight_OriginalParam",reinterpret_cast<void*>(OriginalParam),"vector","int context, int parameter")==SQ_ERROR
		||Script_RegisterFuncTC_S21(vm,"TrainingHighlight_Restore",reinterpret_cast<void*>(RestoreScript),"void","")==SQ_ERROR)
		Warning(eDLL_T::CLIENT,"[TRAINING-HIGHLIGHT] registration failed\n");
}
void TrainingHighlight_Frame(void)
{
	if(!ClassVar_LocalPlayer()){s_scriptStarted=false;return;}
	if(!TrainingAim_SessionReady())return;
	if(!s_scriptStarted && CommandLine()->CheckParm("-offline"))
	{
		s_scriptStarted=true;
		Script_Execute_S21(kTrainingHighlightScript,SQCONTEXT::CLIENT);
	}
}
void VTrainingHighlight::GetAdr(void) const { LogVarAdr("TrainingHighlight_ContextTable",s_table); }
void VTrainingHighlight::GetFun(void) const
{
	const CMemory getter=Module_FindPattern(g_GameDll,"48 6B C8 34 48 8B 05 ?? ?? ?? ?? 0F B6 0C 01");
	if(getter)s_table=getter.Offset(4).ResolveRelativeAddress(3,7).RCast<unsigned char**>();
	if(!s_table)Warning(eDLL_T::CLIENT,"[TRAINING-HIGHLIGHT] native context table unresolved\n");
}
void VTrainingHighlight::Detour(bool attach) const { if(!attach)Restore(); }
