#pragma once
#include <string>
#include <map>
#include <vector>

/**
 * Commit - Plain data object representing one point in history.
 *
 * A commit records:
 *   - A unique SHA-256 ID derived from its own content
 *   - The parent commit ID (empty string for the initial commit)
 *   - The author message
 *   - A timestamp
 *   - A snapshot: map from relative file path → object hash (SHA-256 of contents)
 *   - The branch name at creation time
 *
 * Serialisation format (stored in .minigit/commits/<id>):
 *
 *   id       <sha256>
 *   parent   <sha256 | "">
 *   branch   <name>
 *   time     <YYYY-MM-DD HH:MM:SS>
 *   message  <text>
 *   files    <N>
 *   <path1>  <hash1>
 *   <path2>  <hash2>
 *   ...
 */
struct Commit {
    std::string id;                          // full SHA-256
    std::string parentId;                    // "" for root commit
    std::string message;
    std::string timestamp;
    std::string branch;
    std::map<std::string, std::string> files; // path → content hash

    // Serialise to string (written to disk)
    std::string serialise() const;

    // Deserialise from string (read from disk)
    static Commit deserialise(const std::string& raw);

    // Build a deterministic hash from this commit's metadata + file snapshot
    // Call AFTER filling all fields except `id`
    std::string computeId() const;

    bool isValid() const { return !id.empty(); }
};
