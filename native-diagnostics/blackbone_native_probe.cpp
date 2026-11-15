// Uses unmodified upstream NativeSubsystem.obj and Utils.obj.
// Minimal bootstrap deliberately omits symbol loading and driver privileges.
#include <BlackBone/Subsystem/NativeSubsystem.h>
#include <BlackBone/Misc/DynImport.h>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace blackbone {
bool InitializeOnce() {
    static std::once_flag once;
    std::call_once(once, [] {
        const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        for (const char* name : {"NtQueryInformationProcess", "NtQueryInformationThread", "NtQueryVirtualMemory",
                                 "NtSetInformationProcess", "NtCreateThreadEx", "NtQueueApcThread", "RtlQueueApcWow64Thread"})
            DynImport::Instance().load(name, ntdll);
    });
    return true;
}
}

namespace {
constexpr DWORD kMinimal = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE |
                            PROCESS_VM_OPERATION | PROCESS_CREATE_THREAD;
constexpr DWORD kDefault = kMinimal | PROCESS_SET_QUOTA | PROCESS_TERMINATE |
                            PROCESS_SUSPEND_RESUME | PROCESS_DUP_HANDLE;
struct Entry {
    std::string phase, profile, stage;
    DWORD requested = 0, granted = 0, openError = 0, queryError = 0, allocationError = 0, freeError = 0;
    LONG queryStatus = 0, allocationStatus = 0, basicStatus = 0;
    bool opened = false, wow64Query = false, wow64 = false, allocationAttempted = false;
    bool allocated = false, freed = false, protectedProcess = false;
};

bool DebugPrivilege(bool enable, DWORD& error) {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        error = GetLastError(); return false;
    }
    TOKEN_PRIVILEGES privilege{};
    privilege.PrivilegeCount = 1;
    if (!LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &privilege.Privileges[0].Luid)) {
        error = GetLastError(); CloseHandle(token); return false;
    }
    privilege.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;
    SetLastError(ERROR_SUCCESS);
    const BOOL ok = AdjustTokenPrivileges(token, FALSE, &privilege, 0, nullptr, nullptr);
    error = GetLastError();
    CloseHandle(token);
    return ok && error == ERROR_SUCCESS;
}

