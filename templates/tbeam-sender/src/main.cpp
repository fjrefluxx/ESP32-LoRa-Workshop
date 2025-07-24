/**
 * Examples see:
 * https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series
 *
 * https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series/blob/master/examples/RadioLibExamples/Transmit_Interrupt/Transmit_Interrupt.ino
 *
 *
 * https://www.bundesnetzagentur.de/SharedDocs/Downloads/DE/Sachgebiete/Telekommunikation/Unternehmen_Institutionen/Frequenzen/Allgemeinzuteilungen/FunkanlagenGeringerReichweite/2018_05_SRD_pdf.pdf?__blob=publicationFile&v=7
 *
 * 865-868
 * 868-868,6
 * 869,4-869,65
 * 869,7-870
 *
 * -- SX1276
 */

#include <RadioLib.h>
#include <TinyGPS++.h>

#include "LoRaBoards.h"
#include "SSD1306.h"

SSD1306 display(0x3c, 21, 22);
TinyGPSPlus gps;

#include "Button2.h"
Button2 prgBtn;

#define LORA_BW_250_kHz 250.0
#define LORA_BW_125_kHz 125.0
#define LORA_BW_62_5_kHz 62.5

#define CONFIG_RADIO_FREQ 869.85     // MHz
#define CONFIG_RADIO_OUTPUT_POWER 2  // 17 std, 2-20
#define CONFIG_RADIO_BW 125.0        // 125.0 // kHz
#define CONFIG_RADIO_SF 10
#define CONFIG_RADIO_CR 5
#define CONFIG_RADIO_SYNC 0x14
#define CONFIG_LORA_DC 0.1

SX1276 radio =
    new Module(RADIO_CS_PIN, RADIO_DIO0_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

// save transmission state between loops
static int transmissionState = RADIOLIB_ERR_NONE;
// flag to indicate that a packet was sent
static volatile bool lora_tx_available = false;
static uint32_t counter = 0;
// static String payload;

byte broadcastAddress = 0xFF;
byte localAddress = 0xA1;  // address of this device

bool transmit_loop = true;
RadioLibTime_t lora_transmission_start_time = 0;
RadioLibTime_t lora_transmission_end_time = 0;
RadioLibTime_t lora_dc_time = 0;
///
///
bool lora_transmit_available();
bool lora_dutyCycle_available();
bool lora_send_packet(String payload, byte recipientAddress);
bool lora_send_packet(byte payload[], size_t size, byte recipientAddress);

void click_callback(Button2& b);

// callback when transmission is completed
ICACHE_RAM_ATTR void callback_lora_tx_finished(void) {
  // we sent a packet, set the flag
  Serial.println("Packet sent complete");
  lora_tx_available = true;

  lora_transmission_end_time = millis();
  RadioLibTime_t delta =
      lora_transmission_end_time - lora_transmission_start_time;
  Serial.println("Transmission took about " + String(delta) + " ms");

  lora_dc_time = (delta / CONFIG_LORA_DC);
  Serial.println("DC Timeout " + String(lora_dc_time) + " ms");

  if (transmissionState == RADIOLIB_ERR_NONE) {
    // packet was successfully sent
    Serial.println(F("transmission finished!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(transmissionState);
  }
}

void drawProgressBarDemo() {
  long progress = 0;

  if (lora_dc_time != 0) {
    float a = (millis() - lora_transmission_end_time);
    float b = lora_dc_time;  //(lora_transmission_end_time + lora_dc_time);

    progress = 100 - (a / b) * 100;

    if (progress < 0) {
      progress = 0;
    }

    // Serial.println("m : " + String(millis()));
    // Serial.println("t : " + String(lora_transmission_end_time));
    // Serial.println("e : " + String(lora_dc_time));
    // Serial.println("t+e : " + String(lora_transmission_end_time +
    // lora_dc_time)); Serial.println("Progress : " + String(progress));
  }

  // draw the progress bar
  display.drawProgressBar(0, 32, 120, 10, progress);
  display.setTextAlignment(TEXT_ALIGN_CENTER);

  if (lora_dc_time != 0) {
    RadioLibTime_t waitTime =
        (lora_transmission_end_time + lora_dc_time) - millis();
    display.drawString(64, 15, String(waitTime) + "ms");
  } else {
    display.drawString(64, 15, "ready to send");
  }
  // reset alignment
  display.setTextAlignment(TEXT_ALIGN_LEFT);
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
  // This sketch displays information every time a new sentence is correctly
  // encoded.

  // while (SerialGPS.available() > 0)
  //     if (gps.encode(SerialGPS.read())) {
  //         // GPS
  //         Serial.print("Latitude  : ");
  //         Serial.println(gps.location.lat(), 5);
  //         Serial.print("Longitude : ");
  //         Serial.println(gps.location.lng(), 4);
  //         Serial.print("Satellites: ");
  //         Serial.println(gps.satellites.value());
  //         Serial.print("Altitude  : ");
  //         Serial.print(gps.altitude.feet() / 3.2808);
  //         Serial.println("M");
  //         Serial.print("Time      : ");
  //         Serial.print(gps.time.hour());
  //         Serial.print(":");
  //         Serial.print(gps.time.minute());
  //         Serial.print(":");
  //         Serial.println(gps.time.second());
  //         Serial.println("**********************");
  //     }

  // if (millis() > 15000 && gps.charsProcessed() < 10) {
  //     Serial.println(F("No GPS detected: check wiring."));
  //     delay(500);
  // }

  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  // HEADER
  display.drawString(0, 0,
                     "0x" + String(localAddress, HEX) + " | " + "To: 0x" +
                         String(broadcastAddress, HEX) +
                         " | s = " + String(transmissionState));

  // LORA DISPLAY
  drawProgressBarDemo();

  if (transmit_loop) {
    if (lora_transmit_available()) {
      String payload = "Hello World!";  // "#" + String(counter++);
      Serial.println("LoRa sending packet / payload=" + payload);

      display.drawString(0, 50, "LORA RDY");
      lora_send_packet(payload, broadcastAddress);
    } else {
      display.drawString(0, 50, "LORA TX");
    }
  } else {
    lora_dutyCycle_available();  // required for reset
    display.drawString(0, 50, "LORA OFF");
  }

  // GPS DISPLAY
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  if (gps.location.isUpdated()) {
    display.drawString(120, 50, "GPS [" + String(gps.satellites.value()) + "]");

    // GPS
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
    display.drawString(120, 50, "NO GPS");
  }

  // end, display buffer
  display.display();
  delay(10);

  // if (millis() > 5000 && gps.charsProcessed() < 10)
  //   Serial.println(F("No GPS data received: check wiring"));

  // delay(5000);
}

bool lora_transmit_available() {
  if (lora_tx_available && lora_dutyCycle_available())
    return true;
  else {
    return false;
  }
}

bool lora_dutyCycle_available() {
  // Serial.println(String(lora_transmission_end_time) + " + " +
  // String(lora_dc_time) + "?<" + String(millis()));
  if (lora_transmission_start_time == 0 ||
      lora_transmission_end_time + lora_dc_time < millis()) {
    lora_transmission_start_time = 0;
    lora_transmission_end_time = 0;
    lora_dc_time = 0;
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

  Serial.println("Transmit duration estimated: " +
                 String(radio.getTimeOnAir(sizeof(message))));

  // transmit
  lora_transmission_start_time = millis();
  transmissionState = radio.startTransmit(message, sizeof(message));
  return true;
}

void click_callback(Button2& b) {
  transmit_loop = !transmit_loop;
  Serial.println("Triggering LoRa transmit loop to " + String(transmit_loop));
}