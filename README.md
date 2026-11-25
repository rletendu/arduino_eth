# arduino_eth

PlatformIO project: Arduino UNO + Wiznet Ethernet shield (W5100 / W5500).

The firmware obtains an IP address via DHCP (falling back to 192.168.1.177) and serves a web page on port 80. The page shows the IP address and uptime, and has a button to turn an LED on and off.

## Wiring
Plug the Ethernet shield onto the UNO and connect the network cable. The shield uses SPI (pins 11/12/13) and CS on pin 10; do not use these for anything else (pin 4 is the SD card CS).

The LED is wired between pins A0 (anode, +) and A1 (cathode), with a series resistor of about 220 ohms. If it does not light up, flip it around.

## Commands
```
pio run                 # build
pio run -t upload       # flash
pio device monitor      # serial monitor (115200)
```
