#ifndef IR_H
#define IR_H

#include <stdio.h>

const std::uint16_t GPIO_1  = 28;
const std::uint16_t GPIO_2  = 3;
const std::uint16_t GPIO_3  = 5;
const std::uint16_t GPIO_4  = 7;
const std::uint16_t GPIO_5  = 29;
const std::uint16_t GPIO_6  = 31;
const std::uint16_t GPIO_7  = 26;
const std::uint16_t GPIO_8  = 24;
const std::uint16_t GPIO_9  = 21;
const std::uint16_t GPIO_10 = 19;
const std::uint16_t GPIO_11 = 23;
const std::uint16_t GPIO_12 = 32;
const std::uint16_t GPIO_13 = 33;
const std::uint16_t GPIO_14 = 8;
const std::uint16_t GPIO_15 = 10;
const std::uint16_t GPIO_16 = 36;
const std::uint16_t GPIO_17 = 11;
const std::uint16_t GPIO_18 = 12;
const std::uint16_t GPIO_19 = 35;
const std::uint16_t GPIO_20 = 38;
const std::uint16_t GPIO_21 = 40;
const std::uint16_t GPIO_22 = 15;
const std::uint16_t GPIO_23 = 16;
const std::uint16_t GPIO_24 = 18;
const std::uint16_t GPIO_25 = 22;
const std::uint16_t GPIO_26 = 37;
const std::uint16_t GPIO_27 = 13;


/**
 * Get the binary string representation of an IR command.
 *
 * @param   int   command   The IR command.
 * @param   char  *code     Character array pointer to hold the output.
 *
 * @return  int            0 if the result was written to buffer successfully.
 */
int getIRCode(int command, char * code);

/**
 * Send and IR command over the GPIO.
 *
 * @param   int  command  The IR command to send.
 *
 * @return  int           0 if successful, < 0 otherwise.
 */
int sendIRCommand(int command);

/**
 * Send ON_COMMAND to projector.
 *
 * @return  int     0 if successulf, < 0 otherwise.
 */
int turnOn();

/**
 * Send STANDBY_COMMAND to projector.
 *
 * @return  int     0 if successful, < 0 otherwise.
 */
int turnOff();

#endif
