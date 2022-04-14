#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
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

char HOST[15];

const int SOCK_TIMEOUT_S = 5;
const int SOCK_TIMEOUT_MS = SOCK_TIMEOUT_S * 1000;
const int MAX_RESPONSE_SIZE = 4096;

const int POWER_QUERY_TTL_MS = 10000;

const unsigned char ON_COMMAND[] { 0x21, 0x89, 0x01, 0x50, 0x57, 0x31, 0x0A };
const unsigned char OFF_COMMAND[] { 0x21, 0x89, 0x01, 0x50, 0x57, 0x30, 0x0A };
const unsigned char ON_OFF_ACK[] { 0x06, 0x89, 0x01, 0x50, 0x57, 0x0A };

const unsigned char QUERY_POWER_COMMAND[] { 0x3F, 0x89, 0x01, 0x50, 0x57, 0x0A };
const unsigned char STANDBY_ACK[] { 0x06, 0x89, 0x01, 0x50, 0x57, 0x0A, 0x40, 0x89, 0x01, 0x50, 0x57, 0x30, 0x0A };
const unsigned char POWER_ON_ACK[] { 0x06, 0x89, 0x01, 0x50, 0x57, 0x0A, 0x40, 0x89, 0x01, 0x50, 0x57, 0x31, 0x0A };
const unsigned char COOLING_ACK[] { 0x06, 0x89, 0x01, 0x50, 0x57, 0x0A, 0x40, 0x89, 0x01, 0x50, 0x57, 0x32, 0x0A };
const unsigned char WARMING_ACK[] { 0x06, 0x89, 0x01, 0x50, 0x57, 0x0A, 0x40, 0x89, 0x01, 0x50, 0x57, 0x33, 0x0A };
const unsigned char EMERGENCY_ACK[] { 0x06, 0x89, 0x01, 0x50, 0x57, 0x0A, 0x40, 0x89, 0x01, 0x50, 0x57, 0x34, 0x0A };

const unsigned char NULL_COMMAND[] {0x21, 0x89, 0x01, 0x00, 0x00, 0x0A};


int sendCommand(const char* host, const unsigned char* code, int codeLen, unsigned char* response) {
    int sock { 0 };
    struct sockaddr_in serv_addr;

    char buffer[MAX_RESPONSE_SIZE] { 0 };
    std::array<unsigned char, MAX_RESPONSE_SIZE> response_buffer;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        spdlog::error("Socket creation error");
        return -1;
    }

    const void* timeout = &SOCK_TIMEOUT_S;
    socklen_t len = sizeof(int);
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, timeout, len);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, timeout, len);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, host, &serv_addr.sin_addr) < 1) {
        spdlog::error("Invalid host address: {}", host);
        return -2;
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

        return -3;
    }

    if(read(sock, buffer, 4096) == -1) {
        spdlog::error("Socket read error");
        return -4;
    };

    if (strcmp(OPEN, buffer) != 0) {
        spdlog::error("Unexpected greeting: {}", buffer);
        return -5;
    }

    // Clear buffer
    memset(buffer, 0, sizeof(buffer));
    send(sock, REQUEST, strlen(REQUEST), 0);

    if(read(sock, buffer, 4096) == -1) {
        spdlog::error("Socket read error");
        return -4;
    }

    if (strcmp(ACK, buffer) != 0) {
        spdlog::error("Unexpected ACK: {}", buffer);
        return -5;
    }

    send(sock, code, codeLen, 0);

    ssize_t respLen = read(sock, static_cast<void *>(&response_buffer), 4096);
    if( respLen == -1) {
        spdlog::error("Socket read error");
        return -4;
    }

    spdlog::debug(
        "Received {} bytes from host: {:Xpn}",
        respLen,
        spdlog::to_hex(std::begin(response_buffer), std::begin(response_buffer) + respLen)
    );
    memcpy(response, response_buffer.data(), respLen);
    close(sock);
    // Wait for host to close other end
    this_thread::sleep_for(chrono::seconds(1));
    return respLen;
}

