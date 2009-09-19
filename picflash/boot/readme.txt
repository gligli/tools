This is the bootloader for the PIC 18F2455. You need to program this
bootloader only once. 

There are two ways to program the PIC:

1) Low Voltage Programming (LVP)
2) High Voltage Programming which needs +12V at the MCLR pin.

The LVP mode requires an additional Signal (Port B5, Pin 26). If LVP is disabled via
a configuration bit in the device, Port B5 can be used as normal IO. 

Because programming via LVP is easier, we do not use Port B5 and leave LVP
enabled. You need to add a pull-down (10k) to Pin 26 to prevent the PIC from
entering LVP mode randomly.

There are thousands of more or less sophisticated PIC programmers out there.
Some random examples:

- http://www.finitesite.com/d3jsys/
- http://products.foxdelta.com/programmer/art2003/ART2003-LVP.pdf
- http://www.sprut.de/electronic/pic/icsp/icsp.htm

Once the bootloader is programmed, it can be controlled with Port C6, Pin
17. If the Pin is pulled to ground on reset or power-up, the device will
enter the bootloader.

The same Port will eventually be used for serial communication with the
mainboard, so do not add any pull-ups or pull-downs. A simple jumper /
switch that connects the Pin to ground is enough.

So the following Pins are important for the bootloader:

- Port B5, Pin 26: Pull-down to ground with 10k
- Port C6, Pin 17: Connect to ground if you want to enter bootloader

Once you programmed the bootloader, you can program / update the flasher
onto your device using the following procedure:

1) Connect Port C6, Pin 17 to ground.

2) Power-cycle or reset the PIC.

3) Install the mchpusb driver for the USB device that shows up.

4) Start the programmer (PDFSUSB.exe)

5) Select the only device that shows up in the drop-down box

6) Use "Load Hex File" to load PixXFlash.mhx and click "Program Device"

7) After programming finished, disconnect Port C6, Pin 17 from ground.

8) Power-cycle or reset the PIC.

