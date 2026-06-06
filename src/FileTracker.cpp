#include "FileTracker.h"
#include "HashUtility.h"
#include "FileUtils.h"
#include <algorithm>

// ─── Constructor ──────────────────────────────────────────────────────────────
FileTracker::FileTracker(const std::string& workRoot,
                         const std::string& minigitRoot,
                         const std::vector<std::string>& ignorePatterns)
    : workRoot_(workRoot)
    , minigitRoot_(minigitRoot)
    , ignorePatterns_(ignorePatterns)
{}

// ─── Ignore check ─────────────────────────────────────────────────────────────
bool FileTracker::shouldIgnore(const std::string& relPath) const {
    // Always skip the .minigit directory itself
    if (relPath == ".minigit" || relPath.rfind(".minigit/", 0) == 0) return true;

    for (const auto& pattern : ignorePatterns_) {
        // Simple substring / prefix match (extend to glob if needed)
        if (relPath == pattern || relPath.rfind(pattern, 0) == 0) return true;
        // Also match basename
        if (FileUtils::fileName(relPath) == pattern) return true;
    }
    return false;
}

// ─── List all files in the working tree ──────────────────────────────────────
std::vector<std::string> FileTracker::listWorkingFiles() const {
    auto absFiles = FileUtils::listFiles(workRoot_, /*recursive=*/true);
    std::vector<std::string> result;

    for (const auto& abs : absFiles) {
        std::string rel = FileUtils::relativeTo(workRoot_, abs);
        if (!shouldIgnore(rel)) {
            result.push_back(rel);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

// ─── Hash a single working-tree file ─────────────────────────────────────────
std::string FileTracker::hashWorkingFile(const std::string& relPath) const {
    std::string absPath = FileUtils::joinPath(workRoot_, relPath);
    if (!FileUtils::isFile(absPath)) return "";
    try {
        return HashUtility::hashFile(absPath);
    } catch (...) {
        return "";
    }
}

// ─── Core: compute working tree status ───────────────────────────────────────
WorkingTreeStatus FileTracker::computeStatus(
    const std::map<std::string, std::string>& headSnapshot,
    const std::map<std::string, std::string>& stagingIndex) const
{
    WorkingTreeStatus status;

    // All files currently on disk
    auto workingFiles = listWorkingFiles();
    std::set<std::string> workingSet(workingFiles.begin(), workingFiles.end());

    // ── 1. Classify staged entries vs HEAD ────────────────────────────────────
    for (const auto& [path, stagedHash] : stagingIndex) {
        auto headIt = headSnapshot.find(path);

        if (headIt == headSnapshot.end()) {
            // File wasn't in last commit → staged as new
            status.stagedNew[path] = stagedHash;
        } else if (headIt->second != stagedHash) {
            // File was in last commit but hash changed → staged modification
            status.stagedModified[path] = stagedHash;
        }
        // else: same hash as HEAD → staged but no change (shouldn't happen
        //       if `add` is smart, but harmless)
    }

    // Detect files that were in HEAD but are NOT in the staging area
    // and not in the working tree → staged as deleted
    for (const auto& [path, headHash] : headSnapshot) {
        if (stagingIndex.count(path) == 0 && workingSet.count(path) == 0) {
            status.stagedDeleted.insert(path);
        }
    }

    // ── 2. Classify working-tree files vs staging/HEAD ────────────────────────
    for (const auto& relPath : workingFiles) {
        std::string workHash = hashWorkingFile(relPath);

        auto stageIt = stagingIndex.find(relPath);
        auto headIt  = headSnapshot.find(relPath);

        if (stageIt != stagingIndex.end()) {
            // File is staged — check if working copy differs from staged version
            if (stageIt->second != workHash) {
                status.modifiedNotStaged.insert(relPath);
            }
        } else if (headIt != headSnapshot.end()) {
            // File is in HEAD but not staged — check if it's been modified
            if (headIt->second != workHash) {
                status.modifiedNotStaged.insert(relPath);
            }
        } else {
            // Not in staging or HEAD → untracked
            status.untracked.push_back(relPath);
        }
    }

    // ── 3. Detect working-tree deletions ─────────────────────────────────────
    // Files in staging but missing from working tree
    for (const auto& [path, hash] : stagingIndex) {
        if (workingSet.count(path) == 0) {
            status.deletedNotStaged.insert(path);
        }
    }
    // Files in HEAD but missing from working tree and staging
    for (const auto& [path, hash] : headSnapshot) {
        if (workingSet.count(path) == 0 && stagingIndex.count(path) == 0) {
            // Only flag as deleted-not-staged if not already in stagedDeleted
            if (status.stagedDeleted.count(path) == 0) {
                status.deletedNotStaged.insert(path);
            }
        }
    }

    return status;
}
