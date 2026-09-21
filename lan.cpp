#include <arpa/inet.h>
#include <stdio.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <poll.h>
#include <unistd.h>
#include "spdlog/spdlog.h"
#include "spdlog/fmt/bin_to_hex.h"
#include "socket_with_timeout.h"
#include <time.h>
#include <thread>         // this_thread::sleep_for
#include <chrono>         // chrono::seconds
#include "lan.hpp"

using namespace std;

#define PORT 20554
#define OPEN "PJ_OK"
#define REQUEST "PJREQ"
#define ACK "PJACK"

char HOST[INET_ADDRSTRLEN];

const int SOCK_TIMEOUT_S = 5;
const int SOCK_TIMEOUT_MS = SOCK_TIMEOUT_S * 1000;
const int MAX_RESPONSE_SIZE = 4096;
const int MAX_RETRY_COUNT = 5;

const int POWER_QUERY_TTL_MS = 10000;

const unsigned char ON_COMMAND[] { 0x21, 0x89, 0x01, 0x50, 0x57, 0x31, 0x0A };
const unsigned char OFF_COMMAND[] { 0x21, 0x89, 0x01, 0x50, 0x57, 0x30, 0x0A };
const unsigned char QUERY_POWER_COMMAND[] { 0x3F, 0x89, 0x01, 0x50, 0x57, 0x0A };

const unsigned char NULL_COMMAND[] {0x21, 0x89, 0x01, 0x00, 0x00, 0x0A};

bool has_active_connection = false;


namespace {
using Deadline = chrono::steady_clock::time_point;

Deadline responseDeadline() {
    return chrono::steady_clock::now() + chrono::milliseconds(SOCK_TIMEOUT_MS);
}

bool waitForSocket(int sock, short events, Deadline deadline, const char* stage,
                   size_t completed, size_t expected) {
    for (;;) {
        const auto remaining = chrono::duration_cast<chrono::milliseconds>(
            deadline - chrono::steady_clock::now()).count();
        if (remaining <= 0) {
            spdlog::error("{} timed out after {}ms ({} of {} bytes)",
                          stage, SOCK_TIMEOUT_MS, completed, expected);
            return false;
        }
        struct pollfd descriptor = {sock, events, 0};
        int result = poll(&descriptor, 1, static_cast<int>(remaining));
        if (result > 0) return true; // recv/send reports EOF or socket errors.
        if (result < 0 && errno != EINTR) {
            spdlog::error("{} poll failed: {}", stage, strerror(errno));
            return false;
        }
    }
}

bool readExact(int sock, void* buffer, size_t length, Deadline deadline, const char* stage) {
    auto* bytes = static_cast<unsigned char*>(buffer);
    size_t received = 0;
    while (received < length) {
        if (!waitForSocket(sock, POLLIN, deadline, stage, received, length)) return false;
        ssize_t count = recv(sock, bytes + received, length - received, MSG_DONTWAIT);
        if (count > 0) {
            received += static_cast<size_t>(count);
        } else if (count == 0) {
            spdlog::error("Truncated {}: connection closed after {} of {} bytes", stage, received, length);
            return false;
        } else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
            spdlog::error("{} read failed: {}", stage, strerror(errno));
            return false;
        }
    }
    return true;
}

bool sendExact(int sock, const void* buffer, size_t length, const char* stage) {
    const auto* bytes = static_cast<const unsigned char*>(buffer);
    const auto deadline = responseDeadline();
    size_t sent = 0;
    while (sent < length) {
        if (!waitForSocket(sock, POLLOUT, deadline, stage, sent, length)) return false;
        ssize_t count = send(sock, bytes + sent, length - sent, MSG_DONTWAIT | MSG_NOSIGNAL);
        if (count > 0) {
            sent += static_cast<size_t>(count);
        } else if (count == 0 || (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)) {
            spdlog::error("{} send failed after {} of {} bytes: {}", stage, sent, length, strerror(errno));
            return false;
        }
    }
    return true;
}
}


