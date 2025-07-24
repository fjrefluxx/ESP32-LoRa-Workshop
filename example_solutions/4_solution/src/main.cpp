/**
 * ESP32+LoRa Workshop
 *
 * Solution for Challenge 4.
 * For testing, the device starts from 0x11 as sender and switches to the next
 * sender ID after the fourth reception and decoding of the code message parts.
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
#include <vector>

#include "Button2.h"
Button2 prgBtn;

#define CONFIG_RADIO_FREQ 868.3      // MHz
#define CONFIG_RADIO_OUTPUT_POWER 2  // 17 std, 2-20
#define CONFIG_RADIO_BW 125.0        // kHz
#define CONFIG_RADIO_SF 8            // 10
#define CONFIG_RADIO_CR 5
#define CONFIG_RADIO_SYNC 0x12

#define LORA_DUTY_CYCLE_INTERVAL 1000  // s

// save transmission state between loops
static int lora_tx_state = RADIOLIB_ERR_NONE;
static int lora_rx_state = RADIOLIB_ERR_NONE;
// flag to indicate that transmit is available
static volatile bool lora_tx_available = false;

/*
 * 0 -> receive mode
 * 1 -> message available
 * 2 -> transmitting
 * 3 -> transmission complete
 */
static volatile uint8_t lora_state = 0;

byte broadcastAddress = 0xFF;
byte localAddress = 0x11;
byte receiverAddress = 0x31;

std::vector<char> first_code = std::vector<char>();
std::vector<char> second_code = std::vector<char>();
std::vector<char> third_code = std::vector<char>();

std::map<byte, String> groupkeys{
    {0x11, "zbgj5F"}, {0x22, "A7Fwx4"}, {0x33, "GiZ58h"}, {0x44, "Qqk8Uq"},
    {0x55, "rx9rGN"}, {0x66, "Loe7hJ"}, {0x88, "9VT6qw"}, {0x99, "tsa5PB"},
    {0xAA, "Dj8wWQ"}, {0xBB, "5Ad6d5"}, {0xCC, "o8bZPE"}, {0xDD, "ST2qps"},
    {0xEE, "oP6URu"}, {0xFF, "7wHYvR"},
};

String myKey = groupkeys.at(localAddress);

struct parameterset {
  parameterset(float freq, float bw, int sf)
      : frequency(freq), bandwidth(bw), spreadingfactor(sf) {}

  float frequency;
  float bandwidth;
  int spreadingfactor;
};

parameterset standard_ps =
    parameterset(CONFIG_RADIO_FREQ, CONFIG_RADIO_BW, CONFIG_RADIO_SF);
parameterset next_parameterset = standard_ps;

int message_reception_num = 0;

bool transmit_request = false;
RadioLibTime_t lora_transmission_end_time = 0;

SX1262 radio = new Module(SS, DIO1, RST_LoRa, BUSY_LoRa);

///
///
bool lora_transmit_available();
bool lora_dutyCycle_available();
bool lora_send_packet(String payload, byte recipientAddress);
bool lora_send_packet(byte payload[], size_t size, byte recipientAddress);
void lora_switch_parameters(parameterset ps);

void click_callback(Button2& b);

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

  Serial.println("LoRa Sender");

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

  prgBtn.begin(BUTTON);
  prgBtn.setTapHandler(click_callback);

  // Initialising the UI will init the display too.
  display.init();
  display.setFont(ArialMT_Plain_10);
}

