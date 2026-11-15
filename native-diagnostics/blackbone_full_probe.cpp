// Links the complete BlackBone.lib; uses upstream initialization and Inject.
#include <BlackBone/Process/Process.h>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::wstring Folder() {
    wchar_t path[MAX_PATH] = {};
    const DWORD size = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (!size || size >= MAX_PATH) return {};
    std::wstring result(path, size);
    result.resize(result.find_last_of(L"\\/") + 1);
    return result;
}

std::string Hex(ULONG value) {
    std::ostringstream out;
    out << "\"0x" << std::hex << value << "\"";
    return out.str();
}

struct Log {
    HANDLE file = INVALID_HANDLE_VALUE;
    bool ok = true;
    Log() {
        const auto path = Folder() + L"full-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                          std::to_wstring(GetTickCount64()) + L".jsonl";
        file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, 0, nullptr);
        ok = file != INVALID_HANDLE_VALUE;
    }
    ~Log() { if (file != INVALID_HANDLE_VALUE) CloseHandle(file); }
    void Event(const char* stage, const std::string& fields = "") {
        const std::string bytes = std::string("{\"stage\":\"") + stage + "\",\"tick\":" +
            std::to_string(GetTickCount64()) + (fields.empty() ? "" : "," + fields) + "}\n";
        DWORD actual = 0;
        if (!ok || !WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &actual, nullptr) ||
            actual != bytes.size() || !FlushFileBuffers(file)) ok = false;
    }
};

DWORD SetPrivilege(const wchar_t* name, bool enabled) {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &token)) return GetLastError();
    TOKEN_PRIVILEGES privileges{};
    privileges.PrivilegeCount = 1;
    if (!LookupPrivilegeValueW(nullptr, name, &privileges.Privileges[0].Luid)) {
        const DWORD error = GetLastError(); CloseHandle(token); return error;
    }
    privileges.Privileges[0].Attributes = enabled ? SE_PRIVILEGE_ENABLED : 0;
    SetLastError(ERROR_SUCCESS);
    const BOOL ok = AdjustTokenPrivileges(token, FALSE, &privileges, 0, nullptr, nullptr);
    const DWORD error = GetLastError();
    CloseHandle(token);
    return ok ? error : (error ? error : ERROR_ACCESS_DENIED);
}

void GrantInfo(Log& log, HANDLE process, const char* phase, DWORD requested) {
    struct Basic { ULONG attributes, granted, handles, pointers, reserved[10]; } info{};
    using Query = LONG(NTAPI*)(HANDLE, ULONG, void*, ULONG, ULONG*);
    const auto query = reinterpret_cast<Query>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryObject"));
    const LONG status = query ? query(process, 0, &info, sizeof(info), nullptr) : LONG(0xC000007A);
    log.Event("granted_access", std::string("\"phase\":\"") + phase + "\",\"requested\":" + Hex(requested) +
              ",\"granted\":" + Hex(info.granted) + ",\"ntstatus\":" + Hex(status));
}

DWORD MappedError(NTSTATUS status) {
    using Convert = ULONG(WINAPI*)(NTSTATUS);
    const auto convert = reinterpret_cast<Convert>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlNtStatusToDosError"));
    return convert ? convert(status) : ERROR_PROC_NOT_FOUND;
}

