#include "common/file_system/root_directory.h"

#include "common/exception/io.h"
#include "storage/storage_utils.h"
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/syscall.h>
#include <linux/fs.h>
#elif defined(__APPLE__)
#include <stdio.h>
#endif
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
void RootDirectory::adoptFileIfExists(const std::string&) {
    throw IOException("Restricted adoption is unsupported on Windows (E02).");
}
void RootDirectory::renameFile(const std::string&, const std::string&, bool) {
    throw IOException("Restricted renameFile is unsupported on Windows (E02).");
}
std::string RootDirectory::createSessionTempFile(const std::string&) {
    throw IOException("Restricted bulk temp is unsupported on Windows (E02).");
}
void RootDirectory::removeFile(const std::string&, bool) {
    throw IOException("Restricted removeFileIfExists is unsupported on Windows (E02).");
}
void RootDirectory::syncDirectory() {
    throw IOException("Restricted directory sync is unsupported on Windows (E02).");
}
void RootDirectory::prepareFileSync() {
    throw IOException("Restricted file sync is unsupported on Windows (E02).");
}
#else
#ifdef MAESTRO_NATIVE_OPEN_TEST
// Only the native harness sets this callback; archive reuse leaves it inert.
void (*maestroBeforeRestrictedOpen)(int directory, const char* name) = nullptr;
// Deterministic native syscall faults/crash points; never compiled by ordinary source builds.
int (*maestroRestrictedCall)(const char* operation, int directory, const char* name, struct stat* info) = nullptr;
#endif
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
    std::unordered_set<std::string> ownedFiles;
    std::unordered_set<std::string> registeredTemps;
    uint64_t nextTemp = 0;
    enum class DirectoryState { UNSYNCED, SYNCED, POISONED };
    DirectoryState directoryState = DirectoryState::UNSYNCED;

    ~State() {
        for (auto fd : descriptors) { close(fd); }
    }
    void requireUsable() const {
        if (directoryState == DirectoryState::POISONED) {
            throw IOException("Restricted root durability is poisoned; a fresh capability is required.");
        }
    }
    void validateAncestors() const {
        requireUsable();
        for (size_t i = 1; i < descriptors.size(); ++i) {
            struct stat info{};
            if (fstatat(descriptors[i - 1], names[i].c_str(), &info, AT_SYMLINK_NOFOLLOW) != 0 ||
                !S_ISDIR(info.st_mode) || !sameIdentity(info, identities[i])) {
                throw IOException("Restricted filesystem ancestor identity changed.");
            }
        }
    }
    void syncDirectory() {
        try {
            validateAncestors();
            int injectedError = 0;
#ifdef MAESTRO_NATIVE_OPEN_TEST
            if (maestroRestrictedCall) {
                injectedError = maestroRestrictedCall("directory-sync", descriptors.back(), nullptr, nullptr);
            }
#endif
            if (injectedError || fsync(descriptors.back()) != 0) {
                throw IOException("Restricted directory durability sync failed.");
            }
            validateAncestors();
            directoryState = DirectoryState::SYNCED;
        } catch (...) {
            directoryState = DirectoryState::POISONED;
            throw;
        }
    }
    void requireOwned(const std::string& name) const {
        if (!ownedFiles.contains(name)) {
            throw IOException("Restricted filesystem refuses an unowned file.");
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
#ifdef MAESTRO_NATIVE_OPEN_TEST
    if (maestroBeforeRestrictedOpen) {
        maestroBeforeRestrictedOpen(state->descriptors.back(), name.c_str());
    }
#endif
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
        if (!existed && (flags & O_CREAT)) {
            state->ownedFiles.insert(name);
            state->syncDirectory();
        }
        return fd;
    } catch (...) {
        close(fd);
        throw;
    }
}
void RootDirectory::adoptFileIfExists(const std::string& name) {
    std::lock_guard lock(state->mutex);
    state->validateAncestors();
    struct stat info{};
    if (!state->probe(name, info)) { return; }
#ifdef MAESTRO_NATIVE_OPEN_TEST
    if (maestroRestrictedCall) { maestroRestrictedCall("adopt", state->descriptors.back(), name.c_str(), &info); }
#endif
    if (info.st_uid != state->identities.back().st_uid) {
        throw IOException("Restricted companion owner mismatch.");
    }
    state->observedFiles.insert_or_assign(name, info);
    state->ownedFiles.insert(name);
}

