#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <cstdint>
#include <cwchar>
#include <sstream>
#include <string>

namespace {
struct Handle {
    HANDLE value;
    explicit Handle(HANDLE input) : value(input) {}
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};

struct Result {
    std::string stage = "starting";
    DWORD error = 0, targetPid = 0, requested = 0, granted = 0, remoteExit = 0;
    LONG queryStatus = 0;
    bool allocated = false, pathWritten = false, threadCreated = false, completed = false;
};

Result Fail(Result result, const char* stage, DWORD error) {
    result.stage = stage;
    result.error = error;
    return result;
}

std::wstring OwnFolder() {
    wchar_t path[MAX_PATH] = {};
    const DWORD size = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (!size || size >= MAX_PATH) return {};
    std::wstring result(path, size);
    result.resize(result.find_last_of(L"\\/") + 1);
    return result;
}

bool WriteReport(const Result& result) {
    const std::wstring folder = OwnFolder();
    if (folder.empty()) return false;
    const auto path = folder + L"loader-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                      std::to_wstring(GetTickCount64()) + L".json";
    std::ostringstream text;
    text << "{\"stage\":\"" << result.stage << "\",\"windows_error\":" << result.error
         << ",\"target_pid\":" << result.targetPid << ",\"host_pid\":" << GetCurrentProcessId()
         << ",\"requested_access\":\"0x" << std::hex << result.requested
         << "\",\"granted_access\":\"0x" << result.granted << "\",\"query_status\":\"0x"
         << static_cast<DWORD>(result.queryStatus) << "\",\"remote_load_exit\":" << std::dec << result.remoteExit
         << ",\"remote_allocation\":" << (result.allocated ? "true" : "false")
         << ",\"dll_path_written\":" << (result.pathWritten ? "true" : "false")
         << ",\"remote_thread_created\":" << (result.threadCreated ? "true" : "false")
         << ",\"load_thread_completed\":" << (result.completed ? "true" : "false") << "}\n";
    const auto bytes = text.str();
    Handle file(CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                             FILE_ATTRIBUTE_NORMAL, nullptr));
    if (file.value == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    return WriteFile(file.value, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr)
           && written == bytes.size() && FlushFileBuffers(file.value);
}

Result SelfTest(const std::wstring& dll) {
    Result result;
    HMODULE module = LoadLibraryW(dll.c_str());
    if (!module) return Fail(result, "selftest_load", GetLastError());
    using TestFunction = DWORD(WINAPI*)();
    const auto test = reinterpret_cast<TestFunction>(GetProcAddress(module, "OffsetProbeSelfTest"));
    if (!test) {
        const DWORD error = GetLastError();
        FreeLibrary(module);
        return Fail(result, "selftest_export", error);
    }
    const DWORD error = test();
    FreeLibrary(module);
    return Fail(result, error ? "selftest_failed_case" : "selftest_passed_4_cases", error);
}

bool ResolveRemoteLoader(DWORD pid, uintptr_t& address, DWORD& error) {
    FARPROC local = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    if (!local) { error = GetLastError(); return false; }
    HMODULE owner = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(local), &owner)) { error = GetLastError(); return false; }
    wchar_t ownerPath[MAX_PATH] = {};
    if (!GetModuleFileNameW(owner, ownerPath, MAX_PATH)) { error = GetLastError(); return false; }
    const wchar_t* name = std::wcsrchr(ownerPath, L'\\');
    name = name ? name + 1 : ownerPath;
    Handle modules(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid));
    if (modules.value == INVALID_HANDLE_VALUE) { error = GetLastError(); return false; }
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Module32FirstW(modules.value, &entry)) { error = GetLastError(); return false; }
    do {
        if (!_wcsicmp(entry.szModule, name)) {
            address = reinterpret_cast<uintptr_t>(entry.modBaseAddr) +
                      (reinterpret_cast<uintptr_t>(local) - reinterpret_cast<uintptr_t>(owner));
            return true;
        }
    } while (Module32NextW(modules.value, &entry));
    error = ERROR_MOD_NOT_FOUND;
    return false;
}

