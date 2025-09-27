# Miguelio Magnetic Viking
version 1.1.3

# QMK
QMK Firmware 0.28.10

# Compile firmware

Add submodule dual core rp2040

    git submodule add https://github.com/elpekenin/qmk_modules.git modules/elpekenin
    git submodule update --init --recursive

Make example for this keyboard (after setting up your build environment):

    make miguelio/magnetic_viking:default

Flashing example for this keyboard:

    make miguelio/magnetic_viking:default:flash