/**
 * ESP32+LoRa Workshop
 *
 * Sender for Challenge 4:
 * Waits for a request containing a specific key from a specific sender. If this
 * matches, a message is sent containing a new parameter set (Fq,BW,SF) and one
 * part of a code message. This sender then switches to the next parameter set
 * and sends a new set and the next part of the code message. This is done a
 * total of 4 times, with the last message only stating that the codes from the
 * last messages must be decrypted.
 *
 * Code message can be chosen to be just a plain text scrambled (i.e., every
 * third character) or the characters additionally XOR'd with the requester's
 * key.
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

#include <map>
#include <vector>

#include "LoRaBoards.h"
#include "SSD1306.h"

#define ARRAY_SIZE(x) sizeof(x) / sizeof(x[0])

SSD1306 display(0x3c, 21, 22);
TinyGPSPlus gps;

#include "Button2.h"
Button2 prgBtn;

#define CONFIG_RADIO_FREQ 868.3       // MHz
#define CONFIG_RADIO_OUTPUT_POWER 10  // 17 std, 2-20
#define CONFIG_RADIO_BW 125.0         // kHz
#define CONFIG_RADIO_SF 8
#define CONFIG_RADIO_CR 5
#define CONFIG_RADIO_SYNC 0x12

// Adjust the duty cycle depending on message size and rotation number!
#define LORA_DUTY_CYCLE_INTERVAL 1000  // 0.5

SX1276 radio =
    new Module(RADIO_CS_PIN, RADIO_DIO0_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

// save transmission state between loops
static int lora_tx_state = RADIOLIB_ERR_NONE;
static int lora_rx_state = RADIOLIB_ERR_NONE;

static volatile bool lora_tx_available = false;

/*
 * 0 -> receive mode
 * 1 -> message available
 * 2 -> transmitting
 * 3 -> transmission complete
 */
static volatile uint8_t lora_state = 0;

static byte broadcastAddress = 0xFF;
static byte localAddress = 0x31;
byte receiverAddress = 0x00;
RadioLibTime_t answer_backoff = 0;
RadioLibTime_t lora_transmission_end_time = 0;

//
//
std::map<byte, String> groupkeys{
    {0x11, "zbgj5F"}, {0x22, "A7Fwx4"}, {0x33, "GiZ58h"}, {0x44, "Qqk8Uq"},
    {0x55, "rx9rGN"}, {0x66, "Loe7hJ"}, {0x88, "9VT6qw"}, {0x99, "tsa5PB"},
    {0xaa, "Dj8wWQ"}, {0xbb, "5Ad6d5"}, {0xcc, "o8bZPE"}, {0xdd, "ST2qps"},
    {0xee, "oP6URu"}, {0xf0, "7wHYvR"},
};

std::map<String, String> plaintextmap{
    {"zbgj5F", "Your passphrase: Hot Potato"},
    {"A7Fwx4", "Your passphrase: Minions"},
    {"GiZ58h", "Your passphrase: Factorio"},
    {"Qqk8Uq", "Your passphrase: Macaroni"},
    {"rx9rGN", "Your passphrase: Sushi"},
    {"Loe7hJ", "Your passphrase: Smoking is BAD"},
    {"9VT6qw", "Your passphrase: Paint3D"},
    {"tsa5PB", "Your passphrase: Cold Feet"},
    {"Dj8wWQ", "Your passphrase: Couch Potato"},
    {"5Ad6d5", "Your passphrase: Spill the Beans"},
    {"o8bZPE", "Your passphrase: ESP32-v5"},
    {"ST2qps", "Your passphrase: Piece of Cake"},
    {"oP6URu", "Your passphrase: Bread Crumbs"},
    {"7wHYvR", "Your passphrase: Cocktail Bar"},
};

