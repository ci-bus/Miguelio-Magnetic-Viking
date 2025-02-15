// Copyright 2025 Miguelio (teclados@miguelio.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mv_main.h"
#include "analog.h"
#include "matrix.h"
#include "hardware/gpio.h"
#include "wait.h"
#include "string.h"
#include "via.h"
#include "print.h"
#include "math.h"
#include "quantum.h"
#ifdef JOYSTICK_ENABLE
#    include "joystick.h"
#endif
#ifdef MIDI_ENABLE
extern MidiDevice midi_device;
#endif

static matrix_row_t matrix[MATRIX_ROWS]                                 = {0};
static uint16_t     matrix_hall_base[MATRIX_ROWS * MATRIX_COLS]         = {0};
static uint16_t     matrix_hall_range[MATRIX_ROWS * MATRIX_COLS]        = {0};
static uint16_t     matrix_hall_fast_trigger[MATRIX_ROWS * MATRIX_COLS] = {0};
static pin_t        row_pins[MATRIX_ROWS]                               = MATRIX_ROW_PINS;
static pin_t        col_pins[MATRIX_COLS]                               = MATRIX_COL_PINS;
static uint8_t      hall_threshold                                      = HALL_DEFAULT_THRESHOLD;
static uint8_t      hall_release                                        = HALL_DEFAULT_THRESHOLD - HALL_DEFAULT_PRESS_RELEASE_MARGIN;
static uint16_t     raw_value                                           = 0;
static bool         calibrating                                         = false;
static bool         fast_trigger                                        = false;
static float        curve_response                                      = 0;
static uint8_t      curve_table[101]                                    = {0};
static uint8_t      current_layer                                       = 0;
#ifdef MIDI_ENABLE
static uint16_t     midi_note_key[MATRIX_ROWS * MATRIX_COLS]      = {0};
static matrix_row_t midi_note_on[MATRIX_ROWS]                     = {0};
static uint8_t      midi_last_percent[MATRIX_ROWS * MATRIX_COLS]  = {0};
static uint16_t     midi_velocity_time[MATRIX_ROWS * MATRIX_COLS] = {0};
#endif
#ifdef JOYSTICK_ENABLE
// clang-format off
joystick_config_t joystick_axis[JOYSTICK_AXIS_COUNT] = {
    JOYSTICK_AXIS_VIRTUAL, // Index 0, 1
    JOYSTICK_AXIS_VIRTUAL, // Index 2, 3
    JOYSTICK_AXIS_VIRTUAL, // Index 4, 5
    JOYSTICK_AXIS_VIRTUAL, // Index 6, 7
    JOYSTICK_AXIS_VIRTUAL, // Index 8, 9
    JOYSTICK_AXIS_VIRTUAL  // Index 10, 11
};
// clang-format on
static int    jt_axes_values[JOYSTICK_AXIS_COUNT] = {0};
static jt_key jt_keys[JT_KEY_COUNT]               = {};

uint8_t jt_get_keycode_index(uint16_t keycode) {
    for (int i = 0; i < JT_KEY_COUNT; i++) {
        if (JT_KEYCODES[i] == keycode) {
            return i;
        }
    }
    return JT_KEY_COUNT;
}

void init_joystick(void) {
    uint8_t jt_index = 0;
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            keypos_t key     = (keypos_t){.col = col, .row = row};
            uint16_t keycode = keymap_key_to_keycode(_GAMING, key);
            uint8_t  index   = jt_get_keycode_index(keycode);
            if (index != JT_KEY_COUNT) {
                // Set key
                jt_keys[jt_index] = (jt_key){.key = key, .index = index};
#    ifdef CONSOLE_ENABLE
                uprintf("Joystick key index: %u col: %u row: %u\n", index, col, row);
#    endif
                jt_index += 1;
            }
        }
    }
}

