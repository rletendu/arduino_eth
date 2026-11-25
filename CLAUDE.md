# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

PlatformIO project (env `uno`: atmelavr / Arduino framework) for an Arduino UNO with a Wiznet Ethernet shield (W5100/W5500). The firmware gets an IP via DHCP (falls back to static 192.168.1.177) and serves a small HTML status page (IP + uptime) on port 80. Code comments, README and serial/user-facing strings are in English.

## Commands

```
pio run                 # build
pio run -t upload       # flash the board
pio device monitor      # serial monitor (115200 baud)
pio run -t clean        # clean build
```

There are no tests (`test/` only holds the PlatformIO placeholder README) and no linter configured. Dependencies are `arduino-libraries/Ethernet@^2.0.2` and `olikraus/U8g2@^2.35.0` (declared in [platformio.ini](platformio.ini), installed into `.pio/libdeps/`, which is gitignored along with the rest of `.pio/`).

## Architecture

The whole firmware is a single file, [src/main.cpp](src/main.cpp), using the Arduino `setup()`/`loop()` model:

- `setup()`: opens Serial, configures the LED pins, deselects the SD card (pin 4 HIGH), calls `Ethernet.init(10)`, then does `Ethernet.begin(mac, fallbackIp)` first so `hardwareStatus()` can be checked *before* trying DHCP (halts forever on `EthernetNoHardware`, prints the detected chip otherwise). It then tries DHCP via `Ethernet.begin(mac)` with the static IP as fallback, warns if the cable is unplugged, and starts the `EthernetServer` on port 80.
- `loop()`: calls `Ethernet.maintain()` for DHCP lease renewal, then handles one client at a time with a blocking read loop. It captures the first request line (24-char buffer) and matches `GET /on` / `GET /off` by prefix to switch the LED, then replies with the same 200 page for any path once the blank line ending the headers arrives.
- A 128x64 SSD1306 OLED on I2C is driven through U8g2's text-only `U8x8` class (no frame buffer, 16 columns x 8 rows). `drawStatus()` redraws the whole screen; it is called once a second from `loop()` using a `millis()` timer (never `delay()`) and immediately from `setLed()`. `setLed()` guards the call with `statusReady` because it also runs in `setup()` before Ethernet is initialized. Text goes through `oledLine`/`oledLineP`, which pad to 16 columns so old characters are overwritten.
- The web page shows the LED state and a `<form action="/on|/off">` button offering the opposite action. A GET form with no fields is sent as `/on?` (trailing `?`), which is why the matching is prefix-only — don't add a trailing space to the match strings.

## Hardware constraints

- The shield uses SPI (pins 11/12/13) with CS on pin 10; pin 4 is the SD card CS. Don't use these pins for anything else.
- The LED is wired between A0 (anode, driven HIGH) and A1 (cathode, held LOW), with a series resistor.
- The OLED uses I2C on A4 (SDA) / A5 (SCL), address 0x3C. Avoid SPI displays: they would conflict with the Ethernet shield's bus. RAM is tight (~1 KB static used of 2 KB), and so is flash (~77%); avoid adding a frame-buffer display library (e.g. Adafruit SSD1306 needs a 1 KB buffer).
- The UNO has 2 KB of RAM: keep string literals wrapped in `F()` (as the existing code does) so they stay in flash.
- `setup()` blocks on `while (!Serial)`; the MAC address in the code must be unique on the network.