std::map<String, std::vector<unsigned char>> codetextmap{
    {"zbgj5F", {0x23, 0x0d, 0x12, 0x18, 0x15, 0x36, 0x1b, 0x11, 0x14,
                0x1a, 0x5d, 0x34, 0x1b, 0x11, 0x02, 0x50, 0x15, 0x0e,
                0x15, 0x16, 0x47, 0x3a, 0x5a, 0x32, 0x1b, 0x16, 0x08}},
    {"A7Fwx4",
     {0x18, 0x58, 0x33, 0x05, 0x58, 0x44, 0x20, 0x44, 0x35, 0x07, 0x10, 0x46,
      0x20, 0x44, 0x23, 0x4d, 0x58, 0x79, 0x28, 0x59, 0x2f, 0x18, 0x16, 0x47}},
    {"GiZ58h", {0x1e, 0x06, 0x2f, 0x47, 0x18, 0x18, 0x26, 0x1a, 0x29,
                0x45, 0x50, 0x1a, 0x26, 0x1a, 0x3f, 0x0f, 0x18, 0x2e,
                0x26, 0x0a, 0x2e, 0x5a, 0x4a, 0x01, 0x28}},
    {"Qqk8Uq", {0x81, 0xe1, 0xe4, 0xa7, 0x50, 0x13, 0x00, 0x21, 0x84,
                0x83, 0xd0, 0x33, 0x00, 0x20, 0xe0, 0x27, 0x53, 0xc3,
                0x01, 0x20, 0xa4, 0xa3, 0xa1, 0xf3, 0x8}},
    {"rx9rGN",
     {0x2b, 0x17, 0x4c, 0x00, 0x67, 0x3e, 0x13, 0x0b, 0x4a, 0x02, 0x2f,
      0x3c, 0x13, 0x0b, 0x5c, 0x48, 0x67, 0x1d, 0x07, 0x0b, 0x51, 0x1b}},
    {"Loe7hJ",
     {0x15, 0x00, 0x10, 0x45, 0x48, 0x3a, 0x2d, 0x1c, 0x16, 0x47, 0x00,
      0x38, 0x2d, 0x1c, 0x00, 0x0d, 0x48, 0x19, 0x21, 0x00, 0x0e, 0x5e,
      0x06, 0x2d, 0x6c, 0x06, 0x16, 0x17, 0x2a, 0x0b, 0x08}},
    {"9VT6qw",
     {0x60, 0x39, 0x21, 0x44, 0x51, 0x07, 0x58, 0x25, 0x27, 0x46, 0x19, 0x05,
      0x58, 0x25, 0x31, 0x0c, 0x51, 0x27, 0x58, 0x3f, 0x3a, 0x42, 0x42, 0x33}},
    {"tsa5PB", {0x2d, 0x1c, 0x14, 0x47, 0x70, 0x32, 0x15, 0x00, 0x12,
                0x45, 0x38, 0x30, 0x15, 0x00, 0x04, 0x0f, 0x70, 0x01,
                0x1b, 0x1f, 0x05, 0x15, 0x16, 0x27, 0x11, 0x07}},
    {"Dj8wWQ", {0x1d, 0x05, 0x4d, 0x05, 0x77, 0x21, 0x25, 0x19, 0x4b, 0x07,
                0x3f, 0x23, 0x25, 0x19, 0x5d, 0x4d, 0x77, 0x12, 0x2b, 0x1f,
                0x5b, 0x1f, 0x77, 0x01, 0x2b, 0x1e, 0x59, 0x03, 0x38}},
    {"5Ad6d5",
     {0x6c, 0x2e, 0x11, 0x44, 0x44, 0x45, 0x54, 0x32, 0x17, 0x46, 0x0c,
      0x47, 0x54, 0x32, 0x01, 0x0c, 0x44, 0x66, 0x45, 0x28, 0x08, 0x5a,
      0x44, 0x41, 0x5d, 0x24, 0x44, 0x74, 0x01, 0x54, 0x5b, 0x32}},
    {"o8bZPE", {0x36, 0x57, 0x17, 0x28, 0x70, 0x35, 0x0e, 0x4b, 0x11,
                0x2a, 0x38, 0x37, 0x0e, 0x4b, 0x07, 0x60, 0x70, 0x00,
                0x3c, 0x68, 0x51, 0x68, 0x7d, 0x33, 0x5a}},
    {"ST2qps", {0xa3, 0xb4, 0x70, 0x35, 0x00, 0x33, 0x22, 0x74, 0x10, 0x11,
                0x80, 0x13, 0x22, 0x75, 0x74, 0xb5, 0x02, 0x33, 0xa3, 0x15,
                0x11, 0x45, 0x01, 0xc3, 0x57, 0x47, 0x11, 0x01, 0xb1, 0x6}},
    {"oP6URu", {0x36, 0x3f, 0x43, 0x27, 0x72, 0x05, 0x0e, 0x23, 0x45, 0x25,
                0x3a, 0x07, 0x0e, 0x23, 0x53, 0x6f, 0x72, 0x37, 0x1d, 0x35,
                0x57, 0x31, 0x72, 0x36, 0x1d, 0x25, 0x5b, 0x37, 0x21}},
    {"7wHYvR", {0x6e, 0x18, 0x3d, 0x2b, 0x56, 0x22, 0x56, 0x04, 0x3b, 0x29,
                0x1e, 0x20, 0x56, 0x04, 0x2d, 0x63, 0x56, 0x11, 0x58, 0x14,
                0x23, 0x2d, 0x17, 0x3b, 0x5b, 0x57, 0x0a, 0x38, 0x04}},
};

