#ifndef LAN_H
#define LAN_H

static const char DEFAULT_HOST[] { "192.168.86.87" };

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

/**
 * Whether the host is POWER_ON or WARMING mode.
 *
 * @return  bool
 */
bool isOn();

/**
 * Whether the host is in STANDBY or COOLING mode.
 *
 * @return  bool    [return description]
 */
bool isOff();

/**
 * Get the power status of the host.
 *
 * @return  int     0: STANDBY
 *                  1: POWER_ON
 *                  2: COOLING
 *                  3: WARMING
 *                  4: EMERGENCY
 *                 -1: Unknown status
 */
int queryPowerStatus();

/**
 * Same as queryPowerStatus but caches the result for 10 seconds.
 *
 * This prevents many rapid calls for the power status from overloading
 * the projector's maximum connection count (which appears quite low,
 * empirically).
 *
 * @return  int     @see queryPowerStatus
 */
int queryPowerStatusCached();

#endif
