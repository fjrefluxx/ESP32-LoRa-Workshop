# ESP32 + LoRa Hands-On Tutorial Workshop

Hands-on tutorial on ESP32 programming and LoRa communicatin. 
Provides four sequential challenges for the participants, that get harder and more complicated with each level. 
Since all communication devices run automatically without a required interaction of the tutorial teacher, the entire setup could remain in place after the official end of the tutorial to provide further opportunities for its completion.  

A basic understanding of (C/C++) programming is assumed to successfully complete this tutorial workshop. 

> [!CAUTION]
> The code used in this tutorial workshop runs on REAL hardware and performs REAL RF transmissions! 
> All users must be aware of their actions and the appropriate legal requirements such as usable frequency bands, tx power, duty cycle restrictions, etc. 


## Requirements
#### ESP32 LoRa Devices
+ Heltec WiFi LoRa v3, ESP32S3, 868MHz SX1262 (https://heltec.org/project/wifi-lora-32-v3/)
+ LILYGO T-Beam v1.2, ESP32, 868MHz SX1276, GPS NEO-6M (https://lilygo.cc/products/t-beam)

#### Tutorial Teacher
+ Computer with a suitable IDE. We use Visual Studio Code with the PlatformIO plugin. (The independent PlatformIO IDE should also work out, but was not tested.)
+ 3x LilyGo T-Beam (Level 2, 2a, 4)
+ 2x Heltec LoRa v3 (Level 1, 3)
+ USB cables to flash both ESP32 board types.
+ To run the devices remotely (i.e., not attached to your computer), requires either a suitable battery or a USB PSU.

#### Per Participant/Group
+ A Heltec v3 board and a USB-C cable to connect it to the computer
+ Computer with a suitable IDE (see above).   
  💡 Probably best to use the same setup for all participants.
+ One of the predefined device IDs.

### Hardware Setup
+ (optional) Use the provided [python script](generate-codephases.py) to generate codephrases. Copy the generated maps to the respective device code.
+ Flash the 'level' devices with the appropriate code.
+ Test the entire setup by flashing the sample solutions to more Heltec v3 boards.
+ Place the level devices for Level 2 and 2a at appropriate locations outside the actual tutorial room (if possible and wished). All other devices could (but do not need to) be in the same room. 

---
<br>

# Tutorial Tasks

## Level 1: Reception and Serial Print

Participants will need to receive messages on their devices and print it to console in order to find the clue for the next level. There is template code for participants that includes the needed functionality, which should be analyzed, understood, and applied by the participants. 

Participants will receive the following message to set their devices for the next level:
`"Hello Workshop! Next is 869.525MHz, 250kHz, SF9, CR4/5, sw=0x42."`

> [!NOTE]
> + LoRa Settings: ` 866.5 MHz, 250kHz, SF10, CR4/5, Sync 0x36 `  
> + Level device address: `C1`
> + Sends broadcast (0xFF receiver address)


## Level 2: Fragmented Reception Puzzle

Similar to Level 1, participants will need to recieve messages on the given LoRa parameter settings. However, the next text is fragmented and these fragments are sent sequentially with some delays. Participants will need to save the fragments and put them together in the end to receive the complete text. 

For this level, the message hints at the location of the sender, which should be placed, e.g., in another room nearby: `"Find me in the meeting room on the window to get the next peer address."`

The next settings are displayed on the devices's LED display:
`Next: contact C3 @ 869.85 Mhz, BW=125 kHz, SF=10, CR=4/5, SW=0x14`

> [!NOTE]
> + LoRa Settings: ` 869.525 MHz, 250kHz, SF9, CR4/5, Sync 0x42 `  
> + Level device address: `C2`
> + Sends broadcast (0xFF receiver address)


### 🦆 Care for a little challenge?

By setting up a decoy, you can provide a small extra challenge to the participants. This distractor sends a fixed location (like latitute/longitude) on the same band and settings as the other device. If you trust your participants that they won't turn off the distractor, place it somewhere the participants can find it and read `Distractor` on its display. 

The intended way to face this extra sender is to filter the distractor's messages based on the sender address in the header.

> [!NOTE]
> + LoRa settings: ` 869.525 MHz, 250kHz, SF9, CR4/5, Sync 0x42 `  
> + Level device address: `CD`
> + Sends broadcast (0xFF receiver address)
> + Sending interval: 15 seconds


## Level 3: Send and Receive

With the hint of contacting `C3` and the required parameter settings, participants will now need to send a message to a specific device and then wait for an answer. The contacted device provides them with a new parameter set and a 'personalized' key.

⚠ This key is bound to the participant device's sender address. Make sure that different participants (or groups) use different sender addresses for the next challenge to work. When not filtering for the receiver address, other participants may receive answer messages not intended for them and, thus, provides a challenge they must solve.

Participants will receive the following message for the next level:
`868.3,125,8,5,0x12. Call 0x31 with your key '" + key + "'. But he's kind of a flipping character."`


> [!NOTE]
> + LoRa settings: ` 869.85 MHz, 125kHz, SF10, CR4/5, Sync 0x14 `  
> + Level device address: `C3`
> + Sends ONLY after reception of a message (content is irrelevant)
> + Sends message to the sender address of the received message (with around a 1 second delay) including a personalized key. 


## Level 4: Catch me if you can

Similar to Level 3, participiants will need to actively request information from device `0x31` using their key. The device will tell them their passphrase to complete the tutorial... but not without completing the final challenge!

First of all, the passphrase is sent in multiple fragments – each on different LoRa settings. Therefore, the participants need to read out new settings of the received answers and automatically adapt their LoRa transceiver to it. Manual adaptations should not work due to the random selection of a parameter set. The parameters are given always in the format `fff.fff,bbb.bb,ss.` (frequency, bandwith, spreading factor). The fragmentation idea is already known from Level 2. 

Secondly, the passphrase is 'encrypted' with the personalized key from Level 3. This hint is sent in the final part of the message sequence by the request device (`XOR with your key. Bye.`). Participants will, thus, need to put the entire passphrase together and XOR with their key to decrypt it. The key is shorted than the message and must simply be repeated. 

> [!NOTE]
> + (idle) LoRa settings: ` 868.3 MHz, 125 kHz, SF 8, CR 4/5, Sync 0x12`  
> + Level device address: `0x31`
> + Sends ONLY after reception of a message (content must be the key fitting the sender address)
> + Starts a sequence of messages with instructions for (randomly selected) LoRa paramter sets and a decyphered part of the final passphrase.
> + Returns to the idle LoRa settings after completing a triggered cycle.




---

## Sync Word Problems (!)?

The sync words in LoRa do not behave as one might initially believe, as they do not provide a reliable filter or separation between different 'communications' using different sync words. 

Furthermore, there are differences in the behavior of different LoRa transceivers and their compatability is also not guaranteed! Which came apparent when trying to communicate between the Heltec v3's SX1262 and the LILYGO T-Beam's SX1276 chipset... For example, communication from SX1276 → SX1262 is possible using sync word 0x26, but not for the other way around. 

An interesting and more in-depth description is found here: https://blog.classycode.com/lora-sync-word-compatibility-between-sx127x-and-sx126x-460324d1787a (unrelated, external link)

The standard sync words provided in the different levels have been tested to work for the intended combination of devices. Changing sync words could probably go wrong and should be tested before application in the tutorial.


## Acknowledgments
+ The code for the Heltec v3 relies on library fixes (`heltec_unofficial.h`) done by GitHub contributor ropg, found here https://github.com/ropg/heltec_esp32_lora_v3/blob/main/src/heltec_unofficial.h
+ The code for the LilyGo T-Beam uses utilities (`LoRaBoards.h`,`LoRaBoards.cpp`,`utilities.h`) provided in https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series (e.g., see examples). 

(Based on the latest versions available in late summer 2024.)
