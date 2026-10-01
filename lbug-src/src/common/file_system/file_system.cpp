#include "common/file_system/file_system.h"

#include "common/exception/io.h"
#include "common/string_utils.h"
#include "common/file_system/root_directory.h"
#include "common/file_system/local_file_system.h"
#include "storage/storage_utils.h"
#include <algorithm>
#include <atomic>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif
#include <format>

namespace lbug {
namespace common {

void FileSystem::overwriteFile(const std::string& /*from*/, const std::string& /*to*/) {
    UNREACHABLE_CODE;
}

void FileSystem::renameFile(const std::string& from, const std::string& to) {
    if (restrictedMode) {
        const auto companions = storage::StorageUtils::getCompanionFilePaths(dbPath);
        if (from != dbPath && std::find(companions.begin(), companions.end(), from) == companions.end()) {
            throw IOException("Restricted rename requires a session database or exact companion source.");
        }
        const bool replaceCompanion = std::find(companions.begin(), companions.end(), to) != companions.end();
        root->renameFile(from, to, replaceCompanion);
        return;
    }
    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    if (ec) {
        throw IOException(
            std::format("Error renaming file {} to {}. ErrorMessage: {}", from, to, ec.message()));
    }
}

void FileSystem::adoptCompanionFiles() {
    if (!root) { return; }
    for (const auto& name : storage::StorageUtils::getCompanionFilePaths(dbPath)) {
        root->adoptFileIfExists(name);
    }
}

std::unique_ptr<FileInfo> FileSystem::createPKValidatorSpillFile(const std::string& databasePath) {
    if (root) {
        if (databasePath != dbPath) { throw IOException("Restricted bulk temp database mismatch."); }
        const auto name = root->createSessionTempFile(dbPath);
        return openFile(name, FileOpenFlags(FileFlags::READ_ONLY | FileFlags::WRITE));
    }
    static std::atomic<uint64_t> counter{0};
    return openFile(storage::StorageUtils::getPKValidatorSpillFilePath(databasePath, counter.fetch_add(1)),
        FileOpenFlags(FileFlags::READ_ONLY | FileFlags::WRITE | FileFlags::CREATE_AND_TRUNCATE_IF_EXISTS));
}

void FileSystem::syncDirectoryForFile(const std::string& path) const {
    if (root) {
        RootDirectory::validateName(path);
        root->syncDirectory();
        return;
    }
#ifndef _WIN32
    if (!LocalFileSystem::isLocalPath(path)) { return; }
    auto parent = std::filesystem::path(path).parent_path();
    if (parent.empty()) { parent = "."; }
    const auto fd = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd < 0) { throw IOException("Cannot open parent directory for sync."); }
    const auto result = fsync(fd);
    close(fd);
    if (result != 0) { throw IOException("Cannot sync parent directory."); }
#else
    (void)path;
#endif
}

void FileSystem::copyFile(const std::string& /*from*/, const std::string& /*to*/) {
    UNREACHABLE_CODE;
}

void FileSystem::createDir(const std::string& /*dir*/) const {
    UNREACHABLE_CODE;
}

void FileSystem::removeFileIfExists(const std::string&, const main::ClientContext* /*context*/) {
    UNREACHABLE_CODE;
}

bool FileSystem::fileOrPathExists(const std::string& /*path*/, main::ClientContext* /*context*/) {
    UNREACHABLE_CODE;
}

std::string FileSystem::expandPath(main::ClientContext* /*context*/,
    const std::string& path) const {
    return path;
}

std::string FileSystem::joinPath(const std::string& base, const std::string& part) {
    return base + "/" + part;
}

std::string FileSystem::getFileExtension(const std::filesystem::path& path) {
    auto extension = path.extension();
    if (isCompressedFile(path)) {
        extension = path.stem().extension();
    }
    return extension.string();
}

bool FileSystem::isCompressedFile(const std::filesystem::path& path) {
    return isGZIPCompressed(path);
}

std::string FileSystem::getFileName(const std::filesystem::path& path) {
    return path.filename().string();
}

void FileSystem::writeFile(FileInfo& /*fileInfo*/, const uint8_t* /*buffer*/, uint64_t /*numBytes*/,
    uint64_t /*offset*/) const {
    UNREACHABLE_CODE;
}

void FileSystem::truncate(FileInfo& /*fileInfo*/, uint64_t /*size*/) const {
    UNREACHABLE_CODE;
}

void FileSystem::reset(FileInfo& fileInfo) {
    fileInfo.seek(0, SEEK_SET);
}

bool FileSystem::isGZIPCompressed(const std::filesystem::path& path) {
    auto extensionLowerCase = StringUtils::getLower(path.extension().string());
    return extensionLowerCase == ".gz" || extensionLowerCase == ".gzip";
}

} // namespace common
} // namespace lbug
