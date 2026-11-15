#include <Windows.h>

void ComputeHashRetail(const BYTE* data, UINT length, BYTE* hash);

extern "C" __declspec(dllexport) void TrainingDxbcHash(const void* data, UINT length, void* hash)
{
	ComputeHashRetail(static_cast<const BYTE*>(data),length,static_cast<BYTE*>(hash));
}
