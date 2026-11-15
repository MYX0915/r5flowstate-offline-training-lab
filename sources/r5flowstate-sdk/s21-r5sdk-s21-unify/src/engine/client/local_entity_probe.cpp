//=============================================================================//
// Purpose: Read-only client actor census and in-process diagnostic boxes.
//=============================================================================//
#include "core/stdafx.h"
#include "local_entity_probe.h"
#include "training_aim.h"
#include "entity_probe_math.h"
#include "game/client/cliententitylist.h"
#include "game/client/classvar_natives.h"
#include "game/client/portal/portal_teleport.h"
#include "game/client/portal/c_prop_portal.h"
#include "game/client/sceneview.h"
#include "public/client_class.h"
#include "public/dt_recv.h"
#include "mathlib/mathlib.h"
#include "imgui/imgui.h"
#include <mutex>
#include <limits>

static ConVar local_entity_probe("local_entity_probe", "0",
	FCVAR_DEVELOPMENTONLY | FCVAR_ACCESSIBLE_FROM_THREADS,
	"Count client player/NPC/decoy entities and draw projected diagnostic bounds.");
static ConVar local_entity_boxes("local_entity_boxes","0",FCVAR_DEVELOPMENTONLY | FCVAR_ACCESSIBLE_FROM_THREADS,
	"Legacy diagnostic rectangles; off by default for the model-highlight workflow.");

namespace
{
	constexpr int kActorLimit = 512;
	struct ClassMeta_t
	{
		const ClientClass* type;
		EntityProbeKind_t kind;
		int mins, maxs, health;
	};
	struct ActorHandle_t { uint32_t handle; int meta; };
	struct ActorRow_t
	{
		uint32_t handle;
		EntityProbeKind_t kind;
		float origin[3], mins[3], maxs[3];
		bool estimated;
	};
	struct Snapshot_t
	{
		int players = 0, npcs = 0, decoys = 0, self = 0, count = 0, overflow = 0;
		ActorRow_t rows[kActorLimit];
	};
	ClassMeta_t s_meta[32];
	int s_metaCount = 0;
	ActorHandle_t s_actors[kActorLimit];
	int s_actorCount = 0;
	Snapshot_t s_census, s_snapshot, s_work, s_draw;
	std::mutex s_snapshotMutex;
	double s_nextCensus = 0;
	struct Camera_t { float origin[3], angles[3], fov; bool valid = false; double capturedAt=0; };
	Camera_t s_camera;
	struct MapCounts_t { int players = 0, npcs = 0, dummies = 0, decoys = 0; double sampledAt = 0; };
	MapCounts_t s_map;

	const ClientClass* ActorClass(const void* entity)
	{
		const char* const networkable = static_cast<const char*>(entity) + 0x18;
		using GetClassFn = const ClientClass*(__fastcall*)(const void*);
		const uintptr_t* const table = *reinterpret_cast<const uintptr_t* const*>(networkable);
		return table && table[3] ? reinterpret_cast<GetClassFn>(table[3])(networkable) : nullptr;
	}

	int ResolveMeta(const ClientClass* type, EntityProbeKind_t kind)
	{
		for (int i = 0; i < s_metaCount; ++i) if (s_meta[i].type == type) return i;
		if (s_metaCount == 32) return -1;
		ClassMeta_t& m = s_meta[s_metaCount];
		m.type = type; m.kind = kind;
		m.mins = PortalClient_FindRecvOffset(type->m_pNetworkName, "m_vecMins", type->m_pRecvTable);
		m.maxs = PortalClient_FindRecvOffset(type->m_pNetworkName, "m_vecMaxs", type->m_pRecvTable);
		m.health = PortalClient_FindRecvOffset(type->m_pNetworkName, "m_iHealth", type->m_pRecvTable);
		Msg(eDLL_T::CLIENT, "[ENTITY-PROBE] class=%s worldPose=SDK mins=%d maxs=%d\n",
			type->m_pNetworkName, m.mins, m.maxs);
		return s_metaCount++;
	}

