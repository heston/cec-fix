#include "fifo.hpp"
#include "spdlog/spdlog.h"
#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

bool want_run = true;


int onCallback() {
    spdlog::info("onCallback called");
    return 1;
}

int offCallback() {
    spdlog::info("offCallback called");
    return 1;
}

/**
 * Catch SIGINT and set running state of program to false.
 *
 * @param   int   s  Not used
 *
 * @return  void
 */
void handleSIGINT(int s) {
    want_run = false;
}

int main(int argc, char *argv[]) {
    spdlog::set_pattern("[FIFO] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::debug); // Set global log level to debug

    int ret = 0;
    do {
        f_callback offCallbackPtr = &offCallback;
        f_callback onCallbackPtr = &onCallback;
        if(initFIFO(offCallbackPtr, onCallbackPtr) < 0) {
            spdlog::error("initFIFO failed to initialize!");
            ret = 1;
            break;
        };

        // Handle SIGINT cleanly
        struct sigaction sigIntHandler;
        sigIntHandler.sa_handler = handleSIGINT;
        sigemptyset(&sigIntHandler.sa_mask);
        sigIntHandler.sa_flags = 0;
        sigaction(SIGINT, &sigIntHandler, NULL);

        spdlog::info("Running! Press CTRL-c to exit.");

        while (want_run) {
            pause();
        }
    } while (0);

    int cleanup_ret = cleanupFIFO();

    if (ret > 0) {
        return ret;
    } else {
        return cleanup_ret;
    }
}
