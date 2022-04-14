#include "lan.hpp"
#include "spdlog/spdlog.h"


int main(int argc, char *argv[]) {
    spdlog::set_pattern("[LAN] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::debug); // Set global log level to debug

    if (argc == 1) {
        // No host provided, use default
        setHost(DEFAULT_HOST);
    } else if (argc == 2) {
        // Host provided
        setHost(argv[1]);
    } else {
        spdlog::critical("Invalid invocation. First arg must be ip address of host, or omitted to use default.");
        return -10;
    }

    // if (isOn()) {
    //     sendOff();
    // } else {
    //     sendOn();
    // }
    queryPowerStatus();
    
    return 0;
}
