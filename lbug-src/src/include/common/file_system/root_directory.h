#pragma once

#include <memory>
#include <string>

#include "common/api.h"

namespace lbug::common {

// Existing absolute roots are canonicalized once (initial links are the caller's trust
// decision). Canonical ancestors are held no-follow and revalidated before each operation.
// Below the root only plain child names and single-link regular files are supported.
class LBUG_API RootDirectory final {
public:
    static std::shared_ptr<RootDirectory> open(const std::string& absoluteRoot);
    static void validateName(const std::string& name);
    ~RootDirectory();

    RootDirectory(const RootDirectory&) = delete;
    RootDirectory& operator=(const RootDirectory&) = delete;

    // Returns an owned descriptor. O_TRUNC is deliberately deferred to LocalFileSystem
    // until after identity validation and native lock acquisition.
#ifdef _WIN32
    // Owned native handle, never narrowed to a descriptor. Windows is read-only.
    void* openFile(const std::string& name, int flags);
#else
    int openFile(const std::string& name, int flags);
#endif
    bool probeRegularFile(const std::string& name);
private:
    friend class FileSystem;
    friend class LocalFileSystem;
    void adoptFileIfExists(const std::string& name);
    std::string createSessionTempFile(const std::string& databaseName);
    void renameFile(const std::string& from, const std::string& to, bool replaceCompanion);
    void removeFile(const std::string& name, bool exactCompanion);
    void syncDirectory();
    void prepareFileSync();

    RootDirectory();
    struct State;
    std::unique_ptr<State> state;
};

} // namespace lbug::common
