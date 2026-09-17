#include <sys/socket.h>
#include <fcntl.h>
#include <poll.h>
#include <cerrno>
#include <chrono>
#include <cassert>
#include <cstdarg>
#include <cstdio>

namespace {
int flags, restored_flags, polls;
int connect_result, connect_error, poll_result, socket_error;
bool fail_getsockopt, interrupt_poll;

int fake_fcntl(int, int command, ...) {
    if (command == F_GETFL) return flags;
    va_list args;
    va_start(args, command);
    restored_flags = va_arg(args, int);
    va_end(args);
    return 0;
}
int fake_connect(int, const sockaddr*, socklen_t) {
    errno = connect_error;
    return connect_result;
}
int fake_poll(pollfd*, nfds_t, int timeout) {
    assert(timeout > 4500 && timeout <= 5000);
    ++polls;
    if (interrupt_poll && polls == 1) {
        errno = EINTR;
        return -1;
    }
    return poll_result;
}
int fake_getsockopt(int, int, int, void* value, socklen_t*) {
    if (fail_getsockopt) {
        errno = EBADF;
        return -1;
    }
    *static_cast<int*>(value) = socket_error;
    return 0;
}
void reset() {
    flags = O_NONBLOCK;
    restored_flags = -1;
    polls = 0;
    connect_result = -1;
    connect_error = EINPROGRESS;
    poll_result = 1;
    socket_error = 0;
    fail_getsockopt = interrupt_poll = false;
}
}

#define fcntl fake_fcntl
#define connect fake_connect
#define poll fake_poll
#define getsockopt fake_getsockopt
#include "socket_with_timeout.h"
#undef fcntl
#undef connect
#undef poll
#undef getsockopt

int main() {
    reset();
    connect_result = 0;
    assert(connect_with_timeout(3, nullptr, 0, 5000) == 1);
    assert(polls == 0 && restored_flags == flags);
    reset();
    assert(connect_with_timeout(3, nullptr, 0, 5000) == 1);
    assert(polls == 1 && restored_flags == flags);
    reset();
    poll_result = 0;
    assert(connect_with_timeout(3, nullptr, 0, 5000) == -7);
    assert(errno == ETIMEDOUT && restored_flags == flags);
    reset();
    socket_error = ECONNREFUSED;
    assert(connect_with_timeout(3, nullptr, 0, 5000) == -1);
    assert(errno == ECONNREFUSED && restored_flags == flags);
    reset();
    fail_getsockopt = true;
    assert(connect_with_timeout(3, nullptr, 0, 5000) == -1);
    assert(errno == EBADF && restored_flags == flags);
    reset();
    interrupt_poll = true;
    assert(connect_with_timeout(3, nullptr, 0, 5000) == 1);
    assert(polls == 2 && restored_flags == flags);
    std::puts("Connect timeout regression tests passed");
}
