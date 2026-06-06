#include "ObjectStore.h"
#include "HashUtility.h"
#include "FileUtils.h"
#include <stdexcept>

// ─── Constructor ──────────────────────────────────────────────────────────────
ObjectStore::ObjectStore(const std::string& repoRoot)
    : objectsDir_(FileUtils::joinPath(repoRoot, "objects"))
{
    FileUtils::createDirs(objectsDir_);
}

// ─── Internal: resolve hash → full path (.minigit/objects/ab/cd…) ────────────
std::string ObjectStore::objectPath(const std::string& hash) const {
    if (hash.size() < 4) {
        throw std::runtime_error("Invalid hash (too short): " + hash);
    }
    std::string prefix = hash.substr(0, 2);
    std::string rest   = hash.substr(2);
    return FileUtils::joinPath(FileUtils::joinPath(objectsDir_, prefix), rest);
}

// ─── Store raw content ────────────────────────────────────────────────────────
std::string ObjectStore::storeObject(const std::string& content) {
    std::string hash = HashUtility::sha256(content);

    if (!objectExists(hash)) {
        std::string path = objectPath(hash);
        // createDirs handles the 2-char prefix directory
        if (!FileUtils::writeFile(path, content)) {
            throw std::runtime_error("ObjectStore: failed to write object " + hash);
        }
    }
    return hash;
}

// ─── Store a file from disk ───────────────────────────────────────────────────
std::string ObjectStore::storeFile(const std::string& filePath) {
    if (!FileUtils::isFile(filePath)) {
        throw std::runtime_error("ObjectStore: file not found: " + filePath);
    }
    std::string content = FileUtils::readFile(filePath);
    return storeObject(content);
}

// ─── Load object content ──────────────────────────────────────────────────────
std::string ObjectStore::loadObject(const std::string& hash) const {
    std::string path = objectPath(hash);
    if (!FileUtils::exists(path)) {
        throw std::runtime_error("ObjectStore: object not found: " + hash);
    }
    return FileUtils::readFile(path);
}

// ─── Existence check (stat only, no read) ────────────────────────────────────
bool ObjectStore::objectExists(const std::string& hash) const {
    return FileUtils::exists(objectPath(hash));
}

// ─── Restore object to destination path ──────────────────────────────────────
bool ObjectStore::restoreObject(const std::string& hash, const std::string& destPath) const {
    try {
        std::string content = loadObject(hash);
        return FileUtils::writeFile(destPath, content);
    } catch (const std::exception& e) {
        return false;
    }
}
