#pragma once
#include <string>
#include <map>
#include <vector>

/**
 * RepositoryManager - Owns the .minigit directory and coordinates all subsystems.
 *
 * Responsibilities:
 *   - Locate (or refuse) the repo root by walking up the directory tree
 *   - Initialize a new repository (.minigit scaffold)
 *   - Read / write HEAD  (.minigit/HEAD  → "ref: refs/heads/<branch>")
 *   - Read / write branch refs (.minigit/refs/heads/<name>  → commitId)
 *   - Provide the current commit ID and branch name
 *   - Write the append-only activity log (.minigit/logs/activity.log)
 *
 * All other managers (CommitManager, StagingManager, ObjectStore, …) are
 * instantiated by callers who receive the repoRoot from here.
 *
 * Design note:
 *   HEAD can be either:
 *     "ref: refs/heads/main"    ← normal (symbolic) — points to a branch
 *     "<sha256>"                ← detached HEAD after `checkout <commitId>`
 */
class RepositoryManager {
public:
    RepositoryManager() = default;

    // ── Initialisation ────────────────────────────────────────────────────────

    // Create a brand-new .minigit/ in the given directory.
    // Returns true on success, false if already initialised.
    static bool init(const std::string& directory);

    // Search for an existing .minigit/ starting from `startDir` and walking up.
    // Returns the root directory (containing .minigit) or "" if not found.
    static std::string findRepoRoot(const std::string& startDir);

    // Quick helper: does `dir` contain a .minigit/?
    static bool isRepo(const std::string& dir);

    // ── Paths ─────────────────────────────────────────────────────────────────

    static std::string minigitDir(const std::string& root);
    static std::string objectsDir(const std::string& root);
    static std::string commitsDir(const std::string& root);
    static std::string stagingDir(const std::string& root);
    static std::string refsDir(const std::string& root);
    static std::string logsDir(const std::string& root);
    static std::string headPath(const std::string& root);
    static std::string branchRefPath(const std::string& root, const std::string& branch);

    // ── HEAD management ───────────────────────────────────────────────────────

    // Returns the current branch name, or "" if in detached HEAD state.
    static std::string currentBranch(const std::string& root);

    // Returns the commit ID that HEAD points to (resolves symbolic refs).
    // Returns "" if no commits yet.
    static std::string headCommitId(const std::string& root);

    // Point HEAD to a branch (normal mode).
    static bool setHeadToBranch(const std::string& root, const std::string& branch);

    // Point HEAD directly to a commit hash (detached mode).
    static bool setHeadDetached(const std::string& root, const std::string& commitId);

    // ── Branch ref management ─────────────────────────────────────────────────

    // Read the commit ID a branch points to. Returns "" if branch doesn't exist.
    static std::string readBranchRef(const std::string& root, const std::string& branch);

    // Write a commit ID to a branch ref file.
    static bool writeBranchRef(const std::string& root,
                                const std::string& branch,
                                const std::string& commitId);

    // List all local branches.
    static std::vector<std::string> listBranches(const std::string& root);

    // ── Activity log ──────────────────────────────────────────────────────────

    static void appendLog(const std::string& root, const std::string& entry);

private:
    static std::string readHead(const std::string& root);
};
