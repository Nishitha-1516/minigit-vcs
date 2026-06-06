#include "CommandParser.h"
#include "RepositoryManager.h"
#include "CommitManager.h"
#include "StagingManager.h"
#include "ObjectStore.h"
#include "FileTracker.h"
#include "DiffEngine.h"
#include "HashUtility.h"
#include "FileUtils.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <set>

// ─── ANSI helpers ─────────────────────────────────────────────────────────────
static const std::string C_RESET  = "\033[0m";
static const std::string C_RED    = "\033[31m";
static const std::string C_GREEN  = "\033[32m";
static const std::string C_YELLOW = "\033[33m";
static const std::string C_CYAN   = "\033[36m";
static const std::string C_BOLD   = "\033[1m";
static const std::string C_DIM    = "\033[2m";

// ─── Parse ────────────────────────────────────────────────────────────────────
ParsedCommand CommandParser::parse(int argc, char** argv) {
    ParsedCommand cmd;
    if (argc < 2) { cmd.verb = "help"; return cmd; }

    cmd.verb = argv[1];

    for (int i = 2; i < argc; i++) {
        std::string tok = argv[i];
        if (tok == "-m" && i + 1 < argc) {
            cmd.flags["-m"] = argv[++i];
        } else if (tok == "--oneline") {
            cmd.flags["--oneline"] = "1";
        } else if (tok.rfind("--", 0) == 0 && tok.find('=') != std::string::npos) {
            size_t eq = tok.find('=');
            cmd.flags[tok.substr(0, eq)] = tok.substr(eq + 1);
        } else if (tok[0] == '-') {
            cmd.flags[tok] = "1";
        } else {
            cmd.args.push_back(tok);
        }
    }
    return cmd;
}

// ─── requireRepo helper ───────────────────────────────────────────────────────
std::string CommandParser::requireRepo() {
    std::string root = RepositoryManager::findRepoRoot(FileUtils::currentDir());
    if (root.empty()) {
        std::cerr << C_RED << "error: not a minigit repository "
                     "(or any of the parent directories)" << C_RESET << "\n";
    }
    return root;
}

// ─── Dispatch ─────────────────────────────────────────────────────────────────
int CommandParser::execute(const ParsedCommand& cmd) {
    if (cmd.verb == "help" || cmd.verb == "--help" || cmd.verb == "-h") {
        printHelp(); return 0;
    }
    if (cmd.verb == "init")     return handleInit(cmd);

    // All other commands need a repo
    std::string root = requireRepo();
    if (root.empty()) return 1;

    if (cmd.verb == "add")      return handleAdd(cmd, root);
    if (cmd.verb == "commit")   return handleCommit(cmd, root);
    if (cmd.verb == "status")   return handleStatus(cmd, root);
    if (cmd.verb == "log")      return handleLog(cmd, root);
    if (cmd.verb == "checkout") return handleCheckout(cmd, root);
    if (cmd.verb == "branch")   return handleBranch(cmd, root);
    if (cmd.verb == "diff")     return handleDiff(cmd, root);
    if (cmd.verb == "restore")  return handleRestore(cmd, root);

    std::cerr << C_RED << "error: unknown command '" << cmd.verb << "'\n"
              << "Run 'minigit help' for usage." << C_RESET << "\n";
    return 1;
}