int sendCommand(const char* host, const unsigned char* code, int codeLen, unsigned char* response) {
    if(has_active_connection) {
        spdlog::warn("Active connection to host already established. Only one is allowed at a time. Aborting.");
        return -9;
    }

    int sock { 0 };
    struct sockaddr_in serv_addr;

    char buffer[6] = {};
    unsigned char response_buffer[13] = {};

    int retCode { 0 };

    do {
        has_active_connection = true;

        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            spdlog::error("Socket creation error");
            retCode = -1;
            break;
        }

        const struct timeval timeout = { SOCK_TIMEOUT_S, 0 };
        if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0 ||
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            spdlog::error("Could not set socket timeouts: {}", strerror(errno));
            retCode = -4;
            break;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);

        // Convert IPv4 and IPv6 addresses from text to binary form
        if (inet_pton(AF_INET, host, &serv_addr.sin_addr) < 1) {
            spdlog::error("Invalid host address: {}", host);
            retCode = -2;
            break;
        }

        int connectRet = connect_with_timeout(
            sock,
            (struct sockaddr*)&serv_addr,
            sizeof(serv_addr),
            SOCK_TIMEOUT_MS
        );

        spdlog::debug("connect_with_timeout return code: {}", connectRet);

        if(connectRet < 1) {
            if (connectRet == -7) {
                spdlog::error("Connection to host {} timed out after {}ms", host, SOCK_TIMEOUT_MS);
            } else {
                spdlog::error("Connection to host {} could not be established. Error code {}", host, connectRet);
            }

            retCode = -3;
            break;
        }

        if (!readExact(sock, buffer, 5, responseDeadline(), "Projector greeting")) {
            retCode = -4;
            break;
        }
        if (memcmp(OPEN, buffer, 5) != 0) {
            spdlog::error("Unexpected greeting: {}", buffer);
            retCode = -5;
            break;
        }

        if (!sendExact(sock, REQUEST, 5, "Handshake request") ||
            !readExact(sock, buffer, 5, responseDeadline(), "Handshake reply")) {
            retCode = -4;
            break;
        }
        if (memcmp(ACK, buffer, 5) != 0) {
            spdlog::error("Unexpected ACK: {}", buffer);
            retCode = -5;
            break;
        }

        if (!sendExact(sock, code, codeLen, "Projector command")) {
            retCode = -4;
            break;
        }

        // ACK and status share one deadline; each fragment must not reset it.
        const auto deadline = responseDeadline();
        if (!readExact(sock, response_buffer, 6, deadline, "Command acknowledgment")) {
            retCode = -4;
            break;
        }
        if (response_buffer[0] != 0x06 || memcmp(response_buffer + 1, code + 1, 4) != 0 ||
            response_buffer[5] != 0x0A) {
            spdlog::error("Malformed command acknowledgment: {:Xpn}", spdlog::to_hex(response_buffer, response_buffer + 6));
            retCode = -5;
            break;
        }

        int response_length = 6;
        if (codeLen == sizeof(QUERY_POWER_COMMAND) &&
            memcmp(code, QUERY_POWER_COMMAND, sizeof(QUERY_POWER_COMMAND)) == 0) {
            unsigned char* status = response_buffer + 6;
            if (!readExact(sock, status, 7, deadline, "Power status reply")) {
                retCode = -4;
                break;
            }
            if (status[0] != 0x40 || memcmp(status + 1, code + 1, 4) != 0 || status[6] != 0x0A) {
                spdlog::error("Malformed power status reply: {:Xpn}", spdlog::to_hex(status, status + 7));
                retCode = -5;
                break;
            }
            response_length = 13;
        }

        spdlog::debug("Received {} bytes from host: {:Xpn}", response_length,
                      spdlog::to_hex(response_buffer, response_buffer + response_length));
        memcpy(response, response_buffer, response_length);
        retCode = response_length;
    } while (0);

    close(sock);
    // Wait for host to close other end
    this_thread::sleep_for(chrono::seconds(1));
    has_active_connection = false;
    return retCode;
}

int sendCommandWithRetry(const char* host, const unsigned char* code, int codeLen, unsigned char* response) {
    int retCode { -1 };
    int retry { 0 };
    while (retCode < 0 && retry < MAX_RETRY_COUNT) {
        spdlog::debug("sendCommandWithRetry attempt {} of {}", retry + 1, MAX_RETRY_COUNT);
        retCode = sendCommand(host, code, codeLen, response);
        retry++;
    }

    return retCode;
}

