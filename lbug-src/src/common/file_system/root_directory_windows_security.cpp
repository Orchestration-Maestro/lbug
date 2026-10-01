#include "root_directory_windows_security.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <aclapi.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include "common/exception/io.h"

namespace lbug::common {
namespace {
[[noreturn]] void fail(const char* rule) {
    throw IOException(std::string("maestro-private-root/1: ") + rule);
}
struct HandleDelete { void operator()(void* p) const { if (p) CloseHandle(p); } };
struct LocalDelete { void operator()(void* p) const { if (p) LocalFree(p); } };
using Handle = std::unique_ptr<void, HandleDelete>;
using Local = std::unique_ptr<void, LocalDelete>;
struct Module {
    HMODULE value = LoadLibraryExW(L"advapi32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    Module() { if (!value) fail("system32 security library unavailable"); }
    ~Module() { FreeLibrary(value); }
    Module(const Module&) = delete;
};
// No import-library/build.rs dependency, and no ambient DLL search.
struct Api {
    Module module;
    template<class T> T resolve(const char* name) {
        auto address = GetProcAddress(module.value, name);
        if (!address) fail("required security export unavailable");
        return reinterpret_cast<T>(address);
    }
    decltype(&GetSecurityInfo) securityInfo = resolve<decltype(&GetSecurityInfo)>("GetSecurityInfo");
    decltype(&GetSecurityDescriptorLength) descriptorLength = resolve<decltype(&GetSecurityDescriptorLength)>("GetSecurityDescriptorLength");
    decltype(&GetSecurityDescriptorControl) control = resolve<decltype(&GetSecurityDescriptorControl)>("GetSecurityDescriptorControl");
    decltype(&OpenProcessToken) processToken = resolve<decltype(&OpenProcessToken)>("OpenProcessToken");
    decltype(&OpenThreadToken) threadToken = resolve<decltype(&OpenThreadToken)>("OpenThreadToken");
    decltype(&GetTokenInformation) tokenInfo = resolve<decltype(&GetTokenInformation)>("GetTokenInformation");
    decltype(&IsValidSid) validSid = resolve<decltype(&IsValidSid)>("IsValidSid");
    decltype(&EqualSid) equalSid = resolve<decltype(&EqualSid)>("EqualSid");
    decltype(&IsValidAcl) validAcl = resolve<decltype(&IsValidAcl)>("IsValidAcl");
    decltype(&GetAce) getAce = resolve<decltype(&GetAce)>("GetAce");
    decltype(&MapGenericMask) mapMask = resolve<decltype(&MapGenericMask)>("MapGenericMask");
    decltype(&CreateWellKnownSid) wellKnownSid = resolve<decltype(&CreateWellKnownSid)>("CreateWellKnownSid");
};
#ifdef MAESTRO_WINDOWS_SECURITY_TEST
thread_local windows_security_test::Fault testFault = windows_security_test::Fault::None;
#endif
struct Bytes {
    const BYTE* data;
    size_t size;
    bool contains(size_t offset, size_t length) const {
        return offset <= size && length <= size - offset;
    }
    PSID sid(size_t offset, const Api& api) const {
        if (!contains(offset, 8)) fail("SID bounds");
        const size_t length = 8 + 4 * static_cast<size_t>(data[offset + 1]);
        if (!contains(offset, length)) fail("SID bounds");
        auto result = const_cast<BYTE*>(data + offset);
        if (!api.validSid(result)) fail("invalid SID");
        return result;
    }
};
void noImpersonation(const Api& api) {
    HANDLE raw = nullptr;
    BOOL opened = api.threadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &raw);
    DWORD error = opened ? ERROR_SUCCESS : GetLastError();
    Handle token(raw);
#ifdef MAESTRO_WINDOWS_SECURITY_TEST
    if (testFault == windows_security_test::Fault::Impersonation) {
        opened = FALSE;
        error = ERROR_ACCESS_DENIED;
    }
#endif
    if (opened || error != ERROR_NO_TOKEN) fail("active or unreadable impersonation token");
}
struct PrimaryUser {
    std::vector<BYTE> buffer;
    PSID sid;
    explicit PrimaryUser(const Api& api) {
#ifdef MAESTRO_WINDOWS_SECURITY_TEST
        if (testFault == windows_security_test::Fault::Token) fail("process TokenUser query");
#endif
        HANDLE raw = nullptr;
        const BOOL opened = api.processToken(GetCurrentProcess(), TOKEN_QUERY, &raw);
        Handle token(raw);
        if (!opened) fail("process TokenUser open");
        DWORD size = 0;
        const BOOL sized = api.tokenInfo(raw, TokenUser, nullptr, 0, &size);
        const DWORD error = GetLastError();
        if (sized || error != ERROR_INSUFFICIENT_BUFFER || size < sizeof(TOKEN_USER)) fail("process TokenUser size");
        buffer.resize(size);
        if (!api.tokenInfo(raw, TokenUser, buffer.data(), size, &size) || size > buffer.size() || size < sizeof(TOKEN_USER)) {
            fail("process TokenUser query");
        }
        const auto address = reinterpret_cast<uintptr_t>(reinterpret_cast<const TOKEN_USER*>(buffer.data())->User.Sid);
        const auto begin = reinterpret_cast<uintptr_t>(buffer.data());
        if (address < begin || address - begin < sizeof(TOKEN_USER)) fail("process TokenUser SID bounds");
        sid = Bytes{buffer.data(), size}.sid(address - begin, api);
    }
};
struct TrustedSids {
    alignas(DWORD) std::array<BYTE, SECURITY_MAX_SID_SIZE> system{}, administrators{}, creator{};
    explicit TrustedSids(const Api& api) {
        auto make = [&](WELL_KNOWN_SID_TYPE kind, auto& buffer) {
            DWORD size = static_cast<DWORD>(buffer.size());
            if (!api.wellKnownSid(kind, nullptr, buffer.data(), &size) || size > buffer.size()) fail("trusted SID construction");
        };
        make(WinLocalSystemSid, system);
        make(WinBuiltinAdministratorsSid, administrators);
        make(WinCreatorOwnerSid, creator);
    }
};
SECURITY_DESCRIPTOR_RELATIVE header(Bytes bytes) {
    if (!bytes.data || !bytes.contains(0, sizeof(SECURITY_DESCRIPTOR_RELATIVE))) fail("security descriptor bounds");
    SECURITY_DESCRIPTOR_RELATIVE result{};
    std::memcpy(&result, bytes.data, sizeof(result));
    if (result.Revision != SECURITY_DESCRIPTOR_REVISION || !(result.Control & SE_SELF_RELATIVE)) fail("invalid self-relative security descriptor");
    return result;
}
PSID owner(Bytes bytes, const Api& api) {
    const auto sd = header(bytes);
    if (!sd.Owner) fail("null owner");
    if (sd.Owner < sizeof(sd) || sd.Owner % sizeof(DWORD)) fail("owner bounds");
    return bytes.sid(sd.Owner, api);
}
// Validate bounds before any SDK routine can follow an untrusted offset.
PACL boundedAcl(Bytes bytes, DWORD offset) {
    if (offset < sizeof(SECURITY_DESCRIPTOR_RELATIVE) || offset % sizeof(DWORD) || !bytes.contains(offset, sizeof(ACL))) fail("ACL bounds");
    auto acl = reinterpret_cast<PACL>(const_cast<BYTE*>(bytes.data + offset));
    if ((acl->AclRevision != ACL_REVISION && acl->AclRevision != ACL_REVISION_DS) ||
        acl->AclSize < sizeof(ACL) || acl->AclSize % sizeof(DWORD) || !bytes.contains(offset, acl->AclSize)) fail("malformed ACL");
    return acl;
}
constexpr DWORD knownMask = FILE_ALL_ACCESS | GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE | GENERIC_ALL;
constexpr DWORD mutationMask = FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_EA |
    FILE_WRITE_ATTRIBUTES | FILE_DELETE_CHILD | DELETE | WRITE_DAC | WRITE_OWNER;
static_assert(mutationMask == 0x000d0156);
void evaluate(Bytes bytes, PSID user, bool directory, bool root, PSID parentOwner, const Api& api) {
    const auto sd = header(bytes);
    auto objectOwner = owner(bytes, api);
    if (!api.equalSid(objectOwner, user)) fail("current-user owner");
    if (parentOwner && !api.equalSid(objectOwner, parentOwner)) fail("held-root owner equality");
    // Group/SACL are not queried or used for policy, but offsets must still be bounded.
    if (sd.Group) {
        if (sd.Group < sizeof(sd) || sd.Group % sizeof(DWORD)) fail("group bounds");
        bytes.sid(sd.Group, api);
    }
    if (sd.Sacl) {
        auto sacl = boundedAcl(bytes, sd.Sacl);
        if (!api.validAcl(sacl)) fail("malformed SACL");
    }
    SECURITY_DESCRIPTOR_CONTROL control = 0; DWORD revision = 0;
    BOOL controlled = api.control(const_cast<BYTE*>(bytes.data), &control, &revision);
#ifdef MAESTRO_WINDOWS_SECURITY_TEST
    if (testFault == windows_security_test::Fault::Control) controlled = FALSE;
#endif
    if (!controlled || revision != SECURITY_DESCRIPTOR_REVISION || control != sd.Control) fail("security control query");
    if (!(control & SE_DACL_PRESENT)) fail("absent DACL");
    if (sd.Dacl == 0) fail("null DACL");
    if (root && !(control & SE_DACL_PROTECTED)) fail("protected root DACL required");
    auto acl = boundedAcl(bytes, sd.Dacl);
    const Bytes aclBytes{reinterpret_cast<BYTE*>(acl), acl->AclSize};
    TrustedSids trusted(api);
    size_t offset = sizeof(ACL);
    for (DWORD index = 0; index < acl->AceCount; ++index) {
        if (!aclBytes.contains(offset, sizeof(ACE_HEADER))) fail("ACE header bounds");
        const auto* ace = reinterpret_cast<const ACE_HEADER*>(aclBytes.data + offset);
        if (ace->AceSize < sizeof(ACCESS_ALLOWED_ACE) || ace->AceSize % sizeof(DWORD) || !aclBytes.contains(offset, ace->AceSize)) fail("ACE bounds");
        void* decoded = nullptr;
        BOOL retrieved = api.getAce(acl, index, &decoded);
#ifdef MAESTRO_WINDOWS_SECURITY_TEST
        if (testFault == windows_security_test::Fault::GetAce) retrieved = FALSE;
#endif
        if (!retrieved || decoded != ace) fail("GetAce query or bounds");
        if (ace->AceType != ACCESS_ALLOWED_ACE_TYPE && ace->AceType != ACCESS_DENIED_ACE_TYPE) fail("unsupported ACE type");
        constexpr BYTE flags = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | NO_PROPAGATE_INHERIT_ACE | INHERIT_ONLY_ACE | INHERITED_ACE;
        if (ace->AceFlags & ~flags) fail("unsupported ACE flags");
        if ((ace->AceFlags & (INHERIT_ONLY_ACE | NO_PROPAGATE_INHERIT_ACE)) &&
            !(ace->AceFlags & (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE))) fail("inheritance flags without target");
        DWORD access = 0;
        std::memcpy(&access, aclBytes.data + offset + sizeof(ACE_HEADER), sizeof(access));
        if (access & ~knownMask) fail("unsupported ACE access mask");
        const Bytes aceBytes{aclBytes.data + offset, ace->AceSize};
        auto principal = aceBytes.sid(offsetof(ACCESS_ALLOWED_ACE, SidStart), api);
        GENERIC_MAPPING mapping{FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS};
        api.mapMask(&access, &mapping);
        if (ace->AceType == ACCESS_ALLOWED_ACE_TYPE) {
            const bool creatorOwner = api.equalSid(principal, trusted.creator.data()) != FALSE;
            if (creatorOwner && (!directory || !(ace->AceFlags & INHERIT_ONLY_ACE) ||
                !(ace->AceFlags & (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE)))) fail("CREATOR OWNER requires inherit-only directory ACE");
            if ((access & mutationMask) && !creatorOwner && !api.equalSid(principal, user) &&
                !api.equalSid(principal, trusted.system.data()) && !api.equalSid(principal, trusted.administrators.data())) {
                fail("untrusted mutating grant");
            }
        }
        offset += ace->AceSize;
    }
    if (!api.validAcl(acl)) fail("malformed ACL");
}
Local validateHandle(void* handle, PSID user, bool root, PSID parentOwner, const Api& api) {
    PSECURITY_DESCRIPTOR raw = nullptr;
    DWORD error = api.securityInfo(handle, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        nullptr, nullptr, nullptr, nullptr, &raw);
    Local descriptor(raw);
#ifdef MAESTRO_WINDOWS_SECURITY_TEST
    if (testFault == windows_security_test::Fault::Query) error = ERROR_ACCESS_DENIED;
#endif
    if (error != ERROR_SUCCESS) fail("security query failed");
    if (!descriptor) fail("security query returned no descriptor");
    FILE_STANDARD_INFO info{};
    if (!GetFileInformationByHandleEx(handle, FileStandardInfo, &info, sizeof(info))) fail("security object kind query");
    if (root && !info.Directory) fail("root security requires directory");
    // This SDK length query sees only a descriptor returned by Windows, never injected bytes.
    const DWORD size = api.descriptorLength(raw);
    evaluate(Bytes{static_cast<const BYTE*>(raw), size}, user, info.Directory != FALSE, root, parentOwner, api);
    return descriptor;
}
}
void validateRootSecurity(void* handle) {
    Api api;
    noImpersonation(api);
    PrimaryUser user(api);
    validateHandle(handle, user.sid, true, nullptr, api);
}
void validateChildSecurity(void* child, void* root) {
    Api api;
    noImpersonation(api);
    PrimaryUser user(api);
    auto rootDescriptor = validateHandle(root, user.sid, true, nullptr, api);
    auto rootOwner = owner(Bytes{static_cast<const BYTE*>(rootDescriptor.get()), api.descriptorLength(rootDescriptor.get())}, api);
    validateHandle(child, user.sid, false, rootOwner, api);
}
#ifdef MAESTRO_WINDOWS_SECURITY_TEST
namespace windows_security_test {
void setFault(Fault fault) { testFault = fault; }
void validateDescriptor(const void* descriptor, unsigned long size, const void* tokenSid,
    unsigned long tokenSize, bool directory, const void* parent, unsigned long parentSize) {
    Api api;
    noImpersonation(api);
    if (!tokenSid) fail("null process TokenUser SID");
    auto user = Bytes{static_cast<const BYTE*>(tokenSid), tokenSize}.sid(0, api);
    PSID rootOwner = nullptr;
    if (parent) {
        const Bytes bytes{static_cast<const BYTE*>(parent), parentSize};
        evaluate(bytes, user, true, true, nullptr, api);
        rootOwner = owner(bytes, api);
    } else if (parentSize) fail("validated parent bounds");
    evaluate(Bytes{static_cast<const BYTE*>(descriptor), size}, user, directory, !parent, rootOwner, api);
}
}
#endif
}
#endif
