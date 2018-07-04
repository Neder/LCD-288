/*
 * LCD-288 : CRT-288 emulation with RFID-RC522
 * Copyright (c) 2017-2018 Neder
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Official Repo: https://github.com/Neder/LCD-288
 * This code uses a RFID Library (https://github.com/miguelbalboa/rfid)
 * and rfid_default_keys example.
 * 
 * Tested on: Arduino Uno, Arduino Pro Mini
 * 
 * [Changelog]
 * v3.0 (2018-07-04): First public release
 * 
 */
#include <SoftwareSerial.h>
#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN 9
#define SS_PIN  10
#define VERSION "3.0"

boolean debug_mode = false;

boolean card_exists = false;
char card_uid[4] = {0x00, 0x00, 0x00, 0x00};
char card1[16]   = {0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00};
char card2[16]   = {0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00};
SoftwareSerial crtSerial(7, 8);  // RX, TX
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance.
MFRC522::MIFARE_Key sector_key;

// Declare methods
boolean readCard();
boolean tryKey(MFRC522::MIFARE_Key *);
void updateCard(byte *, byte *, byte);
char calculateBcc(char [32], int);

// Debug Message
void debug(const char mesg[100] = "") {
  if (debug_mode) {
    Serial.print("[DEBUG] ");
    Serial.println(mesg);
  }
}

void setup() {
  Serial.begin(9600);    // Serial for debug
  crtSerial.begin(9600); // Serial for emulation
  Serial.print("LCD-288 : CRT-288 Emulation for TECHNIKA & CYCLON v");
  Serial.println(VERSION);
  Serial.println("Copyright 2018 Neder All rights reserved.");
  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522 card
  Serial.println("");
  Serial.println("Initalized.");
}

void loop() {

// Checks a card and reads UID
  if (card_exists == 0 && mfrc522.PICC_IsNewCardPresent()) {
    card_exists = true;
    readCard(); // Reads card UID
  }

// if received data from CRT_R1.dll
  if (crtSerial.available()) {
    char s_init = crtSerial.read();
    if (s_init == 0x02) { // STX -> Stores Command
      // declare vars for saving command
      byte s_rawcomlength[2];
      byte s_command[20];
      char s_bcc[1];
      debug();
      debug("STX OK");
      crtSerial.readBytes(s_rawcomlength, 2);
      int s_comlength = s_rawcomlength[1]; // Command Length
      crtSerial.readBytesUntil(0x03, s_command, 20); // until ETX
      crtSerial.readBytes(s_bcc, 1);

      // checks bcc
      char bccarray[32];
      int total_length = 3;
      bccarray[0] = 0x02;                // STX
      bccarray[1] = s_rawcomlength[0];   // Length
      bccarray[2] = s_rawcomlength[1];
      for (int i = 0; i < s_comlength; i++) {
        bccarray[i + 3] = s_command[i];
        total_length++;
      }
      bccarray[total_length] = 0x03;    // ETX
      total_length++;

      if (debug_mode) {
        Serial.print("[RECV] ");
        for (int i = 0; i < total_length; i++) {
          Serial.print(bccarray[i], HEX);
          Serial.print(" ");
        }
        Serial.println();
      }
      
      char c_bcc = calculateBcc(bccarray, total_length);
      if (c_bcc == s_bcc[0]) {     // compare calculated bcc with bcc from serial
        debug("BCC OK");
        crtSerial.write(0x06);       // send ACK
        boolean enq_received = false;
        for (int i = 0; i < 50; i++) {  // Timeout: 500 ms
          delay(10);
          if (crtSerial.read() == 0x05) {  // ENQ check
            enq_received = true;
            break;
          }
        }
        if (enq_received) {
          debug("ENQ OK");
          char cmd = s_command[0];        // Command
          char prm = s_command[1];        // Param
          int s_sendlength = 0;           // must be defined
          char s_send[32] = {0x02, 0x00, 0x03, cmd, prm}; // Size(#2) must be defined
          
       /* ========== Select a command ========== */

        // === Check Status or Mifare stat ===
          if ((cmd == 0x31 || cmd == 0x35) && prm == 0x30) {
            // Return status
            s_send[2] = 0x03;         // Size
            if (card_exists)          // Status
              s_send[5] = 0x59;       // Y (Card exists in device)
            else
              s_send[5] = 0x45;       // E (No cards in device)
            s_send[6] = 0x03;         // ETX
            s_sendlength = 7;
            
        // === Eject ===
          } else if (cmd == 0x32 && prm == 0x30) {
            // Simply return success
            s_send[2] = 0x03;         // Size
            s_send[5] = 0x59;         // Status: Y
            s_send[6] = 0x03;         // ETX
            s_sendlength = 7;
            
        // === Get card UID ===
          } else if (cmd == 0x35 && prm == 0x31) {
            s_send[2] = 0x07;           // Size
            if (card_exists) {
              s_send[5] = 0x59;         // Y
              // UID is read when a card was detected
              s_send[6] = card_uid[0];  // Card UID
              s_send[7] = card_uid[1];
              s_send[8] = card_uid[2];
              s_send[9] = card_uid[3];
            } else {
              s_send[5] = 0x45;         // E
              for (int i = 6; i <= 9; i++)
                s_send[i] = 0x00;       // Return 0000
            }
            
            s_send[10] = 0x03;          // ETX
            s_sendlength = 11;

        // === Sector key A validation ===
          } else if (cmd == 0x35 && prm == 0x32) {
            s_send[2] = 0x04;           // Size
            s_send[5] = s_command[2];   // Sector # (maybe 0)

            // Copy key from command
            for (byte i = 0; i < MFRC522::MF_KEY_SIZE; i++)
              sector_key.keyByte[i] = s_command[3 + i];
            boolean check_sectorkey = tryKey(&sector_key); // check key and read s/n
            
            if (check_sectorkey)        // checks validation result
              s_send[6] = 0x59;         // Y
            else {
              s_send[6] = 0x45;         // E
              card_exists = false;
            }
            s_send[7] = 0x03;           // ETX
            s_sendlength = 8;

        // === Read Data from block ===
          } else if (cmd == 0x35 && prm == 0x33) {
            s_send[2] = 0x15;           // Size
            s_send[5] = s_command[2];   // Sector # (maybe 0)
            s_send[6] = s_command[3];   // Block #
            s_send[7] = 0x59;           // Status: Y
            
            // RC522 already read card s/n at this time
            if (s_command[3] == 0x01) {     // if Blk is 1 (First 16 letters)
              for (int i = 0; i < 16; i++)
                s_send[8 + i] = card1[i];     // Copy from card1 var
            } else if (s_command[3] == 0x02) { // if Blk is 2 (Final 4 letters)
              for (int i = 0; i < 16; i++)
                s_send[8 + i] = card2[i];    // Copy from card2 var
              card_exists = false;          // Detach a card after reading
            
            } else {                        // if Blk is not 1 or 2
              for (int i = 0; i < 16; i++)
                s_send[8 + i] = 0x00;       // Empty
            }
            s_send[24] = 0x03;              // ETX
            s_sendlength = 25;              // Note: **not 19**
          
        // Etc (Unknown command)
          } else {
            debug("Unknown command");
            s_send[2] = 0x03;         // Size
            s_send[5] = 0x45;         // Status: E
            s_send[6] = 0x03;         // ETX
            s_sendlength = 7;
          }
          
          // Calculate BCC and reply via crtSerial
          char s_sendbcc = calculateBcc(s_send, s_sendlength);
          s_send[s_sendlength] = s_sendbcc;
          crtSerial.write(s_send, s_sendlength + 1);
          
          // For debug
          if (debug_mode) {
            Serial.print("[SEND] ");
            for (int i = 0; i < s_sendlength; i++) {
              Serial.print(s_send[i], HEX);
              Serial.print(" ");
            }
            Serial.print(s_sendbcc, HEX);
            Serial.println();
            Serial.println();
          }
          
        } else
          Serial.println("[ERROR] ENQ Timeout");

      // if BCC check Failed
      } else {
        Serial.println("[ERROR] BCC check failed");
        crtSerial.write(0x15); // send NAK
      }
    }
  }
  delay(50); // to prevent reading error
}

