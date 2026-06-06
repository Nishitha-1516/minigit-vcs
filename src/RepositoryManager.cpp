#include "RepositoryManager.h"
#include "FileUtils.h"
#include <vector>
#include <sstream>
#include <algorithm>

// ─── Path helpers ─────────────────────────────────────────────────────────────
std::string RepositoryManager::minigitDir(const std::string& root) {
    return FileUtils::joinPath(root, ".minigit");
}
std::string RepositoryManager::objectsDir(const std::string& root) {
    return FileUtils::joinPath(minigitDir(root), "objects");
}
std::string RepositoryManager::commitsDir(const std::string& root) {
    return FileUtils::joinPath(minigitDir(root), "commits");
}
std::string RepositoryManager::stagingDir(const std::string& root) {
    return FileUtils::joinPath(minigitDir(root), "staging");
}
std::string RepositoryManager::refsDir(const std::string& root) {
    return FileUtils::joinPath(minigitDir(root), "refs");
}
std::string RepositoryManager::logsDir(const std::string& root) {
    return FileUtils::joinPath(minigitDir(root), "logs");
}
std::string RepositoryManager::headPath(const std::string& root) {
    return FileUtils::joinPath(minigitDir(root), "HEAD");
}
std::string RepositoryManager::branchRefPath(const std::string& root,
                                              const std::string& branch) {
    return FileUtils::joinPath(refsDir(root), FileUtils::joinPath("heads", branch));
}

// ─── Init ─────────────────────────────────────────────────────────────────────
bool RepositoryManager::init(const std::string& directory) {
    std::string mgDir = minigitDir(directory);
    if (FileUtils::exists(mgDir)) return false;   // already initialised

    // Create directory scaffold
    FileUtils::createDirs(mgDir);
    FileUtils::createDirs(objectsDir(directory));
    FileUtils::createDirs(commitsDir(directory));
    FileUtils::createDirs(stagingDir(directory));
    FileUtils::createDirs(FileUtils::joinPath(refsDir(directory), "heads"));
    FileUtils::createDirs(logsDir(directory));

    // Write initial HEAD → main branch
    FileUtils::writeFile(headPath(directory), "ref: refs/heads/main\n");

    return true;
}

// ─── Find repo root by walking up the tree ────────────────────────────────────
std::string RepositoryManager::findRepoRoot(const std::string& startDir) {
    std::string current = startDir;

    while (true) {
        if (isRepo(current)) return current;

        std::string parent = FileUtils::parentDir(current);
        if (parent == current) break;   // reached filesystem root
        current = parent;
    }
    return "";
}

bool RepositoryManager::isRepo(const std::string& dir) {
    return FileUtils::isDirectory(minigitDir(dir));
}

// ─── Read raw HEAD content ────────────────────────────────────────────────────
std::string RepositoryManager::readHead(const std::string& root) {
    std::string hp = headPath(root);
    if (!FileUtils::exists(hp)) return "";
    std::string raw = FileUtils::readFile(hp);
    // Trim trailing newline
    while (!raw.empty() && (raw.back() == '\n' || raw.back() == '\r'))
        raw.pop_back();
    return raw;
}

// ─── Current branch name ──────────────────────────────────────────────────────
std::string RepositoryManager::currentBranch(const std::string& root) {
    std::string head = readHead(root);
    // Symbolic ref format: "ref: refs/heads/<branch>"
    const std::string prefix = "ref: refs/heads/";
    if (head.rfind(prefix, 0) == 0) {
        return head.substr(prefix.size());
    }
    return "";   // detached HEAD
}

// ─── Resolve HEAD → commit ID ─────────────────────────────────────────────────
std::string RepositoryManager::headCommitId(const std::string& root) {
    std::string head = readHead(root);
    if (head.empty()) return "";

    const std::string prefix = "ref: refs/heads/";
    if (head.rfind(prefix, 0) == 0) {
        std::string branch = head.substr(prefix.size());
        return readBranchRef(root, branch);
    }
    // Detached HEAD: head IS the commit ID
    return head;
}

// ─── HEAD writers ─────────────────────────────────────────────────────────────
bool RepositoryManager::setHeadToBranch(const std::string& root, const std::string& branch) {
    return FileUtils::writeFile(headPath(root), "ref: refs/heads/" + branch + "\n");
}

bool RepositoryManager::setHeadDetached(const std::string& root, const std::string& commitId) {
    return FileUtils::writeFile(headPath(root), commitId + "\n");
}

// ─── Branch ref read / write ──────────────────────────────────────────────────
std::string RepositoryManager::readBranchRef(const std::string& root,
                                              const std::string& branch) {
    std::string path = branchRefPath(root, branch);
    if (!FileUtils::exists(path)) return "";
    std::string id = FileUtils::readFile(path);
    while (!id.empty() && (id.back() == '\n' || id.back() == '\r')) id.pop_back();
    return id;
}

bool RepositoryManager::writeBranchRef(const std::string& root,
                                        const std::string& branch,
                                        const std::string& commitId) {
    std::string path = branchRefPath(root, branch);
    FileUtils::createDirs(FileUtils::parentDir(path));
    return FileUtils::writeFile(path, commitId + "\n");
}

// ─── List branches ────────────────────────────────────────────────────────────
std::vector<std::string> RepositoryManager::listBranches(const std::string& root) {
    std::vector<std::string> branches;
    std::string headsDir = FileUtils::joinPath(refsDir(root), "heads");
    auto files = FileUtils::listFiles(headsDir, false);
    for (const auto& f : files) {
        branches.push_back(FileUtils::fileName(f));
    }
    std::sort(branches.begin(), branches.end());
    return branches;
}

// ─── Activity log ─────────────────────────────────────────────────────────────
void RepositoryManager::appendLog(const std::string& root, const std::string& entry) {
    std::string logFile = FileUtils::joinPath(logsDir(root), "activity.log");
    std::string existing;
    try { existing = FileUtils::readFile(logFile); } catch (...) {}
    FileUtils::writeFile(logFile, existing + FileUtils::nowTimestamp() + "  " + entry + "\n");
}
