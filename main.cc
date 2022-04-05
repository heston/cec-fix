#include <bcm_host.h>
#include <iostream>
#include <unistd.h>
#include <thread>         // this_thread::sleep_for
#include <chrono>         // chrono::seconds
#include "spdlog/spdlog.h"

using namespace std;

bool tv_is_on = 0;

void turnOffTV() {
	if (!tv_is_on) {
		spdlog::info("TV is already off!");
		return;
	}
	spdlog::info("Turning off the TV");
	tv_is_on = 0;
}

void turnOnTV() {
	if (tv_is_on) {
		spdlog::info("TV is already on!");
		return;
	}

	spdlog::info("Turning on the TV");
	tv_is_on = 1;
}

// Check whether a CEC message means that a device requested ImageViewOn.
bool isImageViewOn(VC_CEC_MESSAGE_T &message) {
	return (message.length == 1 &&
		    message.payload[0] == CEC_Opcode_ImageViewOn
	);
}

// Whether a CEC message indicates that the TV should switch to standby.
bool isTVOffCmd(VC_CEC_MESSAGE_T &message) {
	return (
		message.follower == 0 &&
		message.length == 1 &&
		message.payload[0] == CEC_Opcode_Standby
	);
}

// Broadcast a CEC message to all followers to enter standby mode.
void broadcastStandby() {
	spdlog::info("Broadcasting standby");
	uint8_t bytes[1];
	bytes[0] = CEC_Opcode_Standby;
	if (vc_cec_send_message(CEC_BROADCAST_ADDR,
			bytes, 1, VC_FALSE) != 0) {
		spdlog::error( "Failed to broadcast standby command.");
	}
}

bool isRequestForVendorId(VC_CEC_MESSAGE_T &message) {
	return (
		message.follower == CEC_AllDevices_eTV &&
		message.length == 1 &&
		message.payload[0] == CEC_Opcode_GiveDeviceVendorID
	);
}

// Handle 40:8C (Roku asking TV to give vendor ID)
void broadcastVendorId() {
	spdlog::info("Broadcasting Vendor ID");
	uint8_t bytes[4];
	bytes[0] = CEC_Opcode_DeviceVendorID;
	bytes[1] = (CEC_VENDOR_ID_BROADCOM >> 16) & 0xFF;
	bytes[2] = (CEC_VENDOR_ID_BROADCOM >> 8) & 0xFF;
	bytes[3] = (CEC_VENDOR_ID_BROADCOM >> 0) & 0xFF;
	if (vc_cec_send_message(CEC_BROADCAST_ADDR,
			bytes, 4, VC_TRUE) != 0) {
		spdlog::error("Failed to reply with vendor ID.");
	}
}

bool isRequestForPowerStatus(VC_CEC_MESSAGE_T &message) {
	return (
		message.follower == CEC_AllDevices_eTV &&
		message.length == 1 &&
		message.payload[0] == CEC_Opcode_GiveDevicePowerStatus
	);
}

void replyWithPowerStatus(int requestor) {
	spdlog::info("Replying with power status.");
	uint8_t bytes[2];
	bytes[0] = CEC_Opcode_ReportPowerStatus;
	bytes[1] = tv_is_on ? CEC_POWER_STATUS_ON : CEC_POWER_STATUS_STANDBY;
	if (vc_cec_send_message(requestor,
			bytes, 2, VC_TRUE) != 0) {
		spdlog::error("Failed to reply with TV power status.");
	}
}


// Parse a CEC callback into a Message struct.
// Returns true if parsing was successful, false otherwise.
bool parseCECMessage(VC_CEC_MESSAGE_T &message, uint32_t reason, uint32_t param1, uint32_t param2, uint32_t param3, uint32_t param4) {
	int retval = vc_cec_param2message(reason, param1, param2, param3, param4, &message);
	bool success = 0 == retval;

	string content = "";
	for (size_t i = 0; i < message.length; i++)
	{
		content += fmt::format("{:X} ", message.payload[i]);
	}

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

// General callback for all CEC messages.
void handleCECCallback(void *callback_data, uint32_t reason, uint32_t param1, uint32_t param2, uint32_t param3, uint32_t param4) {
	spdlog::debug(
		"Got a callback: reason={reason:X} param1={p1:X} param2={p1:X} param3={p3:X} param4={p4:X}",
		fmt::arg("reason", reason),
		fmt::arg("p1", param1),
		fmt::arg("p2", param2),
		fmt::arg("p3", param3),
		fmt::arg("p4", param4),
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

	if (isRequestForVendorId(message)) {
		spdlog::info("Vendor ID request message received.");
		broadcastVendorId();
		return;
	}

	if (isRequestForPowerStatus(message)) {
		spdlog::info("Power status request message received.");
		replyWithPowerStatus(message.initiator);
		return;
	}
}

void tv_callback(void *callback_data, uint32_t reason, uint32_t p0, uint32_t p1) {
	spdlog::debug(
		"Got a TV callback: reason={reason:X} param0={p0:X} param1={p1:X}",
		fmt::arg("reason", reason),
		fmt::arg("p0", p0),
		fmt::arg("p1", p1)
	);
}

int main(int argc, char *argv[]) {
	spdlog::set_level(spdlog::level::debug); // Set global log level to debug

	bcm_host_init();
	vcos_init();

	VCHI_INSTANCE_T vchi_instance;
	if (vchi_initialise(&vchi_instance) != 0) {
		spdlog::critical("Could not initialize VHCI");
		return 1;
	}

	if (vchi_connect(nullptr, 0, vchi_instance) != 0) {
		spdlog::critical("Failed to connect to VHCI");
		return 1;
	}

	vc_vchi_cec_init(vchi_instance, nullptr, 0);

	if (vc_cec_set_passive(VC_TRUE) != 0) {
		spdlog::critical("Failed to enter passive mode");
		return 1;
	}

	vc_cec_register_callback(handleCECCallback, nullptr);
	vc_tv_register_callback(tv_callback, nullptr);

	if (vc_cec_register_all() != 0) {
		spdlog::critical("Failed to register all opcodes");
		return 1;
	}

	vc_cec_register_command(CEC_Opcode_GivePhysicalAddress);
	vc_cec_register_command(CEC_Opcode_GiveDeviceVendorID);
	vc_cec_register_command(CEC_Opcode_GiveOSDName);
	vc_cec_register_command(CEC_Opcode_GetCECVersion);
	vc_cec_register_command(CEC_Opcode_GiveDevicePowerStatus);
	vc_cec_register_command(CEC_Opcode_MenuRequest);
	vc_cec_register_command(CEC_Opcode_GetMenuLanguage);

	if (vc_cec_set_logical_address(CEC_AllDevices_eTV, CEC_DeviceType_TV, CEC_VENDOR_ID_BROADCOM) != 0) {
		spdlog::critical("Failed to set logical address");
		return 1;
	}

	spdlog::info("Running! Press CTRL-c to exit.");

	while (true) {
		this_thread::sleep_for (chrono::seconds(1));
	}

	return 0;
}
