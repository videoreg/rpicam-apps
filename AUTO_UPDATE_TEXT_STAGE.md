# AutoUpdateTextStage

## Description

AutoUpdateTextStage is a post-processing stage for rpicam-apps that automatically updates annotation text from an external file. It is designed to work together with AnnotateCvStage.

## Features

- **Automatic updates**: Reads text from a file every 5 seconds
- **Low CPU usage**: Uses a background thread that wakes up once per second
- **Thread safety**: Three separate mutexes for `current_text_`, `file_path_`, and `last_read_time_`
- **Dynamic configuration**: The file path can be set either in the JSON config or via per-frame metadata
- **Multi-line text**: All lines from the file are read and joined with `\n`
- **Threadless mode**: When used with `rpicam-jpeg` (where `Start()` is never called), the file is read directly inside `Process()` at the same 5-second interval

## Usage

### Post processing file format

To display text on screen, combine with AnnotateCvStage:

```json
{
    "auto_update_text": {
        "file": "/tmp/text_overlay.txt"
    },
    "annotate_cv": {
        "text": "Default text",
        "fg": 255,
        "bg": 0,
        "scale": 1.0,
        "thickness": 2,
        "alpha": 0.3
    }
}
```

## Example call

```bash
# Terminal 1: start the camera
rpicam-vid --post-process-file auto_update_text_with_annotate.json -t 0

# Terminal 2: update the text
while true; do
    date "+%Y-%m-%d %H:%M:%S" > /tmp/text_overlay.txt
    sleep 1
done
```

This example displays the current time on screen. The file is updated every second, but the camera re-reads it every 5 seconds.

## Configuration parameters

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `file` | string | Path to the text file | "" (empty string) |


## Building

AutoUpdateTextStage is included in the OpenCV post-processing stages build. Make sure OpenCV is enabled:

```bash
meson setup build -Denable_opencv=enabled
meson compile -C build
```