#define MESSAGE_ROTATION_NUM 3
static volatile uint8_t current_parameterset_num = 0;  // 0 == standard
static volatile uint8_t current_message_num = 0;

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

parameterset lora_sets[10]{
    parameterset(869.4, 125.0, 8),    parameterset(869.5, 125.0, 8),
    parameterset(869.525, 250.0, 8),  parameterset(869.525, 250.0, 9),
    parameterset(869.525, 250.0, 10), parameterset(869.525, 250.0, 11),
    parameterset(869.48, 125.0, 8),   parameterset(869.48, 125.0, 10),
    parameterset(869.55, 125.0, 8),   parameterset(869.55, 125.0, 10),
};

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
    Serial.println("Callback at lora_state 1 --- Error, should not happen?");
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
    Serial.println("Callback at lora_state 3 --- Error, should not happen?");
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
  Serial.print(F("[SX1276] Initializing ... "));
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
  radio.setPacketReceivedAction(callback_lora_action);
  lora_tx_available = true;
  lora_state = 0;

  lora_rx_state = radio.startReceive();
  if (lora_rx_state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(lora_rx_state);
    while (true);
  }

  prgBtn.begin(BUTTON_PIN);
  prgBtn.setTapHandler(click_callback);

  delay(1000);
  Serial.println("setup finished");
  Serial.println("starting up........");
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(60, 0, "init ok");
  delay(1000);
  display.clear();
}

