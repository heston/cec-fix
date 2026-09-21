Heston's cec-fix
================

Some Raspberry Pi code that allows my Roku remote to control my home theater over HDMI-CEC and TCP sockets.

My Setup
--------
- NAD T778 receiver
- JVC NX7 projector
- Roku Ultra

Goal
----
**Allow the Roku remote to control all primary aspects of the theater system.**

*Roku remote* means the physical remote that came with the Roku Ultra, as well as the Roku Android/iOS app.

*Primary aspects* means:
1. Turning both the receiver and projector on and off.
1. Controlling the audio volume of the receiver.
1. Automatically switching the receivier's input to the correct one for the Roku.

Basically, I want to pick up the Roku remote, hit the power key, and watch a movie. When I'm done, I want to hit
the power key again, and have everything go into standby mode.

I should only need the NAD and JVC remotes if I'm doing something unusual.

Approach
--------
1. Make the Raspberry Pi pretend to be the TV (CEC logical address `0`), since Roku only sends power commands to the TV.
1. Connect to JVC projector on LAN interface using a TCP socket (JVC projectors do not support CEC).
1. Listen to CEC messages on the HDMI-CEC bus, and send messages back to the bus and projector.


Resources
---------
1. Raspberry Pi Zero W.
1. The original [cec-fix](https://github.com/glywood/cec-fix).
1. [JVC Interface Specifications](https://support.jvc.com/consumer/support/support.jsp?pageID=11), specifically [JVC D-ILA® Projector RS232 / LAN / Infrared Remote Control Codes](https://support.jvc.com/consumer/support/documents/DILAremoteControlGuide.pdf) PDF.

Installation
------------
1. Check out this repo on the Raspberry Pi, as the build references firmware libraries that are only available there.
1. `cd` into the directory and `make` to build it.
1. `/build/cec-fix PROJECTOR_HOST_IP` to run, where `PROJECTOR_HOST_IP` is the IP address of the JVC projector. `CTRL-c` to exit.
1. To run as a service on boot:
    ```
    echo "PROJECTOR_HOST_IP=xxx.xxx.xxx.xxx" > .env
    sudo make install
    ```

**_A note on GPU driver compatibility_**

The default GPU driver was replaced with DRM V4 V3D on newer distributions of Raspian (at least starting at Bullseye). This appears to be incompatible with the Broadcom CEC APIs used by this project. If you run into trouble, you can disable these newer drivers:

1. Open `/boot/config.txt` for editing (e.g. `sudo vim /boot/config.txt`).
1. Replace the line `dtoverlay=vc4-kms-v3d` with `#dtoverlay=vc4-kms-v3d` (i.e. comment it out).
1. Save the file.
1. Restart the Raspberry Pi.

Uninstalling
------------
`sudo make uninstall`

CEC address diagnostics
-----------------------
The service logs its own logical and physical addresses when the firmware sends
an allocation notification, for example:

```
CEC address allocated: logical=0, physical=1.2.0.0
```

This is a firmware notification, not a malformed CEC message. Release, unavailable
address, and address-loss notifications are logged separately. An address-loss
warning does not mean the service has recovered the address automatically.
Messages saying `Cached physical address` describe other devices in the lookup
table; they do not set the Pi's address.

The Pi intentionally uses logical address 0 and device type TV. A root TV normally
has physical address `0.0.0.0`; a Pi connected to a receiver input can instead get
an address such as `1.2.0.0`. The service warns about that mismatch. The legacy
[Broadcom API](https://github.com/raspberrypi/userland/blob/master/interface/vmcs_host/vc_cecservice.h)
documents that `vc_cec_set_logical_address` uses the physical address from EDID;
it provides a getter but no physical-address setter. Sending a fabricated
`Report Physical Address` packet alone does not change that firmware state.

The firmware setting `hdmi_force_cec_address` appears in Raspberry Pi diagnostic
configuration dumps, but its behavior for zero on this setup has not been verified.
A possible **experimental** boot setting is `hdmi_force_cec_address=0` in the
active boot `config.txt` (usually `/boot/config.txt` on the legacy setup). Back up
that file before trying it and reboot afterward. Verify the allocation log says
`physical=0.0.0.0`; configuration readback alone is not proof. Also capture the
Pi's on-wire `Report Physical Address` response to check what peers see. Remove
the setting and reboot to undo the experiment. The service does not edit boot
configuration or force an address itself.

For volume troubleshooting, capture debug logs while pressing volume up/down.
`5 -> F: 72 00` means the audio system broadcasts System Audio Mode off; `72 01`
means on. Volume key presses typically use `44 41` (up), `44 42` (down), followed
by `45` (release). Check whether they target logical address 5 (receiver) or 0
(the emulated TV). The service currently does not forward volume commands sent
to address 0. Absence from this service's log alone does not prove absence on
the bus: firmware may not deliver traffic addressed to other devices.

Regression tests
----------------
GitHub Actions runs `make test` on pull requests and pushes to `main` using Ubuntu.

Run `make test` with a C++11 compiler and Python 3. These tests do not require Pi
hardware. They use a fake projector on `127.0.0.1:20554` and `/tmp/p-cec-fix`, so
stop any local instance first. The suite covers stalled projector replies, socket
connection errors, queued CEC notifications, and FIFO command processing/restart.

CEC callbacks enqueue notifications for the main loop, which also polls the FIFO.
All projector commands and power-cache updates run on that loop. A stalled projector
read times out after five seconds; retries can still delay command handling. The
CEC queue holds 256 notifications and logs any overflow once processing resumes.
TCP replies are accumulated to their protocol lengths, including separate command
acknowledgment and power-status frames. Both frames share a five-second deadline;
fragmented replies cannot extend it. Tests cover split and bytewise replies,
truncation, malformed frames, and slow replies that exceed the deadline.

Compatibility
-------------
This was tested on a Raspberry Pi Zero W running Rapsian Bullseye. Older versions of Raspian should also work, as should similar Raspberry Pi hardware generations (e.g. B, A, B+). However, this has not been verified.

Interprocess Control
--------------------
It is possible for other running processes on the Raspberry Pi to control the theater system over a named pipe (FIFO).
When running, a named pipe is created at `/tmp/p-cec-fix`. Another process can write to this pipe to turn the system
on and off (assuming the default playback device is at logical address 4).

To turn the system on, and set the active source to Playback Device 1 (logical address 4), write "1" to the pipe.

To turn the system off (set all devices to standby), write "0" to the pipe.

To see an example of a Python process controlling the system in response to Google Assistant voice commands,
look at [Theater Commander](https://github.com/heston/theater-commander) and [Theater Commander Server](https://github.com/heston/theater-commander-server).
