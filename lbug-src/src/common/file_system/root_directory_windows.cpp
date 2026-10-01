#ifdef _WIN32
#include "common/file_system/root_directory.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <winternl.h>
#include <fcntl.h>
#include <algorithm>
#include <climits>
#include <cstring>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "common/exception/io.h"
#include "root_directory_windows_security.h"

namespace lbug::common {
#ifdef MAESTRO_NATIVE_OPEN_TEST
// Only the native harness enables metadata substitution and content-call observation.
int (*maestroWindowsCall)(const char* operation, void* handle, void* info) = nullptr;
#endif
namespace {
void point(const char* operation, void* handle, void* info = nullptr) {
#ifdef MAESTRO_NATIVE_OPEN_TEST
    if (maestroWindowsCall) maestroWindowsCall(operation, handle, info);
#else
    (void)operation; (void)handle; (void)info;
#endif
}
struct Close { void operator()(void* h) const { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); } };
using Handle = std::unique_ptr<void, Close>;
Handle checked(HANDLE h) {
    if (!h || h == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        throw IOException("Cannot hold restricted Windows directory: " + std::to_string(error));
    }
    return Handle(h);
}
std::wstring unicode(const std::string& text) {
    if (text.empty() || text.size() > INT_MAX || text.find('\0') != std::string::npos)
        throw IOException("Restricted Windows name or root is invalid.");
    auto size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!size) throw IOException("Restricted Windows name or root requires valid UTF-8.");
    std::wstring result(size, L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size) != size)
        throw IOException("Restricted Windows UTF-8 conversion failed.");
    return result;
}
std::string utf8(const std::wstring& text) {
    const auto size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (!size) throw IOException("Restricted Windows canonical name is unavailable.");
    std::string result(size, '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr) != size)
        throw IOException("Restricted Windows canonical name conversion failed.");
    return result;
}
std::wstring normalized(HANDLE h) {
    const DWORD flags = FILE_NAME_NORMALIZED | VOLUME_NAME_GUID;
    auto size = GetFinalPathNameByHandleW(h, nullptr, 0, flags);
    if (!size || size > 32768) throw IOException("Restricted Windows normalized path is unavailable.");
    std::wstring path(size, L'\0');
    auto written = GetFinalPathNameByHandleW(h, path.data(), size, flags);
    if (!written || written >= size) throw IOException("Restricted Windows normalized path changed or is unavailable.");
    path.resize(written);
    return path;
}
FILE_ID_INFO identity(HANDLE h) {
    FILE_ID_INFO info{};
    if (!GetFileInformationByHandleEx(h, FileIdInfo, &info, sizeof(info)))
        throw IOException("Restricted Windows file identity unavailable.");
    point("identity", h, &info);
    return info;
}
bool sameIdentity(const FILE_ID_INFO& a, const FILE_ID_INFO& b) {
    return a.VolumeSerialNumber == b.VolumeSerialNumber &&
        std::memcmp(a.FileId.Identifier, b.FileId.Identifier, sizeof(a.FileId.Identifier)) == 0;
}
void requireKind(HANDLE h, bool directory) {
    FILE_ATTRIBUTE_TAG_INFO attributes{};
    FILE_STANDARD_INFO standard{};
    if (GetFileType(h) != FILE_TYPE_DISK ||
        !GetFileInformationByHandleEx(h, FileAttributeTagInfo, &attributes, sizeof(attributes)) ||
        !GetFileInformationByHandleEx(h, FileStandardInfo, &standard, sizeof(standard)))
        throw IOException("Restricted Windows object metadata unavailable.");
    point("attributes", h, &attributes);
    point("standard", h, &standard);
    if (attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        throw IOException("Restricted Windows filesystem refuses reparse points.");
    if (standard.DeletePending || (standard.Directory != FALSE) != directory ||
        ((attributes.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) != directory)
        throw IOException("Restricted Windows filesystem refuses delete-pending files and unexpected kinds.");
    if (!directory && standard.NumberOfLinks != 1)
        throw IOException("Restricted Windows filesystem refuses hard links and aliases.");
}
void requireLeaf(HANDLE h, const std::wstring& name) {
    auto path = normalized(h);
    if (path.substr(path.find_last_of(L'\\') + 1) != name)
        throw IOException("Restricted Windows filesystem refuses case and short-name aliases.");
}
Handle relative(HANDLE parent, const std::wstring& name, bool directory, bool data, bool missing = false) {
    auto module = GetModuleHandleW(L"ntdll.dll");
    auto create = module ? reinterpret_cast<decltype(&NtCreateFile)>(GetProcAddress(module, "NtCreateFile")) : nullptr;
    if (!create) throw IOException("Restricted Windows NtCreateFile is unavailable.");
    UNICODE_STRING text{};
    if (name.size() > std::numeric_limits<USHORT>::max() / sizeof(wchar_t))
        throw IOException("Restricted Windows child name is too long.");
    text.Buffer = const_cast<PWSTR>(name.data());
    text.Length = static_cast<USHORT>(name.size() * sizeof(wchar_t)); text.MaximumLength = text.Length;
    OBJECT_ATTRIBUTES attributes{};
    attributes.Length = sizeof(attributes); attributes.RootDirectory = parent; attributes.ObjectName = &text;
    ULONG options = FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_REPARSE_POINT;
    if (directory) options |= FILE_DIRECTORY_FILE;
    ACCESS_MASK access = FILE_READ_ATTRIBUTES | READ_CONTROL | SYNCHRONIZE;
    if (directory) access |= FILE_TRAVERSE;
    if (data) access |= FILE_READ_DATA;
    const ULONG sharing = FILE_SHARE_READ | FILE_SHARE_WRITE | (directory ? 0 : FILE_SHARE_DELETE);
    IO_STATUS_BLOCK io{};
    HANDLE raw = nullptr;
    point("native-open", parent, &options);
    const auto status = create(&raw, access, &attributes, &io, nullptr, FILE_ATTRIBUTE_NORMAL,
        sharing, FILE_OPEN, options, nullptr, 0);
    Handle result(raw);
    constexpr auto nameNotFound = static_cast<NTSTATUS>(0xC0000034UL);
    if (status == nameNotFound && missing) return {};
    if (status < 0 || io.Status < 0 || !raw || raw == INVALID_HANDLE_VALUE)
        throw IOException("Restricted Windows relative open failed: NTSTATUS=" + std::to_string(static_cast<ULONG>(status)) +
            " IO_STATUS_BLOCK=" + std::to_string(static_cast<ULONG>(io.Status)));
    return result;
}
}

void RootDirectory::validateName(const std::string& name) {
    auto wide = unicode(name);
    if (wide == L"." || wide == L".." || wide.size() > 255 || wide.back() == L'.' || wide.back() == L' ' ||
        wide.find_first_of(L"/\\:*?\"<>|") != std::wstring::npos ||
        std::any_of(wide.begin(), wide.end(), [](wchar_t c) { return c < 32 || c == 127; }))
        throw IOException("Restricted Windows filesystem requires a canonical plain child name.");
    auto stem = wide.substr(0, wide.find(L'.'));
    while (!stem.empty() && stem.back() == L' ') stem.pop_back();
    for (auto& c : stem) if (c >= L'a' && c <= L'z') c -= L'a' - L'A';
    if (stem == L"CON" || stem == L"PRN" || stem == L"AUX" || stem == L"NUL" || stem == L"CONIN$" || stem == L"CONOUT$" ||
        ((stem.starts_with(L"COM") || stem.starts_with(L"LPT")) && stem.size() == 4 &&
            ((stem[3] >= L'1' && stem[3] <= L'9') || stem[3] == L'\u00b9' || stem[3] == L'\u00b2' || stem[3] == L'\u00b3')))
        throw IOException("Restricted Windows filesystem refuses DOS device names.");
}

struct RootDirectory::State {
    std::mutex mutex;
    std::vector<Handle> directories;
    std::vector<std::wstring> names;
    std::vector<FILE_ID_INFO> identities;
    std::unordered_map<std::string, FILE_ID_INFO> observedFiles;
    void hold(Handle h, const std::wstring& name) {
        requireKind(h.get(), true);
        auto id = identity(h.get());
        directories.push_back(std::move(h)); names.push_back(name); identities.push_back(id);
    }
    void validateAncestors() const {
        for (size_t i = 0; i < directories.size(); ++i) {
            requireKind(directories[i].get(), true);
            if (!sameIdentity(identity(directories[i].get()), identities[i]))
                throw IOException("Restricted Windows ancestor identity changed.");
            if (i) {
                auto current = relative(directories[i - 1].get(), names[i], true, false);
                requireKind(current.get(), true);
                if (!sameIdentity(identity(current.get()), identities[i]))
                    throw IOException("Restricted Windows ancestor identity changed.");
            }
        }
        validateRootSecurity(directories.back().get());
    }
    FILE_ID_INFO validateChild(HANDLE h, const std::wstring& name) const {
        requireKind(h, false); requireLeaf(h, name);
        auto id = identity(h);
        if (id.VolumeSerialNumber != identities.back().VolumeSerialNumber)
            throw IOException("Restricted Windows child volume identity changed.");
        validateChildSecurity(h, directories.back().get());
        return id;
    }
    Handle probe(const std::string& name, FILE_ID_INFO& id) const {
        auto wide = unicode(name);
        auto h = relative(directories.back().get(), wide, false, false, true);
        auto remembered = observedFiles.find(name);
        if (!h) {
            if (remembered != observedFiles.end()) throw IOException("Restricted Windows remembered file disappeared.");
            return {};
        }
        id = validateChild(h.get(), wide);
        if (remembered != observedFiles.end() && !sameIdentity(remembered->second, id))
            throw IOException("Restricted Windows observed file identity changed.");
        return h;
    }
};
RootDirectory::RootDirectory() : state{std::make_unique<State>()} {}
RootDirectory::~RootDirectory() = default;

std::shared_ptr<RootDirectory> RootDirectory::open(const std::string& absoluteRoot) {
    auto path = unicode(absoluteRoot);
    auto driveAbsolute = [](const std::wstring& p) {
        return p.size() >= 3 && ((p[0] >= L'A' && p[0] <= L'Z') || (p[0] >= L'a' && p[0] <= L'z')) &&
            p[1] == L':' && (p[2] == L'\\' || p[2] == L'/');
    };
    if (!driveAbsolute(path) && !path.starts_with(L"\\\\?\\") && !path.starts_with(L"\\\\"))
        throw IOException("RootDirectory::open requires an existing absolute Windows root.");
    if (path.starts_with(L"\\\\?\\") && !driveAbsolute(path.substr(4)) && !path.starts_with(L"\\\\?\\Volume{"))
        throw IOException("RootDirectory::open refuses drive-relative and device roots.");
    if (driveAbsolute(path)) {
        // Lexical Win32 normalization only; the one trusted filesystem resolution follows.
        auto size = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
        if (!size || size > 32768) throw IOException("Restricted Windows absolute root is unavailable.");
        std::wstring full(size, L'\0');
        auto written = GetFullPathNameW(path.c_str(), size, full.data(), nullptr);
        if (!written || written >= size) throw IOException("Restricted Windows absolute root normalization failed.");
        full.resize(written); path = L"\\\\?\\" + full;
    }
    auto resolving = checked(CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES | READ_CONTROL | SYNCHRONIZE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    auto canonical = normalized(resolving.get());
    auto end = canonical.find(L"}\\");
    if (!canonical.starts_with(L"\\\\?\\Volume{") || end == std::wstring::npos)
        throw IOException("Restricted Windows root requires a normalized local volume GUID.");
    auto anchor = canonical.substr(0, end + 2);
    if (GetDriveTypeW(anchor.c_str()) != DRIVE_FIXED)
        throw IOException("Restricted Windows root requires a fixed local NTFS volume.");
    auto root = std::shared_ptr<RootDirectory>(new RootDirectory());
    auto& state = *root->state;
    state.hold(checked(CreateFileW(anchor.c_str(), FILE_READ_ATTRIBUTES | READ_CONTROL | SYNCHRONIZE | FILE_TRAVERSE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr)), L"");
    wchar_t filesystem[32]{}; DWORD maxComponent = 0;
    if (!GetVolumeInformationByHandleW(state.directories.front().get(), nullptr, 0, nullptr, &maxComponent, nullptr, filesystem, 32) ||
        std::wstring(filesystem) != L"NTFS" || maxComponent < 255)
        throw IOException("Restricted Windows root requires qualified local NTFS component semantics.");
    for (size_t start = anchor.size(); start < canonical.size();) {
        auto next = canonical.find(L'\\', start);
        auto name = canonical.substr(start, next == std::wstring::npos ? next : next - start);
        validateName(utf8(name));
        state.hold(relative(state.directories.back().get(), name, true, false), name);
        if (next == std::wstring::npos) break;
        start = next + 1;
    }
    if (!sameIdentity(identity(resolving.get()), state.identities.back()))
        throw IOException("Restricted Windows resolved root identity changed during acquisition.");
    state.validateAncestors();
    return root;
}

bool RootDirectory::probeRegularFile(const std::string& name) {
    validateName(name);
    std::lock_guard lock(state->mutex);
    state->validateAncestors();
    FILE_ID_INFO id{};
    auto file = state->probe(name, id);
    state->validateAncestors();
    if (!file) return false;
    state->observedFiles.insert_or_assign(name, id);
    return true;
}
void* RootDirectory::openFile(const std::string& name, int flags) {
    validateName(name);
    if (flags != O_RDONLY) throw IOException("Restricted writable openFile is unsupported on Windows (read-only only).");
    std::lock_guard lock(state->mutex);
    state->validateAncestors();
    FILE_ID_INFO before{}, current{};
    auto probed = state->probe(name, before);
    if (!probed) throw IOException("Restricted Windows read-only file is missing.");
    point("after-probe", probed.get());
    auto opened = relative(state->directories.back().get(), unicode(name), false, true);
    auto id = state->validateChild(opened.get(), unicode(name));
    auto now = state->probe(name, current);
    if (!sameIdentity(before, id) || !now || !sameIdentity(id, current))
        throw IOException("Restricted Windows file identity changed during open.");
    state->validateAncestors();
    state->observedFiles.insert_or_assign(name, id);
    return opened.release();
}
} // namespace lbug::common
#endif
