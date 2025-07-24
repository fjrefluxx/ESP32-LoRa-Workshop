/**
 * ESP32+LoRa Workshop
 *
 * Basic code structure and sample code.
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
#include <radiolib_SX1262_wrapper.h>

#include "Button2.h"

Button2 prgBtn;
bool button_action = false;
void click_callback(Button2& b);

// LoRa radio interrupt function.
// IMPORTANT: this function MUST be 'void' type and MUST NOT have any arguments!
//            DO NOT call functions, set radio states, etc.
ICACHE_RAM_ATTR void callback_lora_action(void) {
  /**
   * TODO implement the callback function.
   *
   * Is called by the radio's interrupt pin.
   * Interrupt triggered by
   * - packet reception complete (in receive mode after: radio.startReceive())
   * - transmission complete (in transmit mode after: radio.startTransmit())
   *
   * Recommendation: only set flags
   *
   *
   * To work properly with the wrapper and its given functions, set
   *    lora_tx_available = true;
   *    lora_transmission_end_time = millis();
   * after a completed transmission!
   */
}

/**
 * Setup function is called on startup.
 */
void setup() {
  heltec_setup();
  while (!Serial);

  Serial.println("LoRa Sender");
  lora_radio_setup();

  prgBtn.begin(BUTTON);
  prgBtn.setTapHandler(click_callback);

  // Initialising the UI will init the display too.
  display.init();
  display.setFont(ArialMT_Plain_10);
}

/**
 * Loop function is continuously called after setup() is completed.
 */
void loop() {
  prgBtn.loop();
  // clear the display
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "TEST TEST TEST");

  /**
   * Sample code for receiving a packet.
   * This example is 'non-blocking':
   *  - Flags are set in the interrupts
   *  - loop() regularly checks the interrupt flags
   *  - Active flags trigger actions in the loop
   */

  // check if RX flag is set -> message available
  // if (some reception flag) {

  //     // read received data as byte array, print first byte to serial,
  //     convert to String size_t length = radio.getPacketLength(); byte
  //     payloadArray[length]; lora_rx_state = radio.readData(payloadArray,
  //     length); Serial.println(String(payloadArray[0], HEX)); String payload =
  //     String((char *) payloadArray); // convert to String

  //     // sample code: copy a specific part of length n from one array to
  //     another byte destinationArray[n];
  //     // into destinationArray, copy n bytes starting at sourceArray+4 (i.e.,
  //     5th byte in source) memcpy(destinationArray, sourceArray+4, n);

  //     // read entire received data as String
  //     String payload;
  //     lora_rx_state = radio.readData(payload);

  //     if (lora_rx_state == RADIOLIB_ERR_NONE) {
  //       // packet was correctly received, print some infos to Serial
  //       String rssi = String(radio.getRSSI()) + "dBm";
  //       String snr = String(radio.getSNR()) + "dB";
  //       Serial.println(rssi + ", " + snr);
  //     } else if (lora_rx_state == RADIOLIB_ERR_CRC_MISMATCH) {
  //         // packet was received, but is malformed
  //         Serial.println(F("CRC error!"));
  //     } else {
  //         // some other error occurred
  //         Serial.print(F("failed, code "));
  //     }
  // }

  /**
   * Sample code for sending a packet.
   * Sending action is non-blocking: after the packet is successfully
   * transmitted, the interrupt is triggered.
   */
  // if(some flag to indicate sending action) {
  //   if(lora_transmit_available()) {
  //     Serial.println("LoRa sending request to 0x" + String(receiverAddress,
  //     HEX));
  //     // define message
  //     String message = "Hey!";
  //     lora_send_packet(message, receiverAddress);
  //   }
  //   else {
  //     RadioLibTime_t waitTime = (lora_transmission_end_time +
  //     LORA_DUTY_CYCLE_INTERVAL) - millis(); display.drawString(0, 50, "LORA
  //     DC " + String(waitTime/1000) + "s");
  //   }
  // }
  // else {
  //   lora_dutyCycle_available(); // required for reset
  //   display.drawString(0, 50, "LoRa TX");
  // }

  // write the buffer to the display
  display.display();
  delay(100);
}

void click_callback(Button2& b) {
  //
}