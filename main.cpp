#include <bcm_host.h>
#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include "spdlog/spdlog.h"
#include <string.h>
#include <signal.h>
#include <unordered_map>
#include "lan.hpp"
#include "fifo.hpp"

using namespace std;

bool want_run = true;

// Mapping of logical to physical addresses
unordered_map<CEC_AllDevices_T, uint8_t*> addressMap { 0 };

const char OSD_NAME[] { "JVC NX7" };

/**
 * Return string representation of a message payload.
 *
 * @param unit8_t * payload The message payload as an array of bytes.
 * @param size_t length The length of the message.
 *
 * @return  string
 */
string getOpcodeString(uint8_t* payload, size_t length) {
	string content = "";
	for (size_t i = 0; i < length; i++)
	{
		content += fmt::format("{:X} ", payload[i]);
	}
	return content;
}

/**
 * Request the physical address of a logical address.
 *
 * @param CEC_AllDevices follower Logical address of the device.
 */
void getPhysicalAddress(CEC_AllDevices follower) {
	spdlog::info("Get physical address for {}", follower);
	uint8_t bytes[1];
	bytes[0] = CEC_Opcode_GivePhysicalAddress;
	if (vc_cec_send_message(follower,
			bytes, 1, VC_FALSE) != 0) {
		spdlog::error( "Failed to request physical address.");
	}
}

/**
 * Set the stream path to a physical address.
 *
 * @param uint8_t * physicalAddress Array of bytes representing a device's physical address.
 */
void setStreamPath(uint8_t * physicalAddress) {
	string path = getOpcodeString(physicalAddress, 3);
	spdlog::info("Set stream path to: {}", path);
	uint8_t bytes[3];
	bytes[0] = CEC_Opcode_SetStreamPath;
	bytes[1] = physicalAddress[1];
	bytes[2] = physicalAddress[2];
	if (vc_cec_send_message(CEC_BROADCAST_ADDR,
			bytes, 3, VC_FALSE) != 0) {
		spdlog::error( "Failed to set stream path.");
	}
}

// Marker to remember if we are waiting on a physical address to set the stream path.
bool want_set_stream_path = false;

/**
 * Set the stream path to Playback1 device.
 */
void setStreamPathToPlayback1() {
	uint8_t * address;
	try {
		address = addressMap.at(CEC_AllDevices_eDVD1);
	} catch(const out_of_range &e) {
		want_set_stream_path = true;
		getPhysicalAddress(CEC_AllDevices_eDVD1);
		return;
	}
	setStreamPath(address);
}

/**
 * Whether a CEC message is a device reporting physical address.
 *
 * @return  bool
 */
bool isReportPhysicalAddress(VC_CEC_MESSAGE_T &message) {
	return (
		message.length > 1 &&
		message.payload[0] == CEC_Opcode_ReportPhysicalAddress
	);
}

/**
 * Handler for a CEC message that reports a device's physical address.
 *
 * @param VC_CEC_MESSAGE_T message The message to parse.
 */
void handleReportPhysicalAddress(VC_CEC_MESSAGE_T &message) {
	string content = getOpcodeString(message.payload, message.length);
	spdlog::debug("handleReportPhysicalAddress: {}:{}", message.initiator, content);
	addressMap[message.initiator] = message.payload;

	if (want_set_stream_path) {
		setStreamPath(message.payload);
		want_set_stream_path = false;
	}
}

/**
 * Turn off the TV by sending the correct IR sequence.
 *
 * @return  void
 */
void turnOffTV() {
	if (isOff()) {
		spdlog::info("TV is already off!");
		return;
	}
	spdlog::info("Turning off the TV");
	if(sendOff() == 0) {
		spdlog::info("TV turned off");
	}
}

/**
 * Turn on the TV by sending the correct IR sequence.
 *
 * @return  void
 */
void turnOnTV() {
	if (isOn()) {
		spdlog::info("TV is already on!");
		return;
	}

	spdlog::info("Turning on the TV");
	if(sendOn() == 0) {
		spdlog::info("TV turned on");
	}
}