// ====================================== RFID ======================================

boolean readCard() {
  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial())
    return false;
    
  for (byte i = 0; i < 4; i++)
    card_uid[i] = mfrc522.uid.uidByte[i];
  return true;
}

boolean tryKey(MFRC522::MIFARE_Key *key) {
  boolean result = false;
  byte buffer1[18];
  byte buffer2[18];
  byte block1 = 1; // Sector 0, Block 1
  byte block2 = 2; // Sector 0, Block 2
  MFRC522::StatusCode status1;
  MFRC522::StatusCode status2;

  status1 = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block1, key, &(mfrc522.uid));
  status2 = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block2, key, &(mfrc522.uid));
  
  if (status1 != MFRC522::STATUS_OK) { // sector key mismatch
    //Serial.println(mfrc522.GetStatusCodeName(status1)); // displaying status code
    return false;
  }

  // Read block
  byte byteCount = sizeof(buffer1);
  status1 = mfrc522.MIFARE_Read(block1, buffer1, &byteCount);
  status2 = mfrc522.MIFARE_Read(block2, buffer2, &byteCount);
  if (status1 != MFRC522::STATUS_OK) {
  } else {
    // Successful read
    result = true;
    updateCard(buffer1, buffer2, 16); // Write card number to variable
  }

  mfrc522.PICC_HaltA();       // Halt PICC
  mfrc522.PCD_StopCrypto1();  // Stop encryption on PCD
  return result;
}

void updateCard(byte *buffer1, byte *buffer2, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    card1[i] = buffer1[i];
    card2[i] = buffer2[i];
  }
}

// =================================================================================

// a method for calculating BCC
// d_length: Total length of data e.g: {0x00, 0x01, 0x02} => 3
char calculateBcc(char data[32], int d_length) {
  char bcc = data[0];
  for (int i = 1; i < d_length; i++)
    bcc = bcc ^ data[i];
  return bcc;
}
