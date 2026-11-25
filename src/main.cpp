#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>

// Adresse MAC (à rendre unique sur votre réseau)
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

// Configuration de repli si le DHCP échoue
IPAddress fallbackIp(192, 168, 1, 177);

EthernetServer server(80);

// LED câblée entre A0 et A1 : A0 fournit le +, A1 sert de masse.
// Prévoir une résistance de ~220 Ω en série (les sorties UNO sont limitées à 40 mA).
// Si la LED reste éteinte, inverser son sens.
const uint8_t LED_ANODE = A0;
const uint8_t LED_CATHODE = A1;
bool ledOn = false;

const uint8_t REQ_LEN = 24;  // taille max conservée de la ligne de requête HTTP

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

  // Désélectionner la carte SD (CS pin 4) pour qu'elle ne perturbe pas le bus SPI
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  Ethernet.init(10);  // CS du shield Ethernet

  // Initialisation matérielle (IP statique) pour tester la présence du shield
  // avant de tenter le DHCP
  Ethernet.begin(mac, fallbackIp);
  switch (Ethernet.hardwareStatus()) {
    case EthernetNoHardware:
      Serial.println(F("Shield Ethernet non detecte"));
      while (true) { delay(1); }
    case EthernetW5100: Serial.println(F("Puce detectee : W5100")); break;
    case EthernetW5200: Serial.println(F("Puce detectee : W5200")); break;
    case EthernetW5500: Serial.println(F("Puce detectee : W5500")); break;
    default: break;
  }

  Serial.println(F("Demarrage Ethernet (DHCP)..."));
  if (Ethernet.begin(mac) == 0) {
    Serial.println(F("DHCP echec, IP statique"));
    Ethernet.begin(mac, fallbackIp);
  }

  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println(F("Cable Ethernet non connecte"));
  }

  server.begin();
  Serial.print(F("Serveur sur http://"));
  Serial.println(Ethernet.localIP());
}

void loop() {
  Ethernet.maintain();  // renouvellement du bail DHCP

  EthernetClient client = server.available();
  if (!client) return;

  bool lineBlank = true;
  bool requestLineDone = false;
  char requestLine[REQ_LEN];
  uint8_t reqIdx = 0;
  while (client.connected()) {
    if (!client.available()) continue;
    char c = client.read();

    // Première ligne de la requête (ex. "GET /on HTTP/1.1") : commande LED
    if (!requestLineDone) {
      if (c == '\n') {
        requestLine[reqIdx] = '\0';
        requestLineDone = true;
        // Préfixe seulement : un formulaire GET envoie "/on?" (avec un '?' final)
        if (strncmp_P(requestLine, PSTR("GET /on"), 7) == 0) setLed(true);
        else if (strncmp_P(requestLine, PSTR("GET /off"), 8) == 0) setLed(false);
      } else if (c != '\r' && reqIdx < REQ_LEN - 1) {
        requestLine[reqIdx++] = c;
      }
    }

    // Fin des en-têtes HTTP : ligne vide
    if (c == '\n' && lineBlank) {
      client.println(F("HTTP/1.1 200 OK"));
      client.println(F("Content-Type: text/html"));
      client.println(F("Connection: close"));
      client.println();
      client.println(F("<!DOCTYPE HTML><html><body>"));
      client.println(F("<h1>Arduino UNO + Ethernet</h1>"));
      client.print(F("<p>IP : "));
      client.print(Ethernet.localIP());
      client.println(F("</p>"));
      client.print(F("<p>Uptime : "));
      client.print(millis() / 1000);
      client.println(F(" s</p>"));
      client.print(F("<p>LED : "));
      client.print(ledOn ? F("allumee") : F("eteinte"));
      client.println(F("</p>"));
      // Le bouton propose l'action inverse de l'état courant
      client.print(F("<form action=\"/"));
      client.print(ledOn ? F("off") : F("on"));
      client.print(F("\"><button type=\"submit\">"));
      client.print(ledOn ? F("Eteindre la LED") : F("Allumer la LED"));
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
