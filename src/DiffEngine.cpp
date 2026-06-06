#include "DiffEngine.h"
#include <sstream>
#include <algorithm>
#include <map>
#include <functional>

// ─── ANSI colour codes ────────────────────────────────────────────────────────
static const std::string RESET  = "\033[0m";
static const std::string RED    = "\033[31m";
static const std::string GREEN  = "\033[32m";
static const std::string CYAN   = "\033[36m";
static const std::string BOLD   = "\033[1m";

// ─── Split text into lines (preserves empty lines) ───────────────────────────
std::vector<std::string> DiffEngine::splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }
    return lines;
}

// ─── Binary content heuristic: any null byte → binary ────────────────────────
bool DiffEngine::isBinaryContent(const std::string& content) {
    return content.find('\0') != std::string::npos;
}

// ─── LCS table lengths (O(m*n)) ──────────────────────────────────────────────
std::vector<int> DiffEngine::lcsLengths(const std::vector<std::string>& a,
                                         const std::vector<std::string>& b) {
    int m = static_cast<int>(a.size());
    int n = static_cast<int>(b.size());
    // Use two-row rolling array to save memory
    std::vector<int> prev(n + 1, 0), curr(n + 1, 0);
    // We store the full m×n table for backtracking; for large files a
    // space-optimised version would be used, but this is readable.
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));

    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            if (a[i-1] == b[j-1]) dp[i][j] = dp[i-1][j-1] + 1;
            else                   dp[i][j] = std::max(dp[i-1][j], dp[i][j-1]);
        }
    }

    // Backtrack to build edit script
    std::vector<int> result;
    int i = m, j = n;
    while (i > 0 && j > 0) {
        if (a[i-1] == b[j-1]) { result.push_back(i-1); i--; j--; }
        else if (dp[i-1][j] >= dp[i][j-1]) i--;
        else j--;
    }
    std::reverse(result.begin(), result.end());
    return result;   // indices in `a` that are in the LCS
}

// ─── Core diff ────────────────────────────────────────────────────────────────
FileDiff DiffEngine::diffContent(const std::string& path,
                                  const std::string& oldContent,
                                  const std::string& newContent,
                                  int contextLines) {
    FileDiff result;
    result.path = path;

    if (isBinaryContent(oldContent) || isBinaryContent(newContent)) {
        result.isBinary = true;
        return result;
    }

    auto oldLines = splitLines(oldContent);
    auto newLines = splitLines(newContent);

    // Build diff using LCS
    // lcsIndices[i] = index in oldLines that matches lcsIndices position
    std::vector<bool> oldInLCS(oldLines.size(), false);
    std::vector<bool> newInLCS(newLines.size(), false);

    // Full LCS backtrack
    int m = static_cast<int>(oldLines.size());
    int n = static_cast<int>(newLines.size());
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));
    for (int i = 1; i <= m; i++)
        for (int j = 1; j <= n; j++) {
            if (oldLines[i-1] == newLines[j-1]) dp[i][j] = dp[i-1][j-1] + 1;
            else dp[i][j] = std::max(dp[i-1][j], dp[i][j-1]);
        }

    // Mark matched lines
    {
        int i = m, j = n;
        while (i > 0 && j > 0) {
            if (oldLines[i-1] == newLines[j-1]) {
                oldInLCS[i-1] = true;
                newInLCS[j-1] = true;
                i--; j--;
            } else if (dp[i-1][j] >= dp[i][j-1]) i--;
            else j--;
        }
    }

    // Build flat list of DiffLines
    std::vector<DiffLine> allLines;
    int oi = 0, ni = 0;
    while (oi < m || ni < n) {
        if (oi < m && !oldInLCS[oi]) {
            allLines.push_back({DiffLine::Kind::Removed, oldLines[oi], oi + 1});
            oi++;
        } else if (ni < n && !newInLCS[ni]) {
            allLines.push_back({DiffLine::Kind::Added, newLines[ni], ni + 1});
            ni++;
        } else {
            allLines.push_back({DiffLine::Kind::Context, oldLines[oi], oi + 1});
            oi++; ni++;
        }
    }

    // Apply context window: only include lines within `contextLines` of a change
    std::vector<bool> include(allLines.size(), false);
    for (size_t idx = 0; idx < allLines.size(); idx++) {
        if (allLines[idx].kind != DiffLine::Kind::Context) {
            int lo = std::max(0, (int)idx - contextLines);
            int hi = std::min((int)allLines.size() - 1, (int)idx + contextLines);
            for (int k = lo; k <= hi; k++) include[k] = true;
        }
    }

    for (size_t idx = 0; idx < allLines.size(); idx++) {
        if (include[idx]) result.hunks.push_back(allLines[idx]);
    }

    return result;
}

