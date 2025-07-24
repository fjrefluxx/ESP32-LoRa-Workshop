/**
 * ESP32+LoRa Workshop
 *
 * Sender for a fixed GPS Location, used as distractor and additional challenge
 * for Challenge 2. Trigger LoRa transmission on/off via PRG (middle) button.
 *
 * Written for the LilyGO T-Beam v1.2 SX1276.
 *
 * Examples see:
 * https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series
 *
 * For the German frequency bands and restrictions consult:
 * https://www.bundesnetzagentur.de/SharedDocs/Downloads/DE/Sachgebiete/Telekommunikation/Unternehmen_Institutionen/Frequenzen/Allgemeinzuteilungen/FunkanlagenGeringerReichweite/2018_05_SRD_pdf.pdf?__blob=publicationFile&v=7
 *
 */
#include <RadioLib.h>
#include <TinyGPS++.h>

#include "LoRaBoards.h"
#include "SSD1306.h"

#define ARRAY_SIZE(x) sizeof(x) / sizeof(x[0])

SSD1306 display(0x3c, 21, 22);
TinyGPSPlus gps;

#include "Button2.h"
Button2 prgBtn;

#define CONFIG_RADIO_FREQ 869.525     // MHz
#define CONFIG_RADIO_OUTPUT_POWER 10  // 17 std, 2-20
#define CONFIG_RADIO_BW 250.0         // kHz
#define CONFIG_RADIO_SF 9
#define CONFIG_RADIO_CR 5
#define CONFIG_RADIO_SYNC 0x42

#define LORA_DUTY_CYCLE_INTERVAL 15000  // 15s

