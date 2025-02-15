#!/bin/zsh

source ./env.zsh

if [[ -z $WIFI_SSID ]] then
    echo "WIFI_SSID not defined!"
    echo "    Is env.zsh present?"
    exit 1
fi

if [[ -z $WIFI_PASSWORD ]] then
    echo "WIFI_PASSWORD not defined!"
    echo "    Is env.zsh present?"
    exit 1
fi

export PICO_SDK_PATH=${HOME}/rasp-pico/wifi/pico-sdk
export FREERTOS_KERNEL_PATH=${HOME}/rasp-pico/wifi/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/
export CMAKE_BUILD_TYPE Debug

rm -rf build
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