void matrix_scan_joystick(void) {
    memset(jt_axes_values, 0, sizeof(jt_axes_values));
    uint8_t jt_index = 0;

    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        if (jt_keys[jt_index].key.col != col) {
            continue;
        }
        gpio_write_pin_high(col_pins[col]);
        wait_us(HALL_WAIT_US);

        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            if (jt_keys[jt_index].key.row != row) {
                continue;
            }
            raw_value          = analogReadPin(row_pins[row]);
            uint8_t index      = (row * MATRIX_COLS) + col;
            uint8_t axis_value = 0;
            if (raw_value > matrix_hall_base[index]) {
                axis_value = (raw_value - matrix_hall_base[index]) * 127 / matrix_hall_range[index];
                if (axis_value > 127) {
                    axis_value = 127;
                }
                // TODO curve value?
            }
            // Axes values
            if (jt_keys[jt_index].index < JOYSTICK_AXIS_COUNT * 2) {
                // Even sum and odd subtract
                jt_axes_values[jt_keys[jt_index].index / 2] += axis_value * (jt_keys[jt_index].index & 1 ? 1 : -1);
#    ifdef CONSOLE_ENABLE
                if (jt_keys[jt_index].value != axis_value) {
                    uprintf("Joystick axis index: %u value: %d\n", jt_keys[jt_index].index, axis_value);
                    jt_keys[jt_index].value = axis_value;
                }
#    endif
            } else { // Buttons
                uint8_t jt_button_index = jt_keys[jt_index].index - (JOYSTICK_AXIS_COUNT * 2);
                uint8_t percent         = 0;
                // Calcule percent pressed
                percent = (raw_value - matrix_hall_base[index]) * 100 / matrix_hall_range[index];
                if (percent > 100) {
                    percent = 100;
                }
                // Get curved value
                percent = curve_table[percent];
                if (joystick_state.buttons[jt_button_index / 8] & (1 << (jt_button_index % 8))) {
                    // Check released
                    if (fast_trigger) {
                        if (percent < matrix_hall_fast_trigger[index] - HALL_DEFAULT_PRESS_RELEASE_MARGIN) {
                            matrix[row] &= ~(1 << col);
                            joystick_state.buttons[jt_button_index / 8] &= ~(1 << (jt_button_index % 8));
                            joystick_state.dirty            = true;
                            matrix_hall_fast_trigger[index] = percent;
#    ifdef CONSOLE_ENABLE
                            uprintf("Release joystick button index: %u value: %u\n", jt_button_index, raw_value);
#    endif
                        } else if (percent > matrix_hall_fast_trigger[index]) {
                            matrix_hall_fast_trigger[index] = percent;
                        }
                    } else if (percent < hall_release) {
                        joystick_state.buttons[jt_button_index / 8] &= ~(1 << (jt_button_index % 8));
                        joystick_state.dirty = true;
#    ifdef CONSOLE_ENABLE
                        uprintf("Release joystick button index: %u value: %u\n", jt_button_index, raw_value);
#    endif
                    }
                } else {
                    // Check press
                    if (fast_trigger) {
                        if (percent > hall_threshold && percent > matrix_hall_fast_trigger[index] + HALL_DEFAULT_PRESS_RELEASE_MARGIN) {
                            joystick_state.buttons[jt_button_index / 8] |= 1 << (jt_button_index % 8);
                            joystick_state.dirty            = true;
                            matrix_hall_fast_trigger[index] = percent;
#    ifdef CONSOLE_ENABLE
                            uprintf("Press joystick button index: %u value: %u\n", jt_button_index, raw_value);
#    endif
                        } else if (percent < matrix_hall_fast_trigger[index]) {
                            matrix_hall_fast_trigger[index] = percent;
                        }
                    } else if (percent > hall_threshold) {
                        joystick_state.buttons[jt_button_index / 8] |= 1 << (jt_button_index % 8);
                        joystick_state.dirty = true;
#    ifdef CONSOLE_ENABLE
                        uprintf("Press joystick button index: %u value: %u\n", jt_button_index, raw_value);
#    endif
                    }
                }
            }
            jt_index += 1;
        }
        gpio_write_pin_low(col_pins[col]);
    }
    // Send axes
    for (uint8_t i = 0; i < JOYSTICK_AXIS_COUNT; i++) {
        joystick_set_axis(i, jt_axes_values[i]);
    }
    joystick_flush();
}
#endif

#ifdef MIDI_ENABLE
void init_midi(void) {
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            keypos_t key     = (keypos_t){.col = col, .row = row};
            uint16_t keycode = keymap_key_to_keycode(_MIDI, key);
            uint8_t  index   = (row * MATRIX_COLS) + col;
            if (keycode >= QK_MIDI_NOTE_C_0 && keycode <= QK_MIDI_NOTE_B_5) {
                // Set midi key
                midi_note_key[index] = keycode;
#    ifdef CONSOLE_ENABLE
                uprintf("Midi key index: %u col: %u row: %u\n", index, col, row);
#    endif
            }
        }
    }
}
#endif

layer_state_t layer_state_set_user(layer_state_t state) {
    current_layer = get_highest_layer(state); // Obtiene la capa activa más alta
#ifdef CONSOLE_ENABLE
    uprintf("Layer changed: %d\n", current_layer);
#endif
    switch (current_layer) {
#ifdef JOYSTICK_ENABLE
        case _GAMING:
            init_joystick();
            break;
#endif
#ifdef MIDI_ENABLE
        case _MIDI:
            init_midi();
            break;
#endif
    }
    return state;
}