SX1276 radio =
    new Module(RADIO_CS_PIN, RADIO_DIO0_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

// save transmission state between loops
static int transmissionState = RADIOLIB_ERR_NONE;
// flag to indicate that a packet was sent
static volatile bool lora_tx_available = false;
static uint32_t counter = 0;
// static String payload;

byte broadcastAddress = 0xFF;
byte localAddress = 0xCD;  // address of this device

bool transmit_loop = false;
RadioLibTime_t lora_transmission_end_time = 0;

///
///
bool lora_transmit_available();
bool lora_dutyCycle_available();
bool lora_send_packet(String payload, byte recipientAddress);
bool lora_send_packet(byte payload[], size_t size, byte recipientAddress);

void click_callback(Button2& b);

double fix_lat = 80.82703;
double fix_lon = -66.46059;
String loc = "(80.82703,-66.46059)";

// callback when transmission is completed
ICACHE_RAM_ATTR void callback_lora_tx_finished(void) {
  // we sent a packet, set the flag
  Serial.println("Packet sent complete");
  lora_tx_available = true;

  lora_transmission_end_time = millis();

  if (transmissionState == RADIOLIB_ERR_NONE) {
    // packet was successfully sent
    Serial.println(F("transmission finished!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(transmissionState);
  }
}

void setup() {
  setupBoards();
  delay(1500);

  // Initialising the UI will init the display too.
  display.init();

  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  // initialize radio with default settings
  Serial.print(F("Radio Initializing ... "));
  int state = radio.begin();

  printResult(state == RADIOLIB_ERR_NONE);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
  }

  /*
   *   Sets carrier frequency.
   *   SX1278/SX1276 : Allowed values range from 137.0 MHz to 525.0 MHz.
   *   SX1268/SX1262 : Allowed values are in range from 150.0 to 960.0 MHz.
   * * * */
  if (radio.setFrequency(CONFIG_RADIO_FREQ) == RADIOLIB_ERR_INVALID_FREQUENCY) {
    Serial.println(F("Selected frequency is invalid for this module!"));
    while (true);
  }

  /*
   *   Sets LoRa link bandwidth.
   *   SX1278/SX1276 : Allowed values are 10.4, 15.6, 20.8, 31.25, 41.7, 62.5,
   * 125, 250 and 500 kHz. Only available in %LoRa mode. SX1268/SX1262 : Allowed
   * values are 7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125.0, 250.0 and 500.0
   * kHz.
   * * * */
  if (radio.setBandwidth(CONFIG_RADIO_BW) == RADIOLIB_ERR_INVALID_BANDWIDTH) {
    Serial.println(F("Selected bandwidth is invalid for this module!"));
    while (true);
  }

  /*
   * Sets LoRa link spreading factor.
   * SX1278/SX1276 :  Allowed values range from 6 to 12. Only available in LoRa
   * mode. SX1262        :  Allowed values range from 5 to 12.
   * * * */
  if (radio.setSpreadingFactor(CONFIG_RADIO_SF) ==
      RADIOLIB_ERR_INVALID_SPREADING_FACTOR) {
    Serial.println(F("Selected spreading factor is invalid for this module!"));
    while (true);
  }

  /*
   * Sets LoRa coding rate denominator.
   * SX1278/SX1276/SX1268/SX1262 : Allowed values range from 5 to 8. Only
   * available in LoRa mode.
   * * * */
  if (radio.setCodingRate(CONFIG_RADIO_CR) ==
      RADIOLIB_ERR_INVALID_CODING_RATE) {
    Serial.println(F("Selected coding rate is invalid for this module!"));
    while (true);
  }

  /*
   * Sets LoRa sync word.
   * SX1278/SX1276/SX1268/SX1262/SX1280 : Sets LoRa sync word. Only available in
   * LoRa mode.
   * * */
  if (radio.setSyncWord(CONFIG_RADIO_SYNC) != RADIOLIB_ERR_NONE) {
    Serial.println(F("Unable to set sync word!"));
    while (true);
  }

  /*
   * Sets transmission output power.
   * SX1278/SX1276 :  Allowed values range from +2 to +17 dBm (PA_BOOST pin).
   * High power +20 dBm operation is also supported. Defaults to PA_BOOST.
   * SX1262        :  Allowed values are in range from -9 to 22 dBm. This method
   * is virtual to allow override from the SX1261 class.
   * * * */
  if (radio.setOutputPower(CONFIG_RADIO_OUTPUT_POWER) ==
      RADIOLIB_ERR_INVALID_OUTPUT_POWER) {
    Serial.println(F("Selected output power is invalid for this module!"));
    while (true);
  }

  // Enables or disables CRC check of received packets.
  if (radio.setCRC(false) == RADIOLIB_ERR_INVALID_CRC_CONFIGURATION) {
    Serial.println(F("Selected CRC is invalid for this module!"));
    while (true);
  }

  // set the function that will be called
  // when packet transmission is finished
  radio.setPacketSentAction(callback_lora_tx_finished);
  lora_tx_available = true;

  prgBtn.begin(BUTTON_PIN);
  prgBtn.setTapHandler(click_callback);

  delay(1000);
  Serial.println("setup finished -----------------------");
}

void loop() {
  prgBtn.loop();

  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  // HEADER
  display.drawString(0, 0,
                     "0x" + String(localAddress, HEX) + " | " + "To: 0x" +
                         String(broadcastAddress, HEX));

  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(60, 12, String(fix_lat, 7) + ", " + String(fix_lon, 7));

  // BATTERY
  if (PMU->isBatteryConnect()) {
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    if (PMU->isCharging()) {
      display.drawString(120, 0,
                         "Crg " + String(PMU->getBatteryPercent()) + "%");
    } else {
      display.drawString(120, 0,
                         "Bat " + String(PMU->getBatteryPercent()) + "%");
    }
    display.setTextAlignment(TEXT_ALIGN_LEFT);
  }

  // LORA DISPLAY
  RadioLibTime_t waitTime = LORA_DUTY_CYCLE_INTERVAL;
  if (lora_transmission_end_time == 0) {
    waitTime = 0;
  } else {
    waitTime =
        (lora_transmission_end_time + LORA_DUTY_CYCLE_INTERVAL) - millis();
  }

  if (transmit_loop) {
    if (lora_transmit_available()) {
      Serial.println("LoRa sending packet / payload=" + loc + "[" +
                     String(sizeof(loc)) + "]");
      display.drawString(0, 50, "LORA TX");
      lora_send_packet(loc, broadcastAddress);
    } else {
      display.drawString(0, 50, "LORA DC " + String(waitTime / 1000) + "s");
    }
  } else {
    lora_dutyCycle_available();  // required for reset
    display.drawString(0, 50, "OFF " + String(waitTime / 1000) + "s");
  }

  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(60, 28, "DISTRACTOR");
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  // GPS DISPLAY
  if (gps.location.isUpdated()) {
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(120, 50, "GPS [" + String(gps.satellites.value()) + "]");

    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(20, 30,
                       "∆lat =" + String(gps.location.lat() - fix_lat, 5));
    display.drawString(20, 40,
                       "∆lon =" + String(gps.location.lng() - fix_lon, 5));

    // serial
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
  } else {
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(120, 50, "NO GPS");
  }

  // end, display buffer
  display.display();
  delay(10);
}

bool lora_transmit_available() {
  if (lora_tx_available && lora_dutyCycle_available())
    return true;
  else {
    return false;
  }
}

bool lora_dutyCycle_available() {
  if (lora_transmission_end_time + LORA_DUTY_CYCLE_INTERVAL < millis()) {
    lora_transmission_end_time = 0;
    return true;
  } else {
    return false;
  }
}

bool lora_send_packet(String payload, byte recipientAddress) {
  // String to byte array
  // cf.
  // https://arduino.stackexchange.com/questions/21846/how-to-convert-string-to-byte-array

  /*
   * DO NOT USE sizeof(payload), as it will possibly return not the right length
   * of the String! +1 because of '\0' termination of string
   */
  int payloadSize = payload.length() + 1;
  byte bytePayload[payloadSize];
  payload.getBytes(bytePayload, sizeof(bytePayload));

  // Comment on size of payload cf. https://stackoverflow.com/a/37539
  return lora_send_packet(bytePayload, sizeof(bytePayload) / sizeof(byte),
                          recipientAddress);
}

bool lora_send_packet(byte payload[], size_t size, byte recipientAddress) {
  // check if the previous transmission finished
  if (!lora_transmit_available()) {
    Serial.println(F("Last transmission not finished"));
    return false;
  }

  if (size >= 254) {
    Serial.println(F("Payload exceeds 254 Bytes"));
    return false;
  }

  /**
   * cf.
   * https://stackoverflow.com/questions/71316261/arduino-how-to-concatenate-byte-arrays-to-get-a-final-array
   */
  // concatenate header + payload
  byte header[] = {broadcastAddress, localAddress};
  byte message[sizeof(header) + size];

  memcpy(message, header, sizeof(header));
  memcpy(message + sizeof(header), payload, size);

  // reset flag
  lora_tx_available = false;

  // transmit
  Serial.println("Transmit duration estimated: [" + String(sizeof(message)) +
                 "Byte] " + String(radio.getTimeOnAir(sizeof(message)) / 1000) +
                 "ms");
  transmissionState = radio.startTransmit(message, sizeof(message));
  return true;
}

void click_callback(Button2& b) {
  transmit_loop = !transmit_loop;
  Serial.println("Triggering LoRa transmit loop to " + String(transmit_loop));
}