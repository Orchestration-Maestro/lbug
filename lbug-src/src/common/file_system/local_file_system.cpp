#include "common/file_system/local_file_system.h"

#include "common/assert.h"
#include "common/exception/io.h"
#include "common/string_utils.h"
#include "common/system_message.h"
#include "glob/glob.hpp"
#include "main/client_context.h"
#include "main/settings.h"
#include "storage/storage_utils.h"
#include <algorithm>

#if defined(_WIN32)
#include <sys/stat.h>

#include "common/windows_utils.h"
#include <fileapi.h>
#include <io.h>
#include <windows.h>
#else
#include "sys/stat.h"
#include <unistd.h>
#endif

#include <fcntl.h>

#include <cstring>

#include <format>

namespace lbug {
namespace common {
#if defined(MAESTRO_NATIVE_OPEN_TEST) && defined(_WIN32)
extern int (*maestroWindowsCall)(const char*, void*, void*);
static void windowsContentPoint(const char* operation, const void* handle) {
    if (maestroWindowsCall) maestroWindowsCall(operation, const_cast<void*>(handle), nullptr);
}
#endif
#if defined(MAESTRO_NATIVE_OPEN_TEST) && !defined(_WIN32)
extern int (*maestroRestrictedCall)(const char*, int, const char*, struct stat*);
static void nativeFileSyncPoint(const FileInfo& fileInfo) {
    if (maestroRestrictedCall) { maestroRestrictedCall("file-sync", -1, fileInfo.path.c_str(), nullptr); }
}
#endif

LocalFileInfo::~LocalFileInfo() {
#ifdef _WIN32
    if (handle != nullptr) {
        CloseHandle((HANDLE)handle);
    }
#else
    if (fd != -1) {
        close(fd);
    }
#endif
}

LocalFileSystem::LocalFileSystem(std::string name, std::shared_ptr<RootDirectory> root)
    : FileSystem(std::move(name)) {
    this->root = std::move(root);
    if (!this->root) { throw IOException("Restricted filesystem requires a root capability."); }
    RootDirectory::validateName(dbPath);
    restrictedMode = true;
}

void LocalFileSystem::requireUnrestricted(const char* operation) const {
    if (root) {
        throw IOException(std::string("Restricted filesystem ") + operation +
                          " is unsupported in E01a.");
    }
}

static void validateFileFlags(uint8_t flags) {
    const bool isRead = flags & FileFlags::READ_ONLY;
    const bool isWrite = flags & FileFlags::WRITE;
    UNUSED(isRead);
    UNUSED(isWrite);
    // Require either READ or WRITE (or both).
    DASSERT(isRead || isWrite);
    // CREATE flags require writing.
    DASSERT(isWrite || !(flags & FileFlags::CREATE_IF_NOT_EXISTS));
    DASSERT(isWrite || !(flags & FileFlags::CREATE_AND_TRUNCATE_IF_EXISTS));
    // CREATE_IF_NOT_EXISTS and CREATE_AND_TRUNCATE_IF_EXISTS flags cannot be combined.
    DASSERT(!(flags & FileFlags::CREATE_IF_NOT_EXISTS &&
              flags & FileFlags::CREATE_AND_TRUNCATE_IF_EXISTS));
}

std::unique_ptr<FileInfo> LocalFileSystem::openFile(const std::string& path, FileOpenFlags flags,
    main::ClientContext* context) {
    auto fullPath = root ? path : expandPath(context, path);
    if (root) { RootDirectory::validateName(path); }
    auto fileFlags = flags.flags;
    if (root) {
        const auto create = fileFlags & FileFlags::CREATE_IF_NOT_EXISTS;
        const auto truncate = fileFlags & FileFlags::CREATE_AND_TRUNCATE_IF_EXISTS;
        const auto allowed = FileFlags::READ_ONLY | FileFlags::WRITE | FileFlags::CREATE_IF_NOT_EXISTS |
                             FileFlags::CREATE_AND_TRUNCATE_IF_EXISTS | FileFlags::TEMPORARY;
        if ((fileFlags & ~allowed) || ((create || truncate) && !(fileFlags & FileFlags::WRITE)) ||
            (create && truncate) || !(fileFlags & (FileFlags::READ_ONLY | FileFlags::WRITE))) {
            throw IOException("Invalid restricted openFile flags.");
        }
    }
    validateFileFlags(fileFlags);

    int openFlags = 0;
    bool readMode = fileFlags & FileFlags::READ_ONLY;
    bool writeMode = fileFlags & FileFlags::WRITE;
    if (readMode && writeMode) {
        openFlags = O_RDWR;
    } else if (readMode) {
        openFlags = O_RDONLY;
    } else if (writeMode) {
        openFlags = O_WRONLY;
    } else {
        // LCOV_EXCL_START
        throw InternalException("READ, WRITE or both should be specified when opening a file.");
        // LCOV_EXCL_STOP
    }
    if (writeMode) {
        DASSERT(fileFlags & FileFlags::WRITE);
        if (fileFlags & FileFlags::CREATE_IF_NOT_EXISTS) {
            openFlags |= O_CREAT;
        } else if (fileFlags & FileFlags::CREATE_AND_TRUNCATE_IF_EXISTS) {
            openFlags |= O_CREAT | O_TRUNC;
        }
    }

#if defined(_WIN32)
    if (root && (writeMode || flags.lockType == FileLockType::WRITE_LOCK)) {
        throw IOException("Restricted writable openFile is unsupported on Windows (read-only only).");
    }
    auto dwDesiredAccess = 0ul;
    int dwCreationDisposition;
    if (fileFlags & FileFlags::CREATE_IF_NOT_EXISTS) {
        dwCreationDisposition = OPEN_ALWAYS;
    } else if (fileFlags & FileFlags::CREATE_AND_TRUNCATE_IF_EXISTS) {
        dwCreationDisposition = CREATE_ALWAYS;
    } else {
        dwCreationDisposition = OPEN_EXISTING;
    }
    auto dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    if (openFlags & (O_CREAT | O_WRONLY | O_RDWR)) {
        dwDesiredAccess |= GENERIC_WRITE;
    }
    // O_RDONLY is 0 in practice, so openFlags & (O_RDONLY | O_RDWR) doesn't work.
    if (!(openFlags & O_WRONLY)) {
        dwDesiredAccess |= GENERIC_READ;
    }
    if (openFlags & FileFlags::BINARY) {
        dwDesiredAccess |= _O_BINARY;
    }

    HANDLE handle = root ? root->openFile(path, openFlags) : CreateFileA(fullPath.c_str(), dwDesiredAccess, dwShareMode, nullptr,
        dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        throw IOException(std::format("Cannot open file. path: {} - Error {}: {}", fullPath,
            GetLastError(), std::system_category().message(GetLastError())));
    }
    auto owned = std::unique_ptr<void, decltype(&CloseHandle)>(handle, &CloseHandle);
    if (flags.lockType != FileLockType::NO_LOCK) {
        DWORD dwFlags = flags.lockType == FileLockType::READ_LOCK ?
                            LOCKFILE_FAIL_IMMEDIATELY :
                            LOCKFILE_FAIL_IMMEDIATELY | LOCKFILE_EXCLUSIVE_LOCK;
        OVERLAPPED overlapped = {};
        overlapped.Offset = 0;
        BOOL rc = LockFileEx(handle, dwFlags, 0 /*reserved*/, 1 /*numBytesLow*/, 0 /*numBytesHigh*/,
            &overlapped);
        if (!rc) {
            auto error = GetLastError();
            throw IOException("Could not set lock on file : " + fullPath +
                              " (Error: " + std::to_string(error) + ")\n" +
                              "See the docs: https://docs.ladybugdb.com/concurrency for more "
                              "information.");
        }
    }
    auto result = std::make_unique<LocalFileInfo>(fullPath, handle, this);
    owned.release();
    return result;
#else
    int fd = root ? root->openFile(path, openFlags) : open(fullPath.c_str(), openFlags, 0644);
    if (fd == -1) {
        throw IOException(std::format("Cannot open file {}: {}", fullPath, posixErrMessage()));
    }
    if (flags.lockType != FileLockType::NO_LOCK) {
        struct flock fl {};
        memset(&fl, 0, sizeof fl);
        fl.l_type = flags.lockType == FileLockType::READ_LOCK ? F_RDLCK : F_WRLCK;
        fl.l_whence = SEEK_SET;
        fl.l_start = 0;
        fl.l_len = 0;
        int rc = fcntl(fd, F_SETLK, &fl);
        if (rc == -1) {
            int original_errno = errno;
            if (original_errno == EAGAIN || original_errno == EACCES) {
                struct flock get_fl {};
                memset(&get_fl, 0, sizeof get_fl);
                get_fl.l_type = flags.lockType == FileLockType::READ_LOCK ? F_RDLCK : F_WRLCK;
                get_fl.l_whence = SEEK_SET;
                get_fl.l_start = 0;
                get_fl.l_len = 0;
                if (fcntl(fd, F_GETLK, &get_fl) != -1) {
                    if (get_fl.l_type != F_UNLCK) {
                        close(fd);
                        throw IOException(
                            "Could not set lock on file : " + fullPath + " (Lock is held by PID " +
                            std::to_string(get_fl.l_pid) + ")\n" +
                            "See the docs: https://docs.ladybugdb.com/concurrency for more "
                            "information.");
                    }
                }
            }
            close(fd);
            errno = original_errno;
            throw IOException("Could not set lock on file : " + fullPath +
                              " (Error: " + posixErrMessage() + ")\n" +
                              "See the docs: https://docs.ladybugdb.com/concurrency for more "
                              "information.");
        }
    }
    if (root && (openFlags & O_TRUNC) && ftruncate(fd, 0) != 0) {
        close(fd);
        throw IOException("Cannot truncate restricted file after validation and lock.");
    }
    return std::make_unique<LocalFileInfo>(fullPath, fd, this);
#endif
}

std::vector<std::string> LocalFileSystem::glob(main::ClientContext* context,
    const std::string& path) const {
    requireUnrestricted("glob");
    if (path.empty()) {
        return std::vector<std::string>();
    }
    std::vector<std::string> pathsToGlob;
    if (path[0] == '/' || (std::isalpha(path[0]) && path[1] == ':')) {
        // Note:
        // Unix absolute path starts with '/'
        // Windows absolute path starts with "[DiskID]://"
        pathsToGlob.push_back(path);
    } else if (path[0] == '~') {
        // Expands home directory
        auto homeDirectory =
            context->getCurrentSetting(main::HomeDirectorySetting::name).getValue<std::string>();
        pathsToGlob.push_back(homeDirectory + path.substr(1));
    } else {
        // Relative path to the file search path.
        auto globbedPaths = glob::glob(path);
        if (!globbedPaths.empty()) {
            pathsToGlob.push_back(path);
        } else {
            auto fileSearchPath = context->getCurrentSetting(main::FileSearchPathSetting::name)
                                      .getValue<std::string>();
            if (fileSearchPath != "") {
                auto searchPaths = StringUtils::split(fileSearchPath, ",");
                for (auto& searchPath : searchPaths) {
                    pathsToGlob.push_back(std::format("{}/{}", searchPath, path));
                }
            }
        }
    }
    std::vector<std::string> result;
    for (auto& pathToGlob : pathsToGlob) {
        for (auto& resultPath : glob::glob(pathToGlob)) {
            result.emplace_back(resultPath.string());
        }
    }
    return result;
}

void LocalFileSystem::renameFile(const std::string& from, const std::string& to) {
    FileSystem::renameFile(from, to);
}

void LocalFileSystem::overwriteFile(const std::string& from, const std::string& to) {
    requireUnrestricted("overwriteFile");
    if (!fileOrPathExists(from) || !fileOrPathExists(to)) {
        return;
    }
    std::error_code errorCode;
    if (!std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing,
            errorCode)) {
        // LCOV_EXCL_START
        throw IOException(std::format("Error copying file {} to {}.  ErrorMessage: {}", from, to,
            errorCode.message()));
        // LCOV_EXCL_STOP
    }
}

void LocalFileSystem::copyFile(const std::string& from, const std::string& to) {
    requireUnrestricted("copyFile");
    if (!fileOrPathExists(from)) {
        return;
    }
    std::error_code errorCode;
    if (!std::filesystem::copy_file(from, to, std::filesystem::copy_options::none, errorCode)) {
        // LCOV_EXCL_START
        throw IOException(std::format("Error copying file {} to {}.  ErrorMessage: {}", from, to,
            errorCode.message()));
        // LCOV_EXCL_STOP
    }
}

void LocalFileSystem::createDir(const std::string& dir) const {
    requireUnrestricted("createDir");
    try {
        auto directoryToCreate = dir;
        if (directoryToCreate.ends_with('/')
#if defined(_WIN32)
            || directoryToCreate.ends_with('\\')
#endif
        ) {
            // This is a known issue with std::filesystem::create_directories. (link:
            // https://github.com/llvm/llvm-project/issues/60634). We have to manually remove the
            // last '/' if the path ends with '/'. (Added the second one for windows)
            directoryToCreate = directoryToCreate.substr(0, directoryToCreate.size() - 1);
        }
        std::error_code errCode;
        std::filesystem::create_directories(directoryToCreate, errCode);
        if (errCode) {
            // LCOV_EXCL_START
            throw IOException(std::format("Failed to create directory: {}, error message: {}.", dir,
                errCode.message()));
            // LCOV_EXCL_STOP
        }
    } catch (std::exception& e) {
        // LCOV_EXCL_START
        throw IOException(std::format("Failed to create directory {} due to: {}", dir, e.what()));
        // LCOV_EXCL_STOP
    }
}

static bool isAllowedDeletionPath(const std::string& path, const std::string& dbPath) {
    const auto p = std::filesystem::path(path);
    const auto dbPathP = std::filesystem::path(dbPath);
    const auto dbDir = dbPathP.parent_path();
    // For absolute paths, only allow deletion in the same directory as the database file.
    if (p.is_absolute() && dbPathP.is_absolute() && p.parent_path() != dbDir) {
        return false;
    }

    const auto fileName = p.filename().string();
    const auto extension = p.extension().string();
    const auto stemWithoutExt = p.stem().string();

    const auto dbBase = dbPathP.stem().string();
    const auto dbExt = dbPathP.extension().string();
    const auto dbFileName = dbBase + dbExt;

    // Main DB sidecars: db.lbdb.{wal|shadow|tmp|lock}
    if (extension == ".wal" || extension == ".shadow" || extension == ".tmp" ||
        extension == ".lock" || extension == ".checkpoint") {
        if (stemWithoutExt == dbFileName) {
            return true;
        }
        // Graph/copy sidecars can use either:
        // - db.<graph>.lbdb.{wal|shadow|tmp|lock}
        // - db.lbdb.<graph-or-tag>.{wal|shadow|tmp|lock}
        return (stemWithoutExt.starts_with(dbBase + ".") && stemWithoutExt.ends_with(dbExt)) ||
               stemWithoutExt.starts_with(dbFileName + ".");
    }

    // Graph DB file: db.<graph>.lbdb
    if (extension == dbExt) {
        return fileName.starts_with(dbBase + ".") && fileName != dbFileName;
    }

    return false;
}

static bool isExtensionFile(const main::ClientContext* context, const std::string& path) {
    if (context == nullptr) {
        return false;
    }
    auto extensionDir = context->getExtensionDir();
    std::filesystem::path rel = std::filesystem::relative(path, extensionDir);
    for (const auto& part : rel) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

void LocalFileSystem::removeFileIfExists(const std::string& path,
    const main::ClientContext* context) {
    if (root) {
        const auto companions = storage::StorageUtils::getCompanionFilePaths(dbPath);
        const bool exactCompanion = std::find(companions.begin(), companions.end(), path) != companions.end();
        root->removeFile(path, exactCompanion);
        return;
    }
    if (!isAllowedDeletionPath(path, dbPath) && !isExtensionFile(context, path)) {
        throw IOException(std::format(
            "Error: Path {} is not within the allowed list of files to be removed.", path));
    }
    if (!fileOrPathExists(path)) {
        return;
    }
    std::error_code errCode;
    bool success = false;
    if (std::filesystem::is_directory(path)) {
        success = std::filesystem::remove_all(path, errCode);
    } else {
        success = std::filesystem::remove(path, errCode);
    }
    if (!success) {
        // LCOV_EXCL_START
        throw IOException(std::format("Error removing directory or file {}.  Error Message: {}",
            path, errCode.message()));
        // LCOV_EXCL_STOP
    }
}

bool LocalFileSystem::fileOrPathExists(const std::string& path, main::ClientContext* /*context*/) {
    if (root) { return root->probeRegularFile(path); }
    return std::filesystem::exists(path);
}

#ifndef _WIN32
bool LocalFileSystem::fileExists(const std::string& filename) {
    if (!filename.empty()) {
        if (access(filename.c_str(), 0) == 0) {
            struct stat status = {};
            stat(filename.c_str(), &status);
            if (S_ISREG(status.st_mode)) {
                return true;
            }
        }
    }
    // if any condition fails
    return false;
}
#else
bool LocalFileSystem::fileExists(const std::string& filename) {
    auto unicode_path = WindowsUtils::utf8ToUnicode(filename.c_str());
    const wchar_t* wpath = unicode_path.c_str();
    if (_waccess(wpath, 0) == 0) {
        struct _stati64 status = {};
        _wstati64(wpath, &status);
        if (status.st_mode & _S_IFREG) {
            return true;
        }
    }
    return false;
}
#endif

std::string LocalFileSystem::expandPath(main::ClientContext* context,
    const std::string& path) const {
    requireUnrestricted("expandPath");
    auto fullPath = path;
    if (path.starts_with('~')) {
        fullPath =
            context->getCurrentSetting(main::HomeDirectorySetting::name).getValue<std::string>() +
            fullPath.substr(1);
    }
    return fullPath;
}

bool LocalFileSystem::isLocalPath(const std::string& path) {
    return path.rfind("s3://", 0) != 0 && path.rfind("gs://", 0) != 0 &&
           path.rfind("gcs://", 0) != 0 && path.rfind("http://", 0) != 0 &&
           path.rfind("https://", 0) != 0 && path.rfind("az://", 0) != 0 &&
           path.rfind("abfss://", 0) != 0 && path.rfind("xet://", 0) != 0;
}

void LocalFileSystem::readFromFile(FileInfo& fileInfo, void* buffer, uint64_t numBytes,
    uint64_t position) const {
    auto localFileInfo = fileInfo.constPtrCast<LocalFileInfo>();
    DASSERT(localFileInfo->getFileSize() >= position + numBytes);
    auto outputBuffer = static_cast<uint8_t*>(buffer);
    uint64_t remainingNumBytesToRead = numBytes;
    uint64_t bufferOffset = 0;
    // Keep reads below common OS syscall transfer limits.
    constexpr uint64_t maxBytesToReadAtOnce = 1ull << 30;
    while (remainingNumBytesToRead > 0) {
        const auto numBytesToRead = (std::min)(remainingNumBytesToRead, maxBytesToReadAtOnce);
#if defined(_WIN32)
        DWORD numBytesRead = 0;
        OVERLAPPED overlapped = {};
        overlapped.Offset = position & 0xffffffff;
        overlapped.OffsetHigh = position >> 32;
#ifdef MAESTRO_NATIVE_OPEN_TEST
        windowsContentPoint("read", localFileInfo->handle);
#endif
        if (!ReadFile((HANDLE)localFileInfo->handle, outputBuffer + bufferOffset, numBytesToRead,
                &numBytesRead, &overlapped)) {
            auto error = GetLastError();
            throw IOException(
                std::format("Cannot read from file: {} handle: {} "
                            "numBytesRead: {} numBytesToRead: {} position: {}. Error {}: {}",
                    fileInfo.path, (intptr_t)localFileInfo->handle, numBytesRead, numBytesToRead,
                    position, error, std::system_category().message(error)));
        }
        if (numBytesRead != numBytesToRead && fileInfo.getFileSize() != position + numBytesRead) {
            throw IOException(std::format("Cannot read from file: {} handle: {} "
                                          "numBytesRead: {} numBytesToRead: {} position: {}",
                fileInfo.path, (intptr_t)localFileInfo->handle, numBytesRead, numBytesToRead,
                position));
        }
#else
        auto numBytesRead =
            pread(localFileInfo->fd, outputBuffer + bufferOffset, numBytesToRead, position);
        if (numBytesRead < 0) {
            // LCOV_EXCL_START
            throw IOException(std::format("Cannot read from file: {} fileDescriptor: {} "
                                          "numBytesRead: {} numBytesToRead: {} position: {}",
                fileInfo.path, localFileInfo->fd, numBytesRead, numBytesToRead, position));
            // LCOV_EXCL_STOP
        }
        if (static_cast<uint64_t>(numBytesRead) != numBytesToRead &&
            localFileInfo->getFileSize() != position + numBytesRead) {
            // LCOV_EXCL_START
            throw IOException(std::format("Cannot read from file: {} fileDescriptor: {} "
                                          "numBytesRead: {} numBytesToRead: {} position: {}",
                fileInfo.path, localFileInfo->fd, numBytesRead, numBytesToRead, position));
            // LCOV_EXCL_STOP
        }
#endif
        if (numBytesRead == 0) {
            break;
        }
        remainingNumBytesToRead -= numBytesRead;
        position += numBytesRead;
        bufferOffset += numBytesRead;
    }
}

int64_t LocalFileSystem::readFile(FileInfo& fileInfo, void* buf, size_t nbyte) const {
    auto localFileInfo = fileInfo.constPtrCast<LocalFileInfo>();
#if defined(_WIN32)
    DWORD numBytesRead = 0;
#ifdef MAESTRO_NATIVE_OPEN_TEST
    windowsContentPoint("read", localFileInfo->handle);
#endif
    const auto bytes = static_cast<DWORD>((std::min)(nbyte, size_t{1} << 30));
    if (!ReadFile((HANDLE)localFileInfo->handle, buf, bytes, &numBytesRead, nullptr)) {
        const auto error = GetLastError();
        throw IOException("Cannot read file: " + fileInfo.path + " Error: " + std::to_string(error));
    }
    return numBytesRead;
#else
    return read(localFileInfo->fd, buf, nbyte);
#endif
}

void LocalFileSystem::writeFile(FileInfo& fileInfo, const uint8_t* buffer, uint64_t numBytes,
    uint64_t offset) const {
#ifdef _WIN32
    if (root) throw IOException("Restricted writable handle I/O is unsupported on Windows.");
#endif
    auto localFileInfo = fileInfo.constPtrCast<LocalFileInfo>();
    uint64_t remainingNumBytesToWrite = numBytes;
    uint64_t bufferOffset = 0;
    // Split large writes to 1GB at a time
    uint64_t maxBytesToWriteAtOnce = 1ull << 30; // 1ull << 30 = 1G
    while (remainingNumBytesToWrite > 0) {
        uint64_t numBytesToWrite = (std::min)(remainingNumBytesToWrite, maxBytesToWriteAtOnce);

#if defined(_WIN32)
        DWORD numBytesWritten;
        OVERLAPPED overlapped = {};
        overlapped.Offset = offset & 0xffffffff;
        overlapped.OffsetHigh = offset >> 32;
        if (!WriteFile((HANDLE)localFileInfo->handle, buffer + bufferOffset, numBytesToWrite,
                &numBytesWritten, &overlapped)) {
            auto error = GetLastError();
            throw IOException(
                std::format("Cannot write to file. path: {} handle: {} offsetToWrite: {} "
                            "numBytesToWrite: {} numBytesWritten: {}. Error {}: {}.",
                    fileInfo.path, (intptr_t)localFileInfo->handle, offset, numBytesToWrite,
                    numBytesWritten, error, std::system_category().message(error)));
        }
#else
        auto numBytesWritten =
            pwrite(localFileInfo->fd, buffer + bufferOffset, numBytesToWrite, offset);
        if (numBytesWritten != static_cast<int64_t>(numBytesToWrite)) {
            // LCOV_EXCL_START
            throw IOException(
                std::format("Cannot write to file. path: {} fileDescriptor: {} offsetToWrite: {} "
                            "numBytesToWrite: {} numBytesWritten: {}. Error: {}",
                    fileInfo.path, localFileInfo->fd, offset, numBytesToWrite, numBytesWritten,
                    posixErrMessage()));
            // LCOV_EXCL_STOP
        }
#endif
        remainingNumBytesToWrite -= numBytesWritten;
        offset += numBytesWritten;
        bufferOffset += numBytesWritten;
    }
}

void LocalFileSystem::syncFile(const FileInfo& fileInfo) const {
    // Establish fresh directory durability and reject poison even for already-held handles.
    if (root) { root->prepareFileSync(); }
    auto localFileInfo = fileInfo.constPtrCast<LocalFileInfo>();
#if defined(_WIN32)
    // Note that `FlushFileBuffers` returns 0 when fails, while `fsync` returns 0 when succeeds.
    if (FlushFileBuffers((HANDLE)localFileInfo->handle) == 0) {
        auto error = GetLastError();
        throw IOException(std::format("Failed to sync file {}. Error {}: {}", fileInfo.path, error,
            std::system_category().message(error)));
    }
#else
#if HAS_FULLFSYNC and defined(__APPLE__)
    // Try F_FULLFSYNC first on macOS/iOS, which is required to guarantee durability past power
    // failures.
    if (fcntl(localFileInfo->fd, F_FULLFSYNC) == 0) {
#ifdef MAESTRO_NATIVE_OPEN_TEST
        if (root) { nativeFileSyncPoint(fileInfo); }
#endif
        if (root) { root->prepareFileSync(); }
        return;
    }
    if (errno != ENOTSUP && errno != EINVAL) {
        // LCOV_EXCL_START
        if (errno == EIO) {
            throw IOException("Fatal error: fsync failed!");
        }
        throw IOException(
            std::format("Failed to sync file {}: {}", fileInfo.path, posixErrMessage()));
        // LCOV_EXCL_STOP
    }
#endif
    bool syncSuccess = false;
#if HAS_FDATASYNC
    syncSuccess = fdatasync(localFileInfo->fd) == 0; // Only sync file data + essential metadata.
#else
    syncSuccess = fsync(localFileInfo->fd) == 0; // Sync file data + all metadata.
#endif
    if (!syncSuccess) {
        throw IOException(std::format("Failed to sync file {}.", fileInfo.path));
    }
#ifdef MAESTRO_NATIVE_OPEN_TEST
    if (root) { nativeFileSyncPoint(fileInfo); }
#endif
#endif
    if (root) { root->prepareFileSync(); }
}

int64_t LocalFileSystem::seek(FileInfo& fileInfo, uint64_t offset, int whence) const {
    auto localFileInfo = fileInfo.constPtrCast<LocalFileInfo>();
#if defined(_WIN32)
    LARGE_INTEGER result{};
    LARGE_INTEGER offset_{};
    offset_.QuadPart = offset;
#ifdef MAESTRO_NATIVE_OPEN_TEST
    windowsContentPoint("seek", localFileInfo->handle);
#endif
    if (!SetFilePointerEx((HANDLE)localFileInfo->handle, offset_, &result, whence)) {
        const auto error = GetLastError();
        throw IOException("Cannot set file pointer: " + fileInfo.path + " Error: " + std::to_string(error));
    }
    return result.QuadPart;
#else
    return lseek(localFileInfo->fd, offset, whence);
#endif
}

void LocalFileSystem::truncate(FileInfo& fileInfo, uint64_t size) const {
#ifdef _WIN32
    if (root) throw IOException("Restricted writable truncate is unsupported on Windows.");
#endif
    auto localFileInfo = fileInfo.constPtrCast<LocalFileInfo>();
#if defined(_WIN32)
    auto offsetHigh = (LONG)(size >> 32);
    LONG* offsetHighPtr = NULL;
    if (offsetHigh > 0)
        offsetHighPtr = &offsetHigh;
    if (SetFilePointer((HANDLE)localFileInfo->handle, size & 0xffffffff, offsetHighPtr,
            FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        auto error = GetLastError();
        throw IOException(std::format("Cannot set file pointer for file: {} handle: {} "
                                      "new position: {}. Error {}: {}",
            fileInfo.path, (intptr_t)localFileInfo->handle, size, error,
            std::system_category().message(error)));
    }
    if (!SetEndOfFile((HANDLE)localFileInfo->handle)) {
        auto error = GetLastError();
        throw IOException(std::format("Cannot truncate file: {} handle: {} "
                                      "size: {}. Error {}: {}",
            fileInfo.path, (intptr_t)localFileInfo->handle, size, error,
            std::system_category().message(error)));
    }
#else
    if (ftruncate(localFileInfo->fd, size) < 0) {
        // LCOV_EXCL_START
        throw IOException(
            std::format("Failed to truncate file {}: {}", fileInfo.path, posixErrMessage()));
        // LCOV_EXCL_STOP
    }
#endif
}

uint64_t LocalFileSystem::getFileSize(const FileInfo& fileInfo) const {
    auto localFileInfo = fileInfo.constPtrCast<LocalFileInfo>();
#ifdef _WIN32
    LARGE_INTEGER size;
    if (!GetFileSizeEx((HANDLE)localFileInfo->handle, &size)) {
        auto error = GetLastError();
        throw IOException(std::format("Cannot read size of file. path: {} - Error {}: {}",
            fileInfo.path, error, systemErrMessage(error)));
    }
    return size.QuadPart;
#else
    struct stat s {};
    if (fstat(localFileInfo->fd, &s) == -1) {
        throw IOException(std::format("Cannot read size of file. path: {} - Error {}: {}",
            fileInfo.path, errno, posixErrMessage()));
    }
    DASSERT(s.st_size >= 0);
    return s.st_size;
#endif
}

} // namespace common
} // namespace lbug
