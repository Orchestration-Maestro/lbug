#pragma once
#include "common/file_system/local_file_system.h"
#include "common/file_system/virtual_file_system.h"
#include "storage/storage_utils.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
using namespace lbug::common;
namespace fs = std::filesystem;
inline void require(bool ok, const char* message) { if (!ok) { throw std::runtime_error(message); } }
template<class F> void refused(F op) {
    bool failed = false;
    try { op(); } catch (const std::exception&) { failed = true; }
    require(failed, "unsafe operation accepted");
}
inline std::string bytes(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
#ifndef _WIN32
namespace lbug::common {
extern int (*maestroRestrictedCall)(const char*, int, const char*, struct stat*);
}
struct NativeHook {
    explicit NativeHook(decltype(maestroRestrictedCall) hook) { maestroRestrictedCall = hook; }
    ~NativeHook() { maestroRestrictedCall = nullptr; }
};
struct Fixture {
    fs::path dir = fs::temp_directory_path() / ("maestro-sidecars-" + std::to_string(getpid()));
    fs::path path = dir / "parent" / "root", sentinel = dir / "outside" / "sentinel";
    std::shared_ptr<RootDirectory> root;
    std::unique_ptr<LocalFileSystem> local;
    std::unique_ptr<VirtualFileSystem> vfs;
    struct stat original{};
    Fixture() {
        fs::remove_all(dir);
        fs::create_directories(path);
        fs::create_directory(dir / "outside");
        std::ofstream(sentinel) << "outside sentinel";
        require(lstat(sentinel.c_str(), &original) == 0, "sentinel stat");
        root = RootDirectory::open(path.string());
        local = std::make_unique<LocalFileSystem>("db.lbdb", root);
        vfs = std::make_unique<VirtualFileSystem>("db.lbdb", root);
    }
    ~Fixture() { fs::remove_all(dir); }
    void unchanged() {
        struct stat now{};
        require(lstat(sentinel.c_str(), &now) == 0 && now.st_dev == original.st_dev &&
            now.st_ino == original.st_ino && now.st_nlink == original.st_nlink &&
            bytes(sentinel) == "outside sentinel", "outside sentinel bytes/identity changed");
    }
    void owned(const std::string& name, const char* content = "owned") {
        auto f = local->openFile(name, FileOpenFlags(FileFlags::WRITE | FileFlags::CREATE_IF_NOT_EXISTS));
        f->writeFile(reinterpret_cast<const uint8_t*>(content), std::string(content).size(), 0);
    }
};
#endif
