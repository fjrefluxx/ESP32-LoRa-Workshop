/**
 * ESP32+LoRa Workshop
 *
 * Sender for Challenge 1: Simply send a message
 *
 * Written for the Heltec ESP32 WiFi+LoRa v3.
 *
 * Examples see:
 * https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series
 *
 * For the German frequency bands and restrictions consult:
 * https://www.bundesnetzagentur.de/SharedDocs/Downloads/DE/Sachgebiete/Telekommunikation/Unternehmen_Institutionen/Frequenzen/Allgemeinzuteilungen/FunkanlagenGeringerReichweite/2018_05_SRD_pdf.pdf?__blob=publicationFile&v=7
 *
 */
#define HELTEC_NO_RADIOLIB
#include <RadioLib.h>
#include <SPI.h>
#include <heltec_unofficial.h>

#include <map>

#define CONFIG_RADIO_FREQ 866.5      // MHz
#define CONFIG_RADIO_OUTPUT_POWER 2  // 17 std, 2-20
#define CONFIG_RADIO_BW 250.0        // kHz
#define CONFIG_RADIO_SF 10
#define CONFIG_RADIO_CR 5
#define CONFIG_RADIO_SYNC 0x36  // 0x14

#define LORA_DUTY_CYCLE_INTERVAL 10000  // 10 s

// save transmission state between loops
static int lora_tx_state = RADIOLIB_ERR_NONE;
static int lora_rx_state = RADIOLIB_ERR_NONE;

// flag to indicate that a packet was received
static volatile bool lora_tx_available = false;

/*
 * 0 -> receive mode
 * 1 -> message available
 * 2 -> transmitting
 * 3 -> transmission complete
 */
static volatile uint8_t lora_state = 0;

byte broadcastAddress = 0xFF;
byte localAddress = 0xC1;
RadioLibTime_t lora_transmission_end_time = 0;

SX1262 radio = new Module(SS, DIO1, RST_LoRa, BUSY_LoRa);

//
//
//
bool lora_transmit_available();
bool lora_dutyCycle_available();
bool lora_send_packet(String payload, byte recipientAddress);
bool lora_send_packet(byte payload[], size_t size, byte recipientAddress);

// called when a complete packet is received by the module
// IMPORTANT: this function MUST be 'void' type and MUST NOT have any arguments!
ICACHE_RAM_ATTR void callback_lora_action(void) {
  if (lora_state == 0) {
    // we got a packet, set the flag
    Serial.println("CB - Reception complete");
    lora_state = 1;
  } else if (lora_state == 1) {
    // we got a packet, set the flag
    Serial.println("Error, should not happen?");
    while (true);
  } else if (lora_state == 2) {
    // we sent a packet, set the flag
    Serial.println("CB - Transmission complete");
    lora_tx_available = true;

    lora_transmission_end_time = millis();

    if (lora_tx_state == RADIOLIB_ERR_NONE) {
      // packet was successfully sent
      Serial.println(F("transmission finished!"));
    } else {
      Serial.print(F("failed, code "));
      Serial.println(lora_tx_state);
    }

    lora_state = 3;
  } else if (lora_state == 3) {
    // callback while transmission complete?
    Serial.println("Error, should not happen?");
    while (true);
  }
}

void setup() {
  heltec_setup();
  while (!Serial);

  Serial.println("Challenge 1 Sender");

  // initialize SX1262 with default settings
  Serial.print(F("[SX1262] Initializing ... "));

  int state = radio.begin();

  Serial.println(state);
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
  radio.setDio1Action(callback_lora_action);

  // start listening for LoRa packets
  Serial.print(F("[SX1262] Starting to listen ... "));
  lora_rx_state = radio.startReceive();
  if (lora_rx_state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(lora_rx_state);
    while (true);
  }
  lora_state = 0;
  lora_tx_available = true;

  // Initialising the UI will init the display too.
  display.init();
  display.setFont(ArialMT_Plain_10);
}

void loop() {
  // clear the display
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(80, 0, "CHALLENGE 1 ");
  // Serial.print(F("[SX1262] Waiting for incoming transmission ... "));

  if (lora_transmit_available()) {
    Serial.println("LoRa sending answer");
    display.drawString(0, 50, "SENDING");

    String message =
        "Hello Workshop! Next is 869.525MHz, 250kHz, SF9, CR4/5, sw=0x42.";
    // send message
    lora_send_packet(message, broadcastAddress);
  } else {
    RadioLibTime_t waitTime =
        (lora_transmission_end_time + LORA_DUTY_CYCLE_INTERVAL) - millis();
    display.drawString(0, 50, "LORA DC " + String(waitTime / 1000) + "s");
  }

  // write the buffer to the display
  display.display();
  delay(10);
}

//
//
//
//
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
  byte header[] = {recipientAddress, localAddress};
  byte message[sizeof(header) + size];

  memcpy(message, header, sizeof(header));
  memcpy(message + sizeof(header), payload, size);

  // set flag
  lora_tx_available = false;
  lora_state = 2;

  // transmit
  Serial.println("Transmit duration estimated: [" + String(sizeof(message)) +
                 "Byte] " + String(radio.getTimeOnAir(sizeof(message)) / 1000) +
                 "ms");
  lora_tx_state = radio.startTransmit(message, sizeof(message));
  return true;
}