void loop() {
  prgBtn.loop();

  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(30, 0, "CHAL 4");
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

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

  // put back into receiving/listen mode
  if (lora_state == 3) {
    // switch to next lora setting
    Serial.println("Switching parameterset! ");
    lora_switch_parameters(next_parameterset);
    Serial.println("||| LoRa RCV mode");
    lora_state = 0;
    radio.startReceive();
  }

  // check if RX flag 1 is set -> message available
  if (lora_state == 1) {
    // read received data as byte array
    size_t length = radio.getPacketLength();
    Serial.println("<<< Received " + String(length) + " bytes");

    byte payloadArray[length];
    int lora_rx_state = radio.readData(payloadArray, length);

    if (lora_rx_state == RADIOLIB_ERR_NONE) {
      // packet was successfully received
      if (receiverAddress == 0x00) {
        byte receiver = payloadArray[0];
        byte sender = payloadArray[1];

        byte messageArray[length - 2];
        memcpy(messageArray, payloadArray + 2, length - 2);

        String message = String((char*)messageArray);

        Serial.println("Receiver: " + String(receiver, HEX));
        Serial.println("Sender: " + String(sender, HEX));
        Serial.println("Message string: " + String(message));

        String rssi = String(radio.getRSSI()) + "dBm";
        String snr = String(radio.getSNR()) + "dB";

        // print RSSI (Received Signal Strength Indicator)
        Serial.print(F("Radio RSSI:\t\t"));
        Serial.println(rssi);

        // print SNR (Signal-to-Noise Ratio)
        Serial.print(F("Radio SNR:\t\t"));
        Serial.println(snr);

        if (receiver != localAddress) {
          Serial.println("Message not for me! --- Dropped Packet!");
        } else {
          Serial.println("Message is for me!");

          // check correct sender address and key
          if (groupkeys.find(sender) == groupkeys.end()) {
            // sender not in group
            Serial.println("Sender not allowed, drop");
          } else {
            // sender in group
            if (groupkeys.at(sender).equals(message)) {
              Serial.println("Sender key accepted");
              // set next receiver to send the answer to
              receiverAddress = sender;
              answer_backoff = millis() + 500;
            } else {
              Serial.println("Key not accepted");
              display.drawString(0, 18, "Key not accepted");
            }
          }
        }
      } else {
        Serial.println(F("Dropped Packet!"));
      }
    } else if (lora_rx_state == RADIOLIB_ERR_CRC_MISMATCH) {
      // packet was received, but is malformed
      Serial.println(F("CRC error!"));
    } else {
      // some other error occurred
      Serial.print(F("failed, code "));
      Serial.println(lora_rx_state);
    }

    // put module back to listen mode
    lora_state = 0;
    radio.startReceive();
  }

  // transmit available?
  if (receiverAddress != 0x00 && answer_backoff < millis()) {
    if (lora_transmit_available()) {
      Serial.println("---");
      Serial.println("Sending coded message to " +
                     String(receiverAddress, HEX));

      if (current_message_num < MESSAGE_ROTATION_NUM) {
        // get code message part
        String current_key = groupkeys.at(receiverAddress);
        std::vector<unsigned char> current_message =
            codetextmap.at(current_key);
        // String  current_message = plaintextmap.at(current_key);
        String current_message_part = "";

        int fwd = current_message_num % MESSAGE_ROTATION_NUM;
        // for(int i = 0; i < current_message.length() - fwd; i = i +
        // MESSAGE_ROTATION_NUM) {
        for (int i = 0; i < current_message.size() - fwd;
             i = i + MESSAGE_ROTATION_NUM) {
          char atPos = current_message[i + fwd];
          current_message_part += atPos;
        }
        current_message_num = current_message_num + 1;

        // get parameter set and message string
        // pick random number for next parameter set, different to current set
        uint8_t rndnum = random(0, 10);
        while (rndnum == current_parameterset_num) {
          rndnum = random(0, 10);
        }

        next_parameterset = lora_sets[rndnum];

        // build string for parameters
        String freq =
            String(next_parameterset.frequency, 3);  // 3 decimal places
        String bandw =
            String(next_parameterset.bandwidth, 2);  // 2 decimal places
        String spreadf = String(next_parameterset.spreadingfactor);
        String lora_setting = freq + "," + bandw + "," + spreadf;

        // construct the entire messsage string
        String entire_message = lora_setting + ". " + current_message_part;

        Serial.println(">>> LoRa sending coded message " + String(millis()) +
                       " to " + String(receiverAddress, HEX));
        lora_send_packet(entire_message, receiverAddress);

        Serial.println("---");
        answer_backoff = millis() + 1000;
      } else {
        String entire_message = "XOR with your key. Bye.";
        Serial.println(">>> LoRa sending final message " + String(millis()));
        lora_send_packet(entire_message, receiverAddress);

        // last message sent, reset
        receiverAddress = 0x00;
        answer_backoff = 0;
        current_message_num = 0;
        current_parameterset_num = 0;
        next_parameterset = standard_ps;
        Serial.println("-- sent all messages, reset to standard parameters --");
      }
    } else {
      RadioLibTime_t waitTime =
          (lora_transmission_end_time + LORA_DUTY_CYCLE_INTERVAL) - millis();
      display.drawString(0, 50,
                         "LORA DC " + String(waitTime / 1000) +
                             "s, answering 0x" + String(receiverAddress, HEX));
    }
  } else {
    lora_dutyCycle_available();  // required for reset
  }

  // end, display buffer
  if (receiverAddress != 0x00) {
    if (current_message_num <= MESSAGE_ROTATION_NUM) {
      display.setFont(ArialMT_Plain_16);
      switch (current_message_num) {
        case 0:
          display.drawString(30, 25, ">");
          break;
        case 1:
          display.drawString(30, 25, "> 1");
          break;
        case 2:
          display.drawString(30, 25, "> 1 2");
          break;
        case 3:
          display.drawString(30, 25, "> 1 2 3");
          break;
        case 4:
          display.drawString(30, 25, "> 1 2 3 4");
          break;
        default:
          display.drawString(30, 25, "---------");
          break;
      }

      display.setFont(ArialMT_Plain_10);
      display.drawString(0, 25, "0x" + String(receiverAddress, HEX));
    }
  } else {
    display.drawString(0, 25, "LoRa await request");
  }

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

void click_callback(Button2& b) {
  //
}