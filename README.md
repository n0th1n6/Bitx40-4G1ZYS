# Bitx40-4G1ZYS

Some of the codes from this mod are inspired by the original and moded firmware. Thanks to the following

1. Original [Raduino firmware][raduino] by Ashar
2. [Mods][amunter] from Allard PE1NWL
3. Menus, Encoder, and Keyswitch [Libraries][mdlib] by MajicDesign
4. [Si5351 Library][si5351pavel] by Pavel

[raduino]: https://github.com/afarhan/bitx40
[amunter]: https://github.com/amunters/bitx40
[mdlib]: https://github.com/MajicDesigns?tab=repositories
[si5351pavel]: https://github.com/pavelmc/Si5351mcu/

## Features

1. Menu for
    * Mode
    * RIT
    * Step
2. Encoder Support
    * Push encoder button to enter menu, select submenu and value
    * Turn encoder to cycle trough submenu and value
3. When not in menu mode, turning the encoder sets the frequency. 
4. When RIT is on, turning the encoder inc/dec the frequency by 1Hz step (+/-) 99Hz

## Encoder

The encoder wiring is based on the encoder sold in Ebay. A, B, and SW are pulled up by a 10k resistor. Then connected to digital pins 2, 3, and 4 respectively. Ground pins are to ground.

## Future Plans

1. Capacitive Keyer
2. S Meter
3. SWR and Power Meter
4. CW Decoder
5. Voice Recorder

