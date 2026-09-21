#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>
#include "fifo.hpp"
#include "spdlog/spdlog.h"

namespace {
const char* PIPE_PATH = "/tmp/p-cec-fix";
int fifo_fd = -1;
f_callback off_callback = nullptr;
f_callback on_callback = nullptr;
}

void registerOffCallback(f_callback callback) { off_callback = callback; }
void registerOnCallback(f_callback callback) { on_callback = callback; }

int initFIFO(f_callback off, f_callback on) {
    registerOffCallback(off);
    registerOnCallback(on);
    if (mkfifo(PIPE_PATH, 0777) != 0) {
        // A killed process can leave its pipe behind. Never reuse a regular file or symlink.
        struct stat info;
        if (errno != EEXIST || lstat(PIPE_PATH, &info) != 0 ||
            !S_ISFIFO(info.st_mode) || info.st_uid != geteuid()) {
            spdlog::error("Could not create FIFO {}: {}", PIPE_PATH, strerror(errno));
            return -1;
        }
    }

    // Keeping a write end open prevents POLLHUP spinning when no client is connected.
    fifo_fd = open(PIPE_PATH, O_RDWR | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
    if (fifo_fd < 0) {
        spdlog::error("Could not open FIFO {}: {}", PIPE_PATH, strerror(errno));
        return -1;
    }
    struct stat info;
    if (fstat(fifo_fd, &info) != 0 || !S_ISFIFO(info.st_mode) || info.st_uid != geteuid()) {
        spdlog::error("Opened path is not an owned FIFO: {}", PIPE_PATH);
        close(fifo_fd);
        fifo_fd = -1;
        return -1;
    }
    return 1;
}

int processFIFO(int timeout_ms) {
    struct pollfd descriptor = { fifo_fd, POLLIN, 0 };
    int ready = poll(&descriptor, 1, timeout_ms);
    if (ready < 0) {
        if (errno == EINTR) return 0;
        spdlog::error("FIFO poll failed: {}", strerror(errno));
        return -1;
    }
    if (ready == 0) return 0;
    if (descriptor.revents & (POLLERR | POLLNVAL)) return -1;

    // Dispatch at most one command per main-loop iteration so CEC gets a turn too.
    char buffer[1];
    ssize_t count = read(fifo_fd, buffer, sizeof(buffer));
    if (count < 0) {
        if (errno == EAGAIN || errno == EINTR) return 0;
        spdlog::error("FIFO read failed: {}", strerror(errno));
        return -1;
    }
    for (ssize_t i = 0; i < count; ++i) {
        if (buffer[i] == '0' && off_callback) off_callback();
        else if (buffer[i] == '1' && on_callback) on_callback();
        else if (buffer[i] != '\n' && buffer[i] != '\r' && buffer[i] != '\0') {
            spdlog::warn("Unknown FIFO command byte: {}", static_cast<unsigned char>(buffer[i]));
        }
    }
    return 0;
}

int cleanupFIFO() {
    if (fifo_fd < 0) return 0;
    int result = close(fifo_fd);
    fifo_fd = -1;
    if (unlink(PIPE_PATH) != 0) result = -1;
    return result;
}
