Heston's cec-fix
================

Some Raspberry Pi code that allows my Roku remote to control my home theater over HDMI-CEC and IR.

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

Resources
---------
1. Raspberry Pi Zero W.
1. The original [cec-fix](https://github.com/glywood/cec-fix).
1. LIRC library set up using the Gist [prasanthj/lirc-pi3.txt](https://gist.github.com/prasanthj/c15a5298eb682bde34961c322c95378b).
1. IR hardware and configuration using [How to Send and Receive IR Signals with a Raspberry Pi](https://www.digikey.com/en/maker/blogs/2021/how-to-send-and-receive-ir-signals-with-a-raspberry-pi). (In "production" I'm only sending IR signals, but I needed to receive them too during development in order to learn the correct JVC remote codes and test things.)

Installation
------------
1. Ensure LIRC is configured according to the above guides.
1. Download/compile/run on the raspberry pi itself, as it references some firmware libraries that are only available on the device. Just type 'make' to build it, then './cec-fix' to run.
1. To set it up to run on boot, run, `sudo make install`.
