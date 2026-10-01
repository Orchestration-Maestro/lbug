#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>

namespace windows_test {
namespace fs = std::filesystem;
inline void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
struct Close { void operator()(void* p) const { if (p && p != INVALID_HANDLE_VALUE) CloseHandle(p); } };
using Handle = std::unique_ptr<void, Close>;
inline Handle hold(HANDLE h) {
    require(h && h != INVALID_HANDLE_VALUE, "SETUP handle error=" + std::to_string(GetLastError()));
    return Handle(h);
}
inline std::string utf8(const fs::path& p) {
    auto s = p.u8string();
    return {reinterpret_cast<const char*>(s.data()), s.size()};
}
inline std::vector<BYTE> tokenInfo(HANDLE token, TOKEN_INFORMATION_CLASS kind) {
    DWORD size = 0;
    GetTokenInformation(token, kind, nullptr, 0, &size);
    require(GetLastError() == ERROR_INSUFFICIENT_BUFFER && size, "SETUP token size");
    std::vector<BYTE> bytes(size);
    require(GetTokenInformation(token, kind, bytes.data(), size, &size), "SETUP token query");
    return bytes;
}
inline std::string user(bool standard = false) {
    HANDLE raw = nullptr;
    require(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw), "SETUP token");
    auto token = hold(raw);
    if (standard) {
        auto elevation = tokenInfo(raw, TokenElevation);
        require(!reinterpret_cast<TOKEN_ELEVATION*>(elevation.data())->TokenIsElevated, "SETUP non-elevated token");
        BYTE admin[SECURITY_MAX_SID_SIZE]; DWORD size = sizeof(admin);
        require(CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, admin, &size), "SETUP admin SID");
        auto groups = tokenInfo(raw, TokenGroups);
        auto g = reinterpret_cast<TOKEN_GROUPS*>(groups.data());
        for (DWORD i = 0; i < g->GroupCount; ++i) require(!EqualSid(g->Groups[i].Sid, admin), "SETUP no admin membership");
        auto privileges = tokenInfo(raw, TokenPrivileges);
        auto p = reinterpret_cast<TOKEN_PRIVILEGES*>(privileges.data());
        for (DWORD i = 0; i < p->PrivilegeCount; ++i) {
            char name[256]; DWORD length = sizeof(name);
            require(LookupPrivilegeNameA(nullptr, &p->Privileges[i].Luid, name, &length), "SETUP privilege");
            require(std::string(name) != "SeBackupPrivilege" && std::string(name) != "SeRestorePrivilege", "SETUP no bypass privilege");
        }
        std::cout << "PASS verified_non_admin_token (before fixture handles)\n";
    }
    auto info = tokenInfo(raw, TokenUser);
    LPSTR text = nullptr;
    require(ConvertSidToStringSidA(reinterpret_cast<TOKEN_USER*>(info.data())->User.Sid, &text), "SETUP user SID");
    std::string result(text); LocalFree(text);
    return result;
}
inline void makePrivate(const fs::path& path) {
    auto h = hold(CreateFileW(path.c_str(), READ_CONTROL | WRITE_DAC | WRITE_OWNER,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    PSECURITY_DESCRIPTOR sd = nullptr;
    const auto sddl = "O:" + user() + "D:P(A;OICI;FA;;;" + user() + ")(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)";
    require(ConvertStringSecurityDescriptorToSecurityDescriptorA(sddl.c_str(), SDDL_REVISION_1, &sd, nullptr), "SETUP private DACL");
    BOOL present, defaulted; PACL acl; PSID owner;
    require(GetSecurityDescriptorDacl(sd, &present, &acl, &defaulted) && GetSecurityDescriptorOwner(sd, &owner, &defaulted), "SETUP decode private DACL");
    auto error = SetSecurityInfo(h.get(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION |
        PROTECTED_DACL_SECURITY_INFORMATION, owner, nullptr, acl, nullptr);
    LocalFree(sd);
    require(error == ERROR_SUCCESS, "SETUP private root error=" + std::to_string(error));
}
inline std::string bytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    require(in.good(), "SETUP read bytes");
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
inline std::set<std::wstring> entries(const fs::path& path) {
    std::set<std::wstring> result;
    for (const auto& e : fs::directory_iterator(path)) result.insert(e.path().filename().wstring());
    return result;
}
struct Snapshot {
    FILE_ID_INFO id{};
    DWORD links;
    std::string content;
    explicit Snapshot(const fs::path& path) : content(bytes(path)) {
        auto h = hold(CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        FILE_STANDARD_INFO standard{};
        require(GetFileInformationByHandleEx(h.get(), FileIdInfo, &id, sizeof(id)) &&
            GetFileInformationByHandleEx(h.get(), FileStandardInfo, &standard, sizeof(standard)), "SETUP snapshot metadata");
        links = standard.NumberOfLinks;
    }
    void unchanged(const fs::path& path) const {
        Snapshot now(path);
        require(content == now.content && links == now.links && id.VolumeSerialNumber == now.id.VolumeSerialNumber &&
            std::memcmp(id.FileId.Identifier, now.id.FileId.Identifier, 16) == 0, "outside sentinel bytes, volume, ID or links changed");
    }
};
}
