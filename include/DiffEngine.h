#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>

/**
 * DiffEngine - Produces a unified-diff-style comparison between two text blobs.
 *
 * Algorithm: Myers' LCS-based diff (simplified O(ND) approach).
 * Output is colourised for the terminal when `coloured = true`.
 *
 * Used by the `diff` command to compare:
 *   - Two commits
 *   - A commit vs the working tree
 *   - Two versions of a single file
 */
struct DiffLine {
    enum class Kind { Context, Added, Removed };
    Kind        kind;
    std::string text;
    int         lineNo;   // line number in the relevant file
};

struct FileDiff {
    std::string path;
    bool        isBinary  = false;
    bool        isNew     = false;
    bool        isDeleted = false;
    std::vector<DiffLine> hunks;

    bool hasChanges() const { return isNew || isDeleted || !hunks.empty(); }
};

class DiffEngine {
public:
    // Diff two raw content strings for a given file path.
    static FileDiff diffContent(const std::string& path,
                                const std::string& oldContent,
                                const std::string& newContent,
                                int contextLines = 3);

    // Render a FileDiff to a terminal string (optionally with ANSI colours).
    static std::string renderDiff(const FileDiff& diff, bool coloured = true);

    // Diff two commit snapshots and return one FileDiff per changed file.
    // Caller provides content-loading lambda to avoid coupling to ObjectStore.
    static std::vector<FileDiff> diffSnapshots(
        const std::map<std::string, std::string>& oldSnap,
        const std::map<std::string, std::string>& newSnap,
        std::function<std::string(const std::string& hash)> loadContent,
        int contextLines = 3);

private:
    static std::vector<std::string> splitLines(const std::string& text);
    static std::vector<int>         lcsLengths(const std::vector<std::string>& a,
                                               const std::vector<std::string>& b);
    static bool isBinaryContent(const std::string& content);
};
