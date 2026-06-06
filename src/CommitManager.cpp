#include "CommitManager.h"
#include "FileUtils.h"
#include <stdexcept>
#include <algorithm>

// ─── Constructor ──────────────────────────────────────────────────────────────
CommitManager::CommitManager(const std::string& repoRoot)
    : commitsDir_(FileUtils::joinPath(repoRoot, "commits"))
{
    FileUtils::createDirs(commitsDir_);
}

// ─── Save a commit to disk ────────────────────────────────────────────────────
bool CommitManager::saveCommit(const Commit& commit) {
    if (!commit.isValid()) return false;
    std::string path = FileUtils::joinPath(commitsDir_, commit.id);
    return FileUtils::writeFile(path, commit.serialise());
}

// ─── Load a commit by its full hash ──────────────────────────────────────────
Commit CommitManager::loadCommit(const std::string& commitId) const {
    std::string path = FileUtils::joinPath(commitsDir_, commitId);
    if (!FileUtils::exists(path)) return Commit{};   // empty = invalid

    try {
        std::string raw = FileUtils::readFile(path);
        return Commit::deserialise(raw);
    } catch (...) {
        return Commit{};
    }
}

// ─── Resolve a short hash prefix to a full hash ───────────────────────────────
std::string CommitManager::resolveShortHash(const std::string& prefix) const {
    if (prefix.empty()) return "";

    // If it already looks like a full 64-char hash, verify it directly
    if (prefix.size() == 64 && commitExists(prefix)) return prefix;

    auto allFiles = FileUtils::listFiles(commitsDir_, false);
    std::vector<std::string> matches;

    for (const auto& filePath : allFiles) {
        std::string name = FileUtils::fileName(filePath);
        if (name.rfind(prefix, 0) == 0) {   // starts with prefix
            matches.push_back(name);
        }
    }

    if (matches.size() == 1) return matches[0];
    if (matches.size() > 1) {
        // Ambiguous short hash — return empty to signal error
        return "";
    }
    return "";
}

// ─── Traverse the parent chain ────────────────────────────────────────────────
std::vector<Commit> CommitManager::getHistory(const std::string& commitId) const {
    std::vector<Commit> history;
    std::string current = commitId;

    // Guard against corrupted cycles (should never happen, but be safe)
    const int MAX_DEPTH = 100000;
    int depth = 0;

    while (!current.empty() && depth < MAX_DEPTH) {
        Commit c = loadCommit(current);
        if (!c.isValid()) break;
        history.push_back(c);
        current = c.parentId;
        depth++;
    }

    return history;
}

// ─── Check existence ──────────────────────────────────────────────────────────
bool CommitManager::commitExists(const std::string& commitId) const {
    return FileUtils::exists(FileUtils::joinPath(commitsDir_, commitId));
}