	void Census(const void* local)
	{
		s_census.players = s_census.npcs = s_census.decoys = s_census.self = s_census.overflow = 0;
		s_actorCount = 0;
		for (int i = 0; i < NUM_ENT_ENTRIES; ++i)
		{
			const void* const entity = ClientEntityList_EntityAt(i, -1);
			if (!entity) continue;
			const ClientClass* const type = ActorClass(entity);
			const EntityProbeKind_t kind = EntityProbe_Classify(type ? type->m_pNetworkName : nullptr);
			if (kind == EntityProbeKind_t::None) continue;
			if (kind == EntityProbeKind_t::Player) ++s_census.players;
			else if (kind == EntityProbeKind_t::Npc) ++s_census.npcs;
			else ++s_census.decoys;
			if (entity == local) { ++s_census.self; continue; }
			if (s_actorCount == kActorLimit) { ++s_census.overflow; continue; }
			const int meta = ResolveMeta(type, kind);
			if (meta < 0) continue;
			// S21 C_BaseEntity reference handle; serial is validated again each host frame.
			const uint32_t handle = *reinterpret_cast<const uint32_t*>(static_cast<const char*>(entity) + 8);
			if (handle != UINT32_MAX) s_actors[s_actorCount++] = {handle, meta};
		}
	}

	bool Geometry(const ActorHandle_t& entry, ActorRow_t& row)
	{
		const char* const entity = static_cast<const char*>(ClientEntityList_EntityAt(entry.handle & 0xffff, entry.handle >> 16));
		const ClassMeta_t& m = s_meta[entry.meta];
		if (!entity) return false;
		row.handle = entry.handle; row.kind = m.kind;
		const Vector3D origin = Portal_AbsOrigin(entity);
		row.origin[0] = origin.x; row.origin[1] = origin.y; row.origin[2] = origin.z;
		for (float f : row.origin) if (!std::isfinite(f) || fabsf(f) > 1e7f) return false;
		row.estimated = true;
		row.mins[0] = row.mins[1] = -16; row.mins[2] = 0;
		row.maxs[0] = row.maxs[1] = 16; row.maxs[2] = 72;
		if (m.mins >= 0 && m.maxs >= 0 && m.mins < 0x10000 && m.maxs < 0x10000)
		{
			float mins[3], maxs[3];
			memcpy(mins, entity + m.mins, sizeof(mins));
			memcpy(maxs, entity + m.maxs, sizeof(maxs));
			bool valid = true;
			for (int j = 0; j < 3; ++j)
				valid &= std::isfinite(mins[j]) && std::isfinite(maxs[j]) && mins[j] <= maxs[j] && maxs[j]-mins[j] < 512;
			if (valid && maxs[2]-mins[2] > 1)
			{
				memcpy(row.mins, mins, sizeof(mins)); memcpy(row.maxs, maxs, sizeof(maxs)); row.estimated = false;
			}
		}
		return true;
	}

	bool ProjectBox(const ActorRow_t& row, const float matrix[16], const int rect[4], float box[4])
	{
		box[0] = box[1] = (std::numeric_limits<float>::max)();
		box[2] = box[3] = -(std::numeric_limits<float>::max)();
		for (int corner = 0; corner < 8; ++corner)
		{
			float point[3], screen[2];
			for (int axis = 0; axis < 3; ++axis)
				point[axis] = row.origin[axis] + ((corner & (1 << axis)) ? row.maxs[axis] : row.mins[axis]);
			if (!EntityProbe_Project(matrix, point, rect, screen)) return false;
			box[0] = fminf(box[0], screen[0]); box[1] = fminf(box[1], screen[1]);
			box[2] = fmaxf(box[2], screen[0]); box[3] = fmaxf(box[3], screen[1]);
		}
		return box[2] >= rect[0] && box[0] <= rect[0]+rect[2] && box[3] >= rect[1] && box[1] <= rect[1]+rect[3]
			&& box[2]-box[0] < rect[2]*2 && box[3]-box[1] < rect[3]*2;
	}

