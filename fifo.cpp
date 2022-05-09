#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include "spdlog/spdlog.h"
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include "fifo.hpp"

using namespace std;

const char * PIPE_PATH { "/tmp/p-cec-fix" };
int fifo_fd;
const int FIFO_COMMAND_SIZE { 2 };

const char OFF_COMMAND[] { "0" };
const char ON_COMMAND[] { "1" };

f_callback off_callback;
f_callback on_callback;


void registerOffCallback(f_callback callback) {
    spdlog::debug("Registering OFF callback");
    off_callback = callback;
}

void registerOnCallback(f_callback callback) {
    spdlog::debug("Registering ON callback");
    on_callback = callback;
}

/**
 * Catch SIGIO and read incoming command from pipe.
 *
 * @param   int   s  Not used
 *
 * @return  void
 */
void handleSIGIO(int s) {
    char buffer[FIFO_COMMAND_SIZE] { 0 };

    spdlog::debug("handleSIGIO called");

    if(read(fifo_fd, buffer, FIFO_COMMAND_SIZE) < 1) {
        spdlog::debug("FIFO buffer is empty");
        return;
    };

    spdlog::debug("FIFO read buffer: '{}'", buffer);

    if (strncmp(buffer, OFF_COMMAND, 1) == 0) {
        spdlog::debug("Remote OFF command received on FIFO");
        if (off_callback) {
            f_callback cb = * off_callback;
            cb();
        }
    }

    if (strncmp(buffer, ON_COMMAND, 1) == 0) {
        spdlog::debug("Remote ON command received on FIFO");
        if (on_callback) {
            f_callback cb = * on_callback;
            cb();
        }
    }
}

/**
 * Register callbacks to handle FIFO messages.
 * 
 * @param f_callback    off_callback    Callback function pointer to call when OFF message is received.
 * @param f_callback    on_callback     Callback function pointer to call when ON message is received.  
 *
 * @return  int     1 if init was successful.
 */
int initFIFO(f_callback off_callback, f_callback on_callback) {
    registerOffCallback(off_callback);
    registerOnCallback(on_callback);

    // Handle SIGIO
    struct sigaction sigIOHandler;
    sigIOHandler.sa_handler = handleSIGIO;
    sigemptyset(&sigIOHandler.sa_mask);
    sigIOHandler.sa_flags = 0;
    if(sigaction(SIGIO, &sigIOHandler, NULL) < 0) {
        spdlog::error("Error registering sigaction SIGIO");
        return -1;
    };

    mode_t mode { 0777 };

    if (mkfifo(PIPE_PATH, mode) != 0) {
        spdlog::error("Error calling mkfifo with {}: {}", PIPE_PATH, strerror(errno));
        return -1;
    };

    fifo_fd = open(PIPE_PATH, O_RDONLY | O_NONBLOCK);

    if (fifo_fd < 1) {
        spdlog::error("File descriptor for {} could not be obtained: {}.", PIPE_PATH, strerror(errno));
        return -1;
    }

    spdlog::debug("fd open at {}", fifo_fd);

    // set PID of the receiving process
    if(fcntl(fifo_fd, F_SETOWN, getpid()) != 0) {
        spdlog::error("Could not set owner on fd: {}: {}", fifo_fd, strerror(errno));
        return -1;
    }

    // enable asynchronous beahviour
    if(fcntl(fifo_fd, F_SETFL, fcntl(fifo_fd, F_GETFL) | O_ASYNC) != 0) {
        spdlog::error("Could not set O_ASYNC on fd: {}: {}", fifo_fd, strerror(errno));
        return -1;
    }

    // set the signal that is sent when the kernel tell us that there is a read/write on the fifo.
    if(fcntl(fifo_fd, F_SETSIG, SIGIO) !=0) {
        spdlog::error("Could not set signal SIGIO on fd: {}: {}", fifo_fd, strerror(errno));
        return -1;
    };

    return 1;
}

int cleanupFIFO() {
    if (close(fifo_fd) != 0) {
        spdlog::error("Could not close file descriptor {}: .", fifo_fd, strerror(errno));
        return -1;
    }

    if (remove(PIPE_PATH) != 0){
        spdlog::error("Could not remove name pipe {}: .", PIPE_PATH, strerror(errno));
        return -1;
    }
    
    return 0;
}
