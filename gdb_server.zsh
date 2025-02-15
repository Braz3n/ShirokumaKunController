#!/bin/zsh

# https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html#debugging-with-swd
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000"