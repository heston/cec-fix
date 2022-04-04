#include <bcm_host.h>
#include <iostream>
#include <unistd.h>
#include <thread>         // this_thread::sleep_for
#include <chrono>         // chrono::seconds

using namespace std;


bool tv_is_on = 0;

void turnOffTV() {
	if (!tv_is_on) {
		cerr << "TV is already off!" << endl;
		return;
	}
	cerr << "Turning off the TV" << endl;
	tv_is_on = 0;
}

void turnOnTV() {
	if (tv_is_on) {
		cerr << "TV is already on!" << endl;
		return;
	}

	cerr << "Turning on the TV" << endl;
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
	cerr << "Broadcasting standby" << endl;
	uint8_t bytes[1];
	bytes[0] = CEC_Opcode_Standby;
	if (vc_cec_send_message(CEC_BROADCAST_ADDR,
			bytes, 1, VC_FALSE) != 0) {
		cerr << "Failed to broadcast standby command." << endl;
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
	cerr << "Broadcasting Vendor ID" << endl;
	uint8_t bytes[4];
	bytes[0] = CEC_Opcode_DeviceVendorID;
	bytes[1] = (CEC_VENDOR_ID_BROADCOM >> 16) & 0xFF;
	bytes[2] = (CEC_VENDOR_ID_BROADCOM >> 8) & 0xFF;
	bytes[3] = (CEC_VENDOR_ID_BROADCOM >> 0) & 0xFF;
	if (vc_cec_send_message(CEC_BROADCAST_ADDR,
			bytes, 4, VC_TRUE) != 0) {
		cerr << "Failed to reply with vendor ID." << endl;
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
	cerr << "Replying with power status" << endl;
	uint8_t bytes[2];
	bytes[0] = CEC_Opcode_ReportPowerStatus;
	bytes[1] = tv_is_on ? CEC_POWER_STATUS_ON : CEC_POWER_STATUS_STANDBY;
	if (vc_cec_send_message(requestor,
			bytes, 2, VC_TRUE) != 0) {
		cerr << "Failed to reply with TV power status." << endl;
	}
}


// Parse a CEC callback into a Message struct.
// Returns true if parsing was successful, false otherwise.
bool parseCECMessage(VC_CEC_MESSAGE_T &message, uint32_t reason, uint32_t param1, uint32_t param2, uint32_t param3, uint32_t param4) {
	int retval = vc_cec_param2message(reason, param1, param2, param3, param4, &message);
	bool success = 0 == retval;

	if(success) {
		cerr << hex <<
		"Translated to message initiator=" << message.initiator <<
		" follower=" << message.follower <<
		" length=" << message.length <<
		" content=";
		for (size_t i = 0; i < message.length; i++)
		{
			cerr << hex << (uint32_t)message.payload[i] << " ";
		}
		cerr << endl;
	} else {
		cerr << "Not a valid message!" << endl;
	}

	return success;
}

// General callback for all CEC messages.
void handleCECCallback(void *callback_data, uint32_t reason, uint32_t param1, uint32_t param2, uint32_t param3, uint32_t param4) {
	cerr << endl;
	cerr << "Got a callback!" << endl << hex <<
		"reason = 0x" << reason << endl <<
		"param1 = 0x" << param1 << endl <<
		"param2 = 0x" << param2 << endl <<
		"param3 = 0x" << param3 << endl <<
		"param4 = 0x" << param4 << endl;

	VC_CEC_MESSAGE_T message;
	if (!parseCECMessage(message, reason, param1, param2, param3, param4)) {
		return;
	}

	// Detect when the TV is being told to turn on. Check the power
	// status of the receiver, because if it's not on we'll want to
	// turn it on.
	if (isImageViewOn(message)) {
		turnOnTV();
		// This will result in the audio system sending us back a message
		return;
	}

	// Detect when the TV is being told to go into standby.
	if (isTVOffCmd(message)) {
		turnOffTV();
		broadcastStandby();
		return;
	}

	if (isRequestForVendorId(message)) {
		broadcastVendorId();
		return;
	}

	if (isRequestForPowerStatus(message)) {
		replyWithPowerStatus(message.initiator);
		return;
	}
}

void tv_callback(void *callback_data, uint32_t reason, uint32_t p0, uint32_t p1) {
	cerr << endl;
	cerr << "Got a TV callback!" << endl << hex <<
		"reason = 0x" << reason << endl << 
		"param0 = 0x" << p0 << endl <<
		"param1 = 0x" << p1 << endl;
}

int main(int argc, char *argv[]) {
	bcm_host_init();
	vcos_init();

	VCHI_INSTANCE_T vchi_instance;
	if (vchi_initialise(&vchi_instance) != 0) {
		cerr << "Could not initiaize VHCI" << endl;
		return 1;
	}

	if (vchi_connect(nullptr, 0, vchi_instance) != 0) {
		cerr << "Failed to connect to VHCI" << endl;
		return 1;
	}

	vc_vchi_cec_init(vchi_instance, nullptr, 0);

	if (vc_cec_set_passive(VC_TRUE) != 0) {
		cerr << "Failed to enter passive mode" << endl;
		return 1;
	}

	vc_cec_register_callback(handleCECCallback, nullptr);
	vc_tv_register_callback(tv_callback, nullptr);

	if (vc_cec_register_all() != 0) {
		cerr << "Failed to register all opcodes" << endl;
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
		cerr << "Failed to set logical address" << endl;
		return 1;
	}

	cerr << "Running! Press CTRL-c to exit." << endl;
	cerr << endl;

	while (true) {
		this_thread::sleep_for (chrono::seconds(1));
	}

	return 0;
}
