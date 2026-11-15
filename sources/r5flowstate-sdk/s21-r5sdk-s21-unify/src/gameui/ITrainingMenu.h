#pragma once
#if defined(CLIENT_DLL) && defined(SDK_WIP)
#include "imgui_surface.h"
class CTrainingMenu : public CImguiSurface
{
public:
	CTrainingMenu();
	bool Init() override;
	void Shutdown() override;
	void RunFrame() override;
	bool DrawSurface() override;
	void Toggle();
};
extern CTrainingMenu g_TrainingMenu;
#endif // CLIENT_DLL && SDK_WIP