void RootDirectory::renameFile(const std::string& from, const std::string& to,
    bool replaceCompanion) {
    validateName(to);
    std::lock_guard lock(state->mutex);
    state->validateAncestors();
    state->requireOwned(from);
    struct stat source{}, destination{};
    if (!state->probe(from, source)) { throw IOException("Restricted rename source missing."); }
    const bool exists = state->probe(to, destination);
    int result;
    if (exists) {
        if (!replaceCompanion) { throw IOException("Restricted publication destination exists."); }
        state->requireOwned(to);
        result = renameat(state->descriptors.back(), from.c_str(),
            state->descriptors.back(), to.c_str());
    } else {
        int injectedError = 0;
#ifdef MAESTRO_NATIVE_OPEN_TEST
        if (maestroRestrictedCall) {
            injectedError = maestroRestrictedCall("rename-no-replace", state->descriptors.back(), to.c_str(), nullptr);
        }
#endif
        if (injectedError) { result = -1; }
        else {
#if defined(__linux__)
        result = syscall(SYS_renameat2, state->descriptors.back(), from.c_str(),
            state->descriptors.back(), to.c_str(), RENAME_NOREPLACE);
#elif defined(__APPLE__)
        result = renameatx_np(state->descriptors.back(), from.c_str(),
            state->descriptors.back(), to.c_str(), RENAME_EXCL);
#else
        throw IOException("Restricted atomic no-replace rename is unsupported.");
#endif
        }
    }
    if (result != 0) {
        throw IOException("Restricted atomic rename failed or no-replace is unsupported.");
    }
    state->observedFiles.erase(from);
    state->ownedFiles.erase(from);
    state->observedFiles.insert_or_assign(to, source);
    state->ownedFiles.insert(to);
#ifdef MAESTRO_NATIVE_OPEN_TEST
    if (maestroRestrictedCall) { maestroRestrictedCall("rename", state->descriptors.back(), to.c_str(), nullptr); }
#endif
    state->syncDirectory();
}

void RootDirectory::removeFile(const std::string& name, bool exactCompanion) {
    std::lock_guard lock(state->mutex);
    state->validateAncestors();
    if (!exactCompanion && !state->registeredTemps.contains(name)) {
        throw IOException("Restricted removeFileIfExists requires an exact session companion or registered temp.");
    }
    struct stat info{};
    if (!state->probe(name, info)) { return; }
    state->requireOwned(name);
    int injectedError = 0;
#ifdef MAESTRO_NATIVE_OPEN_TEST
    if (maestroRestrictedCall) {
        injectedError = maestroRestrictedCall("unlink", state->descriptors.back(), name.c_str(), nullptr);
    }
#endif
    if (injectedError || unlinkat(state->descriptors.back(), name.c_str(), 0) != 0) {
        throw IOException("Restricted unlink failed.");
    }
    state->observedFiles.erase(name);
    state->ownedFiles.erase(name);
    state->registeredTemps.erase(name);
    state->syncDirectory();
}

std::string RootDirectory::createSessionTempFile(const std::string& databaseName) {
    std::lock_guard lock(state->mutex);
    state->validateAncestors();
    // A crashed session's generated temps are never adopted. Bound collision work per allocation.
    for (size_t attempt = 0; attempt < 16; ++attempt) {
        const auto name = storage::StorageUtils::getPKValidatorSpillFilePath(databaseName, state->nextTemp++);
        const auto fd = openat(state->descriptors.back(), name.c_str(),
            O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK, 0600);
        if (fd < 0) {
            if (errno == EEXIST) { continue; }
            throw IOException("Cannot create restricted bulk temp.");
        }
#ifdef MAESTRO_NATIVE_OPEN_TEST
        if (maestroRestrictedCall) { maestroRestrictedCall("temp-opened", state->descriptors.back(), name.c_str(), nullptr); }
#endif
        struct stat opened{}, current{};
        const bool valid = fstat(fd, &opened) == 0 &&
            fstatat(state->descriptors.back(), name.c_str(), &current, AT_SYMLINK_NOFOLLOW) == 0 &&
            sameIdentity(opened, current);
        close(fd);
        if (!valid) { throw IOException("Restricted bulk temp identity changed during creation."); }
        requireRegular(opened);
        requireRegular(current);
        state->observedFiles.insert_or_assign(name, opened);
        state->ownedFiles.insert(name);
        state->registeredTemps.insert(name);
        state->syncDirectory();
        return name;
    }
    throw IOException("Restricted bulk temp collision bound exhausted.");
}

void RootDirectory::syncDirectory() {
    std::lock_guard lock(state->mutex);
    state->syncDirectory();
}

void RootDirectory::prepareFileSync() {
    std::lock_guard lock(state->mutex);
    state->requireUsable();
    if (state->directoryState == State::DirectoryState::UNSYNCED) {
        state->syncDirectory();
    }
}

#endif

} // namespace lbug::common
