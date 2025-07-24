#define HELTEC_NO_RADIOLIB
#include <heltec_unofficial.h>
#include <SPI.h>
#include <RadioLib.h>

#define CONFIG_RADIO_FREQ           869.0 // MHz
#define CONFIG_RADIO_OUTPUT_POWER   2 // 17 std, 2-20
#define CONFIG_RADIO_BW             125.0 // kHz
#define CONFIG_RADIO_SF             7
#define CONFIG_RADIO_CR             5
#define CONFIG_RADIO_SYNC           0x14 // standard values: 0x34 for LoRaWAN, 0x12 for private networks. Not every sync word works!

#define LORA_DUTY_CYCLE_INTERVAL    1000 // ms 

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
RadioLibTime_t  lora_transmission_end_time = 0;

byte localAddress = 0x01; 

SX1262 radio = new Module(SS, DIO1, RST_LoRa, BUSY_LoRa);

//
// ---------------------------------------------------------
//

/**
 * Intended to be programmed individually. 
 * 
 * To work properly with this wrapper, set 
 *    lora_tx_available = true;
 *    lora_transmission_end_time = millis();       
 * after a completed transmission! 
 */
ICACHE_RAM_ATTR void callback_lora_action(void);

void lora_radio_setup() {
    // initialize SX1262 with default settings
  Serial.print(F("[SX1262] Initializing ... "));
  
  int state = radio.begin();
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
  *   SX1278/SX1276 : Allowed values are 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125, 250 and 500 kHz. Only available in %LoRa mode.
  *   SX1268/SX1262 : Allowed values are 7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125.0, 250.0 and 500.0 kHz. 
  * * * */
  if (radio.setBandwidth(CONFIG_RADIO_BW) == RADIOLIB_ERR_INVALID_BANDWIDTH) {
      Serial.println(F("Selected bandwidth is invalid for this module!"));
      while (true);
  }

  /*
  * Sets LoRa link spreading factor.
  * SX1278/SX1276 :  Allowed values range from 6 to 12. Only available in LoRa mode.
  * SX1262        :  Allowed values range from 5 to 12.
  * * * */
  if (radio.setSpreadingFactor(CONFIG_RADIO_SF) == RADIOLIB_ERR_INVALID_SPREADING_FACTOR) {
      Serial.println(F("Selected spreading factor is invalid for this module!"));
      while (true);
  }

  /*
  * Sets LoRa coding rate denominator.
  * SX1278/SX1276/SX1268/SX1262 : Allowed values range from 5 to 8. Only available in LoRa mode. 
  * * * */
  if (radio.setCodingRate(CONFIG_RADIO_CR) == RADIOLIB_ERR_INVALID_CODING_RATE) {
      Serial.println(F("Selected coding rate is invalid for this module!"));
      while (true);
  }

  /*
  * Sets LoRa sync word.
  * SX1278/SX1276/SX1268/SX1262/SX1280 : Sets LoRa sync word. Only available in LoRa mode.
  * * */
  if (radio.setSyncWord(CONFIG_RADIO_SYNC) != RADIOLIB_ERR_NONE) {
    Serial.println(F("Unable to set sync word!"));
    while (true);
  }

  /*
  * Sets transmission output power.
  * SX1278/SX1276 :  Allowed values range from +2 to +17 dBm (PA_BOOST pin). High power +20 dBm operation is also supported. Defaults to PA_BOOST.
  * SX1262        :  Allowed values are in range from -9 to 22 dBm. This method is virtual to allow override from the SX1261 class.
  * * * */
  if (radio.setOutputPower(CONFIG_RADIO_OUTPUT_POWER) == RADIOLIB_ERR_INVALID_OUTPUT_POWER) {
      Serial.println(F("Selected output power is invalid for this module!"));
      while (true);
  }

  // Enables or disables CRC check of received packets.
  if (radio.setCRC(false) == RADIOLIB_ERR_INVALID_CRC_CONFIGURATION) {
      Serial.println(F("Selected CRC is invalid for this module!"));
      while (true);
  }

  // set the callback function for SX1262 interrupt
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
}

bool lora_dutyCycle_available() {
  if(lora_transmission_end_time + LORA_DUTY_CYCLE_INTERVAL < millis()) {   
    lora_transmission_end_time = 0;   
    return true;
  }
  else {
    return false;
  }
}

bool lora_transmit_available() {
  if (lora_tx_available && lora_dutyCycle_available())   
    return true;  
  else {    
    return false;
  }
}

/**
 * Send a byte payload to a specific recipient (i.e., the recpient address is added to the header).
 */
bool lora_send_packet(byte payload[], size_t size, byte recipientAddress) {
    // check if the previous transmission finished
  if (!lora_transmit_available()) {
    Serial.println(F("Last transmission not finished"));
    return false;
  }
   
  if(size >= 254) {
    Serial.println(F("Payload exceeds 254 Bytes"));
    return false;
  } 

  /**
  * cf. https://stackoverflow.com/questions/71316261/arduino-how-to-concatenate-byte-arrays-to-get-a-final-array
  */
  // concatenate header + payload
  byte header[] = {recipientAddress, localAddress};
  byte message[sizeof(header)+size];  

  memcpy(message, header, sizeof(header));
  memcpy(message+sizeof(header), payload, size);
  
  // reset flag
  lora_tx_available = false;
  lora_state = 2;
  
  // transmit 
  Serial.println("Transmit duration estimated: [" + String(sizeof(message)) + "Byte] " + String(radio.getTimeOnAir(sizeof(message))/1000) + "ms");  
  lora_tx_state = radio.startTransmit(message, sizeof(message));  
  return true;
}

/**
 * Send a String payload to a specific recipient (i.e., the recpient address is added to the header).
 */
bool lora_send_packet(String payload, byte recipientAddress) {
  // String to byte array
  // cf. https://arduino.stackexchange.com/questions/21846/how-to-convert-string-to-byte-array
 
  /*
  * DO NOT USE sizeof(payload), as it will possibly return not the right length of the String!
  * +1 because of '\0' termination of string
  */
  int payloadSize = payload.length() + 1 ;   
  byte bytePayload[payloadSize]; 
  payload.getBytes(bytePayload, sizeof(bytePayload));
 
  // Comment on size of payload cf. https://stackoverflow.com/a/37539
  return lora_send_packet(bytePayload, sizeof(bytePayload)/sizeof(byte), recipientAddress);
}
