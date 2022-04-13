#ifndef LAN_H
#define LAN_H

static const char DEFAULT_HOST[] { "192.168.86.90" };

/**
 * Set the global projector host.
 *
 * @param   char  host  The IPv4 (or IPv6) address of the projector.
 *
 * @return  void
 */
void setHost(char * host);
void setHost(const char * host);

/**
 * Send the NULL command for testing purposes.
 *
 * @return  int    0 if the command was sent successfully. A negative integer
 *                 if an error was encountered.
 */
int sendNull();

/**
 * Send the Power On command.
 *
 * @return  int    0 if the command was sent successfully. A negative integer
 *                 if an error was encountered.
 */
int sendOn();

/**
 * Send the Power Off/Standby command.
 *
 * @return  int    0 if the command was sent successfully. A negative integer
 *                 if an error was encountered.
 */
int sendOff();

#endif
