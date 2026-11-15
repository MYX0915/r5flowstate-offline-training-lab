//=============================================================================//
//
// Purpose: Client prediction + EntityPortalled handler for portal teleports.
//
//=============================================================================//
#ifndef PORTAL_TELEPORT_CLIENT_H
#define PORTAL_TELEPORT_CLIENT_H

void PortalPredict_BeforeFullWalkMove(void* pMv);
// Shared bounded lookup; diagnostic clients cache resolved offsets per network class.
class CRecvTable;
int PortalClient_FindRecvOffset(const char* pszClass, const char* pszProp, const CRecvTable* table = nullptr);
void PortalPredict_AfterFullWalkMove(void* pPlayer, void* pMv, int nCmd);
void PortalClient_OnPortalled(int nEntWire, int nPortalWire,
	const float flOrigin[3], const float flAngles[3], int nTick);

#endif // PORTAL_TELEPORT_CLIENT_H
