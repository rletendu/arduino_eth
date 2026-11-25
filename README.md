# arduino_eth

Projet PlatformIO : Arduino UNO + shield Ethernet Wiznet (W5100 / W5500).

Le firmware obtient une IP par DHCP (repli sur 192.168.1.177) et sert une page web sur le port 80.

## Montage
Enficher le shield Ethernet sur l'UNO et brancher le câble réseau. Le shield utilise SPI (pins 11/12/13) et CS sur la pin 10 ; ne pas les utiliser pour autre chose (la pin 4 est le CS de la carte SD).

## Commandes
```
pio run                 # compiler
pio run -t upload       # flasher
pio device monitor      # moniteur série (115200)
```
