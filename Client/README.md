# Client — Workstation

Native x86-64 client. Drives the exchange with the Odroid-C2, decodes the JPEG
stream, displays it with OpenCV, and saves the frames tied to a button press.

See the [top-level README](../README.md) for the protocol and system
architecture.

---

## Modules

| Namespace | Files | Responsibility |
|-----------|-------|----------------|
| `Protocol` | `protocol.h` | Message IDs and port. Byte-for-byte identical to the server's copy. |
| `Config` | `config.h` | Server address, timeouts, window and image settings, on-screen messages. |
| `Network` | `tcp_client.*` | Bounded connect, framed send/receive. Reusable after a break. |
| `Metrics` | `cycle_reporter.*` | Accumulates cycle durations, prints one line per second. |
| `Storage` | `image_store.*` | Prepares `images/`, writes JPEG bytes straight to disk. |
| `Application` | `client_app.*` | Owns the three above, runs the session loop, reconnects, renders. |

`main()` is the only function outside a namespace — the language requires it.

---

## Running

```bash
mkdir -p build && cd build
cmake .. && make
./prog
```

Needs OpenCV 4 and a working display. Start order does not matter: if the
server is not up, the window shows `CONNEXION PERDUE` and the client retries
every 500 ms until it answers.

Press **`q`** with the video window focused to shut down. The client sends
`STOP`, waits for `STOP_ACK`, and both processes exit cleanly. Keystrokes typed
into the terminal go nowhere — `cv::waitKey` only reads the OpenCV window.

Images tied to a button press land in `images/capture_frame_<id>.jpg`. The
directory is wiped at startup, so every run begins clean.

Once per second the client prints its throughput:

```
2026-08-13 19:17:31 | cycles/s: 29.8 | cycle moyen: 33.2 ms
```

---

## Display states

The window is recomposed from a `DisplayState` on **every** cycle rather than
painted when a response arrives. That is what keeps the elapsed-time counter
advancing smoothly even when responses stall, and it puts all four states on
one code path.

| State | Trigger | Shown |
|-------|---------|-------|
| `NORMAL` | `FRAME_HDR` / `BUTTON_PRESS` | Last decoded frame, with its ID overlaid |
| `NO_LIGHT` | `NO_LIGHT` from the server | `Lumiere insuffisante (t = 2.3 s)` on black |
| `SENSOR_ERROR` | `SENSOR_ERROR` from the server | `Capteur errone (t = 2.3 s)` on black |
| `LINK_LOST` | Link declared dead, or not yet connected | `CONNEXION PERDUE` on black |

The elapsed counter restarts only on a genuine state *change*, so it keeps
climbing as long as a condition persists and resets when the scene returns to
normal.

Messages carry no accents on purpose: `cv::putText` only ships the Hershey
fonts, which are ASCII-only and would render `è` as `?`.

---

## Timeouts

| Constant | Value | Scope | Purpose |
|----------|------:|-------|---------|
| `RECV_TIMEOUT_MS` | 60 ms | Kernel (`SO_RCVTIMEO`) | Bounds a single `recv`; produces the timeouts the others count |
| `CONNECT_TIMEOUT_MS` | 400 ms | While disconnected | Bounds one connection attempt |
| `RECONNECT_PERIOD_MS` | 500 ms | While disconnected | Spacing between attempts |
| `LINK_TIMEOUT_MS` | 3000 ms | While connected | Accumulated silence before declaring the link dead; also the deadline on `receiveAll` |
| `UI_POLL_MS` | 30 ms | While disconnected | Keyboard polling slice between attempts |

`LINK_TIMEOUT_MS` must stay **above** the server's 2 s idle timeout and **below**
the 5 s cable-pull test window. See the design notes in the top-level README.

---

## Failure handling

An unplugged cable produces no TCP error — the socket stays `ESTABLISHED` and
`recv()` merely times out. Three paths detect a break:

- **`recv() == 0`** — the peer sent a FIN. The server was killed or crashed.
  Detected instantly.
- **Accumulated timeouts** — nothing received for `LINK_TIMEOUT_MS`. This is
  the cable-pull case.
- **`receiveAll` deadline** — the break happened mid-JPEG, statistically the
  most likely moment since most of the cycle is spent in that read.

All three converge on the same recovery: close the socket, switch the window to
`LINK_LOST`, and retry. The first connection goes through the exact same path
as a reconnection, so there is no special case at startup.

`connect()` runs non-blocking with `poll()` and `SO_ERROR`. A blocking connect
toward an unreachable host stalls around 130 s while the stack retransmits
SYNs — the window would freeze and `q` would stop responding well past the 10 s
recovery budget.

---

## Troubleshooting

**`qt.qpa.xcb: could not connect to display`** — no X server reachable.
`cv::namedWindow` calls `abort()`. Check `echo $DISPLAY` and `xdpyinfo`; over
SSH you need `ssh -Y`. A VS Code Remote-SSH terminal often inherits a dead
tunnel from an earlier session, so a fresh SSH terminal is the quickest fix.

**`q` does nothing** — click the video window first. Only it receives the key.

**Cycles far below 30/s** — running the display through an X11 tunnel pushes an
uncompressed 800×600 frame over SSH on every `imshow`. That is the bottleneck,
not the network or the board.