void loop() {
  prgBtn.loop();
  // clear the display
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "Challenge 3 - test");

  // put back into receiving/listen mode
  if (lora_state == 3) {
    // switch to next lora setting
    Serial.println("||| LoRa RCV mode");
    lora_state = 0;
    radio.startReceive();
  }

  // check if RX flag is set -> message available
  if (lora_state == 1) {
    // read received data as byte array
    size_t length = radio.getPacketLength();
    Serial.println("-------");
    Serial.println("Received " + String(length) + " bytes");

    byte payloadArray[length];
    int lora_rx_state = radio.readData(payloadArray, length);

    if (lora_rx_state == RADIOLIB_ERR_NONE) {
      byte receiver = payloadArray[0];
      byte sender = payloadArray[1];

      byte messageArray[length - 2];
      memcpy(messageArray, payloadArray + 2, length - 2);

      String message = String((char*)messageArray);

      Serial.println("Receiver: " + String(receiver, HEX));
      Serial.println("Sender: " + String(sender, HEX));

      String rssi = String(radio.getRSSI()) + "dBm";
      String snr = String(radio.getSNR()) + "dB";

      if (receiver != localAddress) {
        Serial.println("Message not for me!");
      } else {
        Serial.println("Message received --- Full string: " + String(message));

        int del1 = message.indexOf(".", 15);

        String params = message.substring(0, del1);
        String code = message.substring(del1 + 2);

        Serial.println(params);
        Serial.println(code);

        int delcomma1 = params.indexOf(",");
        int delcomma2 = params.indexOf(",", delcomma1 + 1);
        int delcomma3 = params.indexOf(",", delcomma2 + 1);

        String freq = params.substring(0, delcomma1);
        String bw = params.substring(delcomma1 + 1, delcomma2);
        String sf = params.substring(delcomma2 + 1);

        Serial.println(freq);
        Serial.println(bw);
        Serial.println(sf);

        // String code = message.substring(19);
        byte byteCode[code.length() + 1];
        code.getBytes(byteCode, sizeof(byteCode));

        switch (message_reception_num) {
          case 0:
            first_code.insert(first_code.end(), &byteCode[0],
                              &byteCode[sizeof(byteCode) / sizeof(byte)]);
            break;
          case 1:
            second_code.insert(second_code.end(), &byteCode[0],
                               &byteCode[sizeof(byteCode) / sizeof(byte)]);
            break;
          case 2:
            third_code.insert(third_code.end(), &byteCode[0],
                              &byteCode[sizeof(byteCode) / sizeof(byte)]);
            break;
        }

        message_reception_num++;

        if (message_reception_num == 3) {
          std::vector<char> fullcode;
          std::vector<char> keycode;

          String fulltext = "";

          int its = first_code.size() - 1;

          int keycounter = 0;

          for (int i = 0; i < its; i++) {
            fullcode.push_back(first_code[i]);
            fulltext += first_code[i];

            keycode.push_back(myKey[keycounter % myKey.length()]);
            keycounter++;

            if (i < second_code.size() - 1) {
              fullcode.push_back(second_code[i]);
              fulltext += second_code[i];

              keycode.push_back(myKey[keycounter % myKey.length()]);
              keycounter++;
            }

            if (i < third_code.size() - 1) {
              fullcode.push_back(third_code[i]);
              fulltext += third_code[i];

              keycode.push_back(myKey[keycounter % myKey.length()]);
              keycounter++;
            }
          }

          Serial.println(fulltext);

          // decoding
          Serial.print(fullcode.size());
          Serial.print(" vs ");
          Serial.println(keycode.size());

          String decrypt = "";

          for (int a = 0; a < fullcode.size(); a++) {
            Serial.print(fullcode[a], HEX);
            Serial.print(" ");
          }
          Serial.println();

          for (int a = 0; a < keycode.size(); a++) {
            Serial.print(keycode[a], HEX);
            Serial.print(" ");
          }
          Serial.println("  " + myKey);

          Serial.println(" XOR ");

          for (int a = 0; a < fullcode.size(); a++) {
            char x = fullcode[a] ^ keycode[a];
            Serial.print(x, HEX);
            Serial.print(" ");
            decrypt += x;
          }
          Serial.println();

          Serial.println(decrypt);

          // del
          message_reception_num = 0;
          Serial.println(" ---");
        }

        ///
        if (freq.toFloat() != 0) {
          float f_freq = freq.toFloat();
          float f_bw = bw.toFloat();
          int f_sf = sf.toInt();

          lora_switch_parameters(parameterset(f_freq, f_bw, f_sf));
        } else {
          Serial.println("Returning to standard parameters");
          lora_switch_parameters(standard_ps);

          message_reception_num = 0;
          first_code.clear();
          second_code.clear();
          third_code.clear();

          // increase key to next address for testing
          Serial.println(localAddress, HEX);
          int rNibble = (localAddress & 0x0F);
          int lNibble = (localAddress & 0xF0) >> 4;

          int incR = rNibble + 1;
          int incL = lNibble + 1;
          incL = incL << 4;

          localAddress = incL | incR;
          Serial.println(localAddress, HEX);
          myKey = groupkeys.at(localAddress);
        }
      }
    } else if (lora_rx_state == RADIOLIB_ERR_CRC_MISMATCH) {
      // packet was received, but is malformed
      Serial.println(F("CRC error!"));
    } else {
      // some other error occurred
      Serial.print(F("failed, code "));
      Serial.println(lora_rx_state);
    }
    Serial.println("-------");
    // put module back to listen mode
    lora_state = 0;
    radio.startReceive();
  }

  // transmit triggered?
  if (transmit_request) {
    if (lora_transmit_available()) {
      Serial.println("LoRa sending request to 0x" +
                     String(receiverAddress, HEX));
      display.drawString(
          0, 50, "LORA sending REQ to 0x" + String(receiverAddress, HEX));

      lora_send_packet(myKey, receiverAddress);
      // put module back to listen mode
      transmit_request = false;
    } else {
      RadioLibTime_t waitTime =
          (lora_transmission_end_time + LORA_DUTY_CYCLE_INTERVAL) - millis();
      display.drawString(0, 50, "LORA DC " + String(waitTime / 1000) + "s");
    }
  } else {
    lora_dutyCycle_available();  // required for reset
    display.drawString(0, 50, "LoRa TX");
  }

  // write the buffer to the display
  display.display();

  delay(100);
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

  // reset flag
  lora_tx_available = false;
  lora_state = 2;

  // transmit
  Serial.println("Transmit duration estimated: [" + String(sizeof(message)) +
                 "Byte] " + String(radio.getTimeOnAir(sizeof(message)) / 1000) +
                 "ms");
  lora_tx_state = radio.startTransmit(message, sizeof(message));
  return true;
}

void click_callback(Button2& b) {
  if (transmit_request == false && lora_transmission_end_time == 0)
    transmit_request = true;
}

void lora_switch_parameters(parameterset ps) {
  if (radio.setFrequency(ps.frequency) == RADIOLIB_ERR_INVALID_FREQUENCY) {
    Serial.println(F("Selected frequency is invalid for this module!"));
    while (true);
  }

  if (radio.setBandwidth(ps.bandwidth) == RADIOLIB_ERR_INVALID_BANDWIDTH) {
    Serial.println(F("Selected bandwidth is invalid for this module!"));
    while (true);
  }

  if (radio.setSpreadingFactor(ps.spreadingfactor) ==
      RADIOLIB_ERR_INVALID_SPREADING_FACTOR) {
    Serial.println(F("Selected spreading factor is invalid for this module!"));
    while (true);
  }
}