// ─── Render to terminal string ────────────────────────────────────────────────
std::string DiffEngine::renderDiff(const FileDiff& diff, bool coloured) {
    std::ostringstream oss;

    auto col = [&](const std::string& code) { return coloured ? code : ""; };

    oss << col(BOLD) << "--- " << diff.path << col(RESET) << "\n";
    oss << col(BOLD) << "+++ " << diff.path << col(RESET) << "\n";

    if (diff.isBinary) {
        oss << col(CYAN) << "  (binary file)" << col(RESET) << "\n";
        return oss.str();
    }
    if (diff.isNew) {
        oss << col(GREEN) << "  (new file)" << col(RESET) << "\n";
    }
    if (diff.isDeleted) {
        oss << col(RED) << "  (deleted)" << col(RESET) << "\n";
    }

    // Print hunk separator between non-contiguous blocks
    int prevLine = -2;
    for (const auto& dl : diff.hunks) {
        if (dl.lineNo != prevLine + 1 && prevLine != -2) {
            oss << col(CYAN) << "@@ ..." << col(RESET) << "\n";
        }
        prevLine = dl.lineNo;

        switch (dl.kind) {
            case DiffLine::Kind::Added:
                oss << col(GREEN) << "+ " << dl.text << col(RESET) << "\n"; break;
            case DiffLine::Kind::Removed:
                oss << col(RED)   << "- " << dl.text << col(RESET) << "\n"; break;
            case DiffLine::Kind::Context:
                oss << "  " << dl.text << "\n"; break;
        }
    }

    return oss.str();
}

// ─── Diff two commit snapshots ────────────────────────────────────────────────
std::vector<FileDiff> DiffEngine::diffSnapshots(
    const std::map<std::string, std::string>& oldSnap,
    const std::map<std::string, std::string>& newSnap,
    std::function<std::string(const std::string& hash)> loadContent,
    int contextLines)
{
    std::vector<FileDiff> diffs;

    // Collect all paths across both snapshots
    std::set<std::string> allPaths;
    for (const auto& [p, _] : oldSnap) allPaths.insert(p);
    for (const auto& [p, _] : newSnap) allPaths.insert(p);

    for (const auto& path : allPaths) {
        auto oldIt = oldSnap.find(path);
        auto newIt = newSnap.find(path);

        std::string oldContent, newContent;

        if (oldIt == oldSnap.end()) {
            // New file
            newContent = loadContent(newIt->second);
            FileDiff fd = diffContent(path, "", newContent, contextLines);
            fd.isNew = true;
            diffs.push_back(fd);
        } else if (newIt == newSnap.end()) {
            // Deleted file
            oldContent = loadContent(oldIt->second);
            FileDiff fd = diffContent(path, oldContent, "", contextLines);
            fd.isDeleted = true;
            diffs.push_back(fd);
        } else if (oldIt->second != newIt->second) {
            // Modified file
            oldContent = loadContent(oldIt->second);
            newContent = loadContent(newIt->second);
            auto fd = diffContent(path, oldContent, newContent, contextLines);
            if (fd.hasChanges()) diffs.push_back(fd);
        }
        // else: identical hash → no change
    }

    return diffs;
}
