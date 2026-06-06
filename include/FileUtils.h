#pragma once
#include <string>
#include <vector>

/**
 * FileUtils - Thin wrappers around POSIX/STL filesystem operations.
 * Keeps all raw I/O in one place so higher-level classes stay clean.
 */
namespace FileUtils {

    // ── Path helpers ──────────────────────────────────────────────────────────
    bool        exists(const std::string& path);
    bool        isDirectory(const std::string& path);
    bool        isFile(const std::string& path);
    std::string parentDir(const std::string& path);
    std::string fileName(const std::string& path);   // basename
    std::string joinPath(const std::string& a, const std::string& b);
    std::string currentDir();

    // ── Directory operations ──────────────────────────────────────────────────
    bool createDir(const std::string& path);          // mkdir -p
    bool createDirs(const std::string& path);         // recursive
    std::vector<std::string> listFiles(const std::string& dir, bool recursive = false);
    std::vector<std::string> listDirs(const std::string& dir);

    // ── File read / write ─────────────────────────────────────────────────────
    std::string readFile(const std::string& path);
    bool        writeFile(const std::string& path, const std::string& content);
    bool        copyFile(const std::string& src, const std::string& dst);
    bool        removeFile(const std::string& path);
    long long   fileSize(const std::string& path);

    // ── Timestamp ─────────────────────────────────────────────────────────────
    std::string nowTimestamp();   // ISO-8601 style: "2024-01-15 14:30:00"

    // ── Relative path helpers ─────────────────────────────────────────────────
    // Strip leading "./" or absolute prefix relative to base
    std::string relativeTo(const std::string& basePath, const std::string& fullPath);
}
