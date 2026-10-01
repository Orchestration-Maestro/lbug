#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>
#include "root_directory_windows_security.h"
#include "windows_test_support.h"
#include "common/exception/io.h"
using namespace lbug::common;
namespace seam = lbug::common::windows_security_test;
namespace fs = std::filesystem;
void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
struct Handle {
    HANDLE value;
    explicit Handle(HANDLE v) : value(v) { require(v && v != INVALID_HANDLE_VALUE, "SETUP handle " + std::to_string(GetLastError())); }
    ~Handle() { CloseHandle(value); }
    Handle(const Handle&) = delete;
};
struct LocalDelete { void operator()(void* p) const { LocalFree(p); } };
using Local = std::unique_ptr<void, LocalDelete>;
struct Descriptor {
    Local data;
    ULONG size = 0;
    explicit Descriptor(const std::string& sddl) {
        PSECURITY_DESCRIPTOR raw = nullptr;
        require(ConvertStringSecurityDescriptorToSecurityDescriptorA(sddl.c_str(), SDDL_REVISION_1, &raw, &size),
            "SETUP SDDL " + sddl + " error=" + std::to_string(GetLastError()));
        data.reset(raw);
    }
};
Local sid(const std::string& text) {
    PSID raw = nullptr;
    require(ConvertStringSidToSidA(text.c_str(), &raw), "SETUP SID");
    return Local(raw);
}
std::string tokenUser(bool verifyStandard = false) {
    return windows_test::user(verifyStandard);
}
void refused(const std::function<void()>& action, const std::string& assertion, const std::string& rule = "") {
    bool rejection = false;
    try { action(); } catch (const IOException& e) {
        rejection = true;
        require(rule.empty() || std::string(e.what()).find(rule) != std::string::npos, assertion + " wrong rule: " + e.what());
        std::cout << "REFUSE " << assertion << ": " << e.what() << '\n';
    }
    require(rejection, assertion);
}
const std::string U = "S-1-5-21-100-200-300-1001";
const std::string privateSD = "O:" + U + "D:P(A;OICI;FA;;;" + U + ")(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)";
void evaluate(Descriptor& d, char role = 'R', Descriptor* parent = nullptr) {
    auto user = sid(U);
    seam::validateDescriptor(d.data.get(), d.size, user.get(), GetLengthSid(user.get()), role != 'F',
        parent ? parent->data.get() : nullptr, parent ? parent->size : 0);
}
// Independent SDDL decode checks: conversion must preserve the condition being tested.
std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> result; std::stringstream in(text); std::string part;
    while (std::getline(in, part, separator)) result.push_back(part);
    return result;
}
DWORD mask(const std::string& value) {
    if (value.starts_with("0x")) return std::stoul(value, nullptr, 16);
    DWORD result = 0;
    for (size_t i = 0; i < value.size(); i += 2) {
        const auto name = value.substr(i, 2);
        if (name == "FA") result |= FILE_ALL_ACCESS;
        else if (name == "FR") result |= FILE_GENERIC_READ;
        else if (name == "FX") result |= FILE_GENERIC_EXECUTE;
        else if (name == "GW") result |= GENERIC_WRITE;
        else if (name == "GA") result |= GENERIC_ALL;
        else throw std::runtime_error("SETUP unknown fixture mask " + name);
    }
    return result;
}
void checkDecoded(Descriptor& d, const std::string& sddl, const std::string& id) {
    BOOL present = FALSE, defaulted = FALSE; PACL acl = nullptr;
    require(GetSecurityDescriptorDacl(d.data.get(), &present, &acl, &defaulted), "SETUP decode DACL");
    size_t offset = sddl.find('('); DWORD index = 0;
    while (offset != std::string::npos) {
        const auto end = sddl.find(')', offset);
        const auto fields = split(sddl.substr(offset + 1, end - offset - 1), ';');
        require(fields.size() >= 6 && acl, "SETUP decode fields " + id);
        void* raw = nullptr;
        require(GetAce(acl, index++, &raw), "SETUP decode ACE " + id);
        auto header = static_cast<ACE_HEADER*>(raw);
        const auto type = fields[0];
        const BYTE expectedType = type == "A" ? ACCESS_ALLOWED_ACE_TYPE : type == "D" ? ACCESS_DENIED_ACE_TYPE :
            type == "OA" ? ACCESS_ALLOWED_OBJECT_ACE_TYPE : type == "OD" ? ACCESS_DENIED_OBJECT_ACE_TYPE :
            type == "XA" ? ACCESS_ALLOWED_CALLBACK_ACE_TYPE : ACCESS_DENIED_CALLBACK_ACE_TYPE;
        BYTE flags = 0;
        for (size_t i = 0; i < fields[1].size(); i += 2) {
            const auto flag = fields[1].substr(i, 2);
            if (flag == "OI") flags |= OBJECT_INHERIT_ACE;
            else if (flag == "CI") flags |= CONTAINER_INHERIT_ACE;
            else if (flag == "NP") flags |= NO_PROPAGATE_INHERIT_ACE;
            else if (flag == "IO") flags |= INHERIT_ONLY_ACE;
            else if (flag == "ID") flags |= INHERITED_ACE;
            else throw std::runtime_error("SETUP unknown fixture flag");
        }
        DWORD access = 0; std::memcpy(&access, static_cast<BYTE*>(raw) + sizeof(ACE_HEADER), sizeof(access));
        require(header->AceType == expectedType, id + " decoded ACE type");
        require(header->AceFlags == flags, id + " decoded ACE flags");
        require(access == mask(fields[2]), id + " decoded ACE mask");
        // Conditional ACEs are the last ACE; their expression has an inner ')'.
        if (type == "XA" || type == "XD") offset = std::string::npos;
        else offset = sddl.find('(', end + 1);
    }
    require(index == (acl ? acl->AceCount : 0), id + " decoded ACE count");
}
void vectors(const fs::path& fixture) {
    std::ifstream in(fixture); require(in.good(), "SETUP fixture open");
    std::string line; std::getline(in, line); unsigned count = 0;
    Descriptor parent(privateSD);
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto fields = split(line, '\t'); require(fields.size() == 6, "SETUP fixture columns");
        require(fields[2] == U && fields[3] == (fields[1] == "R" ? "none" : "V01"), "SETUP fixture context");
        Descriptor d(fields[4]); checkDecoded(d, fields[4], fields[0]);
        auto run = [&] { evaluate(d, fields[1][0], fields[1] == "R" ? nullptr : &parent); };
        if (fields[5] == "accept") run(); else refused(run, fields[0] + " expected refusal");
        ++count;
    }
    require(count == 190, "fixture expanded case count");
    std::cout << "PASS shared_vectors cases=" << count << " contract=maestro-private-root/1\n";
}
bool fixtureCleanupFailed = false;
struct Fixture {
    std::string userSID;
    fs::path parentPath;
    fs::path root;
    std::unique_ptr<Handle> parent;
    std::unique_ptr<Handle> sentinel;
    FILE_ID_INFO sentinelID{};
    DWORD sentinelLinks = 0;
    std::unique_ptr<Handle> directory;
    std::unique_ptr<Handle> child;
    explicit Fixture(const fs::path& scratch) : userSID(tokenUser()) {
        parentPath = scratch / ("acl-" + std::to_string(GetCurrentProcessId()));
        require(fs::create_directory(parentPath), "SETUP parent directory");
        parent = std::make_unique<Handle>(CreateFileW(parentPath.c_str(), READ_CONTROL | WRITE_DAC,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
        sentinel = std::make_unique<Handle>(CreateFileW((parentPath / "outside").c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        DWORD written;
        require(WriteFile(sentinel->value, "outside", 7, &written, nullptr) && written == 7, "SETUP sentinel bytes");
        require(GetFileInformationByHandleEx(sentinel->value, FileIdInfo, &sentinelID, sizeof(sentinelID)), "SETUP sentinel ID");
        BY_HANDLE_FILE_INFORMATION initial{};
        require(GetFileInformationByHandle(sentinel->value, &initial), "SETUP sentinel links");
        sentinelLinks = initial.nNumberOfLinks;
        root = parentPath / "root";
        require(fs::create_directory(root), "SETUP new fixture root");
        directory = std::make_unique<Handle>(CreateFileW(root.c_str(), READ_CONTROL | WRITE_DAC | FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
        set("D:P(A;OICI;FA;;;" + userSID + ")(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)");
        child = std::make_unique<Handle>(CreateFileW((root / "child").c_str(), READ_CONTROL | WRITE_DAC | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    }
    void set(const std::string& sddl) {
        Descriptor d(sddl); BOOL present, defaulted; PACL acl;
        require(GetSecurityDescriptorDacl(d.data.get(), &present, &acl, &defaulted), "SETUP DACL");
        const DWORD error = SetSecurityInfo(directory->value, SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, acl, nullptr);
        require(error == ERROR_SUCCESS, "SETUP SetSecurityInfo " + std::to_string(error));
    }
    void unchanged() {
        FILE_ID_INFO now{}; BY_HANDLE_FILE_INFORMATION info{};
        require(GetFileInformationByHandleEx(sentinel->value, FileIdInfo, &now, sizeof(now)) &&
            GetFileInformationByHandle(sentinel->value, &info), "sentinel metadata readable");
        require(now.VolumeSerialNumber == sentinelID.VolumeSerialNumber &&
            std::memcmp(now.FileId.Identifier, sentinelID.FileId.Identifier, 16) == 0 &&
            info.nNumberOfLinks == sentinelLinks, "outside sentinel volume ID and links unchanged");
        LARGE_INTEGER zero{};
        require(SetFilePointerEx(sentinel->value, zero, nullptr, FILE_BEGIN), "sentinel seek");
        char bytes[8]{}; DWORD read;
        require(ReadFile(sentinel->value, bytes, sizeof(bytes), &read, nullptr) && read == 7 &&
            std::string(bytes, read) == "outside", "outside sentinel bytes unchanged");
        unsigned count = 0;
        for (const auto& entry : fs::directory_iterator(root)) {
            require(entry.path().filename() == "child" && entry.file_size() == 0, "root directory snapshot unchanged");
            ++count;
        }
        require(count == 1, "root directory listing unchanged");
    }
    ~Fixture() {
        try {
            // Test setup/cleanup only: a restrictive fixture must remain removable.
            // The engine evaluator itself never sets an ACL.
            set("D:P(A;OICI;FA;;;" + userSID + ")");
            child.reset(); directory.reset(); sentinel.reset(); parent.reset();
            std::error_code error; fs::remove_all(parentPath, error);
            require(!error && !fs::exists(parentPath), "SETUP fixture cleanup " + error.message());
        } catch (const std::exception& e) {
            fixtureCleanupFailed = true;
            std::cerr << "SETUP cleanup failure: " << e.what() << '\n';
        }
    }
};
std::vector<BYTE> securityBytes(HANDLE handle) {
    PSECURITY_DESCRIPTOR raw = nullptr;
    const DWORD error = GetSecurityInfo(handle, SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, nullptr, nullptr, nullptr, nullptr, &raw);
    Local descriptor(raw);
    require(error == ERROR_SUCCESS && raw, "SETUP real security snapshot " + std::to_string(error));
    auto begin = static_cast<BYTE*>(raw);
    return std::vector<BYTE>(begin, begin + GetSecurityDescriptorLength(raw));
}
void private_root_and_child_are_accepted(const fs::path& scratch) {
    Fixture f(scratch);
    const auto rootBefore = securityBytes(f.directory->value);
    const auto childBefore = securityBytes(f.child->value);
    BOOL inherited = FALSE, defaulted = FALSE; PACL inheritedAcl = nullptr;
    require(GetSecurityDescriptorDacl(const_cast<BYTE*>(childBefore.data()), &inherited, &inheritedAcl, &defaulted) && inherited && inheritedAcl, "SETUP real inherited child DACL");
    unsigned inheritedCount = 0;
    for (DWORD i = 0; i < inheritedAcl->AceCount; ++i) {
        void* raw = nullptr;
        require(GetAce(inheritedAcl, i, &raw), "SETUP real inherited ACE");
        if (static_cast<ACE_HEADER*>(raw)->AceFlags & INHERITED_ACE) ++inheritedCount;
    }
    require(inheritedCount >= 3, "SETUP real user/SYSTEM/admin child inheritance");
    try { validateRootSecurity(f.directory->value); validateChildSecurity(f.child->value, f.directory->value); }
    catch (const IOException& e) { throw std::runtime_error(std::string("private real root and inherited child accepted: ") + e.what()); }
    require(rootBefore == securityBytes(f.directory->value) && childBefore == securityBytes(f.child->value),
        "validation never modifies owner or DACL");
    // Change the real parent's inheritable ACL; protected root must not acquire it.
    Descriptor broad("D:(A;OICI;FA;;;" + tokenUser() + ")(A;OICI;GW;;;WD)");
    BOOL present; PACL acl;
    require(GetSecurityDescriptorDacl(broad.data.get(), &present, &acl, &defaulted), "SETUP parent DACL");
    require(SetSecurityInfo(f.parent->value, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
        nullptr, nullptr, acl, nullptr) == ERROR_SUCCESS, "SETUP parent inheritance propagation");
    validateRootSecurity(f.directory->value);
    validateChildSecurity(f.child->value, f.directory->value);
    f.unchanged();
    std::cout << "PASS real_GetSecurityInfo private_root_and_child_are_accepted\n";
}
void foreign_owner_or_writable_acl_refuses(const fs::path& scratch) {
    Fixture f(scratch); validateRootSecurity(f.directory->value);
    f.set("D:P(A;;FA;;;" + tokenUser() + ")(A;;GW;;;WD)");
    refused([&] { validateRootSecurity(f.directory->value); }, "unsafe writable ACE refused", "mutating grant");
    Descriptor foreign("O:S-1-5-21-100-200-300-1002D:P(A;;FA;;;" + U + ")");
    refused([&] { evaluate(foreign); }, "foreign owner refused (synthetic, no privileged setup)", "owner");
    Descriptor nullDacl("O:" + U + "D:NO_ACCESS_CONTROL");
    refused([&] { evaluate(nullDacl); }, "null DACL refused", "null DACL");
    f.unchanged();
}
void unreadable_security_is_not_private(const fs::path& scratch) {
    Fixture f(scratch);
    Handle unreadable(CreateFileW((f.root / "child").c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    refused([&] { validateChildSecurity(unreadable.value, f.directory->value); }, "unreadable real child security refused", "security query");
    seam::setFault(seam::Fault::Query);
    try { refused([&] { validateRootSecurity(f.directory->value); }, "security-query failure refused", "security query"); }
    catch (...) { seam::setFault(seam::Fault::None); throw; }
    seam::setFault(seam::Fault::None);
    f.unchanged();
}
void non_sddl_inputs(const fs::path& scratch) {
    Fixture f(scratch); Descriptor good(privateSD);
    for (auto fault : {seam::Fault::Token, seam::Fault::Query, seam::Fault::Control, seam::Fault::GetAce}) {
        seam::setFault(fault);
        try { refused([&] { validateRootSecurity(f.directory->value); }, "injected API/token refusal"); }
        catch (...) { seam::setFault(seam::Fault::None); throw; }
        seam::setFault(seam::Fault::None);
    }
    seam::setFault(seam::Fault::Impersonation);
    try { refused([&] { validateRootSecurity(f.directory->value); }, "unreadable thread token refused", "active or unreadable impersonation token"); }
    catch (...) { seam::setFault(seam::Fault::None); throw; }
    seam::setFault(seam::Fault::None);
    auto user = sid(U);
    refused([&] { seam::validateDescriptor(good.data.get(), good.size, nullptr, 0, true, nullptr, 0); }, "null token SID");
    refused([&] { seam::validateDescriptor(good.data.get(), good.size, user.get(), 1, true, nullptr, 0); }, "truncated token SID");
    refused([&] { seam::validateDescriptor(good.data.get(), 1, user.get(), GetLengthSid(user.get()), true, nullptr, 0); }, "truncated descriptor");
    auto malformed = [&](const std::string& name, const std::function<void(std::vector<BYTE>&)>& mutate) {
        std::vector<BYTE> bytes(good.size); std::memcpy(bytes.data(), good.data.get(), bytes.size()); mutate(bytes);
        auto user = sid(U);
        refused([&] { seam::validateDescriptor(bytes.data(), static_cast<ULONG>(bytes.size()), user.get(), GetLengthSid(user.get()), true, nullptr, 0); }, name);
    };
    auto acl = [](std::vector<BYTE>& b) { return reinterpret_cast<ACL*>(b.data() + reinterpret_cast<SECURITY_DESCRIPTOR_RELATIVE*>(b.data())->Dacl); };
    auto ace = [&](std::vector<BYTE>& b) { return reinterpret_cast<ACCESS_ALLOWED_ACE*>(reinterpret_cast<BYTE*>(acl(b)) + sizeof(ACL)); };
    malformed("null owner", [](auto& b) { reinterpret_cast<SECURITY_DESCRIPTOR_RELATIVE*>(b.data())->Owner = 0; });
    malformed("owner offset bounds", [](auto& b) { reinterpret_cast<SECURITY_DESCRIPTOR_RELATIVE*>(b.data())->Owner = static_cast<DWORD>(b.size() - 1); });
    malformed("invalid owner SID", [](auto& b) { b[reinterpret_cast<SECURITY_DESCRIPTOR_RELATIVE*>(b.data())->Owner] = 0; });
    malformed("owner SID bounds", [](auto& b) { b[reinterpret_cast<SECURITY_DESCRIPTOR_RELATIVE*>(b.data())->Owner + 1] = 255; });
    malformed("malformed ACL size", [&](auto& b) { acl(b)->AclSize = 65535; });
    malformed("malformed ACL revision", [&](auto& b) { acl(b)->AclRevision = 0; });
    malformed("malformed ACE bounds", [&](auto& b) { ace(b)->Header.AceSize = 65532; });
    malformed("short ACE", [&](auto& b) { ace(b)->Header.AceSize = 4; });
    malformed("malformed ACE SID bounds", [&](auto& b) { reinterpret_cast<BYTE*>(&ace(b)->SidStart)[1] = 255; });
    malformed("invalid ACE SID", [&](auto& b) { reinterpret_cast<BYTE*>(&ace(b)->SidStart)[0] = 0; });
    malformed("unknown ACE type", [&](auto& b) { ace(b)->Header.AceType = 255; });
    malformed("unknown ACE flags", [&](auto& b) { ace(b)->Header.AceFlags = 0x20; });
    malformed("NP without inheritance", [&](auto& b) { ace(b)->Header.AceFlags = NO_PROPAGATE_INHERIT_ACE; });
    Descriptor child("O:" + U + "D:AI(A;ID;FA;;;" + U + ")");
    Descriptor badParent("O:" + U + "D:(A;;FA;;;" + U + ")");
    refused([&] { evaluate(child, 'F', &badParent); }, "V07 failing parent", "protected root");
    require(ImpersonateSelf(SecurityImpersonation), "SETUP real impersonation");
    try { refused([&] { validateRootSecurity(f.directory->value); }, "real active impersonation", "impersonation"); }
    catch (...) { RevertToSelf(); throw; }
    require(RevertToSelf(), "SETUP RevertToSelf");
    Descriptor changedOwner("O:S-1-5-21-100-200-300-1002D:AI(A;ID;FA;;;" + U + ")");
    refused([&] { evaluate(changedOwner, 'F', &good); }, "V07 changed child owner", "owner");
    // Root must refuse before use if the real DACL is made unprotected.
    Descriptor safe("D:(A;OICI;FA;;;" + tokenUser() + ")");
    BOOL present, defaulted; PACL safeAcl;
    require(GetSecurityDescriptorDacl(safe.data.get(), &present, &safeAcl, &defaulted), "SETUP safe DACL");
    require(SetSecurityInfo(f.directory->value, SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | UNPROTECTED_DACL_SECURITY_INFORMATION,
        nullptr, nullptr, safeAcl, nullptr) == ERROR_SUCCESS, "SETUP unprotected root");
    refused([&] { validateRootSecurity(f.directory->value); }, "real V05 root protection", "protected root");
    Descriptor protectedFile("D:P(A;;FA;;;" + f.userSID + ")");
    BOOL filePresent, fileDefaulted; PACL fileAcl = nullptr;
    require(GetSecurityDescriptorDacl(protectedFile.data.get(), &filePresent, &fileAcl, &fileDefaulted) && filePresent && fileAcl,
        "SETUP protected regular file DACL");
    require(SetSecurityInfo(f.child->value, SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
        nullptr, nullptr, fileAcl, nullptr) == ERROR_SUCCESS, "SETUP protected regular file SetSecurityInfo");
    refused([&] { validateRootSecurity(f.child->value); }, "protected regular file cannot be root", "root security requires directory");
    f.unchanged();
    std::cout << "PASS non_SDDL_inputs\n";
}
int main(int argc, char** argv) {
    try {
        require(argc >= 3, "SETUP arguments scratch fixture [test]");
        tokenUser(true);
        const std::string filter = argc > 3 ? argv[3] : "";
        unsigned count = 0;
        for (const auto& test : std::vector<std::pair<std::string, std::function<void()>>>{
            {"token_sizing_bad_length_is_handled", [&] {
                require(windows_test::sawBadLength, "real ERROR_BAD_LENGTH token sizing path not reached");
            }},
            {"private_root_and_child_are_accepted", [&] { private_root_and_child_are_accepted(argv[1]); }},
            {"foreign_owner_or_writable_acl_refuses", [&] { foreign_owner_or_writable_acl_refuses(argv[1]); }},
            {"unreadable_security_is_not_private", [&] { unreadable_security_is_not_private(argv[1]); }},
            {"shared_vectors", [&] { vectors(argv[2]); }},
            {"non_sddl_inputs", [&] { non_sddl_inputs(argv[1]); }}}) {
            if (!filter.empty() && filter != test.first) continue;
            ++count;
            try { test.second(); require(!fixtureCleanupFailed, "SETUP fixture cleanup"); std::cout << "PASS " << test.first << '\n'; }
            catch (const std::exception& e) { std::cerr << "FAIL " << test.first << ": " << e.what() << '\n'; return 1; }
        }
        require(count == (filter.empty() ? 6 : 1), "mandatory test group count");
        std::cout << "PASS windows_security groups=" << count << '\n';
        return 0;
    } catch (const std::exception& e) { std::cerr << "SETUP FAILURE: " << e.what() << '\n'; return 2; }
}
