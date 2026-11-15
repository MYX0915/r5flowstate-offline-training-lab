//=============================================================================//
// Purpose: Single-model, per-fragment training highlights on the supported DX11 build.
//=============================================================================//
#include "core/stdafx.h"
#include "training_highlight_shader.h"
#include "tier0/commandline.h"
#include "windows/id3dx.h"
#include <d3d11.h>
#include <atomic>
#include <vector>

void ComputeHashRetail(const BYTE* data, UINT length, BYTE* hash);

namespace
{
	using CreateShaderFn=HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*,const void*,SIZE_T,ID3D11ClassLinkage*,ID3D11PixelShader**);
	CreateShaderFn s_create=nullptr;
	HMODULE s_runtime=nullptr;
	std::atomic<bool> s_ready{false};
	std::atomic<unsigned int> s_seen{0},s_matched{0},s_largeLogged{0};
	std::atomic<uint64_t> s_dumpBytes{0};
	char s_dumpDirectory[MAX_PATH]={};
	struct Profile_t { uint32_t bytes; uint64_t hash; };
	constexpr Profile_t kProfiles[]={
		{76836,0x64a3b01d84dda5e4ull}, // Resident common_early asset.
		{76896,0xdfb9c603c5a78583ull}  // Actual S21 runtime shader, captured and validated locally.
	};
	struct Edit_t { uint32_t word,original,replacement; };
	constexpr Edit_t kEdits[]={
		{837,0x41200000,0x3d4ccccd}, // Occluded depth separation: 10 -> 0.05 world units.
		{844,0x3dcccccd,0x3f800000}, // Full fill opacity instead of the native debug 0.1.
		{850,0x00100246,0x00109246}, // Read the existing immediate red vector, not the outline colour.
		{851,3,0}
	};
	uint32_t Read32(const BYTE* data)
	{
		uint32_t value;memcpy(&value,data,sizeof(value));return value;
	}
	uint64_t Hash(const BYTE* data,size_t size)
	{
		uint64_t value=14695981039346656037ull;
		for(size_t i=0;i<size;++i)value=(value^data[i])*1099511628211ull;
		return value;
	}
	bool Locate(const BYTE* data,size_t size,uint32_t& codeOffset,uint32_t& total)
	{
		if(!data||size<32||memcmp(data,"DXBC",4)!=0||Read32(data+20)!=1)return false;
		total=Read32(data+24);const uint32_t count=Read32(data+28);
		if(total<32||total>size||total>4*1024*1024||count>128||32+count*4>total)return false;
		for(uint32_t i=0;i<count;++i)
		{
			const uint32_t offset=Read32(data+32+i*4);
			if(offset>total-8)return false;
			const uint32_t length=Read32(data+offset+4);
			if(length>total-offset-8)return false;
			if(memcmp(data+offset,"SHEX",4)!=0&&memcmp(data+offset,"SHDR",4)!=0)continue;
			for(const auto& profile:kProfiles)
			{
				if(length==profile.bytes&&Hash(data+offset+8,length)==profile.hash)
				{
					codeOffset=offset+8;return true;
				}
			}
			return false;
		}
		return false;
	}
	void DumpCandidate(const void* bytes,size_t length)
	{
		if(!s_dumpDirectory[0]||!bytes||length<50000||length>250000)return;
		if(memcmp(bytes,"DXBC",4)!=0)return;
		if(s_dumpBytes.fetch_add(length)>64*1024*1024)return;
		char path[MAX_PATH];
		sprintf_s(path,"%s/shader-%016llX.dxbc",s_dumpDirectory,
			static_cast<unsigned long long>(Hash(static_cast<const BYTE*>(bytes),length)));
		FILE* file=nullptr;
		if(fopen_s(&file,path,"wb")==0&&file)
		{
			fwrite(bytes,1,length,file);fclose(file);++s_largeLogged;
		}
	}
	HRESULT STDMETHODCALLTYPE CreateShader(ID3D11Device* device,const void* bytes,SIZE_T length,
		ID3D11ClassLinkage* linkage,ID3D11PixelShader** shader)
	{
		++s_seen;
		DumpCandidate(bytes,length);
		uint32_t offset=0,total=0;
		if(shader&&Locate(static_cast<const BYTE*>(bytes),length,offset,total))
		{
			const BYTE* const original=static_cast<const BYTE*>(bytes);
			bool valid=true;
			for(const auto& edit:kEdits)
				valid=valid&&Read32(original+offset+edit.word*4)==edit.original;
			if(valid)
			{
				std::vector<BYTE> modified(original,original+total);
				for(const auto& edit:kEdits)memcpy(modified.data()+offset+edit.word*4,&edit.replacement,4);
				ComputeHashRetail(modified.data()+20,total-20,modified.data()+4);
				ID3D11PixelShader* candidate=nullptr;
				const HRESULT result=s_create(device,modified.data(),total,linkage,&candidate);
				if(SUCCEEDED(result)&&candidate)
				{
					*shader=candidate;
					s_ready.store(true,std::memory_order_release);++s_matched;
					Msg(eDLL_T::CLIENT,"[TRAINING-SHADER] native depth/colour branch installed; bytes=%u\n",total);
					return result;
				}
				if(candidate)candidate->Release();
				Warning(eDLL_T::CLIENT,"[TRAINING-SHADER] custom shader rejected HRESULT=0x%08X\n",static_cast<unsigned int>(result));
			}
		}
		return s_create(device,bytes,length,linkage,shader);
	}
	void Status_f(const CCommand&)
	{
		Msg(eDLL_T::CLIENT,"[TRAINING-SHADER] ready=%d created=%u matched=%u dumped=%u\n",s_ready.load(),s_seen.load(),s_matched.load(),s_largeLogged.load());
	}
	ConCommand s_status("local_model_shader_status",Status_f,"Report the supported native highlight shader state.",FCVAR_DEVELOPMENTONLY);
}

bool TrainingHighlight_ShaderReady(void) { return s_ready.load(std::memory_order_acquire); }
void VTrainingHighlightShader::GetAdr(void) const { LogFunAdr("Training_CreatePixelShader",s_create); }
void VTrainingHighlightShader::GetFun(void) const
{
	char path[MAX_PATH]={};GetModuleFileNameA(nullptr,path,MAX_PATH);
	if(!CommandLine()->CheckParm("-offline")||V_stristr(path,"r5apex_dx12.exe"))return;
	const char* dump=nullptr;
	if(CommandLine()->CheckParm("-training-shader-dump",&dump)&&dump&&strlen(dump)<MAX_PATH-40)
		V_strncpy(s_dumpDirectory,dump,sizeof(s_dumpDirectory));
	s_runtime=LoadLibraryExA("d3d11.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
	const auto create=s_runtime ? reinterpret_cast<decltype(&D3D11CreateDevice)>(GetProcAddress(s_runtime,"D3D11CreateDevice")) : nullptr;
	ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;D3D_FEATURE_LEVEL level;
	const HRESULT result=create ? create(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context) : E_NOINTERFACE;
	if(SUCCEEDED(result)&&device)
	{
		void** const table=*reinterpret_cast<void***>(device);
		s_create=reinterpret_cast<CreateShaderFn>(table[static_cast<int>(D3D11DeviceVTbl::CreatePixelShader)]);
	}
	else Warning(eDLL_T::CLIENT,"[TRAINING-SHADER] device bootstrap failed HRESULT=0x%08X\n",static_cast<unsigned int>(result));
	if(context)context->Release();if(device)device->Release();
}
void VTrainingHighlightShader::Detour(bool attach) const
{
	if(s_create)DetourSetup(&s_create,&CreateShader,attach);
}
