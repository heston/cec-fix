#include <stdio.h>
#include <string.h>
#include <bitset>
#include "irslinger.h"
#include "ir.hpp"
#include "spdlog/spdlog.h"


using namespace std;

const int outPin = GPIO::PIN19;        // The Broadcom pin number the signal will be sent on
const int frequency = 37900;           // The frequency of the IR signal in Hz
const double dutyCycle = 0.33;         // The duty cycle of the IR signal.
const int leadingPulseDuration = 8440; // The duration of the beginning pulse in microseconds
const int leadingGapDuration = 4220;   // The duration of the gap in microseconds after the leading pulse
const int onePulse = 527;              // The duration of a pulse in microseconds when sending a logical 1
const int zeroPulse = 527;             // The duration of a pulse in microseconds when sending a logical 0
const int oneGap = 1583;               // The duration of the gap in microseconds when sending a logical 1
const int zeroGap = 528;               // The duration of the gap in microseconds when sending a logical 0
const int sendTrailingPulse = 0;       // 1 = Send a trailing pulse with duration equal to "onePulse"
const int repeatCount = 10;
const int maxCommandSize = 512;

const int ADDRESS = 0xCE;  // Address of JVC NX7 projector
const int ON_COMMAND = 0xA0;  // Command to turn on
const int STANDBY_COMMAND = 0x60;  // Command to turn off

int getIRCode(int command, char * code) {
    // A valid IR code should repeat the command repeatCount times,
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
    // http://support.jvc.com/consumer/support/documents/RemoteCodes.pdf
    int space = (46420 - 16880 - 527 - (numOnes * zeroLen));
    int num_zeros = space / zeroLen;

    for (int i = 0; i < num_zeros; i++) {
        // Special code indicating only a gap should be transmitted.
        // Duration is zeroPulse + zeroGap (a 0 pulse interval).
        binary += "-";
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
