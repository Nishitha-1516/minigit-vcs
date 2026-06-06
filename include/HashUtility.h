#pragma once
#include <string>
#include <cstdint>
#include <array>

/**
 * HashUtility - Pure SHA-256 implementation (no external crypto libs)
 * Computes SHA-256 hashes of strings and files.
 * Used throughout the system to deduplicate objects and generate commit IDs.
 */
class HashUtility {
public:
    // Hash a raw string (file contents, commit metadata, etc.)
    static std::string sha256(const std::string& data);

    // Hash a file by path (reads content, then hashes)
    static std::string hashFile(const std::string& filePath);

    // Return shortened hash (first N chars) for display
    static std::string shortHash(const std::string& fullHash, size_t len = 8);

private:
    // SHA-256 internal state
    static void transform(uint32_t state[8], const uint8_t block[64]);
    static std::string bytesToHex(const uint8_t* data, size_t len);
};
