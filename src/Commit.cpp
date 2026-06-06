#include "Commit.h"
#include "HashUtility.h"
#include <sstream>
#include <stdexcept>

// ─── Serialise to on-disk format ──────────────────────────────────────────────
std::string Commit::serialise() const {
    std::ostringstream oss;
    oss << "id      " << id        << "\n";
    oss << "parent  " << parentId  << "\n";
    oss << "branch  " << branch    << "\n";
    oss << "time    " << timestamp << "\n";
    oss << "message " << message   << "\n";
    oss << "files   " << files.size() << "\n";
    for (const auto& [path, hash] : files) {
        oss << path << " " << hash << "\n";
    }
    return oss.str();
}

// ─── Deserialise from on-disk format ─────────────────────────────────────────
Commit Commit::deserialise(const std::string& raw) {
    Commit c;
    std::istringstream iss(raw);
    std::string line;
    int fileCount = 0;
    bool readingFiles = false;

    while (std::getline(iss, line)) {
        if (line.empty()) continue;

        if (readingFiles) {
            // Each file line: "<path> <hash>"
            size_t sep = line.rfind(' ');
            if (sep != std::string::npos) {
                std::string path = line.substr(0, sep);
                std::string hash = line.substr(sep + 1);
                c.files[path] = hash;
            }
            continue;
        }

        // Key-value lines use fixed 8-char key field
        auto parseValue = [&](const std::string& prefix) -> std::string {
            if (line.rfind(prefix, 0) == 0) {
                // Value starts after prefix, trim leading spaces
                std::string val = line.substr(prefix.size());
                size_t start = val.find_first_not_of(" \t");
                return (start == std::string::npos) ? "" : val.substr(start);
            }
            return "";
        };

        if (line.rfind("id ", 0) == 0)      { c.id        = parseValue("id "); }
        else if (line.rfind("parent ", 0) == 0)  { c.parentId  = parseValue("parent "); }
        else if (line.rfind("branch ", 0) == 0)  { c.branch    = parseValue("branch "); }
        else if (line.rfind("time ", 0) == 0)    { c.timestamp = parseValue("time "); }
        else if (line.rfind("message ", 0) == 0) { c.message   = parseValue("message "); }
        else if (line.rfind("files ", 0) == 0) {
            std::string countStr = parseValue("files ");
            fileCount = std::stoi(countStr);
            if (fileCount > 0) readingFiles = true;
        }
    }
    return c;
}

// ─── Compute a deterministic ID for this commit ───────────────────────────────
// Hash = SHA256(parentId + branch + timestamp + message + sorted file entries)
std::string Commit::computeId() const {
    std::ostringstream oss;
    oss << parentId << "\n" << branch << "\n" << timestamp << "\n" << message << "\n";
    for (const auto& [path, hash] : files) {   // map is already sorted
        oss << path << ":" << hash << "\n";
    }
    return HashUtility::sha256(oss.str());
}