bool TestPhase(Log& log, blackbone::Process& process, DWORD pid, const char* phase, bool self) {
    constexpr DWORD access = DEFAULT_ACCESS_P;
    const auto attach = process.Attach(pid, access);
    log.Event("Attach_return", std::string("\"phase\":\"") + phase + "\",\"ntstatus\":" + Hex(attach));
    if (!NT_SUCCESS(attach)) return false;
    GrantInfo(log, process.core().handle(), phase, access);
    if (!self) {
        wchar_t target[MAX_PATH] = {};
        DWORD length = MAX_PATH;
        if (!QueryFullProcessImageNameW(process.core().handle(), 0, target, &length) ||
            _wcsicmp(target, L"D:\\Steam\\steamapps\\common\\Apex Legends\\r5apex_dx12.exe")) {
            log.Event("target_identity_failed"); return false;
        }
    }
    const std::wstring dll = L"E:\\Users\\MYX\\Desktop\\apex\\artifacts\\native-offset-probe\\offset_probe.dll";
    log.Event("ProcessModules_Inject_enter", std::string("\"phase\":\"") + phase + "\"");
    const auto injected = process.modules().Inject(dll);
    const DWORD observed = GetLastError();
    const bool loaded = injected.result_data.has_value() && static_cast<bool>(injected.result());
    log.Event("ProcessModules_Inject_return", std::string("\"phase\":\"") + phase +
              "\",\"ntstatus\":" + Hex(injected.status) + ",\"mapped_win32_error\":" +
              std::to_string(MappedError(injected.status)) + ",\"last_error_observed\":" +
              std::to_string(observed) + ",\"module_returned\":" + (loaded ? "true" : "false"));
    bool passed = false;
    if (self && loaded) {
        using Test = DWORD(WINAPI*)();
        const auto module = reinterpret_cast<HMODULE>(injected.result()->baseAddress);
        const auto test = reinterpret_cast<Test>(GetProcAddress(module, "OffsetProbeSelfTest"));
        const DWORD error = test ? test() : ERROR_PROC_NOT_FOUND;
        log.Event("diagnostic_export_selftest", "\"error\":" + std::to_string(error));
        const auto unloaded = process.modules().Unload(injected.result());
        const bool absent = GetModuleHandleW(L"offset_probe.dll") == nullptr;
        log.Event("control_dll_unload", "\"return_status\":" + Hex(unloaded) +
                  ",\"module_absent\":" + (absent ? "true" : "false"));
        passed = !error && absent;
    }
    // Independent allocation check records an actual API error, not just status mapping.
    blackbone::ptr_t pointer = 0;
    const auto allocation = process.core().native()->VirtualAllocExT(pointer, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    const DWORD allocationError = allocation < 0 ? GetLastError() : ERROR_SUCCESS;
    const auto released = pointer ? process.core().native()->VirtualFreeExT(pointer, 0, MEM_RELEASE) : STATUS_SUCCESS;
    log.Event("allocation_check", std::string("\"phase\":\"") + phase + "\",\"ntstatus\":" + Hex(allocation) +
              ",\"win32_error\":" + std::to_string(allocationError) + ",\"allocated\":" +
              (pointer ? "true" : "false") + ",\"free_status\":" + Hex(released));
    process.Detach();
    return self ? passed : true;
}
}

int wmain(int argc, wchar_t** argv) {
    const bool self = argc == 2 && !wcscmp(argv[1], L"--self-test");
    wchar_t* end = nullptr;
    const DWORD pid = self ? GetCurrentProcessId() : (argc == 2 ? wcstoul(argv[1], &end, 10) : 0);
    if (!pid || (!self && (!end || *end || pid == GetCurrentProcessId()))) return 2;
    Log log;
    if (!log.ok) return 3;
    log.Event("begin", "\"pid\":" + std::to_string(pid) + ",\"host_pid\":" +
              std::to_string(GetCurrentProcessId()) + ",\"self_test\":" + (self ? "true" : "false"));
    // Bound symbol searching to this test's local cache; preserve upstream InitOnce itself.
    const std::wstring cache = Folder() + L"symbols";
    CreateDirectoryW(cache.c_str(), nullptr);
    SetEnvironmentVariableW(L"_NT_SYMBOL_PATH", cache.c_str());
    bool passed = false;
    try {
        {
            log.Event("Process_constructor_enter");
            blackbone::Process process;
            log.Event("Process_constructor_returned");
            const DWORD driverPrivilege = SetPrivilege(L"SeLoadDriverPrivilege", false);
            log.Event("disable_driver_load_privilege", "\"error\":" + std::to_string(driverPrivilege));
            const DWORD disabled = SetPrivilege(L"SeDebugPrivilege", false);
            log.Event("disable_debug_privilege", "\"error\":" + std::to_string(disabled));
            passed = TestPhase(log, process, pid, "debug_disabled", self);
            if (!self) {
                const DWORD enabled = SetPrivilege(L"SeDebugPrivilege", true);
                log.Event("enable_debug_privilege", "\"error\":" + std::to_string(enabled));
                passed = TestPhase(log, process, pid, "debug_enable_attempted", false) && passed;
            }
            const DWORD finalDisable = SetPrivilege(L"SeDebugPrivilege", false);
            log.Event("final_disable_debug_privilege", "\"error\":" + std::to_string(finalDisable));
        }
        log.Event("Process_destructor_returned");
    } catch (const std::exception&) {
        log.Event("Cxx_exception"); passed = false;
    }
    log.Event("finished", std::string("\"test_completed\":") + (passed ? "true" : "false"));
    return passed && log.ok ? 0 : 1;
}
