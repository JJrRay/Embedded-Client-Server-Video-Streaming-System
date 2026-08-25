# Embedded Client–Server Video Streaming System

A real-time video surveillance system built on an **Odroid-C2** (ARM64, Yocto
Linux). The board captures MJPEG frames from a USB camera and streams them over
a custom TCP protocol to a Linux workstation client, which decodes and displays
them with OpenCV.

The system fuses three independent hardware sources — camera, GPIO push button,
and a photoresistor on the SoC's ADC — to detect when the video feed can no
longer be trusted, and stays operational across network failures on either end.

Built for ELE4205 (Operating Systems and Hardware Interfaces) at Polytechnique
Montréal. Code comments and API documentation are in French.

---

## Architecture

```
        USB / OpenCV
   ┌──────────┐
   │  Camera  │
   └────┬─────┘
        ▼
   ┌───────────────┐   TCP/IP (bidirectional, resilient)    ┌──────────────┐
   │   Odroid-C2   │◄──────────────────────────────────────►│  Workstation │
   │    server     │        MJPEG frames @ ~30 fps          │    client    │
   └────┬──────────┘                                        └──────┬───────┘
        │                                                          │
        ├─── Push button      (libgpiod)                           ├── OpenCV window
        └─── Photoresistor    (iio / sysfs ADC)                    └── images/ on disk
```

The client drives every exchange: it sends `GET_FRAME`, the server answers. The
server never initiates traffic, which keeps flow control trivial and makes the
protocol easy to reason about under packet loss.

---

## Protocol

A one-byte header determines the shape of the rest of the message. IDs are
partitioned by direction: 1–99 client→server, 100–199 server→client, 200–255
events and errors.

| ID | Message | Payload after the header |
|---:|---------|--------------------------|
| 1 | `GET_FRAME` | — |
| 2 | `STOP` | — |
| 101 | `FRAME_HDR` | `frameId` + `jpegSize` + `JPEG bytes` |
| 102 | `STOP_ACK` | — |
| 200 | `BUTTON_PRESS` | same as `FRAME_HDR`; also triggers a disk save |
| 201 | `NO_LIGHT` | `frameId` only — no image sent |
| 202 | `SENSOR_ERROR` | `frameId` only — no image sent |

All multi-byte fields travel in network byte order (`htonl` / `ntohl`).

`frameId` counts **transmitted images**, so it stays frozen while the scene is
in an error state.

---

## Features

**Sensor fusion.** The photoresistor's raw ADC value is cross-checked against
the mean pixel intensity of each captured frame. Agreement on darkness means a
genuinely unlit room (`NO_LIGHT`); disagreement in either direction means the
lens is covered or a sensor has failed (`SENSOR_ERROR`). State changes must
persist for 200 ms before being reported, so a flickering condition never
reaches the client.

**Network resilience.** Neither program terminates on connection loss. The
hard part is that an unplugged Ethernet cable produces *no* TCP error — the
socket stays `ESTABLISHED` and `recv()` merely times out. Both sides therefore
track the time since the last successful exchange and infer the break from
accumulated silence. The client reconnects every 500 ms and resumes video in
well under the 10 s budget.

**Non-blocking connect.** A blocking `connect()` toward an unreachable host
stalls for roughly 130 seconds while the TCP stack retransmits SYNs, freezing
the UI. The client instead performs the handshake with `O_NONBLOCK` + `poll()`
+ `SO_ERROR`, bounding each attempt to 400 ms.

**Interrupt-free GPIO.** The Odroid-C2's controller exposes no interrupt on the
button line, so a dedicated thread samples it at 1 kHz with 200 ms debounce.
Presses are latched atomically and consumed only once an image is ready, so a
press is always paired with a frame that actually reached the client.

**Boot integration.** A SysV init script starts the server at boot, waiting for
`/dev/video0` to be enumerated before launching — the kernel probes USB in
parallel with the rest of the boot sequence.

---

## Repository layout

```
odroid/                  ARM64 server, cross-compiled for the Odroid-C2
  inc/  src/
  ele4205-serveur                SysV init script (boot auto-start)
  odroid-deploy-gdbserver.sh     scp + remote gdbserver launcher

poste/                   x86-64 client, builds natively on the workstation
  inc/  src/
```

The two sides are separate CMake projects because they target different
toolchains. `protocol.h` is byte-for-byte identical in both trees — it is the
communication contract and must never diverge.

Each side is organized one namespace per module: `Protocol`, `Config`,
`Vision`, `Gpio`, `Lighting`, `Network`, `Application` on the server;
`Protocol`, `Config`, `Network`, `Metrics`, `Storage`, `Application` on the
client.

---

## Building

**Server** (requires the Yocto SDK for the target):

```bash
source /opt/poky/4.0.28/environment-setup-cortexa53-poky-linux
cd odroid && mkdir -p build && cd build
cmake .. && make
```

**Client** (native, needs OpenCV 4):

```bash
cd poste && mkdir -p build && cd build
cmake .. && make
```

## Running

```bash
# on the board
scp odroid/build/prog root@192.168.7.2:/home/root/
ssh root@192.168.7.2 'cd /home/root && ./prog'

# on the workstation
./poste/build/prog
```

Start order does not matter — the client waits and connects on its own. Press
`q` in the video window to shut both sides down cleanly.

**Auto-start at boot:**

```bash
scp odroid/ele4205-serveur root@192.168.7.2:/etc/init.d/
ssh root@192.168.7.2
chmod +x /etc/init.d/ele4205-serveur
ln -s /etc/init.d/ele4205-serveur /etc/rc5.d/S99ele4205-serveur
```

Logs land in `/var/log/ele4205-serveur.log`.

---

## Design notes

**Two timeouts, one ordering constraint.** The server abandons a silent session
after 2 s; the client declares the link dead after 3 s. The inequality is
mandatory, not tuning. A successful `connect()` only means the *kernel*
completed the handshake — the connection may still be sitting in the backlog,
unaccepted. If the client gave up faster than the server released its stale
session, every reconnect would queue behind an unaccepted socket and the two
would ping-pong indefinitely.

**Rendering is driven by state, not by events.** The client recomposes its
window every cycle from a `DisplayState`, rather than painting when a response
arrives. This is what lets the elapsed-time counter for error states advance
smoothly even when responses stall, and it collapses the "connection lost"
screen onto the same code path for free.

**Bounded reads.** `receiveAll` carries a deadline. Without it, a cable pulled
mid-JPEG — statistically the most likely moment, since most of the cycle is
spent in that `recv` — would spin forever inside the read loop, and the
reconnection logic further up the stack would never be reached.

**Stack:** C++17, OpenCV 4, POSIX sockets, libgpiod, sysfs/iio, CMake,
Doxygen.
