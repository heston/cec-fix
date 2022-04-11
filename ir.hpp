#ifndef IR_H
#define IR_H

#include <stdio.h>

enum GPIO {
    PIN2 = 2,
    PIN3,
    PIN4,
    PIN5,
    PIN6,
    PIN7,
    PIN8,
    PIN9,
    PIN10,
    PIN11,
    PIN12,
    PIN13,
    PIN14,
    PIN15,
    PIN16,
    PIN17,
    PIN18,
    PIN19,
    PIN20,
    PIN21,
    PIN22,
    PIN23,
    PIN24,
    PIN25,
    PIN26,
    PIN27
};

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
