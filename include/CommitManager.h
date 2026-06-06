#pragma once
#include "Commit.h"
#include <string>
#include <vector>
#include <optional>

/**
 * CommitManager - Reads and writes Commit objects to disk.
 *
 * Storage layout:
 *   .minigit/commits/<fullHash>   ← serialised Commit text file
 *
 * Responsibilities:
 *   - Persist new commits
 *   - Load commits by ID (exact or prefix match)
 *   - Traverse the parent chain (for `log`)
 *   - Resolve short hashes to full IDs
 *
 * Note: CommitManager does NOT manage HEAD or branches.
 *       That is the responsibility of RepositoryManager.
 */
class CommitManager {
public:
    explicit CommitManager(const std::string& repoRoot);

    // Write a commit to disk. The commit's `id` must already be set.
    bool saveCommit(const Commit& commit);

    // Load a commit by its full hash. Returns empty Commit on failure.
    Commit loadCommit(const std::string& commitId) const;

    // Resolve a short hash prefix → full hash.
    // Returns "" if not found or ambiguous.
    std::string resolveShortHash(const std::string& prefix) const;

    // Collect the full ancestry chain starting from commitId (inclusive).
    // Ordered from newest → oldest (root last).
    std::vector<Commit> getHistory(const std::string& commitId) const;

    // Check if a commit file exists.
    bool commitExists(const std::string& commitId) const;

private:
    std::string commitsDir_;  // .minigit/commits/
};
