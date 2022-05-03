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
1. `/build/cec-fix` to run. `CTRL-c` to exit.
1. To run as a service on boot: `sudo make install`.

**_A note on GPU driver compatibility_**

The default GPU driver was replaced with DRM V4 V3D on newer distributions of Raspian (at least starting at Bullseye). This appears to be incompatible with the Broadcom CEC APIs used by this project. If you run into trouble, you can disable these newer drivers:

1. Open `/boot/config.txt` for editing (e.g. `sudo vim /boot/config.txt`).
1. Replace the line `dtoverlay=vc4-kms-v3d` with `#dtoverlay=vc4-kms-v3d` (i.e. comment it out).
1. Save the file.
1. Restart the Raspberry Pi.

Uninstalling
------------
`sudo make uninstall`

Compatibility
-------------
This was tested on a Raspberry Pi Zero W running Rapsian Bullseye. Older versions of Raspian should also work, as should similar Raspberry Pi hardware generations (e.g. B, A, B+). However, this has not been verified.
