#include <cstdio>
#include "lan.hpp"

int sendCommand(const char*, const unsigned char*, int, unsigned char*);

int main() {
    const unsigned char query[] = {0x3f, 0x89, 0x01, 0x50, 0x57, 0x0a};
    unsigned char response[4096] = {};
    int result = sendCommand("127.0.0.1", query, sizeof(query), response);
    std::printf("result=%d\n", result);
    return result < 0 ? 1 : 0;
}
