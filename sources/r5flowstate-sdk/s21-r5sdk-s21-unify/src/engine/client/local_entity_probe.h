#pragma once

#if defined(CLIENT_DLL) && defined(SDK_WIP)
#include "entity_probe_math.h"
#include <cstdint>
struct TrainingActor_t { uint32_t handle; EntityProbeKind_t kind; int health; };
struct TrainingCamera_t { float origin[3], angles[3], fov; bool valid; };
int LocalEntityProbe_AimActors(TrainingActor_t* out, int capacity);
bool LocalEntityProbe_Camera(TrainingCamera_t& out);
void LocalEntityProbe_Sample(void);
void LocalEntityProbe_Draw(void);
void LocalEntityProbe_CaptureCamera(const void* player, const float* origin, const float* angles, const float* fov);
#endif // CLIENT_DLL && SDK_WIP
