#include "spdlog/spdlog.h"
#include "ir.hpp"

int main(int argc, char *argv[]) {
    spdlog::set_level(spdlog::level::debug); // Set global log level to debug
    spdlog::info("Running tests...");

    char code[255];
    getIRCode(ON_COMMAND, code);
    spdlog::info("Test 1: input={} output={}", ON_COMMAND, code);

    getIRCode(STANDBY_COMMAND, code);
    spdlog::info("Test 2: input={} output={}", STANDBY_COMMAND, code);
}