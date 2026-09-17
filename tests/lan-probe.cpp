#include <cstdio>
#include <cstring>
#include "lan.hpp"

int sendCommand(const char*, const unsigned char*, int, unsigned char*);

int main(int argc, char** argv) {
    const char* mode = argc > 1 ? argv[1] : "query";
    setHost("127.0.0.1");
    int result;
    if (std::strcmp(mode, "status") == 0) {
        result = queryPowerStatus();
    } else {
        const unsigned char query[] = {0x3f, 0x89, 0x01, 0x50, 0x57, 0x0a};
        const unsigned char on[] = {0x21, 0x89, 0x01, 0x50, 0x57, 0x31, 0x0a};
        const unsigned char off[] = {0x21, 0x89, 0x01, 0x50, 0x57, 0x30, 0x0a};
        const unsigned char null_command[] = {0x21, 0x89, 0x01, 0, 0, 0x0a};
        const unsigned char* command = query;
        int length = sizeof(query);
        if (std::strcmp(mode, "on") == 0) { command = on; length = sizeof(on); }
        if (std::strcmp(mode, "off") == 0) { command = off; length = sizeof(off); }
        if (std::strcmp(mode, "null") == 0) { command = null_command; length = sizeof(null_command); }
        unsigned char response[4096] = {};
        result = sendCommand("127.0.0.1", command, length, response);
    }
    std::printf("result=%d\n", result);
    return result < 0 ? 1 : 0;
}
