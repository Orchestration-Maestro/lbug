#pragma once

#ifdef _WIN32
namespace lbug::common {
// Internal handle boundary; E02c wires this into acquisition/open, never repairs ACLs.
void validateRootSecurity(void* handle);
void validateChildSecurity(void* child, void* root);
#ifdef MAESTRO_WINDOWS_SECURITY_TEST
namespace windows_security_test {
enum class Fault { None, Token, Query, Control, GetAce, Impersonation };
void setFault(Fault fault);
void validateDescriptor(const void* descriptor, unsigned long size, const void* tokenSid,
    unsigned long tokenSize, bool directory, const void* parent, unsigned long parentSize);
}
#endif
}
#endif
