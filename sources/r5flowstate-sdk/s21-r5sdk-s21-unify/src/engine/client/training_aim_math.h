#pragma once
#include "entity_probe_math.h"
#include <cmath>

inline bool TrainingAim_Allowed(EntityProbeKind_t kind, bool self, bool npc, bool players)
{
	return !self && ((kind == EntityProbeKind_t::Npc && npc) || (kind == EntityProbeKind_t::Player && players));
}

inline bool TrainingAim_ShouldSteer(bool enabled,bool offline,bool focused,bool menu,bool held,bool target)
{
	return enabled && offline && focused && !menu && held && target;
}

inline bool TrainingAim_InCircle(float x, float y, float width, float height, float radius)
{
	if (!std::isfinite(x) || !std::isfinite(y) || width <= 0 || height <= 0 || radius <= 0) return false;
	const float dx = x - width*0.5f, dy = y - height*0.5f;
	return dx*dx + dy*dy <= radius*radius;
}

inline float TrainingAim_Wrap(float angle) { return std::remainder(angle, 360.0f); }

// Logarithmic UI scale: 0 = fastest (60/s), 100 = softest (1/s).
inline float TrainingAim_SpeedFromSmoothness(float value)
{
	if (!std::isfinite(value)) return 20.0f;
	value=std::fmin(std::fmax(value,0.0f),100.0f);
	return std::exp(std::log(60.0f)*(1.0f-value/100.0f));
}

inline float TrainingAim_SmoothnessFromSpeed(float speed)
{
	if (!std::isfinite(speed)) speed=20.0f;
	speed=std::fmin(std::fmax(speed,1.0f),60.0f);
	return 100.0f*(1.0f-std::log(speed)/std::log(60.0f));
}

inline bool TrainingAim_Angles(const float eye[3], const float point[3], float angles[3])
{
	const float x=point[0]-eye[0], y=point[1]-eye[1], z=point[2]-eye[2];
	const float horizontal=std::sqrt(x*x+y*y);
	if (!std::isfinite(horizontal) || !std::isfinite(z) || horizontal*horizontal+z*z < 0.001f) return false;
	angles[0] = -std::atan2(z,horizontal)*57.295779513f;
	angles[1] = std::atan2(y,x)*57.295779513f;
	angles[2] = 0;
	return true;
}

inline void TrainingAim_Smooth(const float current[3], const float target[3], float dt, float speed, float out[3])
{
	dt = std::fmin(std::fmax(dt,0.0f),0.05f);
	const float alpha = 1.0f-std::exp(-std::fmax(speed,0.0f)*dt);
	const float limit = 720.0f*dt;
	for (int i=0;i<2;++i)
	{
		const float step = TrainingAim_Wrap(target[i]-current[i])*alpha;
		out[i] = current[i]+std::fmin(std::fmax(step,-limit),limit);
	}
	out[0] = std::fmin(std::fmax(out[0],-89.0f),89.0f);
	out[1] = TrainingAim_Wrap(out[1]); out[2] = current[2];
}
