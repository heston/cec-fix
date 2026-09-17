#pragma once

// Adapted from https://stackoverflow.com/a/61960339.

#include <sys/socket.h>
#include <fcntl.h>
#include <poll.h>
#include <cerrno>
#include <chrono>

// Return 1 on success, -7 on timeout, or -1 on other errors.
inline int connect_with_timeout(int sockfd, const struct sockaddr *addr,
                               socklen_t addrlen, unsigned int timeout_ms) {
    const int original_flags = fcntl(sockfd, F_GETFL, 0);
    if (original_flags < 0 || fcntl(sockfd, F_SETFL, original_flags | O_NONBLOCK) < 0) {
        return -1;
    }

    int result = 1;
    if (connect(sockfd, addr, addrlen) < 0) {
        result = -1;
        if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
            // chrono avoids overflowing a 32-bit long on the Pi with 5000 * 1000000.
            const auto deadline = std::chrono::steady_clock::now() +
                                  std::chrono::milliseconds(timeout_ms);
            for (;;) {
                const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - std::chrono::steady_clock::now()).count();
                if (remaining <= 0) {
                    errno = ETIMEDOUT;
                    result = -7;
                    break;
                }
                struct pollfd descriptor = { sockfd, POLLOUT, 0 };
                int ready = poll(&descriptor, 1, static_cast<int>(remaining));
                if (ready < 0 && errno == EINTR) {
                    continue;
                }
                if (ready == 0) {
                    errno = ETIMEDOUT;
                    result = -7;
                } else if (ready > 0) {
                    int error = 0;
                    socklen_t length = sizeof(error);
                    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &length) == 0) {
                        if (error == 0) {
                            result = 1;
                        } else {
                            errno = error;
                        }
                    }
                }
                break;
            }
        }
    }

    const int saved_errno = errno;
    if (fcntl(sockfd, F_SETFL, original_flags) < 0) {
        return -1;
    }
    errno = saved_errno;
    return result;
}