struct timespec lastPowerQuery;
int lastPowerQueryResult = -1;

void queryPowerStatusCacheClear() {
    lastPowerQueryResult = -1;
}

int queryPowerStatus() {
    spdlog::info("Sending QUERY_POWER_COMMAND to host");
    char unsigned response[MAX_RESPONSE_SIZE] { 0 };
    const int cmdSize = sizeof(QUERY_POWER_COMMAND);
    int ret = sendCommandWithRetry(HOST, QUERY_POWER_COMMAND, cmdSize, response);
    if(ret < 0) {
        spdlog::error("Error communicating with host: {}", ret);
    } else {
        const unsigned char status = response[11];
        if (ret == 13 && status >= '0' && status <= '4') {
            static const char* const status_names[] = {
                "STANDBY", "POWER_ON", "COOLING", "WARMING", "EMERGENCY"
            };
            spdlog::debug("Power status is {}", status_names[status - '0']);
            return status - '0';
        }
        spdlog::error("Unknown power status value: 0x{:02X}", status);
    }
    return -1;
}

int queryPowerStatusCached() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if(lastPowerQueryResult == -1) {
        lastPowerQueryResult = queryPowerStatus();
        lastPowerQuery.tv_sec = now.tv_sec;
        lastPowerQuery.tv_nsec = now.tv_nsec;
        return lastPowerQueryResult;
    }

    int ms_elapsed = (int)((now.tv_sec - lastPowerQuery.tv_sec) * 1000l
                         + (now.tv_nsec - lastPowerQuery.tv_nsec) / 1000000l);

    if(ms_elapsed >= POWER_QUERY_TTL_MS) {
        lastPowerQueryResult = queryPowerStatus();
        lastPowerQuery.tv_sec = now.tv_sec;
        lastPowerQuery.tv_nsec = now.tv_nsec;
    }

    spdlog::debug("Returning cached power status: {}", lastPowerQueryResult);
    return lastPowerQueryResult;
}

int sendOn() {
    spdlog::info("Sending ON_COMMAND to host");
    unsigned char response[MAX_RESPONSE_SIZE] { 0 };
    const int cmdSize = sizeof(ON_COMMAND);
    int ret = sendCommandWithRetry(HOST, ON_COMMAND, cmdSize, response);
    if(ret < 0) {
        spdlog::error("Error communicating with host: {}", ret);
        return ret;;
    }
    queryPowerStatusCacheClear();
    return 0;
}

int sendOff() {
    spdlog::info("Sending OFF_COMMAND to host");
    unsigned char response[MAX_RESPONSE_SIZE] { 0 };
    const int cmdSize = sizeof(OFF_COMMAND);
    int ret = sendCommandWithRetry(HOST, OFF_COMMAND, cmdSize, response);
    if(ret < 0) {
        spdlog::error("Error communicating with host: {}", ret);
        return ret;
    }
    queryPowerStatusCacheClear();
    return 0;
}

int sendNull() {
    spdlog::info("Sending NULL_COMMAND to host");
    unsigned char response[MAX_RESPONSE_SIZE] { 0 };
    const int cmdSize = sizeof(NULL_COMMAND);
    int ret = sendCommandWithRetry(HOST, NULL_COMMAND, cmdSize, response);
    if(ret < 0) {
        spdlog::error("Error communicating with host: {}", ret);
        return ret;
    }
    return 0;
}

bool isOn() {
    int status = queryPowerStatusCached();
    if (status < 0) {
        throw runtime_error("Networking error: " + std::to_string(status));
    }

    return status == 1 || status == 3;
}

bool isOff() {
    int status = queryPowerStatusCached();
    if (status < 0) {
        throw runtime_error("Networking error: " + std::to_string(status));
    }

    return status == 0 || status == 2;
}

void setHost(const char * host) {
    if (strlen(host) >= sizeof(HOST)) {
        throw invalid_argument("Projector IPv4 address is too long");
    }
    strcpy(HOST, host);
}

void setHost(char * host) {
    setHost(static_cast<const char*>(host));
}