Result Inject(DWORD pid, const std::wstring& dll) {
    Result result;
    result.targetPid = pid;
    result.requested = PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                       PROCESS_VM_WRITE | PROCESS_VM_READ;
    Handle process(OpenProcess(result.requested, FALSE, pid));
    if (!process.value) return Fail(result, "OpenProcess", GetLastError());
    struct BasicInformation { ULONG attributes, granted, handles, pointers, reserved[10]; } basic{};
    using QueryObject = LONG(NTAPI*)(HANDLE, ULONG, void*, ULONG, ULONG*);
    const auto query = reinterpret_cast<QueryObject>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryObject"));
    if (!query) return Fail(result, "query_export", ERROR_PROC_NOT_FOUND);
    result.queryStatus = query(process.value, 0, &basic, sizeof(basic), nullptr);
    if (result.queryStatus < 0) return Fail(result, "query_granted_access", ERROR_ACCESS_DENIED);
    result.granted = basic.granted;
    if ((result.granted & result.requested) != result.requested)
        return Fail(result, "required_access_not_granted", ERROR_ACCESS_DENIED);
    wchar_t path[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    if (!QueryFullProcessImageNameW(process.value, 0, path, &size))
        return Fail(result, "target_path_query", GetLastError());
    if (_wcsicmp(path, L"D:\\Steam\\steamapps\\common\\Apex Legends\\r5apex_dx12.exe"))
        return Fail(result, "unexpected_target_path", ERROR_INVALID_PARAMETER);
    uintptr_t loader = 0;
    DWORD loaderError = 0;
    if (!ResolveRemoteLoader(pid, loader, loaderError)) return Fail(result, "remote_loader_resolution", loaderError);
    const SIZE_T bytes = (dll.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(process.value, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) return Fail(result, "VirtualAllocEx", GetLastError());
    result.allocated = true;
    SIZE_T actual = 0;
    if (!WriteProcessMemory(process.value, remote, dll.c_str(), bytes, &actual) || actual != bytes) {
        const DWORD error = GetLastError();
        VirtualFreeEx(process.value, remote, 0, MEM_RELEASE);
        return Fail(result, "WriteProcessMemory_dll_path", error ? error : ERROR_PARTIAL_COPY);
    }
    result.pathWritten = true;
    Handle thread(CreateRemoteThread(process.value, nullptr, 0,
                                    reinterpret_cast<LPTHREAD_START_ROUTINE>(loader), remote, 0, nullptr));
    if (!thread.value) {
        const DWORD error = GetLastError();
        VirtualFreeEx(process.value, remote, 0, MEM_RELEASE);
        return Fail(result, "CreateRemoteThread", error);
    }
    result.threadCreated = true;
    const DWORD wait = WaitForSingleObject(thread.value, 10000);
    if (wait != WAIT_OBJECT_0)
        return Fail(result, "load_thread_not_completed_path_allocation_retained", wait == WAIT_FAILED ? GetLastError() : WAIT_TIMEOUT);
    result.completed = true;
    if (!GetExitCodeThread(thread.value, &result.remoteExit)) {
        const DWORD error = GetLastError();
        VirtualFreeEx(process.value, remote, 0, MEM_RELEASE);
        return Fail(result, "load_thread_exit_query", error);
    }
    if (!VirtualFreeEx(process.value, remote, 0, MEM_RELEASE))
        return Fail(result, "free_dll_path", GetLastError());
    return Fail(result, result.remoteExit ? "load_thread_completed_check_dll_report" : "LoadLibrary_returned_zero",
                result.remoteExit ? ERROR_SUCCESS : ERROR_DLL_INIT_FAILED);
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    Result result;
    const auto dll = OwnFolder() + L"offset_probe.dll";
    if (argc == 2 && !std::wcscmp(argv[1], L"--self-test")) {
        result = SelfTest(dll);
    } else if (argc == 3 && !std::wcscmp(argv[1], L"--inject")) {
        wchar_t* end = nullptr;
        const unsigned long pid = std::wcstoul(argv[2], &end, 10);
        result = pid && end && !*end && pid != GetCurrentProcessId()
                 ? Inject(static_cast<DWORD>(pid), dll)
                 : Fail(result, "invalid_target_pid", ERROR_INVALID_PARAMETER);
    } else {
        result = Fail(result, "usage_self_test_or_inject_pid", ERROR_INVALID_PARAMETER);
    }
    if (!WriteReport(result)) return 100;
    return result.error ? 1 : 0;
}
