This is a daemon that listens to "xilinx_xvc" (xilinx virtual cable) traffic and 
operates JTAG over an FTDI in bitbang mode.

To run w/NanoEVB or PicoEVB: enter bin dir and run `xvcd.exe`

**Note**: Right now, this program connects to the first FTDI device in the system.
If your system has more than one FTDI device, and the NanoEVB/PicoEVB isn't the first device,
this server won't work for you. There is an issue to address this.


To use with vivado, use the following tcl command:

`open_hw; connect_hw_server; open_hw_target -xvc_url localhost:2542`

Note: Make sure the xvcd program is running before issuing the above command

## Program Options
(no program options yet)

## Build instructions
Open src\xvcd2.vcxproj with Visual Studio 2017 (or later) and build

