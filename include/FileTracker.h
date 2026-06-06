#pragma once
#include <string>
#include <map>
#include <set>
#include <vector>

/**
 * FileTracker - Compares the working tree against the last commit and staging area.
 *
 * Used by `status` to classify every file in the working tree into:
 *
 *   Staged (ready for next commit):
 *     - New file added to staging
 *     - Modified file added to staging
 *     - Deleted file removed from staging
 *
 *   Not staged (working tree differs from staging/HEAD):
 *     - Modified since last `add`
 *     - Deleted from working tree but still in last commit
 *
 *   Untracked:
 *     - Never seen before (not in HEAD commit, not in staging)
 *
 * The tracker reads file contents to compute hashes for comparison,
 * so it reflects byte-for-byte changes.
 */

struct WorkingTreeStatus {
    // Files staged for the next commit (relPath → staged hash)
    std::map<std::string, std::string> stagedNew;
    std::map<std::string, std::string> stagedModified;
    std::set<std::string>              stagedDeleted;

    // Changes in working tree not yet staged
    std::set<std::string> modifiedNotStaged;
    std::set<std::string> deletedNotStaged;

    // Files never tracked
    std::vector<std::string> untracked;

    bool isClean() const {
        return stagedNew.empty() && stagedModified.empty() && stagedDeleted.empty()
            && modifiedNotStaged.empty() && deletedNotStaged.empty() && untracked.empty();
    }
};

class FileTracker {
public:
    /**
     * @param workRoot     Path to the repository working directory (where user files live)
     * @param minigitRoot  Path to .minigit/
     * @param ignorePatterns  Simple filename patterns to skip (e.g. ".minigit", ".DS_Store")
     */
    FileTracker(const std::string& workRoot,
                const std::string& minigitRoot,
                const std::vector<std::string>& ignorePatterns = {});

    // Compute the current status given a HEAD snapshot and the staging index.
    WorkingTreeStatus computeStatus(
        const std::map<std::string, std::string>& headSnapshot,  // from last commit
        const std::map<std::string, std::string>& stagingIndex   // from StagingManager
    ) const;

    // Get all files currently in the working tree (relative paths).
    std::vector<std::string> listWorkingFiles() const;

    // Hash a working-tree file (returns "" if the file doesn't exist).
    std::string hashWorkingFile(const std::string& relPath) const;

private:
    std::string workRoot_;
    std::string minigitRoot_;
    std::vector<std::string> ignorePatterns_;

    bool shouldIgnore(const std::string& relPath) const;
};
