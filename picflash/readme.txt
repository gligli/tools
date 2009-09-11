Signal       Mainboard     18F2455
----------------------------------
EJ           J2B1.5        B1 Pin 22 (use resistor)
XX           J2B1.6        B0 Pin 21 (use resistor)
SS           J1D2.2        B4 Pin 25 (use resistor)
SCK          J1D2.3        B3 Pin 24 (use resistor)
MOSI         J1D2.1        B6 Pin 27 (use resistor)
MISO         J1D2.4        B2 Pin 23 (NO RESISTOR)


You can power the PIC from either the USB or from the mainboard. In any
case, C2 (Pin 13) must be connected to +5V from the USB to allow proper sensing
of the USB state. 

