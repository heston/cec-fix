#include <stdio.h>
#include <string.h>
#include <bitset>
#include "irslinger.h"
#include "ir.hpp"
#include "spdlog/spdlog.h"


using namespace std;

int outPin = GPIO::PIN19;        // The Broadcom pin number the signal will be sent on
int frequency = 37900;           // The frequency of the IR signal in Hz
double dutyCycle = 0.33;         // The duty cycle of the IR signal.
int leadingPulseDuration = 8440; // The duration of the beginning pulse in microseconds
int leadingGapDuration = 4220;   // The duration of the gap in microseconds after the leading pulse
int onePulse = 527;              // The duration of a pulse in microseconds when sending a logical 1
int zeroPulse = 527;             // The duration of a pulse in microseconds when sending a logical 0
int oneGap = 1583;               // The duration of the gap in microseconds when sending a logical 1
int zeroGap = 528;               // The duration of the gap in microseconds when sending a logical 0
int sendTrailingPulse = 1;       // 1 = Send a trailing pulse with duration equal to "onePulse"
int repeatCount = 7;
int maxCommandSize = 255;

int ADDRESS = 0xCE;  // Address of JVC NX7 projector
int ON_COMMAND = 0xA0;  // Command to turn on
int STANDBY_COMMAND = 0x60;  // Command to turn off

int getIRCode(int command, char * code) {
    // A valid IR code should repeat the command three times,
    // padding the end of each command with enough space to fill the frame.
    string address = bitset<8>(ADDRESS).to_string();
    string cmd = bitset<8>(command).to_string();
    string binary = address + cmd;

    spdlog::debug("Encoded: address={} command={} binary={}", ADDRESS, command, binary);

    size_t numOnes = 0;
    for (size_t i = 0, max = binary.length(); i < max; i++) {
        if (binary[i] == '1') {
            numOnes++;
        }
    }
    
    int zeroLen = zeroPulse + zeroGap;
    int space = (46420 - 16880 - 5270 - (numOnes * zeroLen));
    int num_zeros = space / zeroLen;

    for (int i = 0; i < num_zeros; i++) {
        binary += "0";
    }

    spdlog::debug("Padding: space={} numZeros={} binary={}", space, num_zeros, binary);

    string output = "";
    for (int i = 0; i < repeatCount - 1; i++) {
        output += binary;
    }

    output += address + cmd;
    if (output.length() > maxCommandSize) {
        spdlog::error(
            "getIRCode error: output length ({}) would exceed max length of buffer ({})",
            output.length(),
            maxCommandSize
        );
        return 1;
    }
    
    strcpy(code, output.c_str());

    return 0;
}

int sendIRCommand(int command) {
    char code[maxCommandSize];
    int ret = getIRCode(command, code);
    if (ret != 0) {
        spdlog::error("getIRCode error: {}", ret);
        return ret;
    }

    spdlog::debug(
        "irSling: outPin={} frequency={} dutyCycle={} leadingPulseDuration={} "
        "leadingGapDuration={} onePulse={} zeroPulse={} oneGap={} zeroGap={} sendTrailingPulse={} "
        "code={}",
        outPin, frequency, dutyCycle, leadingPulseDuration, leadingGapDuration, onePulse,
        zeroPulse, oneGap, zeroGap, sendTrailingPulse, code
    );

    ret = irSling(
        outPin,
        frequency,
        dutyCycle,
        leadingPulseDuration,
        leadingGapDuration,
        onePulse,
        zeroPulse,
        oneGap,
        zeroGap,
        sendTrailingPulse,
        code
    );
    spdlog::debug("sendIRCommand({}) result: {}", command, ret);
    return ret;
}

int turnOn() {
    int ret = sendIRCommand(ON_COMMAND);
    spdlog::debug("sendIRCommand(ON_COMMAND) result: {}", ret);
    return ret;
}

int turnOff() {
    int ret = sendIRCommand(STANDBY_COMMAND);
    spdlog::debug("sendIRCommand(STANDBY_COMMAND) result: {}", ret);
    return ret;
}
