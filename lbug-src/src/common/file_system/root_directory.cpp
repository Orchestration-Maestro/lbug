#include "common/file_system/root_directory.h"

#include "common/exception/io.h"
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <vector>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace lbug::common {

void RootDirectory::validateName(const std::string& name) {
    if (name.empty() || name == "." || name == ".." ||
        name.find_first_of("/\\") != std::string::npos || name.find('\0') != std::string::npos) {
        throw IOException("Restricted filesystem requires a plain child name.");
    }
}

#ifdef _WIN32
struct RootDirectory::State {};
RootDirectory::RootDirectory() = default;
RootDirectory::~RootDirectory() = default;
std::shared_ptr<RootDirectory> RootDirectory::open(const std::string&) {
    throw IOException("Restricted root capability is unsupported on Windows (E02).");
}
int RootDirectory::openFile(const std::string& name, int) {
    validateName(name);
    throw IOException("Restricted openFile is unsupported on Windows (E02).");
}
bool RootDirectory::probeRegularFile(const std::string& name) {
    validateName(name);
    throw IOException("Restricted regular-file probe is unsupported on Windows (E02).");
}
#else
namespace {
bool sameIdentity(const struct stat& a, const struct stat& b) {
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino;
}
void requireRegular(const struct stat& info) {
    if (!S_ISREG(info.st_mode) || info.st_nlink != 1) {
        throw IOException("Restricted filesystem refuses links, aliases and non-regular files.");
    }
}
}

struct RootDirectory::State {
    std::mutex mutex;
    std::vector<int> descriptors;
    std::vector<std::string> names;
    std::vector<struct stat> identities;
    std::unordered_map<std::string, struct stat> observedFiles;

    ~State() {
        for (auto fd : descriptors) { close(fd); }
    }
    void validateAncestors() const {
        for (size_t i = 1; i < descriptors.size(); ++i) {
            struct stat info{};
            if (fstatat(descriptors[i - 1], names[i].c_str(), &info, AT_SYMLINK_NOFOLLOW) != 0 ||
                !S_ISDIR(info.st_mode) || !sameIdentity(info, identities[i])) {
                throw IOException("Restricted filesystem ancestor identity changed.");
            }
        }
    }
    bool probe(const std::string& name, struct stat& info) const {
        if (fstatat(descriptors.back(), name.c_str(), &info, AT_SYMLINK_NOFOLLOW) != 0) {
            if (errno == ENOENT && !observedFiles.contains(name)) { return false; }
            throw IOException("Restricted filesystem probe failed or file identity changed.");
        }
        requireRegular(info);
        auto remembered = observedFiles.find(name);
        if (remembered != observedFiles.end() && !sameIdentity(remembered->second, info)) {
            throw IOException("Restricted filesystem file identity changed.");
        }
        return true;
    }
};

RootDirectory::RootDirectory() : state{std::make_unique<State>()} {}
RootDirectory::~RootDirectory() = default;

std::shared_ptr<RootDirectory> RootDirectory::open(const std::string& absoluteRoot) {
    if (absoluteRoot.find('\0') != std::string::npos ||
        !std::filesystem::path(absoluteRoot).is_absolute()) {
        throw IOException("RootDirectory::open requires an existing absolute root.");
    }
    std::error_code error;
    auto canonical = std::filesystem::canonical(absoluteRoot, error);
    if (error) { throw IOException("Cannot canonicalize restricted root."); }
    auto root = std::shared_ptr<RootDirectory>(new RootDirectory());
    auto& state = *root->state;
    auto hold = [&](int fd, const std::string& name) {
        if (fd < 0) { throw IOException("Cannot hold restricted root ancestor."); }
        struct stat info{};
        if (fstat(fd, &info) != 0 || !S_ISDIR(info.st_mode)) {
            close(fd);
            throw IOException("Restricted root ancestor is not a directory.");
        }
        // Record the descriptor first so construction failures also release it.
        state.descriptors.push_back(fd);
        state.names.push_back(name);
        state.identities.push_back(info);
    };
    hold(::open("/", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC), "");
    for (const auto& part : canonical.relative_path()) {
        auto name = part.string();
        validateName(name);
        hold(openat(state.descriptors.back(), name.c_str(),
            O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC), name);
    }
    state.validateAncestors();
    return root;
}

bool RootDirectory::probeRegularFile(const std::string& name) {
    validateName(name);
    std::lock_guard lock(state->mutex);
    state->validateAncestors();
    struct stat info{};
    if (!state->probe(name, info)) { return false; }
    state->observedFiles.insert_or_assign(name, info);
    return true;
}

int RootDirectory::openFile(const std::string& name, int flags) {
    validateName(name);
    std::lock_guard lock(state->mutex);
    state->validateAncestors();
    struct stat before{};
    const auto existed = state->probe(name, before);
    // Never truncate during open: an opened inode must be checked before content access.
    auto safeFlags = (flags & ~O_TRUNC) | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK;
    if (!existed && (flags & O_CREAT)) { safeFlags |= O_EXCL; }
    const auto fd = openat(state->descriptors.back(), name.c_str(), safeFlags, 0600);
    if (fd < 0) { throw IOException("Cannot open restricted regular file."); }
    try {
        struct stat opened{}, current{};
        if (fstat(fd, &opened) != 0) { throw IOException("Restricted file identity unavailable."); }
        requireRegular(opened);
        if ((existed && !sameIdentity(before, opened)) ||
            fstatat(state->descriptors.back(), name.c_str(), &current, AT_SYMLINK_NOFOLLOW) != 0 ||
            !sameIdentity(opened, current)) {
            throw IOException("Restricted filesystem file identity changed during open.");
        }
        requireRegular(current);
        state->validateAncestors();
        state->observedFiles.insert_or_assign(name, opened);
        return fd;
    } catch (...) {
        close(fd);
        throw;
    }
}
#endif

} // namespace lbug::common
