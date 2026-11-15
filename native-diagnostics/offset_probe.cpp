#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr uintptr_t kPlayerHandle = 0x3D3DCD0;
constexpr uintptr_t kEntities = 0x6B515D8;
constexpr uintptr_t kTeamArray = 0x7083068;
constexpr uintptr_t kTeamCount = 0x7083080;
constexpr DWORD kImageSize = 234819584;

struct Memory {
    virtual bool Read(uintptr_t address, void* output, size_t size) const = 0;
    virtual ~Memory() = default;
};

struct LocalMemory final : Memory {
    bool Read(uintptr_t address, void* output, size_t size) const override {
        SIZE_T actual = 0;
        return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address),
                                 output, size, &actual) && actual == size;
    }
};

template<class T>
bool ReadValue(const Memory& memory, uintptr_t address, T& value) {
    return memory.Read(address, &value, sizeof(value));
}

bool Pointer(uintptr_t value) {
    return value >= 0x10000 && value < 0x800000000000 && !(value & 7);
}

bool Resolve(const Memory& memory, uintptr_t base, uint32_t handle, uintptr_t& entity) {
    entity = 0;
    if (handle == 0xFFFFFFFF) return true;
    const uintptr_t entry = base + kEntities + (handle & 0xFFFF) * 0x20;
    uint32_t serial = 0;
    if (!ReadValue(memory, entry + 8, serial) || serial != (handle >> 16)) return false;
    if (!ReadValue(memory, entry, entity)) return false;
    return entity == 0 || Pointer(entity);
}

struct Weapon {
    unsigned slot;
    uint32_t handle;
    uintptr_t address;
    int32_t ammo;
    bool ownerMatches;
};

struct Team {
    uintptr_t address;
    uint32_t id;
    uint32_t count;
};

struct Snapshot {
    uint32_t handle = 0;
    uintptr_t player = 0;
    uint32_t playerTeam = 0;
    std::vector<Weapon> weapons;
    std::vector<Team> teams;
};

bool ReadWeapons(const Memory& memory, uintptr_t base, Snapshot& sample) {
    uintptr_t vtable = 0, getter = 0;
    const unsigned char expected[] = {0x48, 0x8D, 0x81, 0x58, 0x19, 0, 0, 0xC3};
    unsigned char bytes[sizeof(expected)] = {};
    if (!ReadValue(memory, sample.player, vtable) ||
        !ReadValue(memory, vtable + 0x9C0, getter) || getter != base + 0x22CBD30 ||
        !memory.Read(getter, bytes, sizeof(bytes)) || std::memcmp(bytes, expected, sizeof(bytes))) return false;
    for (unsigned slot = 0; slot < 3; ++slot) {
        uint32_t handle = 0, ownerHandle = 0;
        uintptr_t weapon = 0, owner = 0;
        int32_t ammo = 0;
        if (!ReadValue(memory, sample.player + 0x19B0 + slot * 4, handle) ||
            !Resolve(memory, base, handle, weapon)) return false;
        if (!weapon) continue;
        if (!ReadValue(memory, weapon + 0x15D0, ownerHandle) ||
            !Resolve(memory, base, ownerHandle, owner) ||
            !ReadValue(memory, weapon + 0x1600, ammo)) return false;
        sample.weapons.push_back({slot, handle, weapon, ammo, owner == sample.player});
    }
    return true;
}

bool ReadTeams(const Memory& memory, uintptr_t base, Snapshot& sample) {
    uint32_t count = 0;
    uintptr_t array = 0;
    if (!ReadValue(memory, base + kTeamCount, count) || count > 256 ||
        !ReadValue(memory, base + kTeamArray, array) || (count && !Pointer(array))) return false;
    for (uint32_t i = 0; i < count; ++i) {
        Team team{};
        if (!ReadValue(memory, array + i * 8, team.address) || !Pointer(team.address) ||
            !ReadValue(memory, team.address + 0x94C, team.id) ||
            !ReadValue(memory, team.address + 0x968, team.count)) return false;
        sample.teams.push_back(team);
    }
    uint32_t countAfter = 0;
    uintptr_t arrayAfter = 0;
    return ReadValue(memory, base + kTeamCount, countAfter) && countAfter == count &&
           ReadValue(memory, base + kTeamArray, arrayAfter) && arrayAfter == array;
}