/**
 * Check whether a CEC message means that a device requested ImageViewOn.
 *
 * @param   C_CEC_MESSAGE_T  message  The parsed CEC message.
 *
 * @return  bool
 */
bool isImageViewOn(VC_CEC_MESSAGE_T &message) {
	return (
		message.length == 1 && (
			message.payload[0] == CEC_Opcode_ImageViewOn ||
			message.payload[0] == CEC_Opcode_TextViewOn
		)
	);
}

/**
 * Whether a CEC message indicates that the TV should switch to standby.
 *
 * @param   C_CEC_MESSAGE_T  message  The parsed CEC message.
 *
 * @return  bool
 */
bool isTVOffCmd(VC_CEC_MESSAGE_T &message) {
	return (
		(message.follower == CEC_AllDevices_eTV || message.follower == CEC_BROADCAST_ADDR) &&
		message.initiator != CEC_AllDevices_eTV &&
		message.length == 1 &&
		message.payload[0] == CEC_Opcode_Standby
	);
}

/**
 * Broadcast a CEC message to all followers to enter standby mode.
 *
 * @return  void
 */
void broadcastStandby() {
	spdlog::info("Broadcasting standby");

	if (vc_cec_send_Standby(CEC_BROADCAST_ADDR, false) != 0) {
		spdlog::error( "Failed to broadcast standby command.");
	}
}

/**
 * Whether a CEC message is a request for this device's vendor ID.
 *
 * @param   C_CEC_MESSAGE_T  message  The parsed CEC message.
 *
 * @return  bool
 */
bool isRequestForVendorId(VC_CEC_MESSAGE_T &message) {
	return (
		message.follower == CEC_AllDevices_eTV &&
		message.length == 1 &&
		message.payload[0] == CEC_Opcode_GiveDeviceVendorID
	);
}

/**
 * Broadcast the vendor ID of this device.
 * Raspberry Pi uses Broadcom chipset. Vendor ID is 0x18C086L.
 *
 * @return  void
 */
void broadcastVendorId() {
	spdlog::info("Broadcasting Vendor ID {}", CEC_VENDOR_ID_BROADCOM);
	if(vc_cec_set_vendor_id(CEC_VENDOR_ID_BROADCOM) != 0) {
		spdlog::error("Failed to reply with vendor ID.");
	}
}

/**
 * Whether a CEC message is a request for this device's power status.
 *
 * @param   C_CEC_MESSAGE_T  message  The parsed CEC message.
 *
 * @return  bool
 */
bool isRequestForPowerStatus(VC_CEC_MESSAGE_T &message) {
	return (
		message.follower == CEC_AllDevices_eTV &&
		message.length == 1 &&
		message.payload[0] == CEC_Opcode_GiveDevicePowerStatus
	);
}

/**
 * Reply to a power status request.
 *
 * @param   int   requestor  The CEC logical address of the device requesting power status.
 *
 * @return  void
 */
void replyWithPowerStatus(int requestor) {
	bool tv_is_on = isOn();
	spdlog::info("Replying with power status: {}", tv_is_on);
	uint8_t bytes[2];
	bytes[0] = CEC_Opcode_ReportPowerStatus;
	bytes[1] = tv_is_on ? CEC_POWER_STATUS_ON : CEC_POWER_STATUS_STANDBY;
	if (vc_cec_send_message(requestor,
			bytes, 2, VC_TRUE) != 0) {
		spdlog::error("Failed to reply with TV power status.");
	}
}

/**
 * Set the entire system to standby.
 */
int systemStandby() {
	spdlog::debug("systemStandby called");
	turnOffTV();
	broadcastStandby();
	return 1;
}

/**
 * Turns on the entire system and set active source to Playback 1.
 */
int systemActive() {
	spdlog::debug("systemActive called");
	turnOnTV();
	setStreamPathToPlayback1();
	return 1;
}

/**
 * Parse a CEC callback into a VC_CEC_MESSAGE_T struct.
 *
 * @return  bool    true if parsing was successful, false otherwise.
 */
