#pragma once
#include <string>

/**
 * ObjectStore - Content-addressed storage for file blobs.
 *
 * Design:
 *   Every unique file content is stored exactly ONCE under:
 *     .minigit/objects/<first2chars>/<remaining62chars>
 *
 *   This mirrors Git's loose object layout. The 2-char prefix
 *   directory keeps inode counts manageable in large repos.
 *
 *   Deduplication is automatic: storing the same content twice
 *   is a no-op because the key IS the SHA-256 of the content.
 *
 * Public API:
 *   storeObject(content)  → returns hash, writes to disk if new
 *   loadObject(hash)      → returns raw content string
 *   objectExists(hash)    → fast existence check (no I/O beyond stat)
 *   objectPath(hash)      → full path for external use
 */
class ObjectStore {
public:
    explicit ObjectStore(const std::string& repoRoot);

    // Store raw content; returns its SHA-256 hash.
    // Idempotent: safe to call multiple times for the same content.
    std::string storeObject(const std::string& content);

    // Store a file from disk; returns its hash.
    std::string storeFile(const std::string& filePath);

    // Load object content by hash. Throws if not found.
    std::string loadObject(const std::string& hash) const;

    // Check existence without loading content.
    bool objectExists(const std::string& hash) const;

    // Full filesystem path for a given hash.
    std::string objectPath(const std::string& hash) const;

    // Restore a stored object back to an arbitrary file path.
    bool restoreObject(const std::string& hash, const std::string& destPath) const;

private:
    std::string objectsDir_;   // .minigit/objects/
};
