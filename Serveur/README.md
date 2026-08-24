# Server — Odroid-C2

ARM64 server running on Yocto Linux. Captures MJPEG frames from a USB camera,
cross-checks them against a photoresistor, watches a GPIO push button, and
serves one client at a time over TCP.

See the [top-level README](../README.md) for the protocol and system
architecture.

---

## Modules

| Namespace | Files | Responsibility |
|-----------|-------|----------------|
| `Protocol` | `protocol.h` | Message IDs and port. Byte-for-byte identical to the client's copy. |
| `Config` | `config.h` | Every tunable value: timeouts, GPIO line, ADC path, thresholds. |
| `Vision` | `camera.*` | `cv::VideoCapture` wrapper. Captures, measures mean brightness, encodes JPEG. |
| `Gpio` | `button.*` | Polls the button line in a dedicated thread, latches presses atomically. |
| `Lighting` | `light_monitor.*` | Reads the ADC, cross-checks it against image brightness, filters short-lived state changes. |
| `Network` | `tcp_server.*` | Listen socket, `accept`, framed send/receive. Knows nothing about the application. |
| `Application` | `server_app.*` | Owns the four above, dispatches commands, decides what to answer. |

`main()` is the only function outside a namespace — the language requires it.

---

## Hardware

| Device | Interface | Config constant |
|--------|-----------|-----------------|
| USB camera | V4L2 via OpenCV, 800×600 MJPEG | `FRAME_WIDTH`, `FRAME_HEIGHT`, `CAMERA_FPS` |
| Push button | `libgpiod`, `gpiochip1` line 92, active low | `GPIO_CHIP`, `GPIO_LINE`, `BUTTON_ACTIVE_LOW` |
| Photoresistor | `sysfs` / `iio`, `in_voltage0_raw` (AIN0, pin 40) | `LIGHT_SENSOR_PATH`, `LIGHT_RAW_THRESHOLD` |

The Odroid-C2's GPIO controller exposes no interrupt on these lines, so the
button is sampled at 1 kHz in a worker thread with 200 ms debounce. This keeps
capture and networking off the GPIO's critical path entirely.

The ADC channel is re-opened on every read: the `saradc` driver triggers a
conversion on `open()`, so a cached file handle would return a frozen value.

### Calibration

`LIGHT_RAW_THRESHOLD = 968` with `LIGHT_RAW_RISES = false` — the raw value
*rises* as the room darkens, because the photoresistor sits on the high side of
the divider. Measured references: ~660 under a lamp, ~931 ambient, ~1014 with
the sensor covered.

`IMAGE_DARK_THRESHOLD = 40.0` on a 0–255 mean. A normal scene reads 80–140; a
covered lens drops below 15.

Recalibrate with:

```bash
cat /sys/bus/iio/devices/iio:device0/in_voltage0_raw
```

---

## Build

Requires the Yocto SDK for the target:

```bash
source /opt/poky/4.0.28/environment-setup-cortexa53-poky-linux
mkdir -p build && cd build
cmake .. && make
```

`CMakeLists.txt` globs `inc/*.h` and `src/*.cpp`, so new files are picked up
after a CMake reconfigure.

## Deploy

```bash
./odroid-deploy-gdbserver.sh
```

Stops the boot service, kills any stale `gdbserver`, copies the binary, and
launches `gdbserver` on port 3000. This is the script wired to `preLaunchTask`
in `.vscode/launch.json`, so pressing F5 does all of it.

Manual alternative:

```bash
scp build/prog root@192.168.7.2:/home/root/
ssh root@192.168.7.2 'cd /home/root && ./prog'
```

## Auto-start at boot

```bash
scp ele4205-serveur root@192.168.7.2:/etc/init.d/
ssh root@192.168.7.2
chmod +x /etc/init.d/ele4205-serveur
ln -s /etc/init.d/ele4205-serveur /etc/rc5.d/S99ele4205-serveur
/etc/init.d/ele4205-serveur start
```

The image uses BusyBox/SysV init, not systemd — `cat /proc/1/comm` returns
`init`. The script waits up to 15 s for `/dev/video0` to appear, since the
kernel enumerates USB in parallel with the boot sequence and `VideoCapture`
would otherwise fail on a cold start.

Stopping is deliberately patient: `SIGTERM` first, `SIGKILL` only after 5 s. A
hard kill mid-capture can leave the UVC driver wedged and make `/dev/video0`
disappear until the next reboot.

Logs go to `/var/log/ele4205-serveur.log`. Note that SysV has no equivalent of
`Restart=always` — if the server dies it stays dead until the next boot.

---

## Behaviour under failure

The server never terminates on a connection problem. Only an explicit `STOP`
ends the process; every other outcome returns it to `accept()`.

Two mechanisms make that hold:

- `SIGPIPE` is ignored and every `send` uses `MSG_NOSIGNAL`, so writing to a
  departed client returns an error instead of killing the process.
- A session with no command for `CLIENT_IDLE_TIMEOUT_MS` (2 s) is abandoned.
  An unplugged cable produces no TCP error at all — `recv()` just keeps timing
  out — so without this the server would stay bound to a client that no longer
  exists and never accept the new connection.

That 2 s must stay **below** the client's 3 s link timeout. See the design
notes in the top-level README.

---

## Troubleshooting

**`Caméra indisponible : /dev/video0 absent ou déjà utilisé`** — either another
`prog` still holds the device (V4L2 does not share), or the camera was
re-enumerated onto a different index after a hot unplug. Check with
`ls -l /dev/video*`; a reboot restores index 0.

**`meson-saradc: failed to read sample`** in `dmesg` — the ADC occasionally
drops samples or returns the wrong channel. `readRaw()` returns `std::nullopt`
and `classify()` falls back to `NORMAL` rather than raising a false alarm.

**Nothing accepts on port 4099** — the boot service is probably already
running. `/etc/init.d/ele4205-serveur stop` before debugging.
