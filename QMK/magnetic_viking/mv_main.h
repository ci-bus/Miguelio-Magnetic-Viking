// Copyright 2025 Miguelio (teclados@miguelio.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include QMK_KEYBOARD_H
#include "ch.h"
#include "hal.h"
#include "analog.h"
#include "string.h"
#include "matrix.h"
#include "quantum.h"
#include "wait.h"
#include "math.h"
#include "dynamic_keymap.h"
#include "eeprom.h"
#include "eeconfig.h"
#include "stdatomic.h"
#include "chmboxes.h"
#include "chbsem.h"

/*\
    Layers definition
    0 Base keyboard
    1 Keyboard customs keycodes
    2 Joystick configs (This actives joystick and disables keyboard)
    3 Midi music instrument
    4 Threshold by key, config keys 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
    5 Curve response by key, config keys 0, 1, 2, 3, 4
\*/
enum layer_names { _BASE, _FN, _GAMING, _MIDI, _THRESHOLD, _CURVE };

// Hall sensor configs
#define HALL_GET_BASE_SCANS 100   // Rounds to get base value
#define HALL_MIN_BASE 100         // Min base value
#define HALL_MIN_RANGE 30        // Min analog value from base
#define HALL_MAX_RANGE 512        // Max analog value from base
#define HALL_WAIT_US_LOAD 3       // Wait to load
#define HALL_WAIT_US_DISCHARGE 18 // Wait to discharge

#define HALL_DEFAULT_THRESHOLD 50   // Threshold trigger value in percent
#define HALL_THRESHOLD_MARGIN 6     // Margin threshold in percent
#define HALL_PRESS_RELEASE_MARGIN 4 // Margin pressed / release point percent
#define HALL_FAST_RELEASE_MARGIN 4  // Margin fast release point percent

#define CORE1_WAIT_INIT 600        // Wait milliseconds to init core 1
#define CORE1_WAIT_SLEEP_WAKEUP 10 // Timeout to wait core 1

// Shared
extern binary_semaphore_t    init_core1;
extern volatile atomic_bool  run_core_1;
extern atomic_uint_least32_t matrix_hall_raw[MATRIX_COLS * MATRIX_ROWS];
extern pin_t                 row_pins[MATRIX_ROWS];
extern pin_t                 col_pins[MATRIX_COLS];
// extern volatile char         message[64];

#ifdef MIDI_ENABLE
#    define HALL_MIDI_THRESHOLD 80         // Threshold trigger value in percent to midi
#    define HALL_MIDI_KEY_PRESS_ELAPSED 20 // Elapsed time pressing keys
#endif

// VIA and custom configs
#define EEPROM_CUSTOM_CONFIG (VIA_EEPROM_CUSTOM_CONFIG_ADDR)
#define EEPROM_HALL_RANGE_START (EEPROM_CUSTOM_CONFIG + 6)

// clang-format off
enum via_custom_value_id {
    id_hall_sensors_calibrate = 1, // Calibrate sensors
    id_layout_reset_keymap,        // Reset keymaps to default
    id_hall_threshold,             // Threshold point
    id_hall_fast_release,          // Fast release and press again
    id_hall_curve,                 // Hall sensor curve response
    id_btn_reset_threshold                    // Free button
};
// clang-format on

// JoyStick configs
#ifdef JOYSTICK_ENABLE
#    define JT_KEY_COUNT (JOYSTICK_AXIS_COUNT * 2 + JOYSTICK_BUTTON_COUNT + 1)
#    define JT_MARGIN 5 // Margin to secure readers
// Axis button
typedef struct {
    keypos_t key;     // Matrix position
    uint8_t  index;   // JT_KEYCODES index
    uint8_t  value;   // Threshold value
    bool     changed; // Last value changed
    bool     axis;    // Axis key
} jt_key;
// clang-format off
enum jt_states {
    jt_initiate = 1,
    jt_inited
};
static const uint16_t JT_KEYCODES[JT_KEY_COUNT] = {
    KC_A, KC_D, KC_W, KC_S,            // Joystick left
    KC_LEFT, KC_RIGHT, KC_UP, KC_DOWN, // Joystick right
    KC_J, KC_L, KC_I, KC_K,            // Joystick or triggers
    KC_F1, KC_F2, KC_F3, KC_F4,        // Buttons
    KC_F5, KC_F6, KC_F7, KC_F8,
    KC_F9, KC_F10, KC_F11, KC_F12,
    20992                              // To layer 0
};
// clang-format on
#endif
