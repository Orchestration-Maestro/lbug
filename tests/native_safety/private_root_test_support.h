#pragma once
#include <filesystem>
#include <stdexcept>

inline void makePrivateTestRoot(const std::filesystem::path& path) {
    std::filesystem::permissions(path, std::filesystem::perms::owner_all);
    if ((std::filesystem::status(path).permissions() & std::filesystem::perms::all) !=
        std::filesystem::perms::owner_all) {
        throw std::runtime_error("cannot set private test root mode");
    }
}
