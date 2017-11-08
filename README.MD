This is a daemon that listens to "xilinx_xvc" (xilinx virtual cable) traffic and 
operates JTAG over an FTDI in bitbang mode.

Works for many FTDI devices (FTDI 2232 & FT230X have been tested)

Linux and Windows have different instructions for use, so see the README files in the appropriate directories for more info

**Note 1**: This code is licensed CC0. However, the Windows version uses the proprietary FTDI D2XX lib.
FTDI library license terms are here: http://www.ftdichip.com/Drivers/FTDriverLicenceTerms.htm

**Note 2**: About performance: The bitbang modes in the FTDI chips tested (FT230X, FT2232) suffer from a performance issue.
In theory, they can be set to 3M baud, which should result in 1.5Mb/s transfer rate (since we have to bit-bang the clock line).
However, I've observed that the FTDI chip appears to have stall during transmit, every 64 bits, for ~100us. I've asked
FTDI about this (no response yet). It happens on Windows & Linux and with both drivers (libFTDI & D2XX). As such, the 
baud rate achieved is ~300K at the moment. Therefore, it takes ~20minutes to program the flash chip on the NanoEVB/PicoEVB
via indirect programming.

