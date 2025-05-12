#!/bin/bash

rm -rf build-host
mkdir build-host && cd build-host
cmake .. -DBUILD_PICO=OFF
make forth_host   # now builds natively without any ARM stubs

cd .. 
rm -rf build-pico
mkdir build-pico && cd build-pico
cmake .. -DBUILD_PICO=ON -DPICO_BOARD=pico2
make forth_pico   # builds UF2 with Pico SDK includes