bool led_update_kb(led_t led_state) {
    bool res = led_update_user(led_state);
    if (res) {
        if (led_state.caps_lock) {
            rgblight_enable_noeeprom();
        } else {
            rgblight_disable_noeeprom();
        }
    }
    return res;
}

void keyboard_post_init_user(void) {
    rgblight_disable_noeeprom();
}

int8_t calcule_curve(int x, float n) {
    return (int8_t)(100 * (1 - pow(1 - x / 100.0, n)));
}

matrix_row_t matrix_get_row(uint8_t row) {
    return matrix[row];
}

void matrix_print(void) {
    // TODO: use print() to dump the current matrix state to console
}

void matrix_hall_get_base(void) {
    for (uint8_t cal = 0; cal < HALL_CALIBRATE_ROUNDS; cal++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            gpio_write_pin_high(col_pins[col]);
            wait_us(HALL_WAIT_US);
            for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
                raw_value                                   = analogReadPin(row_pins[row]);
                matrix_hall_base[(row * MATRIX_COLS) + col] = raw_value > HALL_MIN_BASE ? raw_value : HALL_MIN_BASE;
            }
            gpio_write_pin_low(col_pins[col]);
        }
    }
}

void matrix_hall_get_range(void) {
    for (uint8_t index = 0; index < MATRIX_ROWS * MATRIX_COLS; index++) {
        void *address            = ((void *)EEPROM_HALL_RANGE_START) + (index * 2);
        matrix_hall_range[index] = eeprom_read_byte(address) << 8;
        matrix_hall_range[index] |= eeprom_read_byte(address + 1);
        //  Needs to calibrate, get default range
        if (matrix_hall_range[index] < HALL_MIN_RANGE) {
            matrix_hall_range[index] = HALL_MIN_RANGE;
        }
        if (matrix_hall_range[index] > HALL_MAX_RANGE) {
            matrix_hall_range[index] = HALL_MAX_RANGE;
        }
    }
}

void create_table_curve_response(float curve_response) {
    if (curve_response > 1 && curve_response < 6) {
        for (int x = 0; x <= 100; x++) {
            curve_table[x] = calcule_curve(x, curve_response);
        }
    } else {
        // Lineal response
        for (int x = 0; x <= 100; x++) {
            curve_table[x] = x;
        }
    }
}

void matrix_hall_reset_range(void) {
    for (uint8_t index = 0; index < MATRIX_ROWS * MATRIX_COLS; index++) {
        matrix_hall_range[index] = HALL_MIN_RANGE;
    }
}

void get_configuration_calibrating(void) {
    // 170(10101010) define first calibrate needed
    calibrating = ((eeprom_read_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + id_hall_sensors_calibrate) == 1) || (eeprom_read_byte((uint8_t *)EEPROM_CUSTOM_CONFIG) != 170));
    if (calibrating) {
        // Go to base layer
        layer_clear();
        // Reset range eeprom
        matrix_hall_reset_range();
#ifdef CONSOLE_ENABLE
        uprintf("Range reseted!\n");
        uprintf("Calibrating...\n");
#endif
    }
}

void get_configuration_hall_threshold(void) {
    hall_threshold = eeprom_read_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + id_hall_threshold);
    if (hall_threshold < HALL_DEFAULT_THRESHOLD_MIN || hall_threshold > HALL_DEFAULT_THRESHOLD_MAX) {
        hall_threshold = HALL_DEFAULT_THRESHOLD;
        eeprom_update_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + id_hall_threshold, hall_threshold);
    }
    hall_release = hall_threshold - HALL_DEFAULT_PRESS_RELEASE_MARGIN;
}

void get_configuration_fast_trigger(void) {
    fast_trigger = (eeprom_read_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + id_hall_fast_trigger) == 1);
}

void get_configuration_curve_response(void) {
    curve_response = (eeprom_read_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + id_hall_curve_response)) / 10;
    // To performance calcule curve response
    create_table_curve_response(curve_response);
}

void get_configurations(void) {
    // Base sensor signal
    matrix_hall_get_base();
    // Range switch sensor value
    matrix_hall_get_range();
    // Calibrating
    get_configuration_calibrating();
    // Threshold and release points
    get_configuration_hall_threshold();
    // Fast trigger
    get_configuration_fast_trigger();
    // Curve response
    get_configuration_curve_response();
}

void matrix_init(void) {
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        gpio_set_pin_output(col_pins[col]);
        gpio_write_pin_low(col_pins[col]);
    }
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        gpio_set_pin_input(row_pins[row]);
        analogReadPin(row_pins[row]);
    }
    get_configurations();
    matrix_init_kb();
    wait_ms(1000);
}