int sendOn() {
    spdlog::info("Sending ON_COMMAND to host");
    unsigned char response[MAX_RESPONSE_SIZE] { 0 };
    const int cmdSize = sizeof(ON_COMMAND);
    int ret = sendCommand(HOST, ON_COMMAND, cmdSize, response);
    if(ret < 0) {
        spdlog::error("Error communicating with host: {}", ret);
        return ret;;
    }
    return 0;
}

int sendOff() {
    spdlog::info("Sending OFF_COMMAND to host");
    unsigned char response[MAX_RESPONSE_SIZE] { 0 };
    const int cmdSize = sizeof(OFF_COMMAND);
    int ret = sendCommand(HOST, OFF_COMMAND, cmdSize, response);
    if(ret < 0) {
        spdlog::error("Error communicating with host: {}", ret);
        return ret;
    }
    return 0;
}

int sendNull() {
    spdlog::info("Sending NULL_COMMAND to host");
    unsigned char response[MAX_RESPONSE_SIZE] { 0 };
    const int cmdSize = sizeof(NULL_COMMAND);
    int ret = sendCommand(HOST, NULL_COMMAND, cmdSize, response);
    if(ret < 0) {
        spdlog::error("Error communicating with host: {}", ret);
        return ret;
    }
    return 0;
}

struct timespec lastPowerQuery;
int lastPowerQueryResult = -1;

int queryPowerStatusCached() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if(lastPowerQueryResult == -1) {
        lastPowerQueryResult = queryPowerStatus();
        lastPowerQuery.tv_sec = now.tv_sec;
        lastPowerQuery.tv_nsec = now.tv_nsec;
        return lastPowerQueryResult;
    }

    int ms_elapsed = (int)((lastPowerQuery.tv_sec - now.tv_sec) * 1000l
                         + (lastPowerQuery.tv_nsec - now.tv_nsec) / 1000000l);

    if(ms_elapsed >= POWER_QUERY_TTL_MS) {
        lastPowerQueryResult = queryPowerStatus();
        lastPowerQuery.tv_sec = now.tv_sec;
        lastPowerQuery.tv_nsec = now.tv_nsec;
    }

    spdlog::debug("Returning cached power status: {}", lastPowerQueryResult);
    return lastPowerQueryResult;
}

int queryPowerStatus() {
    spdlog::info("Sending QUERY_POWER_COMMAND to host");
    char unsigned response[MAX_RESPONSE_SIZE] { 0 };
    const int cmdSize = sizeof(QUERY_POWER_COMMAND);
    int ret = sendCommand(HOST, QUERY_POWER_COMMAND, cmdSize, response);
    if(ret < 0) {
        spdlog::error("Error communicating with host: {}", ret);
    } else {
        if(memcmp(response, STANDBY_ACK, sizeof(STANDBY_ACK)) == 0) {
            spdlog::debug("Power status is STANDBY");
            return 0;
        }
        if(memcmp(response, POWER_ON_ACK, sizeof(POWER_ON_ACK)) == 0) {
            spdlog::debug("Power status is POWER_ON");
            return 1;
        }
        if(memcmp(response, COOLING_ACK, sizeof(COOLING_ACK)) == 0) {
            spdlog::debug("Power status is COOLING");
            return 2;
        }
        if(memcmp(response, WARMING_ACK, sizeof(WARMING_ACK)) == 0) {
            spdlog::debug("Power status is WARMING");
            return 3;
        }
        if(memcmp(response, EMERGENCY_ACK, sizeof(EMERGENCY_ACK)) == 0) {
            spdlog::debug("Power status is EMERGENCY");
            return 4;
        }
        spdlog::warn("Unknown power status encountered");
    }
    return -1;
}

bool isOn() {
    int status = queryPowerStatusCached();
    return status == 1 || status == 3;
}

bool isOff() {
    int status = queryPowerStatusCached();
    return status == 0 || status == 2;
}

void setHost(const char * host) {
    strcpy(HOST, host);
}

void setHost(char * host) {
    strcpy(HOST, host);
}
