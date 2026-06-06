#pragma once
#include <string>
#include <vector>
#include <map>

/**
 * CommandParser - Parses argv and dispatches to the correct handler.
 *
 * Each handler receives:
 *   - The parsed positional arguments
 *   - A map of flags/options (e.g. {"-m" → "commit message"})
 *   - The resolved repository root path
 *
 * Supported commands:
 *   init
 *   add <file> [<file>…]
 *   commit -m <message>
 *   status
 *   log [--oneline]
 *   checkout <commit-id|branch>
 *   branch [<name>]
 *   diff [<commit1> [<commit2>]]
 *   restore <file>
 *   help
 */
struct ParsedCommand {
    std::string              verb;       // e.g. "add", "commit"
    std::vector<std::string> args;       // positional arguments after the verb
    std::map<std::string, std::string> flags; // e.g. {"-m" → "my message"}
};

class CommandParser {
public:
    // Parse argv[1..] into a ParsedCommand.
    static ParsedCommand parse(int argc, char** argv);

    // Execute the parsed command.
    // Returns 0 on success, non-zero on error.
    static int execute(const ParsedCommand& cmd);

    // Print usage to stdout.
    static void printHelp();

private:
    // Individual command handlers — each returns 0/non-zero exit code.
    static int handleInit   (const ParsedCommand& cmd);
    static int handleAdd    (const ParsedCommand& cmd, const std::string& root);
    static int handleCommit (const ParsedCommand& cmd, const std::string& root);
    static int handleStatus (const ParsedCommand& cmd, const std::string& root);
    static int handleLog    (const ParsedCommand& cmd, const std::string& root);
    static int handleCheckout(const ParsedCommand& cmd, const std::string& root);
    static int handleBranch (const ParsedCommand& cmd, const std::string& root);
    static int handleDiff   (const ParsedCommand& cmd, const std::string& root);
    static int handleRestore(const ParsedCommand& cmd, const std::string& root);

    // Shared helpers
    static std::string requireRepo(); // finds root or prints error and exits
};
