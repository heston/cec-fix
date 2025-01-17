// All credit goes to https://stackoverflow.com/a/61960339
#include <sys/socket.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include "spdlog/spdlog.h"


int connect_with_timeout(int sockfd, const struct sockaddr *addr, socklen_t addrlen, unsigned int timeout_ms) {
    int rc = 0;
    // Set O_NONBLOCK
    int sockfd_flags_before;
    if((sockfd_flags_before = fcntl(sockfd, F_GETFL, 0) < 0))
        return -9;
    if(fcntl(sockfd, F_SETFL, sockfd_flags_before | O_NONBLOCK) < 0)
        return -9;

    // Start connecting (asynchronously)
    do {
        if (connect(sockfd, addr, addrlen) < 0) {
            // Did connect return an error? If so, we'll fail.
            if ((errno != EWOULDBLOCK) && (errno != EINPROGRESS)) {
                rc = -1;
            } else {  // Otherwise, we'll wait for it to complete.
                // Set a deadline timestamp 'timeout' ms from now (needed b/c poll can be interrupted)
                struct timespec now;
                if(clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
                    rc = -1;
                    break;
                }

                struct timespec deadline = { .tv_sec = now.tv_sec,
                                             .tv_nsec = now.tv_nsec + timeout_ms * 1000000l};
                // Wait for the connection to complete.
                do {
                    // Calculate how long until the deadline
                    if(clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
                        rc = -1;
                        break;
                    }
                    int ms_until_deadline = (int)(  (deadline.tv_sec  - now.tv_sec) * 1000l
                                                  + (deadline.tv_nsec - now.tv_nsec) / 1000000l);
                    if(ms_until_deadline < 0) {
                        rc = 0;
                        break;
                    }

                    // Wait for connect to complete (or for the timeout deadline)
                    struct pollfd pfds[] = { { .fd = sockfd, .events = POLLOUT } };
                    rc = poll(pfds, 1, ms_until_deadline);
                    spdlog::debug("poll return: {}", rc);
                    // If poll 'succeeded', make sure it *really* succeeded
                    if(rc > 0) {
                        int error = 0; socklen_t len = sizeof(error);
                        int retval = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len);
                        spdlog::debug("SO_ERROR on sockfd {}: {}", sockfd, error);
                        if(retval == 0)
                            errno = error;
                        if(error != 0)
                            rc = -1;
                    }
                } while(rc == -1 && errno == EINTR);  // If poll was interrupted, try again.
                // Did poll timeout? If so, fail.
                if(rc == 0) {
                    errno = ETIMEDOUT;
                    rc = -1;
                }
            }
        }
    } while(0);
    // Restore original O_NONBLOCK state
    if(fcntl(sockfd, F_SETFL, sockfd_flags_before) < 0)
        return -1;

    // Success
    return rc;
}
