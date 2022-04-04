#include <bcm_host.h>
#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
using namespace std;


int serial_fd;
bool want_on = 0;
bool tv_is_on = 0;

void opcodeToBytes(long param, uint8_t bytes[4]) {
	bytes[3] = (param >> 24) & 0xFF;
	bytes[2] = (param >> 16) & 0xFF;
	bytes[1] = (param >> 8) & 0xFF;
	bytes[0] = (param >> 0) & 0xFF;
}

void turnOffTV() {
	if (!tv_is_on) {
		std::cerr << "TV is already off!" << std::endl;
		return;
	}
	std::cerr << "Turning off the TV" << std::endl;
	tv_is_on = 0;
}

void turnOnTV() {
	if (tv_is_on) {
		std::cerr << "TV is already on!" << std::endl;
		return;
	}

	std::cerr << "Turning on the TV" << std::endl;
	tv_is_on = 1;
}

// Set the system state machine to indicate we are powering on.
void setWantOn() {
	want_on = 1;
}

// Reset the system state machine.
void resetWantOn() {
	want_on = 0;
}

// Check whether a CEC message means that a device requested ImageViewOn.
bool isImageViewOn(VC_CEC_MESSAGE_T &message) {
	return (message.length == 1 &&
		    message.payload[0] == CEC_Opcode_ImageViewOn
	);
}

// Check whether a CEC message means that the receiver is on.
bool isReceiverOn(VC_CEC_MESSAGE_T &message) {
	bool is_receiver_msg = (
		message.length == 2 &&
		message.initiator == CEC_AllDevices_eAudioSystem &&
		message.payload[0] == CEC_Opcode_ReportPowerStatus
	);

	if (is_receiver_msg) {
		std::cerr << "Receiver has power status " << (int) message.payload[1] << ". (0=on, 1=off, 2=on_pending, 3=off_pending)" << std::endl;
	}

	return (
		is_receiver_msg && (
			message.payload[1] == CEC_POWER_STATUS_ON ||
			message.payload[1] == CEC_POWER_STATUS_ON_PENDING
		)
	);
}

// Send a CEC message to the receiver to indicate that the power on button was pressed.
void turnOnReceiver() {
	std::cerr << "Sending audio system power on button pressed" << std::endl;
	uint8_t bytes[2];
	bytes[0] = CEC_Opcode_UserControlPressed;
	bytes[1] = CEC_User_Control_Power;
	if (vc_cec_send_message(
			CEC_AllDevices_eAudioSystem,
			bytes, 2, VC_FALSE) != 0
		) {
		std::cerr << "Failed to press Power On." << std::endl;
	}
}

// Whether a received CEC message is an acklowlegement of a power on command
bool isReceiverOnCmdAck(uint32_t reason, VC_CEC_MESSAGE_T &message) {
	return (
		(reason & VC_CEC_TX) &&
		message.length == 2 &&
		message.payload[0] == CEC_Opcode_UserControlPressed &&
		message.payload[1] == CEC_User_Control_Power
	);
}

// Send a CEC message to indicate that the power on button has been released.
void turnReceiverOnComplete() {
	std::cerr << "Sending audio system power on button released" << std::endl;
	uint8_t bytes[1];
	bytes[0] = CEC_Opcode_UserControlReleased;
	if (vc_cec_send_message(CEC_AllDevices_eAudioSystem,
			bytes, 1, VC_FALSE) != 0) {
		std::cerr << "Failed to release Power On." << std::endl;
	}
}

// Whether a received CEC message is an acknowlegment of thw poer on button being released.
bool isTurnReceiverOnCompleteCmdAck(uint32_t reason, VC_CEC_MESSAGE_T &message) {
	return (
		(reason & VC_CEC_TX) &&
		message.length == 1 &&
		message.payload[0] == CEC_Opcode_UserControlReleased
	);
}