bool parseCECMessage(VC_CEC_MESSAGE_T &message, uint32_t reason, uint32_t param1, uint32_t param2, uint32_t param3, uint32_t param4) {
	int retval = vc_cec_param2message(reason, param1, param2, param3, param4, &message);
	bool success = 0 == retval;

	string content = getOpcodeString(message.payload, message.length);

	if(success) {
		spdlog::debug(
			"Translated to message: initiator={initiator:X} follower={follower:X} length={length:d} content={content}",
			fmt::arg("initiator", message.initiator),
			fmt::arg("follower", message.follower),
			fmt::arg("length", message.length),
			fmt::arg("content", content)
		);
	} else {
		spdlog::warn("Not a valid message!");
	}

	return success;
}

bool isGiveOSDName(VC_CEC_MESSAGE_T &message) {
	return (
		message.length == 1 &&
		message.payload[0] == CEC_Opcode_GiveOSDName
	);
}

void setOSDName() {
	spdlog::info("Replying with OSD name: {}", OSD_NAME);
	vc_cec_set_osd_name(OSD_NAME);
}

/**
 * Callback function for host side notification.
 * This is the SAME as the callback function type defined in vc_cec.h
 * Host applications register a single callback for all CEC related notifications.
 * See vc_cec.h for meanings of all parameters
 *
 * @param callback_data is the context passed in by user in <DFN>vc_cec_register_callback</DFN>
 *
 * @param reason bits 15-0 is VC_CEC_NOTIFY_T in vc_cec.h;
 *               bits 23-16 is the valid length of message in param1 to param4 (LSB of param1 is the byte0, MSB of param4 is byte15), little endian
 *               bits 31-24 is the return code (if any)
 *
 * @param param1 is the first parameter
 *
 * @param param2 is the second parameter
 *
 * @param param3 is the third parameter
 *
 * @param param4 is the fourth parameter
 *
 * @return void
 */
void handleCECCallback(void *callback_data, uint32_t reason, uint32_t param1, uint32_t param2, uint32_t param3, uint32_t param4) {
	spdlog::debug(
		"Got a callback: reason={reason:X} param1={p1:X} param2={p1:X} param3={p3:X} param4={p4:X}",
		fmt::arg("reason", reason),
		fmt::arg("p1", param1),
		fmt::arg("p2", param2),
		fmt::arg("p3", param3),
		fmt::arg("p4", param4)
	);

	VC_CEC_MESSAGE_T message;
	if (!parseCECMessage(message, reason, param1, param2, param3, param4)) {
		return;
	}

	// Detect when the TV is being told to turn on. Check the power
	// status of the receiver, because if it's not on we'll want to
	// turn it on.
	if (isImageViewOn(message)) {
		spdlog::info("ImageViewOn message received.");
		turnOnTV();
		// This will result in the audio system sending us back a message
		return;
	}

	// Detect when the TV is being told to go into standby.
	if (isTVOffCmd(message)) {
		spdlog::info("Standby message received.");
		turnOffTV();
		broadcastStandby();
		return;
	}

	// Roku likes to ask for this.
	if (isRequestForVendorId(message)) {
		spdlog::info("Vendor ID request message received.");
		broadcastVendorId();
		return;
	}

	// Roku also likes to ask for this.
	if (isRequestForPowerStatus(message)) {
		spdlog::info("Power status request message received.");
		replyWithPowerStatus(message.initiator);
		return;
	}

	if(isReportPhysicalAddress(message)) {
		spdlog::info("Rerport physical address message received.");
		handleReportPhysicalAddress(message);
		return;
	}

	if (isGiveOSDName(message)) {
		spdlog::info("Give OSD name message received.");
		setOSDName();
		return;
	}
}

/**
 * Callback function for host side notification.
 * Host applications register a single callback for all TV related notifications.
 * See <DFN>VC_HDMI_NOTIFY_T</DFN> and <DFN>VC_SDTV_NOTIFY_T</DFN> in vc_hdmi.h and vc_sdtv.h
 * respectively for list of reasons and respective param1 and param2
 *
 * @param callback_data is the context passed in during the call to vc_tv_register_callback
 *
 * @param reason is the notification reason
 *
 * @param param1 is the first optional parameter
 *
 * @param param2 is the second optional parameter
 *
 * @return void
 */
