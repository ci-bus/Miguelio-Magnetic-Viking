// Copyright 2025 Miguelio (teclados@miguelio.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "via.h"
#ifdef JOYSTICK_ENABLE
#    include "keycodes.h"
#endif

/*\
    Layers definition
    0 Base keyboard
    1 Keyboard customs keycodes
    2 Joystick configs (This actives joystick and disables keyboard)
\*/
enum layer_names { _BASE, _FN, _GAMING, _MIDI };

// Hall sensor configs
#define HALL_CALIBRATE_ROUNDS 20            // Rounds to get base value
#define HALL_MIN_BASE 100                   // Min base value
#define HALL_MIN_RANGE 10                   // Min analog value from base
#define HALL_MAX_RANGE 512                  // Max analog value from base
#define HALL_WAIT_US 250                    // Wait to change column active
#define HALL_DEFAULT_THRESHOLD 50           // Threshold trigger value in percent
#define HALL_DEFAULT_THRESHOLD_MIN 10       // Min threshold trigger value in percent
#define HALL_DEFAULT_THRESHOLD_MAX 90       // Max threshold trigger value in percent
#define HALL_DEFAULT_PRESS_RELEASE_MARGIN 5 // Margin pressed / release point percent
#define HALL_MIDI_THRESHOLD 80              // Threshold trigger value in percent to midi
#define HALL_MIDI_KEY_PRESS_ELAPSED 20      // Elapsed time pressing keys

// VIA and custom configs
#define EEPROM_CUSTOM_CONFIG (VIA_EEPROM_CUSTOM_CONFIG_ADDR)
#define EEPROM_HALL_RANGE_START (EEPROM_CUSTOM_CONFIG + 6)

// clang-format off
enum via_custom_value_id {
    id_hall_sensors_calibrate = 1, // Calibrate sensors
    id_layout_reset_keymap,        // Reset keymaps to default
    id_hall_threshold,             // Threshold point
    id_hall_fast_trigger,          // Fast release and press again
    id_hall_curve_response         // Sensors response curve
};
// clang-format on

// JoyStick configs
#ifdef JOYSTICK_ENABLE
#    define JT_KEY_COUNT (JOYSTICK_AXIS_COUNT * 2 + JOYSTICK_BUTTON_COUNT)
// Axis button
typedef struct {
    keypos_t key;     // Matrix position
    uint8_t  index;   // JT_KEYCODES index
    uint8_t  value;   // Threshold value
    bool     changed; // Last value changed
    bool     axis;    // Axis key
} jt_key;
// clang-format off
const uint16_t JT_KEYCODES[JT_KEY_COUNT] = {
    KC_A, KC_D, KC_W, KC_S,            // Joystick left
    KC_LEFT, KC_RIGHT, KC_UP, KC_DOWN, // Joystick right
    KC_J, KC_L, KC_I, KC_K,            // Joystick or triggers
    KC_F1, KC_F2, KC_F3, KC_F4,        // Buttons
    KC_F5, KC_F6, KC_F7, KC_F8,
    KC_F9, KC_F10, KC_F11, KC_F12
};
// clang-format on
#endif
