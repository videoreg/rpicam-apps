# rpicam-apps
This is a small suite of libcamera-based applications to drive the cameras on a Raspberry Pi platform.

> [!NOTE]
> **This fork extends the original project with additional features:**
>
> - **AutoUpdateTextStage** — a post-processing stage that automatically updates the text annotation on frames by reading text from an external file. Allows changing the on-screen label dynamically without restarting the application. See [AUTO_UPDATE_TEXT_STAGE.md](AUTO_UPDATE_TEXT_STAGE.md) for details.

>[!WARNING]
>These applications and libraries have been renamed from `libcamera-*` to `rpicam-*`. Symbolic links to allow users to keep using the old application names have now been removed.

Build
-----
For usage and build instructions, see the official Raspberry Pi documentation pages [here.](https://www.raspberrypi.com/documentation/computers/camera_software.html#building-libcamera-and-rpicam-apps)

License
-------

The source code is made available under the simplified [BSD 2-Clause license](https://spdx.org/licenses/BSD-2-Clause.html).

Status
------

[![ToT libcamera build/run test](https://github.com/raspberrypi/rpicam-apps/actions/workflows/rpicam-test.yml/badge.svg)](https://github.com/raspberrypi/rpicam-apps/actions/workflows/rpicam-test.yml)
