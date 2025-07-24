#include <LoRa.h>
#include <TinyGPS++.h>

#include "LoRaBoards.h"
#include "SSD1306.h"

#define SCK 5    // GPIO5  -- SX1278's SCK
#define MISO 19  // GPIO19 -- SX1278's MISnO
#define MOSI 27  // GPIO27 -- SX1278's MOSI
#define SS 18    // GPIO18 -- SX1278's CS
#define RST 14   // GPIO14 -- SX1278's RESET
#define DI0 26   // GPIO26 -- SX1278's IRQ(Interrupt Request)
#define BAND 868E6

#define LORA_FREQ 868E6

unsigned int counter = 0;

SSD1306 display(0x3c, 21, 22);
String rssi = "RSSI --";
String packSize = "--";
String packet;

byte broadcastAddress = 0xFF;
byte localAddress = 0xF2;  // address of this device

TinyGPSPlus gps;
// HardwareSerial SerialGPS(1);

// void LoRa_txMode();
// void LoRa_rxMode();

void displayInfo();

void LoRa_sendMessage(String message) {
  // LoRa_txMode();                        // set tx mode
  LoRa.beginPacket();  // start packet
  LoRa.print(broadcastAddress);
  LoRa.write(localAddress);
  LoRa.print(message);  // add payload
  LoRa.endPacket();     // finish packet and send it
  // LoRa_rxMode();                        // set rx mode
}

void LoRa_onReceive(int packetSize) {
  Serial.println("lora on receive");

  if (packetSize == 0) return;  // if there's no packet, return

  // read packet header bytes:
  int recipient = LoRa.read();  // recipient address
  byte sender = LoRa.read();    // sender address

  String incoming = "";

  while (LoRa.available()) {
    incoming += (char)LoRa.read();
  }

  // if message is for this device, or broadcast, print details:
  Serial.println("Received from: 0x" + String(sender, HEX));
  Serial.println("Sent to: 0x" + String(recipient, HEX));
  Serial.println("Message: " + incoming);
  Serial.println("RSSI: " + String(LoRa.packetRssi()));
  Serial.println("Snr: " + String(LoRa.packetSnr()));
  // Serial.println();

  /*
  display.drawString(0, 20, String(sender, HEX) + ">" + String(recipient, HEX));
  Serial.print(" dis1 ");
  display.drawString(0, 30, "RSSI>" + String(LoRa.packetRssi()) + ",SNR>" +
  String(LoRa.packetSnr())); Serial.print(" dis2 "); display.drawString(0, 40,
  "Message: " + incoming); Serial.print(" dis3 "); display.display();
  */
  Serial.println("onReceive Done");
}

void LoRa_onTxDone() { Serial.println("TxDone"); }

static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    while (SerialGPS.available()) gps.encode(SerialGPS.read());
  } while (millis() - start < ms);
}

void setup() {
  setupBoards();
  delay(1500);

  // Initialising the UI will init the display too.
  display.init();

  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  Serial.println("Init LoRa");
  LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }

  LoRa.setTxPower(2);
  LoRa.setSpreadingFactor(8);
  LoRa.setFrequency(868E6);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x05);
  LoRa.disableCrc();

  // register tx done callback
  // LoRa.onTxDone(LoRa_onTxDone);
  // register the receive callback
  // LoRa.onReceive(LoRa_onReceive);

  // put the radio into receive mode
  LoRa.receive();
  Serial.println("LoRa success");

  delay(500);
  Serial.println("setup finished");
}

void loop() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  display.drawString(100, 0, "0x" + String(localAddress, HEX));

  if (gps.location.isUpdated()) {
    display.drawString(0, 0, "GPS [" + String(gps.satellites.value()) + "]");
  } else {
    display.drawString(0, 0, "--- [" + String(gps.satellites.value()) + "]");
  }

  // try to parse packet
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    // received a packet
    Serial.print("Received packet '");

    // read packet
    while (LoRa.available()) {
      Serial.print((char)LoRa.read());
    }

    // print RSSI of packet
    Serial.print("' with RSSI ");
    Serial.println(LoRa.packetRssi());
    Serial.println(LoRa.packetFrequencyError());
  }

  // try to parse packet
  // int packetSize = LoRa.parsePacket();
  // Serial.println(String(packetSize));
  /*
  if(gps.time.isUpdated()) {
    display.drawString(0, 10, "GPS time " + String(gps.time.age()));
  }
  else {
    display.drawString(0, 10, "no GPS time");
  }

  if (gps.satellites.isUpdated())
  {
    display.drawString(0, 20, "SAT fix age " + String(gps.satellites.age()) + ",
  " + );
  }
  */
  display.display();

  // GPS
  /*
  Serial.print("Latitude  : ");
  Serial.println(gps.location.lat(), 5);
  Serial.print("Longitude : ");
  Serial.println(gps.location.lng(), 4);
  Serial.print("Satellites: ");
  Serial.println(gps.satellites.value());
  Serial.print("Altitude  : ");
  Serial.print(gps.altitude.feet() / 3.2808);
  Serial.println("M");
  Serial.print("Time      : ");
  Serial.print(gps.time.hour());
  Serial.print(":");
  Serial.print(gps.time.minute());
  Serial.print(":");
  Serial.println(gps.time.second());
  Serial.println("**********************");
  */
  smartDelay(1000);

  if (millis() > 5000 && gps.charsProcessed() < 10)
    Serial.println(F("No GPS data received: check wiring"));

  // LoRa.receive();

  delay(1000);
}

void displayInfo() {
  Serial.print(F("Location: "));
  if (gps.location.isValid()) {
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(","));
    Serial.print(gps.location.lng(), 6);
  } else {
    Serial.print(F("INVALID"));
  }

  Serial.print(F("  Date/Time: "));
  if (gps.date.isValid()) {
    Serial.print(gps.date.month());
    Serial.print(F("/"));
    Serial.print(gps.date.day());
    Serial.print(F("/"));
    Serial.print(gps.date.year());
  } else {
    Serial.print(F("INVALID"));
  }

  Serial.print(F(" "));
  if (gps.time.isValid()) {
    if (gps.time.hour() < 10) Serial.print(F("0"));
    Serial.print(gps.time.hour());
    Serial.print(F(":"));
    if (gps.time.minute() < 10) Serial.print(F("0"));
    Serial.print(gps.time.minute());
    Serial.print(F(":"));
    if (gps.time.second() < 10) Serial.print(F("0"));
    Serial.print(gps.time.second());
    Serial.print(F("."));
    if (gps.time.centisecond() < 10) Serial.print(F("0"));
    Serial.print(gps.time.centisecond());
  } else {
    Serial.print(F("INVALID"));
  }

  Serial.println();
}