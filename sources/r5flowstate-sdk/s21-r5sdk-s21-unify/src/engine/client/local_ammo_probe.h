#pragma once

#if defined(CLIENT_DLL) && defined(SDK_WIP)
#include <atomic>
#include <cstdint>
#include "tier1/convar.h"

extern ConVar local_ammo_probe;
inline std::atomic<int> g_nLocalAmmoProbeAmmo{-1};
inline std::atomic<uint64_t> g_nLocalAmmoProbeSamples{0};
#endif // CLIENT_DLL && SDK_WIP
