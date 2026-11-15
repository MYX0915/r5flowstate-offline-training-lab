//=============================================================================//
// Purpose: Native offline training controls.
//=============================================================================//
#include "core/stdafx.h"
#include "ITrainingMenu.h"
#include "engine/client/training_aim.h"
#include "engine/client/training_aim_math.h"
#include "engine/client/training_highlight.h"
#include "engine/client/training_weapon.h"
#include "windows/id3dx.h"
#include "imgui/misc/imgui_utility.h"
#include <cfloat>

extern volatile bool g_bImguiWndProcSeesKeys;
CTrainingMenu g_TrainingMenu;

namespace
{
	void Checkbox(const char* label,ConVar& setting)
	{
		bool value=setting.GetBool();
		if (ImGui::Checkbox(label,&value)) setting.SetValue(value);
	}
	void ProbeCheckbox(const char* label,const char* name)
	{
		ConVar* const variable=g_pCVar ? g_pCVar->FindVar(name) : nullptr;
		if (variable) Checkbox(label,*variable);
	}
	void Toggle_f(const CCommand&) { g_TrainingMenu.Toggle(); }
	ConCommand s_toggle("training_menu_toggle",Toggle_f,"Toggle the offline training controls.",FCVAR_DEVELOPMENTONLY);
}

CTrainingMenu::CTrainingMenu() { m_surfaceLabel=u8"离线训练设置"; }
bool CTrainingMenu::Init() { SetStyleVar(); m_initialized=true; return true; }
void CTrainingMenu::Shutdown() { SetActive(false); m_initialized=false; }
void CTrainingMenu::Toggle() { ToggleActive(); ResetInput(); }

void CTrainingMenu::RunFrame()
{
	// WndProc owns Insert. Fall back only when it has never received key events.
	static bool previous=false;
	static ULONGLONG pending=0;
	const bool down=(GetAsyncKeyState(VK_INSERT)&0x8000)!=0;
	const bool focused=GetForegroundWindow()==DirectX_GetMainWindow();
	if (down && !previous && focused && !g_bImguiWndProcSeesKeys) pending=GetTickCount64();
	previous=down;
	if (g_bImguiWndProcSeesKeys) pending=0;
	if (pending && GetTickCount64()-pending>=150)
	{
		pending=0;
		if (focused) Toggle();
	}
	if (!m_activated) return;
	if (!m_initialized) Init();
	DrawSurface();
}

bool CTrainingMenu::DrawSurface()
{
	ImGui::SetNextWindowSizeConstraints(ImVec2(560,650),ImVec2(FLT_MAX,FLT_MAX));
	ImGui::SetNextWindowPos(ImVec2(35,355),ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(560,510),ImGuiCond_FirstUseEver);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,6);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(14,14));
	const bool open=ImGui::Begin(m_surfaceLabel,&m_activated,ImGuiWindowFlags_NoCollapse,&ResetInput);
	if (open)
	{
		ImGui::TextUnformatted(u8"Insert：显示或隐藏菜单  |  按住鼠标右键：辅助瞄准");
		ImGui::Separator();
		Checkbox(u8"启用辅助瞄准",local_aim_enable);
		Checkbox(u8"瞄准 NPC",local_aim_npc); ImGui::SameLine();
		Checkbox(u8"瞄准其他玩家",local_aim_players);
		ImGui::TextDisabled(u8"排除自己和诱饵；只瞄准可见部位。");
		int part=local_aim_part.GetInt();
		if (ImGui::Combo(u8"瞄准部位",&part,u8"头部\0颈部\0胸部\0手部\0腿部（膝盖）\0")) local_aim_part.SetValue(part);
		ImGui::TextDisabled(u8"手和腿选择距离准星较近的有效骨骼。");
		float radius=local_aim_radius.GetFloat();
		if (ImGui::SliderFloat(u8"范围半径（像素）",&radius,20,600,"%.0f")) local_aim_radius.SetValue(radius);
		float smoothness=TrainingAim_SmoothnessFromSpeed(local_aim_speed.GetFloat());
		if (ImGui::SliderFloat(u8"平滑度",&smoothness,0,100,"%.1f"))
			local_aim_speed.SetValue(TrainingAim_SpeedFromSmoothness(smoothness));
		ImGui::TextDisabled(u8"越小越快，越大越柔和；Ctrl+单击数值可精确输入。");
		Checkbox(u8"显示屏幕中心范围圆",local_aim_circle);
		ImGui::Separator();
		Checkbox(u8"无后坐力",local_no_recoil);
		Checkbox(u8"弹道修正（不穿墙）",local_bullet_assist);
		float cone=local_bullet_angle.GetFloat();
		bool allDirections=cone>=179.99f;
		static float previousCone=10;
		if (!allDirections) previousCone=cone;
		if (ImGui::Checkbox(u8"360° 全方位搜索",&allDirections))
			local_bullet_angle.SetValue(allDirections ? 180.0f : previousCone);
		if (!allDirections && ImGui::SliderFloat(u8"弹道修正范围（度）",&cone,0,30,"%.1f")) local_bullet_angle.SetValue(cone);
		if (allDirections) ImGui::TextDisabled(u8"优先最近的可见目标；射出后不转向。");
		ImGui::TextDisabled(allDirections ? u8"找到目标时校准初始方向；障碍物仍会挡住子弹。" : u8"修正可见目标的方向；保留武器原有散布。");
		ImGui::Separator();
		ProbeCheckbox(u8"显示人物统计","local_entity_probe");
		Checkbox(u8"模型高亮（实验）",local_model_highlight);
		ProbeCheckbox(u8"显示弹药数量","local_ammo_probe");
		ImGui::Separator();
		const TrainingAimStatus_t status=TrainingAim_Status();
		ImGui::Text(u8"状态：%s",TrainingAim_StateName(status.state));
		ImGui::Text(u8"候选点：%d   被遮挡点：%d",status.considered,status.blocked);
		ImGui::TextDisabled(u8"菜单打开时暂停辅助瞄准。");
		ImGui::TextDisabled(u8"仅用于本机离线 R5Flowstate 训练场。");
	}
	ImGui::End(); ImGui::PopStyleVar(2);
	return open;
}
