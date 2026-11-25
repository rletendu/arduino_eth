#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Ethernet.h>
#include <U8x8lib.h>

// MAC address (must be unique on your network)
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

// Fallback configuration if DHCP fails
IPAddress fallbackIp(192, 168, 1, 177);

EthernetServer server(80);

// SSD1306 128x64 OLED on I2C (SDA = A4, SCL = A5, address 0x3C).
// Text-only U8x8 mode: no frame buffer, so almost no RAM used. 16 columns x 8 rows.
U8X8_SSD1306_128X64_NONAME_HW_I2C oled(U8X8_PIN_NONE);
const uint32_t DISPLAY_REFRESH_MS = 1000;
bool statusReady = false;  // true once Ethernet is up and the status screen can be drawn
bool usingDhcp = false;
uint32_t lastDisplayMs = 0;

// LED wired between A0 and A1: A0 supplies the +, A1 acts as ground.
// Use a series resistor of about 220 ohms (UNO outputs are limited to 40 mA).
// If the LED stays off, flip it around.
const uint8_t LED_ANODE = A0;
const uint8_t LED_CATHODE = A1;
bool ledOn = false;

const uint8_t REQ_LEN = 24;  // max number of HTTP request line characters kept

// Draw a line padded to the full 16 columns (overwrites any previous content).
void oledLine(uint8_t row, const char* text) {
  char buf[17];
  snprintf_P(buf, sizeof(buf), PSTR("%-16s"), text);
  oled.drawString(0, row, buf);
}

// Same, but for a text stored in flash (PSTR).
void oledLineP(uint8_t row, PGM_P text) {
  char buf[17];
  snprintf_P(buf, sizeof(buf), PSTR("%-16S"), text);
  oled.drawString(0, row, buf);
}

void drawStatus() {
  char line[17];

  oledLineP(0, PSTR("UNO + Ethernet"));

  PGM_P chip;
  switch (Ethernet.hardwareStatus()) {
    case EthernetW5100: chip = PSTR("W5100"); break;
    case EthernetW5200: chip = PSTR("W5200"); break;
    case EthernetW5500: chip = PSTR("W5500"); break;
    default:            chip = PSTR("?");     break;
  }
  snprintf_P(line, sizeof(line), PSTR("%S %S"), chip, usingDhcp ? PSTR("DHCP") : PSTR("Static"));
  oledLine(2, line);

  IPAddress ip = Ethernet.localIP();
  snprintf_P(line, sizeof(line), PSTR("%u.%u.%u.%u"), ip[0], ip[1], ip[2], ip[3]);
  oledLine(3, line);

  PGM_P link;
  switch (Ethernet.linkStatus()) {
    case LinkON:  link = PSTR("UP");   break;
    case LinkOFF: link = PSTR("DOWN"); break;
    default:      link = PSTR("?");    break;
  }
  snprintf_P(line, sizeof(line), PSTR("Link: %S"), link);
  oledLine(5, line);

  snprintf_P(line, sizeof(line), PSTR("LED: %S"), ledOn ? PSTR("ON") : PSTR("OFF"));
  oledLine(6, line);

  snprintf_P(line, sizeof(line), PSTR("Up: %lus"), millis() / 1000);
  oledLine(7, line);
}

void setLed(bool on) {
  ledOn = on;
  digitalWrite(LED_ANODE, on ? HIGH : LOW);
  if (statusReady) drawStatus();  // show the new LED state immediately
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  oled.begin();
  oled.setFont(u8x8_font_chroma48medium8_r);
  oledLineP(0, PSTR("UNO + Ethernet"));
  oledLineP(2, PSTR("Starting..."));

  pinMode(LED_ANODE, OUTPUT);
  pinMode(LED_CATHODE, OUTPUT);
  digitalWrite(LED_CATHODE, LOW);
  setLed(false);

  // Deselect the SD card (CS pin 4) so it does not disturb the SPI bus
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  Ethernet.init(10);  // Ethernet shield CS

  // Hardware initialization (static IP) to check the shield is present
  // before attempting DHCP
  Ethernet.begin(mac, fallbackIp);
  switch (Ethernet.hardwareStatus()) {
    case EthernetNoHardware:
      Serial.println(F("Ethernet shield not detected"));
      oledLineP(2, PSTR("No shield!"));
      while (true) { delay(1); }
    case EthernetW5100: Serial.println(F("Chip detected: W5100")); break;
    case EthernetW5200: Serial.println(F("Chip detected: W5200")); break;
    case EthernetW5500: Serial.println(F("Chip detected: W5500")); break;
    default: break;
  }

  Serial.println(F("Starting Ethernet (DHCP)..."));
  oledLineP(2, PSTR("DHCP..."));
  if (Ethernet.begin(mac) == 0) {
    Serial.println(F("DHCP failed, using static IP"));
    Ethernet.begin(mac, fallbackIp);
    usingDhcp = false;
  } else {
    usingDhcp = true;
  }

  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println(F("Ethernet cable not connected"));
  }

  server.begin();
  Serial.print(F("Server at http://"));
  Serial.println(Ethernet.localIP());

  statusReady = true;
  drawStatus();
  lastDisplayMs = millis();
}

void loop() {
  Ethernet.maintain();  // DHCP lease renewal

  // Periodic, non-blocking display refresh (uptime, link status, IP after renewal)
  if (millis() - lastDisplayMs >= DISPLAY_REFRESH_MS) {
    lastDisplayMs = millis();
    drawStatus();
  }

  EthernetClient client = server.available();
  if (!client) return;

  bool lineBlank = true;
  bool requestLineDone = false;
  char requestLine[REQ_LEN];
  uint8_t reqIdx = 0;
  while (client.connected()) {
    if (!client.available()) continue;
    char c = client.read();

    // First request line (e.g. "GET /on HTTP/1.1"): LED command
    if (!requestLineDone) {
      if (c == '\n') {
        requestLine[reqIdx] = '\0';
        requestLineDone = true;
        // Prefix match only: a GET form sends "/on?" (with a trailing '?')
        if (strncmp_P(requestLine, PSTR("GET /on"), 7) == 0) setLed(true);
        else if (strncmp_P(requestLine, PSTR("GET /off"), 8) == 0) setLed(false);
      } else if (c != '\r' && reqIdx < REQ_LEN - 1) {
        requestLine[reqIdx++] = c;
      }
    }

    // End of HTTP headers: blank line
    if (c == '\n' && lineBlank) {
      client.println(F("HTTP/1.1 200 OK"));
      client.println(F("Content-Type: text/html"));
      client.println(F("Connection: close"));
      client.println();
      client.println(F("<!DOCTYPE HTML><html><body>"));
      client.println(F("<h1>Arduino UNO + Ethernet</h1>"));
      client.print(F("<p>IP: "));
      client.print(Ethernet.localIP());
      client.println(F("</p>"));
      client.print(F("<p>Uptime: "));
      client.print(millis() / 1000);
      client.println(F(" s</p>"));
      client.print(F("<p>LED: "));
      client.print(ledOn ? F("on") : F("off"));
      client.println(F("</p>"));
      // The button offers the opposite of the current state
      client.print(F("<form action=\"/"));
      client.print(ledOn ? F("off") : F("on"));
      client.print(F("\"><button type=\"submit\">"));
      client.print(ledOn ? F("Turn LED off") : F("Turn LED on"));
      client.println(F("</button></form>"));
      client.println(F("</body></html>"));
      break;
    }
    if (c == '\n') lineBlank = true;
    else if (c != '\r') lineBlank = false;
  }
  delay(1);
  client.stop();
}