	void Dump_f(const CCommand&)
	{
		Snapshot_t copy; Camera_t camera;
		{ std::lock_guard<std::mutex> lock(s_snapshotMutex); copy = s_snapshot; camera = s_camera; }
		Msg(eDLL_T::CLIENT, "[ENTITY-PROBE] players=%d self=%d npc=%d decoy=%d geometry=%d overflow=%d\n",
			copy.players, copy.self, copy.npcs, copy.decoys, copy.count, copy.overflow);
		Msg(eDLL_T::CLIENT, "[ENTITY-PROBE] camera=%d fov=%.2f eye=(%.2f %.2f %.2f) angles=(%.2f %.2f %.2f)\n",
			camera.valid, camera.fov, camera.origin[0], camera.origin[1], camera.origin[2], camera.angles[0], camera.angles[1], camera.angles[2]);
		for (int i = 0; i < copy.count && i < 128; ++i)
		{
			const ActorRow_t& r = copy.rows[i];
			Msg(eDLL_T::CLIENT, "[ENTITY-PROBE] handle=%u kind=%d origin=(%.2f %.2f %.2f) estimated=%d\n",
				r.handle, static_cast<int>(r.kind), r.origin[0], r.origin[1], r.origin[2], r.estimated);
		}
	}
	ConCommand s_dump("local_entity_dump", Dump_f, "Dump the read-only actor census and sampled geometry.", FCVAR_DEVELOPMENTONLY);

	void MapCounts_f(const CCommand& args)
	{
		if (args.ArgC() != 5) { Warning(eDLL_T::CLIENT, "[ENTITY-PROBE] mapcounts needs players/npc/dummies/decoys\n"); return; }
		int values[4];
		for (int i = 0; i < 4; ++i)
		{
			char* end = nullptr; const long value = strtol(args.Arg(i+1), &end, 10);
			if (!end || *end || value < 0 || value > 65536) return;
			values[i] = static_cast<int>(value);
		}
		if (values[2] > values[1]) return;
		std::lock_guard<std::mutex> lock(s_snapshotMutex);
		s_map = {values[0], values[1], values[2], values[3], Plat_FloatTime()};
	}
	ConCommand s_mapCounts("local_entity_mapcounts", MapCounts_f, "Receive bounded counts from the authenticated local-server monitor.", FCVAR_DEVELOPMENTONLY);
}

void LocalEntityProbe_CaptureCamera(const void* player, const float* origin, const float* angles, const float* fov)
{
	if ((!local_entity_probe.GetBool() && !TrainingAim_Enabled()) || player != ClassVar_LocalPlayer() || !origin || !angles || !fov) return;
	if (!std::isfinite(*fov) || *fov <= 1 || *fov >= 179) return;
	for (int i = 0; i < 3; ++i) if (!std::isfinite(origin[i]) || !std::isfinite(angles[i])) return;
	std::lock_guard<std::mutex> lock(s_snapshotMutex);
	memcpy(s_camera.origin, origin, 12); memcpy(s_camera.angles, angles, 12);
	s_camera.fov = *fov; s_camera.valid = true; s_camera.capturedAt=Plat_FloatTime();
}

void LocalEntityProbe_Sample(void)
{
	if (!local_entity_probe.GetBool() && !TrainingAim_Enabled()) return;
	const void* const local = ClassVar_LocalPlayer();
	const double now = Plat_FloatTime();
	if (now >= s_nextCensus)
	{
		Census(local);
		s_nextCensus = now + 0.25;
	}
	s_work.players = s_census.players; s_work.npcs = s_census.npcs; s_work.decoys = s_census.decoys;
	s_work.self = s_census.self; s_work.overflow = s_census.overflow; s_work.count = 0;
	for (int i = 0; i < s_actorCount; ++i)
		if (Geometry(s_actors[i], s_work.rows[s_work.count])) ++s_work.count;
	std::lock_guard<std::mutex> lock(s_snapshotMutex);
	s_snapshot = s_work;
}

int LocalEntityProbe_AimActors(TrainingActor_t* out, int capacity)
{
	const int count = capacity < s_actorCount ? capacity : s_actorCount;
	for (int i=0;i<count;++i) out[i] = {s_actors[i].handle,s_meta[s_actors[i].meta].kind,s_meta[s_actors[i].meta].health};
	return count;
}

bool LocalEntityProbe_Camera(TrainingCamera_t& out)
{
	std::lock_guard<std::mutex> lock(s_snapshotMutex);
	memcpy(out.origin,s_camera.origin,12); memcpy(out.angles,s_camera.angles,12);
	out.fov=s_camera.fov; out.valid=s_camera.valid && Plat_FloatTime()-s_camera.capturedAt<0.5;
	return out.valid;
}

