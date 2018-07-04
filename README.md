# LCD-288
LCD-288 is a **PARTIAL** emulating software of [CRT-288](http://www.china-creator.com/card-reader/manual-card-reader/crt-288-ic-rfid-card-reader-writer.html) for Arduino with RFID-RC522.

## Requirements
LCD-288 was tested on Arduino Uno and Arduino Pro Mini.
Additionally, [RFID-RC522](https://www.youtube.com/watch?v=23aMjljCLZI) module and TTL to RS232 converter(e.g. MAX3232) are required.

## Supported features
* Working as CRT-288 reader
* Get a sector key from serial to read a card via RC522
* Return read value to program (MIFARE block 1, 2 **ONLY**)

I don't have any plan to add new features.
# How to use
## In Uno and Pro Mini
1. Connect RC522 to Arduino same with [this](https://youtu.be/23aMjljCLZI).
and connect software serial line to 7(RX), 8(TX).
2. Download RFID library from [here](https://github.com/miguelbalboa/rfid) and install it to Arduino IDE's library folder.
3. Download a sketch and upload to Arduino.
4. ????
5. PROFIT!
## In other boards
I don't know.

# Disclaimer
I'm not responsible for any risks or damages while using this sketch.

# License
LCD-288 is available under the [GPLv3](https://www.gnu.org/licenses/gpl-3.0.html) license.