bool TakeSnapshot(const Memory& memory, uintptr_t base, Snapshot& sample) {
    return ReadValue(memory, base + kPlayerHandle, sample.handle) &&
           Resolve(memory, base, sample.handle, sample.player) && sample.player &&
           ReadValue(memory, sample.player + 0x340, sample.playerTeam) &&
           ReadWeapons(memory, base, sample) && ReadTeams(memory, base, sample);
}

std::string Hex(uintptr_t value) {
    std::ostringstream text;
    text << "\"0x" << std::hex << value << "\"";
    return text.str();
}

std::string Serialize(const Snapshot& sample) {
    std::ostringstream text;
    text << "{\"local_player_handle\":" << Hex(sample.handle)
         << ",\"local_player\":" << Hex(sample.player)
         << ",\"local_team\":" << sample.playerTeam << ",\"weapons\":[";
    bool comma = false;
    for (const auto& weapon : sample.weapons) {
        if (comma) text << ',';
        comma = true;
        text << "{\"slot\":" << weapon.slot << ",\"handle\":" << Hex(weapon.handle)
             << ",\"address\":" << Hex(weapon.address) << ",\"ammo\":" << weapon.ammo
             << ",\"owner_matches\":" << (weapon.ownerMatches ? "true" : "false") << '}';
    }
    text << "],\"teams\":[";
    comma = false;
    for (const auto& team : sample.teams) {
        if (comma) text << ',';
        comma = true;
        text << "{\"address\":" << Hex(team.address) << ",\"team_id\":" << team.id
             << ",\"reported_count\":" << team.count << '}';
    }
    text << "]}";
    return text.str();
}

bool CheckImage(const LocalMemory& memory, uintptr_t base) {
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS64 nt{};
    return memory.Read(base, &dos, sizeof(dos)) && dos.e_magic == IMAGE_DOS_SIGNATURE &&
           dos.e_lfanew > 0 && dos.e_lfanew < 0x100000 &&
           memory.Read(base + dos.e_lfanew, &nt, sizeof(nt)) && nt.Signature == IMAGE_NT_SIGNATURE &&
           nt.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 && nt.OptionalHeader.SizeOfImage == kImageSize;
}

DWORD RunProbe(HMODULE self) {
    wchar_t path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(self, path, MAX_PATH);
    if (!length || length >= MAX_PATH) return ERROR_INSUFFICIENT_BUFFER;
    std::wstring folder(path, length);
    folder.resize(folder.find_last_of(L"\\/") + 1);
    const auto outputPath = folder + L"offset-probe-" + std::to_wstring(GetCurrentProcessId()) +
                            L"-" + std::to_wstring(GetTickCount64()) + L".json";
    LocalMemory memory;
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"r5apex_dx12.exe"));
    Snapshot first, second;
    const bool validImage = base && CheckImage(memory, base);
    const bool firstRead = validImage && TakeSnapshot(memory, base, first);
    Sleep(400);
    const bool secondRead = firstRead && TakeSnapshot(memory, base, second);
    std::ostringstream text;
    text << "{\"pid\":" << GetCurrentProcessId() << ",\"module_base\":" << Hex(base)
         << ",\"image_valid\":" << (validImage ? "true" : "false")
         << ",\"read_complete\":" << (secondRead ? "true" : "false")
         << ",\"game_field_writes\":0,\"snapshots\":[";
    if (firstRead) text << Serialize(first);
    if (secondRead) text << ',' << Serialize(second);
    text << "]}\n";
    const std::string bytes = text.str();
    HANDLE file = CreateFileW(outputPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                              CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return GetLastError();
    DWORD written = 0;
    const bool saved = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr)
                       && written == bytes.size() && FlushFileBuffers(file);
    const DWORD error = saved ? ERROR_SUCCESS : GetLastError();
    CloseHandle(file);
    return saved && secondRead ? ERROR_SUCCESS : (error ? error : ERROR_INVALID_DATA);
}

