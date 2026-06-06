#pragma once
#include <string>
#include <map>
#include <set>

/**
 * StagingManager - Manages the staging area (index).
 *
 * The staging area tracks which files have been explicitly "added"
 * and are ready to be included in the next commit.
 *
 * On-disk format (.minigit/staging/index):
 *
 *   <relativePath> <contentHash>
 *   <relativePath> <contentHash>
 *   ...
 *
 * Each entry maps a working-tree path to the hash of the content
 * that was staged (i.e. the content at the time of `add`).
 *
 * Design decisions:
 *   - Re-adding a file updates its entry (last-write wins)
 *   - Unstaging removes the entry
 *   - The index is written to disk after every mutating operation
 *     so a crash never leaves a partially-written index
 */
class StagingManager {
public:
    explicit StagingManager(const std::string& repoRoot);

    // Stage a file: compute hash, store in ObjectStore, record in index.
    // Returns the content hash.
    bool stageFile(const std::string& relPath, const std::string& contentHash);

    // Remove a file from the staging area (does NOT touch the working tree).
    bool unstageFile(const std::string& relPath);

    // Clear all staged entries (called after a successful commit).
    void clearAll();

    // Query the staged snapshot.
    bool isStaged(const std::string& relPath) const;
    std::string stagedHash(const std::string& relPath) const; // "" if not staged

    // Full snapshot: path → hash
    const std::map<std::string, std::string>& stagedFiles() const;

    // Persistence
    bool save() const;
    bool load();

private:
    std::string indexPath_;
    std::map<std::string, std::string> index_;  // relPath → contentHash
};