Entry TestOpen(DWORD pid, DWORD access, const char* phase, const char* profile, bool self) {
    Entry entry;
    entry.phase = phase; entry.profile = profile; entry.requested = access;
    HANDLE process = OpenProcess(access, FALSE, pid);
    if (!process) { entry.stage = "OpenProcess"; entry.openError = GetLastError(); return entry; }
    entry.opened = true;
    struct Basic { ULONG attributes, granted, handles, pointers, reserved[10]; } basic{};
    using QueryObject = LONG(NTAPI*)(HANDLE, ULONG, void*, ULONG, ULONG*);
    const auto query = reinterpret_cast<QueryObject>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryObject"));
    if (!query) { entry.stage = "query_export"; CloseHandle(process); return entry; }
    entry.queryStatus = query(process, 0, &basic, sizeof(basic), nullptr);
    entry.granted = basic.granted;
    wchar_t path[MAX_PATH] = {};
    DWORD length = MAX_PATH;
    if (!QueryFullProcessImageNameW(process, 0, path, &length) ||
        (!self && _wcsicmp(path, L"D:\\Steam\\steamapps\\common\\Apex Legends\\r5apex_dx12.exe"))) {
        entry.stage = "target_identity"; entry.queryError = GetLastError(); CloseHandle(process); return entry;
    }
    BOOL wow = FALSE;
    entry.wow64Query = IsWow64Process(process, &wow) != FALSE;
    entry.wow64 = wow != FALSE;
    // This is the real upstream implementation, not a reimplementation.
    blackbone::Native native(process);
    blackbone::_PROCESS_EXTENDED_BASIC_INFORMATION_T<DWORD64> info{};
    info.Size = sizeof(info);
    entry.basicStatus = native.QueryProcessInfoT(ProcessBasicInformation, &info, sizeof(info));
    entry.protectedProcess = info.Flags.IsProtectedProcess != 0;
    blackbone::ptr_t address = 0;
    entry.allocationAttempted = true;
    entry.allocationStatus = native.VirtualAllocExT(address, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    entry.allocationError = entry.allocationStatus < 0 ? GetLastError() : ERROR_SUCCESS;
    entry.allocated = address != 0;
    if (entry.allocated) {
        const auto status = native.VirtualFreeExT(address, 0, MEM_RELEASE);
        entry.freeError = status < 0 ? GetLastError() : ERROR_SUCCESS;
        entry.freed = status >= 0;
    }
    entry.stage = entry.allocated ? "allocation_probe_completed_no_payload" : "Native_VirtualAllocExT_failed";
    CloseHandle(process);
    return entry;
}

void AddPhase(std::vector<Entry>& rows, DWORD pid, const char* phase, bool self) {
    const std::pair<DWORD, const char*> profiles[] = {
        {PROCESS_QUERY_LIMITED_INFORMATION, "query_limited"}, {kMinimal, "minimal_loader"},
        {kDefault, "blackbone_default_access"}, {PROCESS_ALL_ACCESS, "all_access"}};
    for (const auto& profile : profiles)
        rows.push_back(TestOpen(pid, profile.first, phase, profile.second, self));
}

std::string SerializeEntry(const Entry& e) {
    std::ostringstream text;
    text << std::boolalpha << "{\"phase\":\"" << e.phase << "\",\"profile\":\"" << e.profile
         << "\",\"stage\":\"" << e.stage << "\",\"requested\":\"0x" << std::hex << e.requested
         << "\",\"granted\":\"0x" << e.granted << "\",\"query_status\":\"0x" << DWORD(e.queryStatus)
         << "\",\"allocation_status\":\"0x" << DWORD(e.allocationStatus)
         << "\",\"basic_info_status\":\"0x" << DWORD(e.basicStatus) << "\",\"open_error\":" << std::dec << e.openError
         << ",\"query_error\":" << e.queryError << ",\"allocation_error\":" << e.allocationError
         << ",\"free_error\":" << e.freeError << ",\"opened\":" << e.opened
         << ",\"wow64_query_ok\":" << e.wow64Query << ",\"target_wow64\":" << e.wow64
         << ",\"protected_process_bit\":" << e.protectedProcess << ",\"allocation_attempted\":" << e.allocationAttempted
         << ",\"allocated\":" << e.allocated << ",\"freed\":" << e.freed << '}';
    return text.str();
}

bool Save(const std::string& bytes) {
    wchar_t file[MAX_PATH] = {};
    if (!GetModuleFileNameW(nullptr, file, MAX_PATH)) return false;
    std::wstring path(file);
    path.resize(path.find_last_of(L"\\/") + 1);
    path += L"access-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()) + L".json";
    HANDLE output = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, 0, nullptr);
    if (output == INVALID_HANDLE_VALUE) return false;
    DWORD actual = 0;
    const bool ok = WriteFile(output, bytes.data(), static_cast<DWORD>(bytes.size()), &actual, nullptr)
                    && actual == bytes.size() && FlushFileBuffers(output);
    CloseHandle(output);
    return ok;
}
}

int wmain(int argc, wchar_t** argv) {
    bool self = argc == 2 && !wcscmp(argv[1], L"--self-test");
    wchar_t* end = nullptr;
    DWORD pid = self ? GetCurrentProcessId() : (argc == 2 ? wcstoul(argv[1], &end, 10) : 0);
    if (!pid || (!self && (!end || *end || pid == GetCurrentProcessId()))) return 2;
    DWORD disableError = 0, enableError = 0, restoreError = 0;
    const bool disabled = DebugPrivilege(false, disableError);
    std::vector<Entry> rows;
    AddPhase(rows, pid, "debug_disabled", self);
    const bool enabled = DebugPrivilege(true, enableError);
    AddPhase(rows, pid, "debug_enable_attempted", self);
    DebugPrivilege(false, restoreError);
    bool controlPassed = true;
    if (self) {
        for (const auto& row : rows) {
            if (row.profile == "query_limited") controlPassed &= !row.allocated;
            else controlPassed &= row.allocated && row.freed;
        }
    }
    std::ostringstream text;
    text << std::boolalpha << "{\"source_commit\":\"5ede6ce50cd8ad34178bfa6cae05768ff6b3859b\","
         << "\"scope\":\"upstream NativeSubsystem and Utils objects plus minimal custom bootstrap; not complete Blackbone injection\","
         << "\"pid\":" << pid << ",\"host_pid\":" << GetCurrentProcessId() << ",\"self_test\":" << self
         << ",\"self_test_passed\":" << (self && controlPassed)
         << ",\"debug_disable_ok\":" << disabled << ",\"debug_disable_error\":" << disableError
         << ",\"debug_enable_ok\":" << enabled << ",\"debug_enable_error\":" << enableError
         << ",\"debug_restore_error\":" << restoreError << ",\"rows\":[";
    bool comma = false;
    for (const auto& row : rows) { if (comma) text << ','; comma = true; text << SerializeEntry(row); }
    text << "],\"remote_threads_created\":0,\"payload_bytes_written\":0,\"drivers_loaded\":0}\n";
    return Save(text.str()) && (!self || controlPassed) ? 0 : 1;
}
