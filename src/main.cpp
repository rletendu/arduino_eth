#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>

// MAC address (must be unique on your network)
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

// Fallback configuration if DHCP fails
IPAddress fallbackIp(192, 168, 1, 177);

EthernetServer server(80);

// LED wired between A0 and A1: A0 supplies the +, A1 acts as ground.
// Use a series resistor of about 220 ohms (UNO outputs are limited to 40 mA).
// If the LED stays off, flip it around.
const uint8_t LED_ANODE = A0;
const uint8_t LED_CATHODE = A1;
bool ledOn = false;

const uint8_t REQ_LEN = 24;  // max number of HTTP request line characters kept

void setLed(bool on) {
  ledOn = on;
  digitalWrite(LED_ANODE, on ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

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
      while (true) { delay(1); }
    case EthernetW5100: Serial.println(F("Chip detected: W5100")); break;
    case EthernetW5200: Serial.println(F("Chip detected: W5200")); break;
    case EthernetW5500: Serial.println(F("Chip detected: W5500")); break;
    default: break;
  }

  Serial.println(F("Starting Ethernet (DHCP)..."));
  if (Ethernet.begin(mac) == 0) {
    Serial.println(F("DHCP failed, using static IP"));
    Ethernet.begin(mac, fallbackIp);
  }

  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println(F("Ethernet cable not connected"));
  }

  server.begin();
  Serial.print(F("Server at http://"));
  Serial.println(Ethernet.localIP());
}

void loop() {
  Ethernet.maintain();  // DHCP lease renewal

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
