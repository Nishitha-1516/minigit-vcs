#include "CommandParser.h"
#include <iostream>

/**
 * minigit — entry point
 *
 * Parses the command line and delegates to CommandParser::execute().
 * All error messages are printed inside the handlers; main just
 * propagates the exit code.
 */
int main(int argc, char** argv) {
    try {
        ParsedCommand cmd = CommandParser::parse(argc, argv);
        return CommandParser::execute(cmd);
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return 2;
    }
}
