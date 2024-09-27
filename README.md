# melonpad
basic framework for custom code for the WiiU gamepad

## BIG FAT WARNING

For now, this codebase is provided for educational and documentation purposes.

DO NOT ATTEMPT TO RUN THIS ON YOUR WIIU GAMEPAD UNLESS YOU KNOW WHAT YOU ARE DOING.

I am currently using a FPGA to upload custom code to the gamepad. 
If your gamepad isn't equipped with that kind of hardware mod, the only way to upload code to the gamepad is via the wifi
update functionality. However, this requires having a functional stock firmware running on the gamepad.

IF YOU UPLOAD THIS TO YOUR GAMEPAD, YOU WILL BE UNABLE TO RESTORE IT TO STOCK FUNCTIONALITY, OR UPLOAD A NEW BINARY,
UNLESS YOU RESORT TO HARDWARE MODDING.

I have plans to work around this, but for now, you're warned.

## Why?

This is a little project I've had since 2016. Since the WiiU gamepad has its own CPU and RAM, it is _technically_
able to run as a standalone device, without requiring a WiiU or a PC. I've always thought it would be nifty to do so.

I've made attempts back in 2016 and 2022, but never been far at all. Obviously, there's no official documentation
on the gamepad's internals, so I have to figure out things by looking at the stock firmware's code and experimenting
with the hardware. Also, the gamepad's firmware is stored on a FLASH memory which is soldered to the motherboard, which
complicates things. As mentioned above, the only way to upload a binary to the gamepad is over wifi, but that way requires
having a functional firmware, which excludes any sort of reverse-engineering work or general fumbling. Getting around that
requires some amount of hardware modding.

Lately I felt like giving it a new try with better means. I decided to replace the FLASH memory with a FPGA that emulates
it. The advantages are that I can very easily upload code to the gamepad, but also that the FPGA can act as a debug output,
which has been very helpful. Figuring out how to turn on the gamepad's LCD was what I was stuck on, and the FPGA debug output
helped me a lot there.

This is fun and all, but the FPGA-based hardware mod isn't accessible to everybody out there. It's also not a permanent
solution -- the FPGA wouldn't even fit in the gamepad's case. We do have plans to make things easier -- see the roadmap down there.

## Currently supported

* I2C
* SPI
* LCD and framebuffer initialization

## Roadmap

* Adapting newlib to this environment
* Input
* Wifi
* Audio, and the other fun devices the gamepad has

The plan is to eventually provide a loader of sorts, that retains compatibility with the original firmware update
functionality (so new binaries can be flashed this way). The loader would also support launching the stock firmware,
and restoring stock functionality entirely (basically uninstalling the loader).

The loader would also provide a way to store and launch homebrew apps, providing a basic environment where you wouldn't
brick your gamepad at the first mistake.

Ideally, we could even figure out a way to add a SD card to the gamepad, to make loading apps easier. But at this point,
we're hyperphanting.

## Gamepad documentation

We're also documenting the gamepad's internals: https://kuribo64.net/wup/doku.php?id=start
