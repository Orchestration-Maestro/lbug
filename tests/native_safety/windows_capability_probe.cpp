// E02a diagnostic only. This executable never links or enables the engine.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winternl.h>
#include <sddl.h>
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>

namespace fs = std::filesystem;
void require(bool ok, const char* name) {
    if (!ok) throw std::runtime_error(name);
}
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE value) : value(value) {}
    ~Handle() { if (value != INVALID_HANDLE_VALUE && value) CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
struct Module {
    HMODULE value;
    Module() : value(LoadLibraryExW(L"advapi32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)) {
        require(value != nullptr, "system32 advapi32 load");
    }
    ~Module() { FreeLibrary(value); }
};
template<class T> T resolve(HMODULE module, const char* name) {
    auto address = GetProcAddress(module, name);
    require(address != nullptr, name);
    return reinterpret_cast<T>(address);
}
void verifyToken() {
    Module security;
    auto open = resolve<decltype(&OpenProcessToken)>(security.value, "OpenProcessToken");
    auto query = resolve<decltype(&GetTokenInformation)>(security.value, "GetTokenInformation");
    auto sidText = resolve<decltype(&ConvertSidToStringSidW)>(security.value, "ConvertSidToStringSidW");
    auto privilegeName = resolve<decltype(&LookupPrivilegeNameW)>(security.value, "LookupPrivilegeNameW");
    HANDLE raw = nullptr;
    require(open(GetCurrentProcess(), TOKEN_QUERY, &raw), "OpenProcessToken");
    Handle token(raw);
    auto info = [&](TOKEN_INFORMATION_CLASS kind) {
        DWORD size = 0;
        query(token.value, kind, nullptr, 0, &size);
        require(size != 0, "token information size");
        std::vector<BYTE> bytes(size);
        require(query(token.value, kind, bytes.data(), size, &size), "token information");
        return bytes;
    };
    auto sid = [&](PSID value) {
        LPWSTR text = nullptr;
        require(sidText(value, &text), "SID formatting");
        std::wstring result(text);
        LocalFree(text);
        return result;
    };
    const auto elevation = info(TokenElevation);
    const auto type = info(TokenElevationType);
    const auto user = info(TokenUser);
    const auto integrity = info(TokenIntegrityLevel);
    std::wcout << L"token user=" << sid(reinterpret_cast<const TOKEN_USER*>(user.data())->User.Sid)
               << L" elevation=" << reinterpret_cast<const TOKEN_ELEVATION*>(elevation.data())->TokenIsElevated
               << L" type=" << *reinterpret_cast<const TOKEN_ELEVATION_TYPE*>(type.data())
               << L" integrity=" << sid(reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(integrity.data())->Label.Sid) << L'\n';
    require(reinterpret_cast<const TOKEN_ELEVATION*>(elevation.data())->TokenIsElevated == 0,
        "non_elevated_token_required");
    require(sid(reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(integrity.data())->Label.Sid) == L"S-1-16-8192",
        "medium_integrity_token_required");
    const auto groups = info(TokenGroups);
    const auto* groupList = reinterpret_cast<const TOKEN_GROUPS*>(groups.data());
    for (DWORD i = 0; i < groupList->GroupCount; ++i) {
        auto text = sid(groupList->Groups[i].Sid);
        std::wcout << L"token group=" << text << L" attributes=" << groupList->Groups[i].Attributes << L'\n';
        require(text != L"S-1-5-32-544", "no_administrators_group_even_deny_only");
    }
    const auto privileges = info(TokenPrivileges);
    const auto* list = reinterpret_cast<const TOKEN_PRIVILEGES*>(privileges.data());
    for (DWORD i = 0; i < list->PrivilegeCount; ++i) {
        wchar_t name[256];
        DWORD count = 256;
        auto luid = list->Privileges[i].Luid;
        require(privilegeName(nullptr, &luid, name, &count), "privilege name");
        std::wcout << L"token privilege=" << name << L" attributes=" << list->Privileges[i].Attributes << L'\n';
        require(std::wstring(name) != L"SeBackupPrivilege" && std::wstring(name) != L"SeRestorePrivilege",
            "no_backup_or_restore_privilege");
    }
    std::cout << "PASS verified_non_admin_token (before fixture handles)\n";
}
FILE_ID_INFO identity(HANDLE handle) {
    FILE_ID_INFO id{};
    require(GetFileInformationByHandleEx(handle, FileIdInfo, &id, sizeof(id)), "FileIdInfo");
    return id;
}
bool equal(const FILE_ID_INFO& a, const FILE_ID_INFO& b) {
    return a.VolumeSerialNumber == b.VolumeSerialNumber &&
        std::memcmp(a.FileId.Identifier, b.FileId.Identifier, sizeof(a.FileId.Identifier)) == 0;
}
void printIdentity(const char* name, HANDLE handle) {
    const auto id = identity(handle);
    FILE_STANDARD_INFO standard{};
    require(GetFileInformationByHandleEx(handle, FileStandardInfo, &standard, sizeof(standard)), "FileStandardInfo");
    std::cout << name << " volume=" << id.VolumeSerialNumber << " id=";
    for (auto byte : id.FileId.Identifier) std::cout << std::hex << static_cast<unsigned>(byte) << ':';
    std::cout << std::dec << " links=" << standard.NumberOfLinks << " directory=" << static_cast<unsigned>(standard.Directory) << '\n';
}
// Published WDK signature; only SDK NTSTATUS/IO_STATUS_BLOCK types, no private structures.
using NtFlush = NTSTATUS (NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PIO_STATUS_BLOCK);
using NtSetInfo = NTSTATUS (NTAPI*)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS);
struct Api {
    bool nt;
    NtFlush ntFlush;
    HANDLE file;
    unsigned calls = 0;
    HANDLE last = INVALID_HANDLE_VALUE;
    bool success = false;
    DWORD error = ERROR_SUCCESS;
    NTSTATUS status = 0;
    IO_STATUS_BLOCK iosb{};
    void invoke(HANDLE handle) {
        last = handle;
        if (nt) {
            if (!ntFlush) { error = ERROR_PROC_NOT_FOUND; return; }
            ++calls;
            iosb.Status = static_cast<NTSTATUS>(0x7fffffff);
            status = ntFlush(handle, 0, nullptr, 0, &iosb);
            success = status == 0 && iosb.Status == 0;
        } else {
            ++calls;
            const BOOL ok = FlushFileBuffers(handle);
            error = ok ? ERROR_SUCCESS : GetLastError();
            success = ok != FALSE;
        }
    }
};
bool isSameDirectory(HANDLE tested, HANDLE directory) {
    FILE_STANDARD_INFO kind{};
    FILE_ID_INFO id{};
    return GetFileInformationByHandleEx(tested, FileStandardInfo, &kind, sizeof(kind)) && kind.Directory &&
        GetFileInformationByHandleEx(tested, FileIdInfo, &id, sizeof(id)) && equal(id, identity(directory));
}
bool barrier(Api& api, HANDLE requested, HANDLE directory) {
    api.invoke(requested);
    return api.calls == 1 && isSameDirectory(api.last, directory) && api.success;
}
bool qualified(bool documentedGuarantee, bool observed) {
    return documentedGuarantee && observed;
}
void result(const char* stage, const Api& api, bool observed) {
    std::cout << "barrier stage=" << stage << " api=" << (api.nt ? "NtFlushBuffersFileEx" : "FlushFileBuffers")
              << " calls=" << api.calls << " win32=" << api.error << " ntstatus=0x" << std::hex
              << static_cast<ULONG>(api.status) << " iosb=0x" << static_cast<ULONG>(api.iosb.Status)
              << std::dec << " iosb_information=" << api.iosb.Information << " observed=" << observed << '\n';
    if (api.last != INVALID_HANDLE_VALUE) printIdentity("barrier_target", api.last);
}
struct Fixture {
    fs::path root;
    std::vector<std::unique_ptr<Handle>> ancestors;
    std::vector<FILE_ID_INFO> ids;
    std::unique_ptr<Handle> writable;
    std::unique_ptr<Handle> leaf;
    NtFlush ntFlush;
    NtSetInfo ntSet;
    decltype(&NtCreateFile) ntCreate;
    explicit Fixture(const fs::path& parent) {
        root = fs::absolute(parent) / (L"fixture-" + std::to_wstring(GetCurrentProcessId()));
        require(fs::create_directory(root), "new private fixture directory");
        auto ntdll = GetModuleHandleW(L"ntdll.dll");
        require(ntdll != nullptr, "loaded ntdll");
        ntFlush = reinterpret_cast<NtFlush>(GetProcAddress(ntdll, "NtFlushBuffersFileEx"));
        ntCreate = resolve<decltype(&NtCreateFile)>(ntdll, "NtCreateFile");
        ntSet = resolve<NtSetInfo>(ntdll, "NtSetInformationFile");
        std::cout << "exports NtCreateFile=1 NtSetInformationFile=1 NtFlushBuffersFileEx=" << (ntFlush != nullptr) << '\n';
        fs::path current = root.root_path();
        auto hold = [&](const fs::path& path) {
            auto handle = std::make_unique<Handle>(CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            require(handle->value != INVALID_HANDLE_VALUE, "held ancestor open");
            FILE_ATTRIBUTE_TAG_INFO tag{};
            require(GetFileInformationByHandleEx(handle->value, FileAttributeTagInfo, &tag, sizeof(tag)) &&
                !(tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT), "held ancestor no reparse");
            printIdentity("held_ancestor", handle->value);
            ids.push_back(identity(handle->value));
            ancestors.push_back(std::move(handle));
        };
        hold(current);
        for (const auto& part : root.relative_path()) { current /= part; hold(current); }
        writable = std::make_unique<Handle>(CreateFileW(root.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        auto error = writable->value == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
        std::cout << "directory writable_access=GENERIC_READ|GENERIC_WRITE flags=BACKUP_SEMANTICS|OPEN_REPARSE_POINT"
                  << " win32=" << error << '\n';
        if (writable->value != INVALID_HANDLE_VALUE)
            require(isSameDirectory(writable->value, directory()), "writable directory identity");
        wchar_t volume[MAX_PATH], filesystem[MAX_PATH];
        DWORD serial, maximum, flags;
        require(GetVolumeInformationByHandleW(directory(), volume, MAX_PATH, &serial, &maximum, &flags,
            filesystem, MAX_PATH), "volume metadata");
        const auto drive = GetDriveTypeW(root.root_path().c_str());
        std::wcout << L"filesystem=" << filesystem << L" drive_type=" << drive << L" volume_serial=" << serial
                   << L" component_limit=" << maximum << L" flags=" << flags << L'\n';
        require(std::wstring(filesystem) == L"NTFS" && drive == DRIVE_FIXED, "local NTFS fixture required");
        leaf = relativeCreate(L"source", FILE_CREATE, GENERIC_READ | GENERIC_WRITE | DELETE | SYNCHRONIZE);
        printIdentity("leaf", leaf->value);
    }
    ~Fixture() {
        leaf.reset(); writable.reset(); ancestors.clear();
        std::error_code error;
        fs::remove_all(root, error); // Only this freshly-created probe fixture, never engine cleanup.
    }
    HANDLE directory() const { return ancestors.back()->value; }
    HANDLE barrierHandle() const { return writable->value == INVALID_HANDLE_VALUE ? directory() : writable->value; }
    std::unique_ptr<Handle> relativeCreate(const std::wstring& name, ULONG disposition, ACCESS_MASK access) {
        UNICODE_STRING text{};
        text.Buffer = const_cast<PWSTR>(name.data());
        text.Length = static_cast<USHORT>(name.size() * sizeof(wchar_t));
        text.MaximumLength = text.Length;
        OBJECT_ATTRIBUTES attributes{};
        attributes.Length = sizeof(attributes);
        attributes.RootDirectory = directory();
        attributes.ObjectName = &text;
        IO_STATUS_BLOCK io{};
        HANDLE handle = INVALID_HANDLE_VALUE;
        auto status = ntCreate(&handle, access, &attributes, &io, nullptr, FILE_ATTRIBUTE_NORMAL,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, disposition,
            FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_REPARSE_POINT, nullptr, 0);
        std::cout << "NtCreateFile disposition=" << disposition << " access=0x" << std::hex << access << " ntstatus=0x"
                  << static_cast<ULONG>(status) << " iosb=0x" << static_cast<ULONG>(io.Status) << std::dec << '\n';
        require(status >= 0, "relative NtCreateFile fixture");
        return std::make_unique<Handle>(handle);
    }
    void revalidate() const {
        for (size_t i = 0; i < ancestors.size(); ++i)
            require(equal(ids[i], identity(ancestors[i]->value)), "held ancestor identity unchanged");
    }
};
void controls(Fixture& f, const std::string& filter) {
    auto test = [&](const char* name, auto action) {
        if (filter.empty() || filter == name) { action(); std::cout << "PASS " << name << '\n'; }
    };
    test("barrier_no_call_cannot_qualify", [&] {
        Api api{false, f.ntFlush, f.leaf->value};
        const bool observed = barrier(api, f.barrierHandle(), f.directory());
        require(api.calls == 1, "barrier_no_call_cannot_qualify: actual API not called");
        require(qualified(true, observed) == api.success, "barrier_no_call_cannot_qualify: fabricated success");
    });
    test("barrier_error_cannot_qualify", [&] {
        Api api{false, f.ntFlush, f.leaf->value};
        // Read-only file: real FlushFileBuffers access failure, not a pre-call fault hook.
        auto readonly = f.relativeCreate(L"source", FILE_OPEN, GENERIC_READ | SYNCHRONIZE);
        const bool observed = barrier(api, readonly->value, readonly->value);
        require(api.calls == 1 && !api.success && api.error == ERROR_ACCESS_DENIED,
            "barrier_error_cannot_qualify: actual access-denied fixture");
        require(!qualified(true, observed), "barrier_error_cannot_qualify: failure qualified");
        // Also make the target a real directory: error cannot hide behind a kind refusal.
        Api denied{false, f.ntFlush, f.leaf->value};
        const bool directoryObserved = barrier(denied, f.directory(), f.directory());
        require(!denied.success, "barrier_error_cannot_qualify: read-only directory must fail");
        require(!qualified(true, directoryObserved), "barrier_error_cannot_qualify: directory failure qualified");
        result("denied_directory", denied, directoryObserved);
        Api ntDenied{true, f.ntFlush, f.leaf->value};
        const bool ntObserved = barrier(ntDenied, f.directory(), f.directory());
        result("read_only_directory_diagnostic", ntDenied, ntObserved);
        require(!qualified(false, ntObserved), "read-only diagnostic cannot qualify");
    });
    test("barrier_file_handle_cannot_qualify", [&] {
        Api api{false, f.ntFlush, f.leaf->value};
        const bool observed = barrier(api, f.barrierHandle(), f.directory());
        require(api.last == f.barrierHandle() && isSameDirectory(api.last, f.directory()),
            "barrier_file_handle_cannot_qualify: actual target is not held directory");
        require(!isSameDirectory(f.leaf->value, f.directory()), "file differs from directory");
        require(qualified(true, observed) == api.success, "directory result retained");
    });
    test("barrier_unsupported_cannot_qualify", [&] {
        Api api{true, nullptr, f.leaf->value};
        const bool observed = barrier(api, f.barrierHandle(), f.directory());
        require(api.calls == 0 && api.error == ERROR_PROC_NOT_FOUND, "missing export fixture");
        require(!qualified(true, observed), "barrier_unsupported_cannot_qualify: unsupported qualified");
    });
    test("successful_call_without_documentation_is_NO", [&] {
        Api api{false, f.ntFlush, f.leaf->value};
        api.invoke(f.leaf->value);
        require(api.calls == 1 && api.success, "actual file flush positive control");
        require(!qualified(false, api.success), "successful_call_without_documentation_is_NO");
    });
}
void rename(Fixture& f, HANDLE source, const std::wstring& destination, bool expected, bool nt) {
    std::vector<BYTE> bytes(offsetof(FILE_RENAME_INFO, FileName) + destination.size() * sizeof(wchar_t));
    auto* info = reinterpret_cast<FILE_RENAME_INFO*>(bytes.data());
    info->ReplaceIfExists = FALSE;
    info->RootDirectory = f.directory();
    info->FileNameLength = static_cast<DWORD>(destination.size() * sizeof(wchar_t));
    std::memcpy(info->FileName, destination.data(), info->FileNameLength);
    if (nt) {
        // SDK FILE_RENAME_INFO has the published FILE_RENAME_INFORMATION layout.
        // SDK winternl.h exposes only FileDirectoryInformation; the documented rename class is 10.
        const auto renameClass = static_cast<FILE_INFORMATION_CLASS>(10);
        IO_STATUS_BLOCK io{};
        const auto status = f.ntSet(source, &io, info, static_cast<ULONG>(bytes.size()), renameClass);
        std::cout << "NT_relative_rename_no_replace ntstatus=0x" << std::hex << static_cast<ULONG>(status)
                  << " iosb=0x" << static_cast<ULONG>(io.Status) << std::dec << '\n';
        require((status == 0) == expected, "NT relative rename expected result");
        if (!expected) require(static_cast<ULONG>(status) == 0xc0000035, "NT rename collision status");
    } else {
        const BOOL ok = SetFileInformationByHandle(source, FileRenameInfo, info, static_cast<DWORD>(bytes.size()));
        const DWORD error = ok ? ERROR_SUCCESS : GetLastError();
        std::cout << "Win32_relative_rename_no_replace result=" << ok << " win32=" << error
                  << " supported=0 (RootDirectory must be NULL per documentation)\n";
        require(!ok && error == ERROR_INVALID_PARAMETER, "Win32 relative rename unsupported without fallback");
    }
}
void probe(Fixture& f) {
    auto rtlVersion = resolve<NTSTATUS (WINAPI*)(PRTL_OSVERSIONINFOW)>(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
    RTL_OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    require(rtlVersion(&version) >= 0, "RtlGetVersion");
    std::cout << "OS=" << version.dwMajorVersion << '.' << version.dwMinorVersion << '.' << version.dwBuildNumber
              << " SDK=" << MAESTRO_WINDOWS_SDK << " compiler=" << _MSC_VER << " sha=" << MAESTRO_PROBE_SHA << '\n';
    for (bool nt : {false, true}) {
        auto flush = [&](const char* stage) {
            f.revalidate();
            Api api{nt, f.ntFlush, f.leaf->value};
            const bool observed = barrier(api, f.barrierHandle(), f.directory());
            result(stage, api, observed);
            require(!qualified(false, observed), "undocumented barrier cannot qualify");
            f.revalidate();
        };
        flush("initial");
        auto file = f.relativeCreate(nt ? L"nt-source" : L"win32-source", FILE_CREATE,
            GENERIC_READ | GENERIC_WRITE | DELETE | SYNCHRONIZE);
        const auto fileId = identity(file->value);
        const std::string payload = "E02a live file bytes";
        DWORD written;
        require(WriteFile(file->value, payload.data(), static_cast<DWORD>(payload.size()), &written, nullptr) &&
            written == payload.size(), "file write");
        require(FlushFileBuffers(file->value), "file flush");
        flush("create_and_file_flush");
        const std::wstring destination = nt ? L"nt-destination" : L"win32-destination";
        auto winner = f.relativeCreate(destination, FILE_CREATE, GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE);
        DWORD winnerWritten;
        require(WriteFile(winner->value, payload.data(), static_cast<DWORD>(payload.size()), &winnerWritten, nullptr) &&
            winnerWritten == payload.size(), "collision sentinel bytes");
        const auto winnerId = identity(winner->value);
        LARGE_INTEGER zero{};
        char bytes[64]{}; DWORD read;
        const auto unchangedLeaf = [&](HANDLE handle, const FILE_ID_INFO& id) {
            require(equal(id, identity(handle)), "refused mutation preserves file ID and volume serial");
            require(SetFilePointerEx(handle, zero, nullptr, FILE_BEGIN), "sentinel seek");
            require(ReadFile(handle, bytes, sizeof(bytes), &read, nullptr) && std::string(bytes, read) == payload,
                "refused mutation preserves sentinel bytes");
            FILE_STANDARD_INFO standard{};
            require(GetFileInformationByHandleEx(handle, FileStandardInfo, &standard, sizeof(standard)) &&
                standard.NumberOfLinks == 1 && !standard.Directory && !standard.DeletePending,
                "refused mutation preserves sentinel link count and kind");
        };
        const auto originalName = nt ? L"nt-source" : L"win32-source";
        const auto snapshot = [&] {
            std::vector<fs::path> names;
            for (const auto& entry : fs::directory_iterator(f.root)) names.push_back(entry.path().filename());
            std::sort(names.begin(), names.end());
            return names;
        };
        const auto beforeCollision = snapshot();
        rename(f, file->value, destination, false, false);
        require(snapshot() == beforeCollision, "unsupported Win32 rename namespace unchanged");
        unchangedLeaf(file->value, fileId); unchangedLeaf(winner->value, winnerId);
        rename(f, file->value, destination, false, true);
        require(snapshot() == beforeCollision && equal(fileId, identity(file->value)) &&
            fs::exists(f.root / originalName), "NT rename collision source and namespace unchanged");
        unchangedLeaf(file->value, fileId); unchangedLeaf(winner->value, winnerId);
        const std::wstring published = nt ? L"nt-published" : L"win32-published";
        auto denied = f.relativeCreate(destination, FILE_OPEN, GENERIC_READ | SYNCHRONIZE);
        FILE_DISPOSITION_INFO_EX deniedDisposition{FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_POSIX_SEMANTICS};
        const BOOL deniedDelete = SetFileInformationByHandle(denied->value, FileDispositionInfoEx,
            &deniedDisposition, sizeof(deniedDisposition));
        const DWORD deniedError = deniedDelete ? ERROR_SUCCESS : GetLastError();
        std::cout << "denied_disposition result=" << deniedDelete << " win32=" << deniedError << '\n';
        require(!deniedDelete && deniedError == ERROR_ACCESS_DENIED, "disposition without DELETE denied");
        require(snapshot() == beforeCollision, "denied deletion preserves namespace");
        unchangedLeaf(file->value, fileId); unchangedLeaf(winner->value, winnerId);
        rename(f, file->value, published, true, false);
        require(snapshot() == beforeCollision && equal(fileId, identity(file->value)) &&
            !fs::exists(f.root / published), "unsupported absent-target rename preserves source and destination");
        unchangedLeaf(file->value, fileId); unchangedLeaf(winner->value, winnerId);
        rename(f, file->value, published, true, true);
        auto reopened = f.relativeCreate(published, FILE_OPEN, GENERIC_READ | SYNCHRONIZE);
        require(equal(fileId, identity(reopened->value)) && equal(fileId, identity(file->value)) &&
            !fs::exists(f.root / originalName), "NT renamed destination identity and source absence");
        unchangedLeaf(reopened->value, fileId); unchangedLeaf(winner->value, winnerId);
        flush("rename_with_live_leaf");
        auto live = f.relativeCreate(published, FILE_OPEN, GENERIC_READ | SYNCHRONIZE);
        FILE_DISPOSITION_INFO_EX disposition{FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_POSIX_SEMANTICS};
        const BOOL deleted = SetFileInformationByHandle(file->value, FileDispositionInfoEx, &disposition, sizeof(disposition));
        const DWORD error = deleted ? ERROR_SUCCESS : GetLastError();
        std::cout << "posix_disposition result=" << deleted << " win32=" << error << '\n';
        require(deleted, "POSIX disposition fixture supported");
        file.reset();
        require(!fs::exists(f.root / published), "POSIX namespace removed with live leaf");
        require(equal(fileId, identity(live->value)), "live deleted leaf identity retained");
        flush("delete_with_live_leaf");
        require(SetFilePointerEx(live->value, zero, nullptr, FILE_BEGIN), "live leaf seek");
        require(ReadFile(live->value, bytes, sizeof(bytes), &read, nullptr) &&
            std::string(bytes, read) == payload, "live deleted leaf readable");
    }
    std::cout << "VERDICT=NO documented_parent_entry_power_loss_ordering=UNRESOLVED production_windows=FAIL_CLOSED\n";
}
int main(int argc, char** argv) {
    try {
        verifyToken(); // No tested filesystem handle is opened before token verification.
        require(argc >= 2, "usage: probe FIXTURE_PARENT [TEST_NAME]");
        Fixture fixture{fs::path(argv[1])};
        controls(fixture, argc > 2 ? argv[2] : "");
        if (argc == 2) probe(fixture);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << " win32=" << GetLastError() << '\n';
        return 1;
    }
}
#endif