#ifdef MIDI_ENABLE
void matrix_scan_midi(uint8_t index, uint8_t row, uint8_t col, uint8_t percent, uint16_t time) {
    uint8_t midi_note = midi_note_key[index] - QK_MIDI_NOTE_C_0 + ((midi_config.octave - 2) * 12);
    uint8_t velocity  = 0;
    if ((midi_note_on[row] & (1 << col))) {
        // Release
        if (percent < HALL_MIDI_THRESHOLD - HALL_DEFAULT_PRESS_RELEASE_MARGIN) {
            midi_send_noteoff(&midi_device, midi_config.channel, midi_note, velocity);
            midi_note_on[row] &= ~(1 << col);
#    ifdef CONSOLE_ENABLE
            uprintf("Midi note: %u octave %u velocity: %u\n", midi_note, midi_config.octave, velocity);
#    endif
        }
    } else {
        // Midi calcule velocity
        if (percent > HALL_DEFAULT_THRESHOLD_MIN && midi_last_percent[index] <= HALL_DEFAULT_THRESHOLD_MIN) {
            midi_velocity_time[index] = time;
        }
        // Press
        if (percent > HALL_MIDI_THRESHOLD) {
            velocity = 127 - timer_elapsed(midi_velocity_time[index]) + HALL_MIDI_KEY_PRESS_ELAPSED;
            if (velocity < 64) {
                velocity = 64;
            } else if (velocity > 127) {
                velocity = 127;
            }
            midi_send_noteon(&midi_device, midi_config.channel, midi_note, velocity);
            midi_note_on[row] |= (1 << col);
#    ifdef CONSOLE_ENABLE
            uprintf("Midi note: %u octave: %u velocity: %u\n", midi_note, midi_config.octave, velocity);
#    endif
        }
    }
    midi_last_percent[index] = percent;
}
#endif

uint8_t matrix_scan_keyboard(void) {
    bool changed = false;
#ifdef MIDI_ENABLE
    uint16_t time = timer_read();
#endif
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        gpio_write_pin_high(col_pins[col]);
        wait_us(HALL_WAIT_US);

        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            raw_value       = analogReadPin(row_pins[row]);
            uint8_t index   = (row * MATRIX_COLS) + col;
            uint8_t percent = 0;
            if (raw_value > matrix_hall_base[index]) {
                // Calcule percent pressed
                percent = (raw_value - matrix_hall_base[index]) * 100 / matrix_hall_range[index];
                if (percent > 100) {
                    percent = 100;
                }
                // Get curved value
                percent = curve_table[percent];
            }
#ifdef MIDI_ENABLE
            if (midi_note_key[index] > 0) {
                matrix_scan_midi(index, row, col, percent, time);
                continue;
            }
#endif
            if (matrix[row] & (1 << col)) {
                // Check released
                if (fast_trigger) {
                    if (percent < matrix_hall_fast_trigger[index] - HALL_DEFAULT_PRESS_RELEASE_MARGIN) {
                        matrix[row] &= ~(1 << col);
                        changed                         = true;
                        matrix_hall_fast_trigger[index] = percent;
#ifdef CONSOLE_ENABLE
                        uprintf("Release col: %u row: %u base: %u range: %u value: %u\n", col, row, matrix_hall_base[index], matrix_hall_range[index], raw_value);
#endif
                    } else if (percent > matrix_hall_fast_trigger[index]) {
                        matrix_hall_fast_trigger[index] = percent;
                    }
                } else if (percent < hall_release) {
                    matrix[row] &= ~(1 << col);
                    changed = true;
#ifdef CONSOLE_ENABLE
                    uprintf("Release col: %u row: %u base: %u range: %u value: %u\n", col, row, matrix_hall_base[index], matrix_hall_range[index], raw_value);
#endif
                }
            } else {
                // Check press
                if (fast_trigger) {
                    if (percent > hall_threshold && percent > matrix_hall_fast_trigger[index] + HALL_DEFAULT_PRESS_RELEASE_MARGIN) {
                        matrix[row] |= (1 << col);
                        changed                         = true;
                        matrix_hall_fast_trigger[index] = percent;
#ifdef CONSOLE_ENABLE
                        uprintf("Press col: %u row: %u base: %u range: %u value: %u\n", col, row, matrix_hall_base[index], matrix_hall_range[index], raw_value);
#endif
                    } else if (percent < matrix_hall_fast_trigger[index]) {
                        matrix_hall_fast_trigger[index] = percent;
                    }
                } else if (percent > hall_threshold) {
                    matrix[row] |= (1 << col);
                    changed = true;
#ifdef CONSOLE_ENABLE
                    uprintf("Press col: %u row: %u base: %u range: %u value: %u\n", col, row, matrix_hall_base[index], matrix_hall_range[index], raw_value);
#endif
                }
            }
        }
        gpio_write_pin_low(col_pins[col]);
    }

    matrix_scan_kb();
    return changed;
}