void LocalEntityProbe_Draw(void)
{
	if (!local_entity_probe.GetBool()) return;
	Camera_t cameraCopy; MapCounts_t mapCopy;
	{ std::lock_guard<std::mutex> lock(s_snapshotMutex); s_draw = s_snapshot; cameraCopy = s_camera; mapCopy = s_map; }
	float matrix[16], tanX, tanY; int rect[4];
	bool camera = SceneView_GetMainCamera(nullptr, nullptr, nullptr, nullptr, matrix, &tanX)
		&& SceneView_GetMainProjection(&tanX, &tanY, rect);
	if (!camera && cameraCopy.valid)
	{
		const ImVec2 display = ImGui::GetIO().DisplaySize;
		rect[0] = rect[1] = 0; rect[2] = static_cast<int>(display.x); rect[3] = static_cast<int>(display.y);
		if (rect[2] > 0 && rect[3] > 0)
		{
			Vector3D forward, right, up;
			AngleVectors(QAngle(cameraCopy.angles[0], cameraCopy.angles[1], cameraCopy.angles[2]), &forward, &right, &up);
			// Source player-view FOV is horizontal at 4:3. Scale it to the actual viewport.
			tanY = tanf(cameraCopy.fov * 0.00872664626f) * 0.75f;
			tanX = tanY * display.x / display.y;
			camera = EntityProbe_BasisMatrix(cameraCopy.origin, &forward.x, &right.x, &up.x, tanX, tanY, matrix);
		}
	}
	ImDrawList* const draw = ImGui::GetForegroundDrawList();
	int boxes = 0, estimated = 0;
	if (camera)
	{
		for (int i = 0; i < s_draw.count; ++i)
		{
			const ActorRow_t& row = s_draw.rows[i]; float box[4];
			if (!ProjectBox(row, matrix, rect, box)) continue;
			const ImU32 color = row.kind == EntityProbeKind_t::Player ? IM_COL32(80,220,255,255)
				: row.kind == EntityProbeKind_t::Npc ? IM_COL32(255,190,70,255) : IM_COL32(220,110,255,255);
			char label[48]; const char* kind = row.kind == EntityProbeKind_t::Player ? "PLAYER" : row.kind == EntityProbeKind_t::Npc ? "NPC" : "DECOY";
			snprintf(label, sizeof(label), "%s #%u%s", kind, row.handle & 0xffff, row.estimated ? " ~" : "");
			if (local_entity_boxes.GetBool())
			{
				draw->AddRect(ImVec2(box[0],box[1]), ImVec2(box[2],box[3]), color, 0, 0, 1.5f);
				draw->AddText(ImVec2(box[0],box[1]-15), color, label);
			}
			++boxes; estimated += row.estimated;
		}
	}
	char counts[160], status[160], mapText[180];
	snprintf(counts, sizeof(counts), "Player entities: %d (self %d) | NPC: %d | Decoy: %d", s_draw.players, s_draw.self, s_draw.npcs, s_draw.decoys);
	snprintf(status, sizeof(status), "Client scope | in view %d | estimated %d | overflow %d | camera %s", boxes, estimated, s_draw.overflow, camera ? "OK" : "unavailable");
	const double age = Plat_FloatTime() - mapCopy.sampledAt;
	if (mapCopy.sampledAt > 0)
		snprintf(mapText, sizeof(mapText), "Local server: players %d | NPC %d (dummy %d) | decoy %d | %.1fs%s",
			mapCopy.players, mapCopy.npcs, mapCopy.dummies, mapCopy.decoys, age, age > 3.0 ? " STALE" : "");
	else snprintf(mapText, sizeof(mapText), "Local server totals: waiting for authenticated monitor");
	draw->AddRectFilled(ImVec2(18,240), ImVec2(760,340), IM_COL32(0,0,0,210), 6);
	draw->AddText(ImVec2(30,252), IM_COL32(255,255,255,255), counts);
	draw->AddText(ImVec2(30,280), IM_COL32(200,210,220,255), status);
	draw->AddText(ImVec2(30,308), IM_COL32(150,230,180,255), mapText);
}
