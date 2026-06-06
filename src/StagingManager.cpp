#include "StagingManager.h"
#include "FileUtils.h"
#include <sstream>
#include <stdexcept>

// ─── Constructor ──────────────────────────────────────────────────────────────
StagingManager::StagingManager(const std::string& repoRoot) {
    std::string stagingDir = FileUtils::joinPath(repoRoot, "staging");
    FileUtils::createDirs(stagingDir);
    indexPath_ = FileUtils::joinPath(stagingDir, "index");
    load();   // populate index_ from disk (no-op if file absent)
}

// ─── Stage a file ─────────────────────────────────────────────────────────────
bool StagingManager::stageFile(const std::string& relPath, const std::string& contentHash) {
    index_[relPath] = contentHash;
    return save();
}

// ─── Unstage a file ───────────────────────────────────────────────────────────
bool StagingManager::unstageFile(const std::string& relPath) {
    auto it = index_.find(relPath);
    if (it == index_.end()) return false;
    index_.erase(it);
    return save();
}

// ─── Clear after commit ───────────────────────────────────────────────────────
void StagingManager::clearAll() {
    index_.clear();
    save();
}

// ─── Queries ──────────────────────────────────────────────────────────────────
bool StagingManager::isStaged(const std::string& relPath) const {
    return index_.count(relPath) > 0;
}

std::string StagingManager::stagedHash(const std::string& relPath) const {
    auto it = index_.find(relPath);
    return (it != index_.end()) ? it->second : "";
}

const std::map<std::string, std::string>& StagingManager::stagedFiles() const {
    return index_;
}

// ─── Persistence: save ────────────────────────────────────────────────────────
bool StagingManager::save() const {
    std::ostringstream oss;
    for (const auto& [path, hash] : index_) {
        oss << path << " " << hash << "\n";
    }
    return FileUtils::writeFile(indexPath_, oss.str());
}

// ─── Persistence: load ────────────────────────────────────────────────────────
bool StagingManager::load() {
    index_.clear();
    if (!FileUtils::exists(indexPath_)) return true;  // fresh repo, nothing to load

    std::string raw;
    try { raw = FileUtils::readFile(indexPath_); }
    catch (...) { return false; }

    std::istringstream iss(raw);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        size_t sep = line.rfind(' ');
        if (sep == std::string::npos) continue;
        std::string path = line.substr(0, sep);
        std::string hash = line.substr(sep + 1);
        index_[path] = hash;
    }
    return true;
}