void matrix_scan_calibrate(void) {
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        gpio_write_pin_high(col_pins[col]);
        wait_us(HALL_WAIT_US);
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            raw_value     = analogReadPin(row_pins[row]);
            uint8_t index = (row * MATRIX_COLS) + col;
            // Update range
            if (raw_value > matrix_hall_base[index] && raw_value - matrix_hall_base[index] > matrix_hall_range[index]) {
                matrix_hall_range[index] = raw_value - matrix_hall_base[index];
#ifdef CONSOLE_ENABLE
                uprintf("New col: %u row: %u range value: %u\n", col, row, matrix_hall_range[index]);
#endif
            }
        }
        gpio_write_pin_low(col_pins[col]);
    }
}

uint8_t matrix_scan(void) {
#ifdef JOYSTICK_ENABLE
    // Joystick funcionality
    if (current_layer == _GAMING) {
        matrix_scan_joystick();
        return false;
    }
#endif
    if (calibrating) {
        matrix_scan_calibrate();
        return false;
    }
    // Keyboard funcionality
    return matrix_scan_keyboard();
}

void matrix_init_kb(void) {
    matrix_init_user();
}

void matrix_scan_kb(void) {
    matrix_scan_user();
}

// user-defined overridable functions
__attribute__((weak)) void matrix_init_user(void) {}

__attribute__((weak)) void matrix_scan_user(void) {}

__attribute__((weak)) void matrix_slave_scan_user(void) {}

//[{ VIA }]//////////////////////////////////////////////////////////////////

void custom_set_value(uint8_t *data) {
    uint8_t *value_id   = &(data[0]);
    uint8_t *value_data = &(data[1]);
#ifdef CONSOLE_ENABLE
    uprintf("custom_set_value! value: %u, data: %u\n", *value_id, *value_data);
#endif
    // Save value
    eeprom_update_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + *value_id, value_data[0]);
    // Update configs
    switch (*value_id) {
        case id_hall_sensors_calibrate:
            if (value_data[0] == 0) {
                // Save ranges
                for (uint8_t index = 0; index < MATRIX_ROWS * MATRIX_COLS; index++) {
                    void *address = ((void *)EEPROM_HALL_RANGE_START) + (index * 2);
                    eeprom_update_byte(address, (uint8_t)(matrix_hall_range[index] >> 8));
                    eeprom_update_byte(address + 1, (uint8_t)(matrix_hall_range[index] & 0xFF));
                }
                // Define first calibrate done
                eeprom_update_byte((uint8_t *)EEPROM_CUSTOM_CONFIG, 170);
#ifdef CONSOLE_ENABLE
                uprintf("Ranges saved!\n");
#endif
            }
            get_configuration_calibrating();
            break;
        case id_layout_reset_keymap:
            if (value_data[0] == 1) {
                eeconfig_init_via();
                eeprom_update_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + *value_id, 0);
#ifdef CONSOLE_ENABLE
                uprintf("Keymap reseted!\n");
#endif
            }
            break;
        case id_hall_curve_response:
            get_configuration_curve_response();
            break;
        case id_hall_fast_trigger:
            get_configuration_fast_trigger();
            break;
        case id_hall_threshold:
            get_configuration_hall_threshold();
            break;
    }
}

void custom_get_value(uint8_t *data) {
    uint8_t *value_id   = &(data[0]);
    uint8_t *value_data = &(data[1]);
    value_data[0]       = eeprom_read_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + *value_id);
}

void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    // data = [ command_id, channel_id, value_id, value_data ]
    uint8_t *command_id        = &(data[0]);
    uint8_t *channel_id        = &(data[1]);
    uint8_t *value_id_and_data = &(data[2]);

    if (*channel_id == id_custom_channel) {
        switch (*command_id) {
            case id_custom_set_value: {
                custom_set_value(value_id_and_data);
                break;
            }
            case id_custom_get_value: {
                custom_get_value(value_id_and_data);
                break;
            }
            case id_custom_save: {
                break;
            }
            default: {
                // Unhandled message.
                *command_id = id_unhandled;
                break;
            }
        }
        return;
    }

    // Return the unhandled state
    *command_id = id_unhandled;
}
