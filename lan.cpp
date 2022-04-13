// #define STANDALONE 1
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "spdlog/spdlog.h"
#include "socket_with_timeout.h"
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
const unsigned char ON_COMMAND[] { 0x21, 0x89, 0x01, 0x50, 0x57, 0x31, 0x0A };
const unsigned char OFF_COMMAND[] { 0x21, 0x89, 0x01, 0x50, 0x57, 0x30, 0x0A };
const unsigned char QUERY_POWER_COMMAND[] { 0x3F, 0x89, 0x01, 0x50, 0x57, 0x0A };
const unsigned char NULL_COMMAND[] {0x21, 0x89, 0x01, 0x00, 0x00, 0x0A};


int sendCommand(const char* host, const unsigned char* code, int codeLen, char* response) {
    int sock { 0 };
    struct sockaddr_in serv_addr;

    char buffer[MAX_RESPONSE_SIZE] { 0 };
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

    // Clear buffer
    memset(buffer, 0, sizeof(buffer));

    if(read(sock, buffer, 4096) == -1) {
        spdlog::error("Socket read error");
        return -4;
    }
    spdlog::debug("Received response from host: {}", buffer);

    strcpy(response, buffer);

    close(sock);
    return 0;
}

int sendOn() {
    spdlog::info("Sending ON_COMMAND to host");
    char response[MAX_RESPONSE_SIZE] { 0 };
    const int cmdSize = sizeof(ON_COMMAND);
    int ret = sendCommand(HOST, ON_COMMAND, cmdSize, response);
    if(ret < 0) {
        spdlog::error("Error communicating with host: {}", ret);
    }
    spdlog::info("Got reply from host: {}", response);
    return ret;
}

int sendOff() {
    spdlog::info("Sending ON_COMMAND to host");
    char response[MAX_RESPONSE_SIZE] { 0 };
    const int cmdSize = sizeof(OFF_COMMAND);
    int ret = sendCommand(HOST, OFF_COMMAND, cmdSize, response);
    if(ret < 0) {
        spdlog::error("Error communicating with host: {}", ret);
    }
    spdlog::info("Got reply from host: {}", response);
    return ret;
}

int sendNull() {
    spdlog::info("Sending NULL_COMMAND to host");
    char response[MAX_RESPONSE_SIZE] { 0 };
    const int cmdSize = sizeof(NULL_COMMAND);
    int ret = sendCommand(HOST, NULL_COMMAND, cmdSize, response);
    if(ret < 0) {
        spdlog::error("Error communicating with host: {}", ret);
    } else {
        spdlog::info("Got reply from host: {}", response);
    }
    return ret;
}

// TODO: query power status and return result

void setHost(const char * host) {
    strcpy(HOST, host);
}

#ifdef STANDALONE
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

    return sendNull();
}
#endif
