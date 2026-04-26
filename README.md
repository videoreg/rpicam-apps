# rpicam-apps

> [!NOTE]
> **This fork extends the original [raspberrypi/rpicam-apps](https://github.com/raspberrypi/rpicam-apps) project with additional features for [Pi-Videoreg project](https://github.com/videoreg/pi-videoreg):**
>
>
> - **AutoUpdateTextStage** — a post-processing stage that automatically updates the text annotation on frames by reading text from an external file. Allows changing the on-screen label dynamically without restarting the application. See [AUTO_UPDATE_TEXT_STAGE.md](AUTO_UPDATE_TEXT_STAGE.md) for details.
>
> - `--output` now accepts placeholders with date and time (see table below).
> 
> - Added `--screenshot` argument that saves first frame of every segment (chunk) of a video to specified path template (takes screenshot). Usefull with `--segment`. Example: `--screenshot /home/admin/screenshots/%V.jpg`, where `%V` is name of video segment. Aim is to fasten video previews extraction.
>

| Placeholder | Value | Example |
|--------------|----------|--------|
| %Y | year (4 dights) | 2026 |
| %y | year (2 dights) | 26 |
| %m | month | 03 |
| %d | already used to substitute the number of segment |
| %H | hours | 14 |
| %M | minutes | 30 |
| %S | seconds | 00 |
| %F | %Y-%m-%d	| 2026-03-07 |
| %T | %H:%M:%S	| 14:30:00 |

Original
--------

This is a small suite of libcamera-based applications to drive the cameras on a Raspberry Pi platform.

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