DWORD WINAPI ProbeThread(void* argument) {
    DWORD result = ERROR_UNHANDLED_EXCEPTION;
    try { result = RunProbe(static_cast<HMODULE>(argument)); }
    catch (...) { OutputDebugStringW(L"offset_probe: diagnostic C++ exception\n"); }
    FreeLibraryAndExitThread(static_cast<HMODULE>(argument), result);
}

struct Fixture final : Memory {
    std::map<uintptr_t, std::vector<unsigned char>> blocks;
    mutable unsigned reads = 0;
    bool Read(uintptr_t address, void* output, size_t size) const override {
        ++reads;
        const auto entry = blocks.find(address);
        if (entry == blocks.end() || entry->second.size() != size) return false;
        std::memcpy(output, entry->second.data(), size);
        return true;
    }
    template<class T> void Put(uintptr_t address, T value) {
        const auto bytes = reinterpret_cast<const unsigned char*>(&value);
        blocks[address] = std::vector<unsigned char>(bytes, bytes + sizeof(value));
    }
};

Fixture MakeFixture(uintptr_t base) {
    Fixture f;
    constexpr uintptr_t player = 0x220000000, weapon = 0x230000000;
    constexpr uintptr_t table = 0x142000000, teams = 0x240000000, team = 0x250000000;
    const uint32_t playerHandle = 0x40003, weaponHandle = 0x3201FE;
    f.Put(base + kPlayerHandle, playerHandle);
    f.Put(base + kEntities + 3 * 0x20, player);
    f.Put(base + kEntities + 3 * 0x20 + 8, uint32_t(4));
    f.Put(base + kEntities + 0x1FE * 0x20, weapon);
    f.Put(base + kEntities + 0x1FE * 0x20 + 8, uint32_t(0x32));
    f.Put(player, table);
    f.Put(table + 0x9C0, base + 0x22CBD30);
    f.blocks[base + 0x22CBD30] = {0x48, 0x8D, 0x81, 0x58, 0x19, 0, 0, 0xC3};
    f.Put(player + 0x340, uint32_t(4));
    f.Put(player + 0x19B0, weaponHandle);
    f.Put(player + 0x19B4, uint32_t(0xFFFFFFFF));
    f.Put(player + 0x19B8, uint32_t(0xFFFFFFFF));
    f.Put(weapon + 0x15D0, playerHandle);
    f.Put(weapon + 0x1600, int32_t(31));
    f.Put(base + kTeamArray, teams);
    f.Put(base + kTeamCount, uint32_t(1));
    f.Put(teams, team);
    f.Put(team + 0x94C, uint32_t(4));
    f.Put(team + 0x968, uint32_t(1));
    return f;
}
} // namespace

extern "C" __declspec(dllexport) DWORD WINAPI OffsetProbeSelfTest() {
    constexpr uintptr_t base = 0x140000000;
    auto f = MakeFixture(base);
    Snapshot sample;
    if (!TakeSnapshot(f, base, sample) || sample.weapons.size() != 1 || sample.weapons[0].ammo != 31 ||
        !sample.weapons[0].ownerMatches || sample.teams.size() != 1 || sample.teams[0].count != 1) return 1;
    uintptr_t entity = 1;
    const unsigned reads = f.reads;
    if (!Resolve(f, base, 0xFFFFFFFF, entity) || entity || f.reads != reads) return 2;
    f.Put(base + kEntities + 3 * 0x20 + 8, uint32_t(5));
    if (Resolve(f, base, 0x40003, entity)) return 3;
    f.Put(base + kTeamCount, uint32_t(257));
    Snapshot invalid;
    if (ReadTeams(f, base, invalid)) return 4;
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH && GetModuleHandleW(L"r5apex_dx12.exe")) {
        HANDLE thread = CreateThread(nullptr, 0, ProbeThread, self, 0, nullptr);
        if (!thread) return FALSE;
        CloseHandle(thread);
    }
    return TRUE;
}