// Send a CEC message to query the power status of the receiver.
void checkAudioSystemPowerStatus() {
	std::cerr << "Checking audio system power status" << std::endl;
	uint8_t bytes[1];
	bytes[0] = CEC_Opcode_GiveDevicePowerStatus;
	if (vc_cec_send_message(CEC_AllDevices_eAudioSystem,
			bytes, 1, VC_FALSE) != 0) {
		std::cerr << "Failed to check power status." << std::endl;
	}
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
	std::cerr << "Broadcasting standby" << std::endl;
	uint8_t bytes[1];
	bytes[0] = CEC_Opcode_Standby;
	if (vc_cec_send_message(CEC_BROADCAST_ADDR,
			bytes, 1, VC_FALSE) != 0) {
		std::cerr << "Failed to broadcast standby command." << std::endl;
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
void replyWithVendorId(int requestor) {
	std::cerr << "Replying with Vendor ID" << std::endl;
	uint8_t bytes[4];
	opcodeToBytes(CEC_VENDOR_ID_BROADCOM, bytes);
	bytes[0] = CEC_Opcode_DeviceVendorID;
	if (vc_cec_send_message(requestor,
			bytes, 4, VC_TRUE) != 0) {
		std::cerr << "Failed to reply with vendor ID." << std::endl;
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
	std::cerr << "Replying with power status" << std::endl;
	uint8_t bytes[2];
	bytes[0] = CEC_Opcode_ReportPowerStatus;
	bytes[1] = tv_is_on ? CEC_POWER_STATUS_ON : CEC_POWER_STATUS_STANDBY;
	if (vc_cec_send_message(requestor,
			bytes, 2, VC_TRUE) != 0) {
		std::cerr << "Failed to reply with TV power status." << std::endl;
	}
}


// Parse a CEC callback into a Message struct.
// Returns true if parsing was successful, false otherwise.
bool parseCECMessage(VC_CEC_MESSAGE_T &message, uint32_t reason, uint32_t param1, uint32_t param2, uint32_t param3, uint32_t param4) {
	int retval = vc_cec_param2message(reason, param1, param2, param3, param4, &message);
	bool success = 0 == retval;

	if(success) {
		std::cerr << std::hex <<
		"Translated to message i=" << message.initiator <<
		" f=" << message.follower <<
		" len=" << message.length <<
		" content=" << (uint32_t)message.payload[0] <<
		" " << (uint32_t)message.payload[1] <<
		" " << (uint32_t)message.payload[2] << std::endl;
	} else {
		std::cerr << "Not a valid message!" << std::endl;
	}

	return success;
}

// General callback for all CEC messages.
void handleCECCallback(void *callback_data, uint32_t reason, uint32_t param1, uint32_t param2, uint32_t param3, uint32_t param4) {
	std::cerr << "Got a callback!" << std::endl << std::hex <<
		"reason = 0x" << reason << std::endl <<
		"param1 = 0x" << param1 << std::endl <<
		"param2 = 0x" << param2 << std::endl <<
		"param3 = 0x" << param3 << std::endl <<
		"param4 = 0x" << param4 << std::endl;

	VC_CEC_MESSAGE_T message;
	if (!parseCECMessage(message, reason, param1, param2, param3, param4)) {
		return;
	}

	// Detect when the TV is being told to turn on. Check the power
	// status of the receiver, because if it's not on we'll want to
	// turn it on.
	if (isImageViewOn(message)) {
		std::cerr << "ImageViewOn, checking power status of receiver." << std::endl;
		// setWantOn();
		turnOnTV();
		// This will result in the audio system sending us back a message
		// checkAudioSystemPowerStatus();
		return;
	}

	// // TODO: logic here seems questionable. Verify behavior.
	// if (isReceiverOn(message)) {
	// 	std::cerr << "Receiver is now on." << std::endl;
	// 	if (want_on) {
	// 		resetWantOn();
	// 	} else {
	// 		std::cerr << "Receiver is off but we want it on." << std::endl;
	// 		turnOnReceiver();
	// 	}
	// 	return;
	// }

	// // As soon as the power-on button press is finished sending,
	// // also send a button release.
	// if (isReceiverOnCmdAck(reason, message)) {
	// 	std::cerr << "Power on press complete, now sending release." << std::endl;
	// 	turnReceiverOnComplete();
	// 	return;
	// }

	// // As soon as the power-on button release is finished sending,
	// // query the power status again.
	// if (isTurnReceiverOnCompleteCmdAck(reason, message)) {
	// 	std::cerr << "Power on release complete, now querying power status." << std::endl;
	// 	checkAudioSystemPowerStatus();
	// 	return;
	// }

	// Detect when the TV is being told to go into standby.
	if (isTVOffCmd(message)) {
		// resetWantOn();
		turnOffTV();
		broadcastStandby();
		return;
	}

	if (isRequestForVendorId(message)) {
		replyWithVendorId(message.initiator);
		return;
	}

	if (isRequestForPowerStatus(message)) {
		replyWithPowerStatus(message.initiator);
		return;
	}
}

void tv_callback(void *callback_data, uint32_t reason, uint32_t p0, uint32_t p1) {
	std::cerr << "Got a TV callback!" << std::endl << std::hex <<
		"reason = 0x" << reason << std::endl << 
		"param0 = 0x" << p0 << std::endl <<
		"param1 = 0x" << p1 << std::endl;
}

int main(int argc, char *argv[]) {
	bcm_host_init();
	vcos_init();

	VCHI_INSTANCE_T vchi_instance;
	if (vchi_initialise(&vchi_instance) != 0) {
		std::cerr << "Could not initiaize VHCI" << std::endl;
		return 1;
	}

	if (vchi_connect(nullptr, 0, vchi_instance) != 0) {
		std::cerr << "Failed to connect to VHCI" << std::endl;
		return 1;
	}

	vc_vchi_cec_init(vchi_instance, nullptr, 0);

	if (vc_cec_set_passive(VC_TRUE) != 0) {
		std::cerr << "Failed to enter passive mode" << std::endl;
		return 1;
	}

	vc_cec_register_callback(handleCECCallback, nullptr);
	vc_tv_register_callback(tv_callback, nullptr);

	if (vc_cec_register_all() != 0) {
		std::cerr << "Failed to register all opcodes" << std::endl;
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
		std::cerr << "Failed to set logical address" << std::endl;
		return 1;
	}

	// serial_fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY);
	// if (serial_fd < 0) {
	// 	perror("failed to open /dev/ttyUSB0");
	// 	return 1;
	// }

	struct termios tio;
	tcgetattr(serial_fd, &tio);
	tio.c_cflag = B9600 | CRTSCTS | CS8 | CLOCAL | CREAD;
	tcsetattr(serial_fd, TCSANOW, &tio);

	std::cerr << "Press CTRL-c to exit" << std::endl;
	std::cerr << "Controls: " << std::endl;
	std::cerr << "  i   Send ImageViewOn to receiver" << std::endl;
	std::cerr << "  s   Broadcast Standby message" << std::endl;
	std::cerr << "  p   Request receiver power status" << std::endl;
	std::cerr << std::endl;
	while (true) {
		char ch;
		std::cin >> ch;

		if (ch == 'i') {
			std::cerr << "ImageViewOn" << std::endl;
			uint8_t bytes[1];
			bytes[0] = CEC_Opcode_ImageViewOn;
			if (vc_cec_send_message(CEC_AllDevices_eAudioSystem,
					       	bytes, 2, VC_FALSE) != 0) {
				std::cerr << "Failed to press Power On." << std::endl;
			}
		} else if (ch == 's') {
			std::cerr << "Broadcast Standby" << std::endl;
			uint8_t bytes[1];
			bytes[0] = CEC_Opcode_Standby;
			if (vc_cec_send_message(CEC_BROADCAST_ADDR,
					       	bytes, 2, VC_FALSE) != 0) {
				std::cerr << "Failed to press Power On." << std::endl;
			}
		} else if (ch == 'p') {
			std::cerr << "GiveDevicePowerStatus" << std::endl;
			uint8_t bytes[1];
			bytes[0] = CEC_Opcode_GiveDevicePowerStatus;
			if (vc_cec_send_message(CEC_AllDevices_eAudioSystem,
					       	bytes, 2, VC_FALSE) != 0) {
				std::cerr << "Failed to press Power On." << std::endl;
			}
		}
	}
	return 0;
}
