#include "FileUtils.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <ctime>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

namespace FileUtils {

// ── Path helpers ──────────────────────────────────────────────────────────────

bool exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool isDirectory(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool isFile(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

std::string parentDir(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return path.substr(0, pos);
}

std::string fileName(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return path;
    return path.substr(pos + 1);
}

std::string joinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    if (a.back() == '/') return a + b;
    return a + "/" + b;
}

std::string currentDir() {
    char buf[4096];
    if (getcwd(buf, sizeof(buf)) == nullptr) {
        throw std::runtime_error("Cannot get current directory");
    }
    return std::string(buf);
}

// ── Directory operations ──────────────────────────────────────────────────────

bool createDir(const std::string& path) {
    if (exists(path)) return true;
    return mkdir(path.c_str(), 0755) == 0;
}

// Recursive mkdir -p
bool createDirs(const std::string& path) {
    if (path.empty() || exists(path)) return true;
    // Ensure parent exists first
    std::string parent = parentDir(path);
    if (!parent.empty() && parent != path) {
        if (!createDirs(parent)) return false;
    }
    return createDir(path);
}

// List files in a directory (optionally recursive)
std::vector<std::string> listFiles(const std::string& dir, bool recursive) {
    std::vector<std::string> files;
    DIR* d = opendir(dir.c_str());
    if (!d) return files;

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        std::string name(entry->d_name);
        if (name == "." || name == "..") continue;

        std::string fullPath = joinPath(dir, name);

        if (isDirectory(fullPath)) {
            if (recursive) {
                auto sub = listFiles(fullPath, true);
                files.insert(files.end(), sub.begin(), sub.end());
            }
        } else {
            files.push_back(fullPath);
        }
    }
    closedir(d);
    return files;
}

std::vector<std::string> listDirs(const std::string& dir) {
    std::vector<std::string> dirs;
    DIR* d = opendir(dir.c_str());
    if (!d) return dirs;

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        std::string name(entry->d_name);
        if (name == "." || name == "..") continue;

        std::string fullPath = joinPath(dir, name);
        if (isDirectory(fullPath)) {
            dirs.push_back(fullPath);
        }
    }
    closedir(d);
    return dirs;
}

// ── File read / write ─────────────────────────────────────────────────────────

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot read file: " + path);
    }
    std::ostringstream oss;
    oss << f.rdbuf();
    return oss.str();
}

bool writeFile(const std::string& path, const std::string& content) {
    // Ensure parent directory exists
    std::string parent = parentDir(path);
    if (!parent.empty()) createDirs(parent);

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) return false;
    f << content;
    return f.good();
}

bool copyFile(const std::string& src, const std::string& dst) {
    try {
        std::string content = readFile(src);
        return writeFile(dst, content);
    } catch (...) {
        return false;
    }
}

bool removeFile(const std::string& path) {
    return std::remove(path.c_str()) == 0;
}

long long fileSize(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return -1;
    return static_cast<long long>(st.st_size);
}

// ── Timestamp ─────────────────────────────────────────────────────────────────

std::string nowTimestamp() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
    return std::string(buf);
}

// ── Relative path ─────────────────────────────────────────────────────────────

std::string relativeTo(const std::string& basePath, const std::string& fullPath) {
    if (fullPath.find(basePath) == 0) {
        std::string rel = fullPath.substr(basePath.size());
        if (!rel.empty() && rel[0] == '/') rel = rel.substr(1);
        return rel;
    }
    return fullPath;
}

} // namespace FileUtils