// ═══════════════════════════════════════════════════════════════════════════════
// init
// ═══════════════════════════════════════════════════════════════════════════════
int CommandParser::handleInit(const ParsedCommand& cmd) {
    std::string dir = cmd.args.empty() ? FileUtils::currentDir() : cmd.args[0];

    if (!FileUtils::exists(dir)) {
        std::cerr << C_RED << "error: directory does not exist: " << dir << C_RESET << "\n";
        return 1;
    }

    bool ok = RepositoryManager::init(dir);
    if (!ok) {
        std::cout << "Reinitialized existing minigit repository in "
                  << RepositoryManager::minigitDir(dir) << "\n";
    } else {
        std::cout << C_GREEN << "Initialized empty minigit repository in "
                  << RepositoryManager::minigitDir(dir) << C_RESET << "\n";
        RepositoryManager::appendLog(dir, "init: repository created");
    }
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// add <file> [<file>…]
// ═══════════════════════════════════════════════════════════════════════════════
int CommandParser::handleAdd(const ParsedCommand& cmd, const std::string& root) {
    if (cmd.args.empty()) {
        std::cerr << C_RED << "error: nothing specified to add\n"
                  << "Usage: minigit add <file>..." << C_RESET << "\n";
        return 1;
    }

    std::string mgDir = RepositoryManager::minigitDir(root);
    ObjectStore  store(mgDir);
    StagingManager staging(mgDir);

    int errors = 0;
    for (const auto& arg : cmd.args) {
        // Support "." to add all working-tree files
        std::vector<std::string> targets;
        if (arg == ".") {
            FileTracker tracker(root, mgDir);
            targets = tracker.listWorkingFiles();
        } else {
            targets.push_back(arg);
        }

        for (const auto& relPath : targets) {
            std::string absPath = FileUtils::joinPath(root, relPath);

            if (!FileUtils::isFile(absPath)) {
                std::cerr << C_RED << "error: pathspec '" << relPath
                          << "' did not match any files" << C_RESET << "\n";
                errors++;
                continue;
            }

            try {
                std::string hash = store.storeFile(absPath);
                staging.stageFile(relPath, hash);
                std::cout << "add '" << relPath << "'\n";
            } catch (const std::exception& e) {
                std::cerr << C_RED << "error: " << e.what() << C_RESET << "\n";
                errors++;
            }
        }
    }

    if (errors == 0) RepositoryManager::appendLog(root, "add: staged " + std::to_string(cmd.args.size()) + " path(s)");
    return errors ? 1 : 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// commit -m <message>
// ═══════════════════════════════════════════════════════════════════════════════
int CommandParser::handleCommit(const ParsedCommand& cmd, const std::string& root) {
    auto msgIt = cmd.flags.find("-m");
    if (msgIt == cmd.flags.end() || msgIt->second.empty()) {
        std::cerr << C_RED << "error: commit message required\n"
                  << "Usage: minigit commit -m \"your message\"" << C_RESET << "\n";
        return 1;
    }
    const std::string& message = msgIt->second;

    std::string mgDir = RepositoryManager::minigitDir(root);
    StagingManager staging(mgDir);
    CommitManager  commits(mgDir);

    if (staging.stagedFiles().empty()) {
        std::cout << "On branch " << RepositoryManager::currentBranch(root) << "\n"
                  << "nothing to commit, working tree clean\n";
        return 0;
    }

    // Build the commit object
    Commit c;
    c.parentId  = RepositoryManager::headCommitId(root);  // "" for first commit
    c.branch    = RepositoryManager::currentBranch(root);
    c.timestamp = FileUtils::nowTimestamp();
    c.message   = message;
    c.files     = staging.stagedFiles();

    // If we have a parent, merge its snapshot with the staged changes
    // (staged files override; unstaged files from parent are carried forward)
    if (!c.parentId.empty()) {
        Commit parent = commits.loadCommit(c.parentId);
        if (parent.isValid()) {
            // Start from parent snapshot, apply staged changes on top
            auto merged = parent.files;
            for (const auto& [path, hash] : staging.stagedFiles()) {
                merged[path] = hash;
            }
            c.files = merged;
        }
    }

    c.id = c.computeId();

    if (!commits.saveCommit(c)) {
        std::cerr << C_RED << "error: failed to write commit" << C_RESET << "\n";
        return 1;
    }

    // Advance branch ref and clear staging
    std::string branch = RepositoryManager::currentBranch(root);
    if (!branch.empty()) {
        RepositoryManager::writeBranchRef(root, branch, c.id);
    } else {
        // Detached HEAD: update HEAD directly
        RepositoryManager::setHeadDetached(root, c.id);
    }

    staging.clearAll();

    std::cout << C_YELLOW << "[" << branch << " " << HashUtility::shortHash(c.id) << "] "
              << C_RESET << message << "\n"
              << " " << c.files.size() << " file(s) tracked\n";

    RepositoryManager::appendLog(root, "commit " + c.id + ": " + message);
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// status
// ═══════════════════════════════════════════════════════════════════════════════
int CommandParser::handleStatus(const ParsedCommand& cmd, const std::string& root) {
    std::string mgDir = RepositoryManager::minigitDir(root);
    StagingManager staging(mgDir);
    CommitManager  commits(mgDir);

    std::string branch   = RepositoryManager::currentBranch(root);
    std::string headId   = RepositoryManager::headCommitId(root);

    // Print branch line
    if (!branch.empty()) {
        std::cout << "On branch " << C_BOLD << branch << C_RESET << "\n";
    } else {
        std::cout << "HEAD detached at " << C_YELLOW
                  << HashUtility::shortHash(headId) << C_RESET << "\n";
    }

    // Load HEAD snapshot
    std::map<std::string, std::string> headSnapshot;
    if (!headId.empty()) {
        Commit head = commits.loadCommit(headId);
        if (head.isValid()) headSnapshot = head.files;
    }

    FileTracker tracker(root, mgDir, {".minigit"});
    WorkingTreeStatus status = tracker.computeStatus(headSnapshot, staging.stagedFiles());

    if (status.isClean()) {
        std::cout << "nothing to commit, working tree clean\n";
        return 0;
    }

    // ── Staged changes ────────────────────────────────────────────────────────
    bool hasStagedChanges = !status.stagedNew.empty()
                         || !status.stagedModified.empty()
                         || !status.stagedDeleted.empty();
    if (hasStagedChanges) {
        std::cout << "\nChanges to be committed:\n"
                  << C_DIM << "  (use \"minigit restore <file>\" to unstage)\n" << C_RESET;
        for (const auto& [p, _] : status.stagedNew)      std::cout << C_GREEN << "\tnew file:   " << p << C_RESET << "\n";
        for (const auto& [p, _] : status.stagedModified) std::cout << C_GREEN << "\tmodified:   " << p << C_RESET << "\n";
        for (const auto& p      : status.stagedDeleted)  std::cout << C_GREEN << "\tdeleted:    " << p << C_RESET << "\n";
    }

    // ── Unstaged changes ──────────────────────────────────────────────────────
    bool hasUnstaged = !status.modifiedNotStaged.empty() || !status.deletedNotStaged.empty();
    if (hasUnstaged) {
        std::cout << "\nChanges not staged for commit:\n"
                  << C_DIM << "  (use \"minigit add <file>\" to stage)\n" << C_RESET;
        for (const auto& p : status.modifiedNotStaged) std::cout << C_RED << "\tmodified:   " << p << C_RESET << "\n";
        for (const auto& p : status.deletedNotStaged)  std::cout << C_RED << "\tdeleted:    " << p << C_RESET << "\n";
    }

    // ── Untracked ─────────────────────────────────────────────────────────────
    if (!status.untracked.empty()) {
        std::cout << "\nUntracked files:\n"
                  << C_DIM << "  (use \"minigit add <file>\" to include)\n" << C_RESET;
        for (const auto& p : status.untracked) std::cout << C_RED << "\t" << p << C_RESET << "\n";
    }

    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// log [--oneline]
// ═══════════════════════════════════════════════════════════════════════════════
int CommandParser::handleLog(const ParsedCommand& cmd, const std::string& root) {
    std::string mgDir  = RepositoryManager::minigitDir(root);
    std::string headId = RepositoryManager::headCommitId(root);
    bool oneline = cmd.flags.count("--oneline") > 0;

    if (headId.empty()) {
        std::cout << "No commits yet.\n";
        return 0;
    }

    CommitManager commits(mgDir);
    auto history = commits.getHistory(headId);

    for (const auto& c : history) {
        if (oneline) {
            std::cout << C_YELLOW << HashUtility::shortHash(c.id) << C_RESET
                      << " " << c.message << "\n";
        } else {
            std::cout << C_YELLOW << "commit " << c.id << C_RESET << "\n";
            if (!c.parentId.empty())
                std::cout << "Parent:  " << c.parentId << "\n";
            std::cout << "Branch:  " << c.branch    << "\n"
                      << "Date:    " << c.timestamp  << "\n"
                      << "\n    " << c.message << "\n\n"
                      << C_DIM << "  " << c.files.size() << " file(s) in snapshot\n" << C_RESET
                      << "─────────────────────────────────────────────────────────────\n";
        }
    }
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// checkout <commit-id|branch>
// ═══════════════════════════════════════════════════════════════════════════════
int CommandParser::handleCheckout(const ParsedCommand& cmd, const std::string& root) {
    if (cmd.args.empty()) {
        std::cerr << C_RED << "error: checkout requires a commit ID or branch name\n"
                  << "Usage: minigit checkout <commit-id|branch>" << C_RESET << "\n";
        return 1;
    }

    std::string target = cmd.args[0];
    std::string mgDir  = RepositoryManager::minigitDir(root);
    CommitManager commits(mgDir);
    ObjectStore   store(mgDir);

    // ── Try as branch name first ──────────────────────────────────────────────
    std::string branchCommit = RepositoryManager::readBranchRef(root, target);
    bool isBranch = !branchCommit.empty();
    std::string commitId;

    if (isBranch) {
        commitId = branchCommit;
    } else {
        // Try as full or short commit hash
        commitId = commits.resolveShortHash(target);
        if (commitId.empty()) {
            std::cerr << C_RED << "error: pathspec '" << target
                      << "' did not match any branch or commit" << C_RESET << "\n";
            return 1;
        }
    }

    Commit c = commits.loadCommit(commitId);
    if (!c.isValid()) {
        std::cerr << C_RED << "error: could not load commit " << commitId << C_RESET << "\n";
        return 1;
    }

    // Restore every file in the commit's snapshot to the working tree
    int restored = 0, failed = 0;
    for (const auto& [relPath, hash] : c.files) {
        std::string absPath = FileUtils::joinPath(root, relPath);
        FileUtils::createDirs(FileUtils::parentDir(absPath));
        if (store.restoreObject(hash, absPath)) restored++;
        else { std::cerr << C_RED << "warning: could not restore " << relPath << C_RESET << "\n"; failed++; }
    }

    // Update HEAD
    if (isBranch) {
        RepositoryManager::setHeadToBranch(root, target);
        std::cout << "Switched to branch '" << C_BOLD << target << C_RESET << "'\n";
    } else {
        RepositoryManager::setHeadDetached(root, commitId);
        std::cout << C_YELLOW << "HEAD is now at " << HashUtility::shortHash(commitId)
                  << C_RESET << " " << c.message << "\n";
        std::cout << C_DIM << "You are in 'detached HEAD' state.\n" << C_RESET;
    }

    // Clear staging to match the checked-out state
    StagingManager staging(mgDir);
    staging.clearAll();

    std::cout << "Restored " << restored << " file(s)";
    if (failed) std::cout << ", " << C_RED << failed << " failed" << C_RESET;
    std::cout << "\n";

    RepositoryManager::appendLog(root, "checkout: " + target);
    return failed ? 1 : 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// branch [<name>]
// ═══════════════════════════════════════════════════════════════════════════════
int CommandParser::handleBranch(const ParsedCommand& cmd, const std::string& root) {
    std::string currentBranch = RepositoryManager::currentBranch(root);

    if (cmd.args.empty()) {
        // List all branches
        auto branches = RepositoryManager::listBranches(root);
        if (branches.empty()) {
            std::cout << "(no branches yet)\n";
        }
        for (const auto& b : branches) {
            if (b == currentBranch) std::cout << C_GREEN << "* " << b << C_RESET << "\n";
            else                    std::cout << "  " << b << "\n";
        }
        return 0;
    }

    // Create a new branch at HEAD
    std::string newBranch = cmd.args[0];
    std::string headId    = RepositoryManager::headCommitId(root);

    if (headId.empty()) {
        std::cerr << C_RED << "error: cannot create branch before first commit" << C_RESET << "\n";
        return 1;
    }

    std::string existing = RepositoryManager::readBranchRef(root, newBranch);
    if (!existing.empty()) {
        std::cerr << C_RED << "error: branch '" << newBranch << "' already exists" << C_RESET << "\n";
        return 1;
    }

    RepositoryManager::writeBranchRef(root, newBranch, headId);
    std::cout << "Created branch '" << C_BOLD << newBranch << C_RESET
              << "' at " << HashUtility::shortHash(headId) << "\n";

    RepositoryManager::appendLog(root, "branch: created " + newBranch);
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// diff [<commit1> [<commit2>]]
// ═══════════════════════════════════════════════════════════════════════════════
int CommandParser::handleDiff(const ParsedCommand& cmd, const std::string& root) {
    std::string mgDir = RepositoryManager::minigitDir(root);
    CommitManager commits(mgDir);
    ObjectStore   store(mgDir);

    auto loadContent = [&](const std::string& hash) -> std::string {
        try { return store.loadObject(hash); } catch (...) { return ""; }
    };

    std::map<std::string, std::string> oldSnap, newSnap;

    if (cmd.args.size() >= 2) {
        // diff <commit1> <commit2>
        std::string id1 = commits.resolveShortHash(cmd.args[0]);
        std::string id2 = commits.resolveShortHash(cmd.args[1]);
        if (id1.empty() || id2.empty()) {
            std::cerr << C_RED << "error: could not resolve commit IDs" << C_RESET << "\n";
            return 1;
        }
        oldSnap = commits.loadCommit(id1).files;
        newSnap = commits.loadCommit(id2).files;
    } else if (cmd.args.size() == 1) {
        // diff <commit> → compare commit vs working tree
        std::string id = commits.resolveShortHash(cmd.args[0]);
        if (id.empty()) { std::cerr << C_RED << "error: unknown commit" << C_RESET << "\n"; return 1; }
        oldSnap = commits.loadCommit(id).files;
        // Build newSnap from working tree
        FileTracker tracker(root, mgDir);
        for (const auto& relPath : tracker.listWorkingFiles()) {
            std::string hash = tracker.hashWorkingFile(relPath);
            if (!hash.empty()) newSnap[relPath] = hash;
        }
    } else {
        // diff → HEAD vs working tree
        std::string headId = RepositoryManager::headCommitId(root);
        if (!headId.empty()) oldSnap = commits.loadCommit(headId).files;
        FileTracker tracker(root, mgDir);
        for (const auto& relPath : tracker.listWorkingFiles()) {
            // For working-tree diff we need a temporary store of working content
            std::string absPath = FileUtils::joinPath(root, relPath);
            std::string hash;
            try { hash = store.storeFile(absPath); } catch (...) {}
            if (!hash.empty()) newSnap[relPath] = hash;
        }
    }

    auto diffs = DiffEngine::diffSnapshots(oldSnap, newSnap, loadContent);

    if (diffs.empty()) {
        std::cout << "No differences found.\n";
        return 0;
    }

    for (const auto& d : diffs) {
        std::cout << DiffEngine::renderDiff(d, /*coloured=*/true);
    }
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// restore <file>   — unstage a file (revert staging area to HEAD version)
// ═══════════════════════════════════════════════════════════════════════════════
int CommandParser::handleRestore(const ParsedCommand& cmd, const std::string& root) {
    if (cmd.args.empty()) {
        std::cerr << C_RED << "error: restore requires a file path\n"
                  << "Usage: minigit restore <file>" << C_RESET << "\n";
        return 1;
    }

    std::string mgDir = RepositoryManager::minigitDir(root);
    StagingManager staging(mgDir);
    ObjectStore    store(mgDir);
    CommitManager  commits(mgDir);

    // Load HEAD snapshot to find the previous version
    std::string headId = RepositoryManager::headCommitId(root);
    std::map<std::string, std::string> headSnap;
    if (!headId.empty()) {
        Commit head = commits.loadCommit(headId);
        if (head.isValid()) headSnap = head.files;
    }

    int ok = 0, failed = 0;
    for (const auto& relPath : cmd.args) {
        // Remove from staging index
        staging.unstageFile(relPath);

        // Restore working tree file to HEAD version (if it exists in HEAD)
        auto it = headSnap.find(relPath);
        if (it != headSnap.end()) {
            std::string absPath = FileUtils::joinPath(root, relPath);
            if (store.restoreObject(it->second, absPath)) {
                std::cout << "Restored '" << relPath << "'\n";
                ok++;
            } else {
                std::cerr << C_RED << "error: failed to restore " << relPath << C_RESET << "\n";
                failed++;
            }
        } else {
            std::cout << C_DIM << "Unstaged '" << relPath << "' (not in HEAD)\n" << C_RESET;
            ok++;
        }
    }

    return failed ? 1 : 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// help
// ═══════════════════════════════════════════════════════════════════════════════
void CommandParser::printHelp() {
    std::cout << C_BOLD << "minigit" << C_RESET << " — a lightweight version control system\n\n"
              << C_YELLOW << "Usage:" << C_RESET << " minigit <command> [options]\n\n"
              << C_YELLOW << "Commands:\n" << C_RESET
              << "  init [dir]              Initialize a new repository\n"
              << "  add <file>…             Stage files for the next commit\n"
              << "  add .                   Stage all tracked & new files\n"
              << "  commit -m <msg>         Record staged changes as a commit\n"
              << "  status                  Show working tree status\n"
              << "  log [--oneline]         Show commit history\n"
              << "  checkout <id|branch>    Switch branch or restore commit\n"
              << "  branch [name]           List or create branches\n"
              << "  diff [c1 [c2]]          Show differences between commits or working tree\n"
              << "  restore <file>          Unstage file, restore to HEAD version\n"
              << "  help                    Show this message\n";
}