void handleTVCallback(void *callback_data, uint32_t reason, uint32_t p0, uint32_t p1) {
	spdlog::debug(
		"Got a TV callback: reason={reason:X} param0={p0:X} param1={p1:X}",
		fmt::arg("reason", reason),
		fmt::arg("p0", p0),
		fmt::arg("p1", p1)
	);
}

/**
 * Set up CEC handlers.
 *
 * @return  bool    Whether CEC was configured successfully.
 */
bool initCEC() {
	bcm_host_init();
	vcos_init();

	VCHI_INSTANCE_T vchi_instance;
	if (vchi_initialise(&vchi_instance) != 0) {
		spdlog::critical("Could not initialize VHCI");
		return false;
	}

	if (vchi_connect(nullptr, 0, vchi_instance) != 0) {
		spdlog::critical("Failed to connect to VHCI");
		return false;
	}

	vc_vchi_cec_init(vchi_instance, nullptr, 0);

	if (vc_cec_set_passive(VC_TRUE) != 0) {
		spdlog::critical("Failed to enter passive mode");
		return false;
	}

	vc_cec_register_callback(handleCECCallback, nullptr);
	vc_tv_register_callback(handleTVCallback, nullptr);

	if (vc_cec_register_all() != 0) {
		spdlog::critical("Failed to register all opcodes");
		return false;
	}

	// vc_cec_register_command(CEC_Opcode_GivePhysicalAddress);
	// vc_cec_register_command(CEC_Opcode_GiveDeviceVendorID);
	vc_cec_register_command(CEC_Opcode_GiveOSDName);
	// vc_cec_register_command(CEC_Opcode_GetCECVersion);
	vc_cec_register_command(CEC_Opcode_GiveDevicePowerStatus);
	// vc_cec_register_command(CEC_Opcode_MenuRequest);
	// vc_cec_register_command(CEC_Opcode_GetMenuLanguage);

	if (vc_cec_set_logical_address(CEC_AllDevices_eTV, CEC_DeviceType_TV, CEC_VENDOR_ID_BROADCOM) != 0) {
		spdlog::critical("Failed to set logical address");
		return false;
	}

	spdlog::debug("CEC init successful");
	return true;
}

/**
 * Catch SIGINT and set running state of program to false.
 *
 * @param   int   s  Not used
 *
 * @return  void
 */
void handleSIGINT(int s) {
	want_run = false;
}

/**
 * Set the projector IP address from the command line args, is present.
 *
 * @param   int   argc  argc from main.
 * @param   char  argv  argv from main.
 *
 * @return  bool        Whether LAN was configured successfully.
 */
bool initLAN(int argc, char *argv[]) {
	if (argc == 1) {
		// No host provided, use default
		setHost(DEFAULT_HOST);
		spdlog::info("Projector host is {}", DEFAULT_HOST);
	} else if (argc == 2) {
		// Host provided
		setHost(argv[1]);
		spdlog::info("Projector host is {}", argv[1]);
	} else {
		spdlog::critical("Invalid invocation. First arg must be ip address of host, or omitted to use default.");
		return false;
	}

	if(sendNull() < 0) {
		spdlog::critical("Could not communicate with projector host.");
		return false;
	}

	spdlog::debug("LAN init successful");
	return true;
}

/**
 * Bootstrap all the things!
 *
 * @param   int   argc  Not used
 * @param   char  argv  Not used
 *
 * @return  int         0: process exited normally.
 * 						1: process exited due to critical CEC error.
 * 						2: process exited due to critical LAN error.
 */
int main(int argc, char *argv[]) {
	spdlog::set_level(spdlog::level::debug); // Set global log level to debug

	if (!initLAN(argc, argv)) {
		return 1;
	}

	if (!initCEC()) {
		return 1;
	}

	if (!initFIFO(systemStandby, systemActive)) {
		return 1;
	}

	// Handle SIGINT cleanly
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = handleSIGINT;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);

	spdlog::info("Running! Press CTRL-c to exit.");

	while (want_run) {
		pause();
	}

	return cleanupFIFO();;
}
