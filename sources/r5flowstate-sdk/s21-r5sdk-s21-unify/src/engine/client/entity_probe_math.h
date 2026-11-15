#pragma once
#include <cmath>
#include <cstring>

enum class EntityProbeKind_t { None, Player, Npc, Decoy };

inline bool EntityProbe_BasisMatrix(const float eye[3], const float forward[3], const float right[3],
	const float up[3], float tanX, float tanY, float matrix[16])
{
	if (!std::isfinite(tanX) || !std::isfinite(tanY) || tanX <= 0 || tanY <= 0) return false;
	for (int i = 0; i < 16; ++i) matrix[i] = 0;
	for (int j = 0; j < 3; ++j)
	{
		matrix[j] = right[j]/tanX; matrix[4+j] = up[j]/tanY; matrix[12+j] = forward[j];
		matrix[3] -= matrix[j]*eye[j];
		matrix[7] -= matrix[4+j]*eye[j];
		matrix[15] -= matrix[12+j]*eye[j];
	}
	return true;
}

inline EntityProbeKind_t EntityProbe_Classify(const char* name)
{
	if (!name) return EntityProbeKind_t::None;
	if (!strcmp(name, "CPlayerDecoy") || !strcmp(name, "C_PlayerDecoy")) return EntityProbeKind_t::Decoy;
	if (!strcmp(name, "CPlayer") || !strcmp(name, "C_Player")) return EntityProbeKind_t::Player;
	if (!strcmp(name, "CAI_BaseNPC") || !strcmp(name, "C_AI_BaseNPC")) return EntityProbeKind_t::Npc;
	return EntityProbeKind_t::None;
}

inline bool EntityProbe_Project(const float m[16], const float p[3], const int rect[4], float out[2])
{
	if (rect[2] <= 0 || rect[3] <= 0) return false;
	const float x = m[0]*p[0] + m[1]*p[1] + m[2]*p[2] + m[3];
	const float y = m[4]*p[0] + m[5]*p[1] + m[6]*p[2] + m[7];
	const float w = m[12]*p[0] + m[13]*p[1] + m[14]*p[2] + m[15];
	if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || w <= 0.01f) return false;
	out[0] = rect[0] + (1.0f + x/w) * rect[2] * 0.5f;
	out[1] = rect[1] + (1.0f - y/w) * rect[3] * 0.5f;
	return std::isfinite(out[0]) && std::isfinite(out[1]);
}
