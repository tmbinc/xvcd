This is a daemon that listens to "xilinx_xvc" (xilinx virtual cable) traffic and 
operates JTAG over an FTDI in bitbang mode.

To run w/NanoEVB: enter bin dir and run `sudo ./xvcd`

To run w/PicoEVB: enter bin dir and run `sudo ./xvcd -P 0x6015`

To use with vivado, use the following tcl command:

`open_hw; connect_hw_server; open_hw_target -xvc_url localhost:2542`

**Note**: Make sure the xvcd program is running before issuing the above command

## Program Options
| option | description |
| ------ | ----------- |
|-p (port) | set port to listen on |
|-V (vendor-id) | Attach to FTDI device with specific vendor-id |
|-P (product-id) | Attach to FTDI device with specific product-id |
|-v | Enable verbose mode |

## Build instructions (Linux)
sudo apt-get install libftdi-dev
from xvcd dir, run make

