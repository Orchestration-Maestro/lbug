#pragma once
// Linux linker wrapping plus the harness's static libstdc++ observes ambient
// std::filesystem probes/opens as well as rooted calls.
#ifdef MAESTRO_SYSCALL_WRAP
#include <cstdarg>
#include <cstdio>
#include <dlfcn.h>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
extern bool observeFilesystem;
extern int filesystemCalls;
extern std::string observedExternalPrefix;
inline void observed(const char* path = nullptr) {
    if (observeFilesystem && (observedExternalPrefix.empty() ||
        (path && std::string_view(path).starts_with(observedExternalPrefix)))) { ++filesystemCalls; }
}
extern "C" {
int realOpen(const char*, int, ...) asm("__real_open");
int wrapOpen(const char*, int, ...) asm("__wrap_open");
int wrapOpen(const char* path, int flags, ...) {
    observed(path);
    mode_t mode = 0;
    if (flags & O_CREAT) { va_list args; va_start(args, flags); mode = va_arg(args, int); va_end(args); }
    return realOpen(path, flags, mode);
}
int realOpenAt(int, const char*, int, ...) asm("__real_openat");
int wrapOpenAt(int, const char*, int, ...) asm("__wrap_openat");
int wrapOpenAt(int dir, const char* path, int flags, ...) {
    observed(path);
    mode_t mode = 0;
    if (flags & O_CREAT) { va_list args; va_start(args, flags); mode = va_arg(args, int); va_end(args); }
    return realOpenAt(dir, path, flags, mode);
}
int realStat(const char*, struct stat*) asm("__real_stat");
int wrapStat(const char*, struct stat*) asm("__wrap_stat");
int wrapStat(const char* p, struct stat* s) { observed(p); return realStat(p, s); }
int realLstat(const char*, struct stat*) asm("__real_lstat");
int wrapLstat(const char*, struct stat*) asm("__wrap_lstat");
int wrapLstat(const char* p, struct stat* s) { observed(p); return realLstat(p, s); }
int realFstat(int, struct stat*) asm("__real_fstat");
int wrapFstat(int, struct stat*) asm("__wrap_fstat");
int wrapFstat(int fd, struct stat* s) { observed(); return realFstat(fd, s); }
int realFstatAt(int, const char*, struct stat*, int) asm("__real_fstatat");
int wrapFstatAt(int, const char*, struct stat*, int) asm("__wrap_fstatat");
int wrapFstatAt(int fd, const char* p, struct stat* s, int flags) { observed(p); return realFstatAt(fd, p, s, flags); }
FILE* realFopen(const char*, const char*) asm("__real_fopen");
FILE* wrapFopen(const char*, const char*) asm("__wrap_fopen");
FILE* wrapFopen(const char* p, const char* mode) { observed(p); return realFopen(p, mode); }
void* realDlopen(const char*, int) asm("__real_dlopen");
void* wrapDlopen(const char*, int) asm("__wrap_dlopen");
void* wrapDlopen(const char* p, int flags) { observed(p); return realDlopen(p, flags); }
void* realMmap(void*, size_t, int, int, int, off_t) asm("__real_mmap");
void* wrapMmap(void*, size_t, int, int, int, off_t) asm("__wrap_mmap");
void* wrapMmap(void* p, size_t n, int prot, int flags, int fd, off_t off) { observed(); return realMmap(p, n, prot, flags, fd, off); }
}
#endif
