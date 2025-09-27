// Copyright 2025 Miguelio (teclados@miguelio.com)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "mv_main.h"
#include "via.h"
#include "raw_hid.h"
#ifdef JOYSTICK_ENABLE
#    include "joystick.h"
#    include "keycodes.h"
#endif
#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

// Shared variables
BSEMAPHORE_DECL(init_core1, true);
volatile atomic_bool  run_core_1                                 = true;
atomic_uint_least32_t matrix_hall_raw[MATRIX_COLS * MATRIX_ROWS] = {0};
// volatile char         message[64]                                = {0};
pin_t row_pins[MATRIX_ROWS] = MATRIX_ROW_PINS;
pin_t col_pins[MATRIX_COLS] = MATRIX_COL_PINS;
// Variables
static uint16_t matrix_hall_base[MATRIX_ROWS * MATRIX_COLS]         = {0};
static uint16_t matrix_hall_range[MATRIX_ROWS * MATRIX_COLS]        = {0};
static uint16_t matrix_hall_fast_release[MATRIX_ROWS * MATRIX_COLS] = {0};
static uint16_t matrix_hall_last_value[MATRIX_ROWS * MATRIX_COLS]   = {0};

static matrix_row_t matrix[MATRIX_ROWS]    = {0};
int16_t             col_reads[MATRIX_ROWS] = {0};
#ifdef COUNT_SCANS
static uint32_t count     = 0;
static uint16_t last_time = 0;
#endif

static uint8_t current_layer = 0;
static bool    calibrating   = false;
static bool    fast_release  = false;

// Threshold
static uint8_t hall_threshold                             = HALL_DEFAULT_THRESHOLD;
static uint8_t hall_thresholds[MATRIX_ROWS * MATRIX_COLS] = {0};

// Curve
static uint8_t curves_table[5][101]                   = {0};
static uint8_t hall_curve                             = 0;
static uint8_t hall_curves[MATRIX_ROWS * MATRIX_COLS] = {0};

// Replaceable functions
__attribute__((weak)) void matrix_init_kb(void) {
    matrix_init_user();
}
__attribute__((weak)) void matrix_scan_kb(void) {
    matrix_scan_user();
}
__attribute__((weak)) void matrix_init_user(void) {}
__attribute__((weak)) void matrix_scan_user(void) {}

matrix_row_t matrix_get_row(uint8_t row) {
    return matrix[row];
}

void matrix_print(void) {
    // TODO: use print() to dump the current matrix state to console
}

void matrix_init(void) {
    matrix_init_user();
}

bool valid_sensor(uint8_t index) {
    return (index != 17 && index != 35);
}

void matrix_hall_reset_range(void) {
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            uint8_t index            = (row * MATRIX_COLS) + col;
            matrix_hall_range[index] = HALL_MIN_RANGE;
        }
    }
}

// 1: 0.003x^2 + 0.7x
// 2: 0.006x^2 + 0.4x
// 3: 0.0085x^2 + 0.15x
// 4: 0.01x^2
void create_table_curves(void) {
    for (int x = 0; x <= 100; x++) {
        curves_table[0][x] = x;
        curves_table[1][x] = (sqrt(0.012 * x + 0.49) - 0.7) / 0.006;
        curves_table[2][x] = (sqrt(0.024 * x + 0.16) - 0.4) / 0.012;
        curves_table[3][x] = (sqrt(0.034 * x + 0.0225) - 0.15) / 0.017;
        curves_table[4][x] = 10 * sqrt(x);
    }
}

uint8_t hall_keycode_to_curve(uint16_t keycode, uint8_t default_curve) {
    uint8_t curve = default_curve;
    switch (keycode) {
        case KC_P0:
            curve = 0;
            break;
        case KC_P1:
            curve = 1;
            break;
        case KC_P2:
            curve = 2;
            break;
        case KC_P3:
            curve = 3;
            break;
        case KC_P4:
            curve = 4;
            break;
    }
    return curve;
}

uint8_t hall_keycode_to_threshold(uint16_t keycode, uint8_t default_threshold) {
    uint8_t threshold = default_threshold;
    switch (keycode) {
        case KC_P0:
            threshold = 0;
            break;
        case KC_P1:
            threshold = 10;
            break;
        case KC_P2:
            threshold = 20;
            break;
        case KC_P3:
            threshold = 30;
            break;
        case KC_P4:
            threshold = 40;
            break;
        case KC_P5:
            threshold = 50;
            break;
        case KC_P6:
            threshold = 60;
            break;
        case KC_P7:
            threshold = 70;
            break;
        case KC_P8:
            threshold = 80;
            break;
        case KC_P9:
            threshold = 90;
            break;
    }
    threshold = threshold * (100 - HALL_THRESHOLD_MARGIN * 2) / 100 + HALL_THRESHOLD_MARGIN;
    // Check limits to valid threshold
    if (threshold < HALL_THRESHOLD_MARGIN || threshold > (100 - HALL_THRESHOLD_MARGIN)) {
        threshold = HALL_DEFAULT_THRESHOLD;
    }
    return threshold;
}

void check_minimun_threshold(uint8_t index) {
    if (hall_thresholds[index] < curves_table[hall_curves[index]][HALL_THRESHOLD_MARGIN]) {
#ifdef CONSOLE_ENABLE
        uprintf("Threshold min value changed: %u => %u\n", hall_thresholds[index], curves_table[hall_curves[index]][HALL_THRESHOLD_MARGIN]);
#endif
        hall_thresholds[index] = curves_table[hall_curves[index]][HALL_THRESHOLD_MARGIN];
    }
}

void matrix_hall_get_base(void) {
    for (uint8_t t = 0; t < HALL_GET_BASE_SCANS; t++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
                uint8_t index = row * MATRIX_COLS + col;
                matrix_hall_base[index] += atomic_load(&matrix_hall_raw[index]);
                if (t == HALL_GET_BASE_SCANS - 1) {
                    matrix_hall_base[index] = matrix_hall_base[index] / HALL_GET_BASE_SCANS;
                }
            }
        }
    }
}

void matrix_hall_get_range(void) {
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            uint8_t index            = (row * MATRIX_COLS) + col;
            void   *address          = ((void *)EEPROM_HALL_RANGE_START) + (index * 2);
            matrix_hall_range[index] = eeprom_read_byte(address) << 8;
            matrix_hall_range[index] |= eeprom_read_byte(address + 1);
            // Secure min range
            if (matrix_hall_range[index] < HALL_MIN_RANGE) {
                matrix_hall_range[index] = 0;
            }
        }
    }
}

void get_configuration_calibrating(void) {
    calibrating = (eeprom_read_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + id_hall_sensors_calibrate) == 1);
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

void get_configuration_curves(void) {
    // Default curve response
    hall_curve = eeprom_read_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + id_hall_curve);
    // To performance calcule curve response
    create_table_curves();
    // Get curve by key
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            uint8_t  index     = row * MATRIX_COLS + col;
            keypos_t key       = (keypos_t){.col = col, .row = row};
            uint16_t keycode   = keymap_key_to_keycode(_THRESHOLD, key);
            hall_curves[index] = hall_keycode_to_curve(keycode, hall_curve);
        }
    }
}

void get_configuration_hall_thresholds(void) {
    hall_threshold = eeprom_read_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + id_hall_threshold);
    // Get threshold by key
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            uint8_t  index         = row * MATRIX_COLS + col;
            keypos_t key           = (keypos_t){.col = col, .row = row};
            uint16_t keycode       = keymap_key_to_keycode(_THRESHOLD, key);
            hall_thresholds[index] = hall_keycode_to_threshold(keycode, hall_threshold);
            // Secure min threshold with curve
            check_minimun_threshold(index);
        }
    }

#ifdef CONSOLE_ENABLE
    uprintf("Threshold default: %u\n", hall_threshold);
#endif
}

void get_configuration_fast_release(void) {
    fast_release = (eeprom_read_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + id_hall_fast_release) == 1);
}

void get_configurations(void) {
    // Base sensor signal
    matrix_hall_get_base();
    // Range switch sensor value
    matrix_hall_get_range();
    // Calibrating
    get_configuration_calibrating();
    // Curve response
    get_configuration_curves();
    // Threshold points, IMPORTANT do it after configuration curve
    get_configuration_hall_thresholds();
    // Fast trigger
    get_configuration_fast_release();
}

void keyboard_post_init_kb(void) {
    wait_ms(CORE1_WAIT_INIT);
    get_configurations();
    rgblight_disable();
#ifdef COUNT_SCANS
    last_time = timer_read();
#endif
}

uint8_t matrix_scan_keyboard(void) {
    bool changed = false;
#ifdef MIDI_ENABLE
    uint16_t time = timer_read();
#endif
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            uint8_t               index = row * MATRIX_COLS + col;
            atomic_uint_least32_t raw   = atomic_load(&matrix_hall_raw[index]);
            if (matrix_hall_last_value[index] == raw || matrix_hall_range[index] == 0) {
                continue;
            } else {
                matrix_hall_last_value[index] = raw;
            }
            uint16_t temp_diff = raw < matrix_hall_base[index] ? matrix_hall_base[index] - raw : raw - matrix_hall_base[index];
            // Calcule percent pressed
            uint8_t percent = temp_diff * 100 / matrix_hall_range[index];
            if (percent > 100) {
                percent = 100;
            }
            // Get curved value
            percent = curves_table[hall_curves[index]][percent];
#ifdef MIDI_ENABLE
            if (midi_note_key[index] > 0) {
                matrix_scan_midi(index, row, col, percent, time);
                continue;
            }
#endif
            // Doing calibrate
            if (calibrating) {
                uint16_t temp_diff = raw < matrix_hall_base[index] ? matrix_hall_base[index] - raw : raw - matrix_hall_base[index];
                // Update range
                if (temp_diff > matrix_hall_range[index] && valid_sensor(index)) {
                    matrix_hall_range[index] = temp_diff;
#ifdef CONSOLE_ENABLE
                    uprintf("Col: %u row: %u range: %u\n", col, row, matrix_hall_range[index]);
#endif
                }
            } else {
                if (matrix[row] & (1 << col)) {
                    // Check released
                    if (fast_release) {
                        if (percent < matrix_hall_fast_release[index] - HALL_FAST_RELEASE_MARGIN) {
                            matrix[row] &= ~(1 << col);
                            changed                         = true;
                            matrix_hall_fast_release[index] = percent;
#ifdef CONSOLE_ENABLE
                            uprintf("Release col: %u row: %u base: %u range: %u value: %lu\n", col, row, matrix_hall_base[index], matrix_hall_range[index], raw);
#endif
                        } else if (percent > matrix_hall_fast_release[index]) {
                            matrix_hall_fast_release[index] = percent;
                        }
                    } else if (percent < hall_thresholds[index] - HALL_PRESS_RELEASE_MARGIN) {
                        matrix[row] &= ~(1 << col);
                        changed = true;
#ifdef CONSOLE_ENABLE
                        uprintf("Release col: %u row: %u base: %u range: %u value: %lu\n", col, row, matrix_hall_base[index], matrix_hall_range[index], raw);
#endif
                    }
                } else {
                    // Check press
                    if (fast_release) {
                        if (percent > hall_thresholds[index] && percent > matrix_hall_fast_release[index] + HALL_FAST_RELEASE_MARGIN) {
                            matrix[row] |= (1 << col);
                            changed                         = true;
                            matrix_hall_fast_release[index] = percent;
#ifdef CONSOLE_ENABLE
                            uprintf("Press col: %u row: %u base: %u range: %u value: %lu threshold: %u\n", col, row, matrix_hall_base[index], matrix_hall_range[index], raw, hall_threshold);
#endif
                        } else if (percent < matrix_hall_fast_release[index]) {
                            matrix_hall_fast_release[index] = percent;
                        }
                    } else if (percent > hall_thresholds[index]) {
                        matrix[row] |= (1 << col);
                        changed = true;
#ifdef CONSOLE_ENABLE
                        uprintf("Press col: %u row: %u base: %u range: %u value: %lu threshold: %u\n", col, row, matrix_hall_base[index], matrix_hall_range[index], raw, hall_threshold);
#endif
                    }
                }
            }
        }
    }
    return changed;
}

//[{ VIA }]////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void custom_set_value(uint8_t *data) {
    uint8_t *value_id   = &(data[0]);
    uint8_t *value_data = &(data[1]);
#ifdef CONSOLE_ENABLE
    uprintf("custom_set_value! value: %u, data: %u\n", *value_id, *value_data);
#endif
    // Save value with limited value id
    if (*value_id >= id_hall_sensors_calibrate && *value_id <= id_hall_curve) {
        eeprom_update_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + *value_id, value_data[0]);
    }
    // Update configs
    switch (*value_id) {
        case id_hall_sensors_calibrate:
            if (value_data[0] == 0) {
                // Save ranges
                for (uint8_t index = 0; index < MATRIX_ROWS * MATRIX_COLS; index++) {
                    void *address = ((void *)EEPROM_HALL_RANGE_START) + (index * 2);
                    // Secure min range
                    if (matrix_hall_range[index] <= HALL_MIN_RANGE) {
                        matrix_hall_range[index] = 0;
                    }
                    eeprom_update_byte(address, (uint8_t)(matrix_hall_range[index] >> 8));
                    eeprom_update_byte(address + 1, (uint8_t)(matrix_hall_range[index] & 0xFF));
                }
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
        case id_hall_fast_release:
            get_configuration_fast_release();
            break;
        case id_hall_threshold:
            get_configuration_hall_thresholds();
            break;
        case id_hall_curve:
            get_configuration_curves();
            get_configuration_hall_thresholds();
            break;
        case id_btn_reset_threshold:
#ifdef CONSOLE_ENABLE
            uprintf("Run core 1!\n");
#endif
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
                // Sleep core 1
                atomic_store(&run_core_1, false);
                wait_ms(CORE1_WAIT_SLEEP_WAKEUP);
                custom_set_value(value_id_and_data);
                // Wakeup core 1
                atomic_store(&run_core_1, true);
                chBSemSignal(&init_core1);
                wait_ms(CORE1_WAIT_SLEEP_WAKEUP);
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

void on_via_change_threshold_by_key(uint8_t row, uint8_t col, uint16_t keycode) {
    uint8_t index          = row * MATRIX_COLS + col;
    hall_thresholds[index] = hall_keycode_to_threshold(keycode, hall_threshold);
    // Secure min threshold with curve
    check_minimun_threshold(index);
#ifdef CONSOLE_ENABLE
    uprintf("Changed key threshold, col: %u, row: %u, threshold: %u\n", col, row, hall_thresholds[index]);
#endif
}

void on_via_change_curve_by_key(uint8_t row, uint8_t col, uint16_t keycode) {
    uint8_t index      = (row * MATRIX_COLS) + col;
    hall_curves[index] = hall_keycode_to_curve(keycode, hall_curve);
    // Secure min threshold with curve
    check_minimun_threshold(index);
#ifdef CONSOLE_ENABLE
    uprintf("Changed key curve, col: %u, row: %u, curve: %u\n", col, row, hall_curves[index]);
#endif
}

bool via_command_kb(uint8_t *data, uint8_t length) {
    uint8_t *command_id   = &(data[0]);
    uint8_t *command_data = &(data[1]);
    if (*command_id == id_dynamic_keymap_set_keycode) {
        // Sleep core 1
        atomic_store(&run_core_1, false);
        wait_ms(CORE1_WAIT_SLEEP_WAKEUP);
        // Process change
        uint8_t  layer   = command_data[0];
        uint8_t  row     = command_data[1];
        uint8_t  col     = command_data[2];
        uint16_t keycode = (command_data[3] << 8) | command_data[4];
        switch (layer) {
            case _THRESHOLD:
                on_via_change_threshold_by_key(row, col, keycode);
                break;
            case _CURVE:
                on_via_change_curve_by_key(row, col, keycode);
                break;
        }
        // Save data
        dynamic_keymap_set_keycode(command_data[0], command_data[1], command_data[2], (command_data[3] << 8) | command_data[4]);
        // Wakeup core 1
        atomic_store(&run_core_1, true);
        chBSemSignal(&init_core1);
        wait_ms(CORE1_WAIT_SLEEP_WAKEUP);
        // Feedback to VIA
        raw_hid_send(data, length);
        return true;
    }
#ifdef CONSOLE_ENABLE
    uprintf("VIA command: %u\n", *command_id);
#endif
    return false;
}

//[{ JOYSTICK }]///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
static int     jt_axes_values[JOYSTICK_AXIS_COUNT] = {0};
static jt_key  jt_keys[JT_KEY_COUNT]               = {};
static uint8_t jt_state                            = 0; // 1: initiate, 2: inited

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
    jt_state = jt_inited;
}

void matrix_scan_joystick(void) {
    memset(jt_axes_values, 0, sizeof(jt_axes_values));
    uint8_t jt_index = 0;
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        if (jt_keys[jt_index].key.col != col) {
            continue;
        }
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            if (jt_keys[jt_index].key.row != row) {
                continue;
            }
            uint8_t               index      = (row * MATRIX_COLS) + col;
            atomic_uint_least32_t raw        = atomic_load(&matrix_hall_raw[index]);
            uint16_t              temp_diff  = raw < matrix_hall_base[index] ? matrix_hall_base[index] - raw : raw - matrix_hall_base[index];
            uint8_t               axis_value = 0;
            if (temp_diff > HALL_PRESS_RELEASE_MARGIN && matrix_hall_range[index] != 0) {
                axis_value = temp_diff * 127 / matrix_hall_range[index];
                if (axis_value > 127) {
                    axis_value = 127;
                }
            }
            // Axes
            if (jt_keys[jt_index].index < JOYSTICK_AXIS_COUNT * 2) {
                // Even sum and odd subtract
                jt_axes_values[jt_keys[jt_index].index / 2] += axis_value * (jt_keys[jt_index].index & 1 ? 1 : -1);
#    ifdef CONSOLE_ENABLE
                // Secure reads
                if (axis_value > jt_keys[jt_index].value + JT_MARGIN || axis_value < jt_keys[jt_index].value - JT_MARGIN || (axis_value > 127 - JT_MARGIN && axis_value > jt_keys[jt_index].value) || (axis_value < JT_MARGIN && axis_value < jt_keys[jt_index].value)) {
                    uprintf("Joystick axis index: %u value: %d\n", jt_keys[jt_index].index, axis_value);
                    jt_keys[jt_index].value = axis_value;
                }
#    endif
                // Buttons
            } else {
                uint8_t jt_button_index = jt_keys[jt_index].index - (JOYSTICK_AXIS_COUNT * 2);
                uint8_t percent         = 0;
                // Calcule percent pressed
                percent = temp_diff * 100 / matrix_hall_range[index];
                if (percent > 100) {
                    percent = 100;
                }
                // Get curved value
                percent = curves_table[hall_curves[index]][percent];
                if (joystick_state.buttons[jt_button_index / 8] & (1 << (jt_button_index % 8))) {
                    // Check released
                    if (fast_release) {
                        if (percent < matrix_hall_fast_release[index] - HALL_PRESS_RELEASE_MARGIN) {
                            matrix[row] &= ~(1 << col);
                            joystick_state.buttons[jt_button_index / 8] &= ~(1 << (jt_button_index % 8));
                            joystick_state.dirty            = true;
                            matrix_hall_fast_release[index] = percent;
#    ifdef CONSOLE_ENABLE
                            uprintf("Release joystick button index: %u value: %lu\n", jt_button_index, raw);
#    endif
                        } else if (percent > matrix_hall_fast_release[index]) {
                            matrix_hall_fast_release[index] = percent;
                        }
                    } else if (percent < hall_thresholds[index] - HALL_PRESS_RELEASE_MARGIN) {
                        joystick_state.buttons[jt_button_index / 8] &= ~(1 << (jt_button_index % 8));
                        joystick_state.dirty = true;
#    ifdef CONSOLE_ENABLE
                        uprintf("Release joystick button index: %u value: %lu\n", jt_button_index, raw);
#    endif
                    }
                } else {
                    // Check press
                    if (fast_release) {
                        if (percent > hall_thresholds[index] && percent > matrix_hall_fast_release[index] + HALL_PRESS_RELEASE_MARGIN) {
                            joystick_state.buttons[jt_button_index / 8] |= 1 << (jt_button_index % 8);
                            joystick_state.dirty            = true;
                            matrix_hall_fast_release[index] = percent;
#    ifdef CONSOLE_ENABLE
                            uprintf("Press joystick button index: %u value: %lu\n", jt_button_index, raw);
#    endif
                        } else if (percent < matrix_hall_fast_release[index]) {
                            matrix_hall_fast_release[index] = percent;
                        }
                    } else if (percent > hall_thresholds[index]) {
                        if (JT_KEYCODES[jt_keys[jt_index].index] == 20992) {
                            jt_state = 0;
                            layer_move(0);
                            wait_ms(1000);
                        } else {
                            joystick_state.buttons[jt_button_index / 8] |= 1 << (jt_button_index % 8);
                            joystick_state.dirty = true;
#    ifdef CONSOLE_ENABLE
                            uprintf("Press joystick button index: %u value: %lu\n", jt_button_index, raw);
#    endif
                        }
                    }
                }
            }
            jt_index += 1;
        }
    }
    // Send axes
    for (uint8_t i = 0; i < JOYSTICK_AXIS_COUNT; i++) {
        joystick_set_axis(i, jt_axes_values[i]);
    }
    joystick_flush();
}
#endif

layer_state_t layer_state_set_user(layer_state_t state) {
    current_layer = get_highest_layer(state); // Get upper active layer
#ifdef CONSOLE_ENABLE
    uprintf("Layer changed: %d\n", current_layer);
#endif
    switch (current_layer) {
#ifdef JOYSTICK_ENABLE
        case _GAMING:
            if (jt_state == 0) {
                jt_state = jt_initiate;
                init_joystick();
            }
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

//[{ MAIN MATRIX SCAN }]//////////////////////////////////////////////////////////////////////////////

uint8_t matrix_scan(void) {
#ifdef COUNT_SCANS
    uint16_t elapsed = timer_elapsed_fast(last_time);
    if (elapsed >= 1000) {
        atomic_uint_least32_t raw = atomic_load(&matrix_hall_raw[0]);
        printf("%s %lu %s %lu\n", "Core 0 scans:", count, "Value sensor:", raw);
        count     = 0;
        last_time = timer_read();
    }

    count++;
#endif
#ifdef JOYSTICK_ENABLE
    // Joystick funcionality
    if (current_layer == _GAMING) {
        if (jt_state == jt_inited) {
            matrix_scan_joystick();
        }
        return false;
    }
#endif
    return matrix_scan_keyboard();
}

/*

// Copyright 2025 Miguelio (teclados@miguelio.com)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "mv_main.h"
#ifdef MIDI_ENABLE
extern MidiDevice midi_device;
#endif

// Semaphore and mutexes
BSEMAPHORE_DECL(init_core1, false);
mutex_t mutex_cols[MATRIX_COLS];

// Shared variables
int16_t matrix_hall_raw[MATRIX_COLS][MATRIX_ROWS] = {0};
pin_t   row_pins[MATRIX_ROWS]                     = MATRIX_ROW_PINS;
pin_t   col_pins[MATRIX_COLS]                     = MATRIX_COL_PINS;
// Variables
static matrix_row_t matrix[MATRIX_ROWS]                                 = {0};


static uint8_t      current_layer                                       = 0;


// Status
// static uint8_t base_skip_rounds = 0;
// static uint8_t status = waiting_to_get_base;

#ifdef COUNT_SCANS
static uint32_t count     = 0;
static uint16_t last_time = 0;
#endif

// Replaceable functions
__attribute__((weak)) void matrix_init_kb(void) {
    matrix_init_user();
}

__attribute__((weak)) void matrix_scan_kb(void) {
    matrix_scan_user();
}

__attribute__((weak)) void matrix_init_user(void) {}

__attribute__((weak)) void matrix_scan_user(void) {}

void keyboard_pre_init_user(void) {
    for (uint8_t i = 0; i < MATRIX_COLS; i++) {
        chMtxObjectInit(&mutex_cols[i]);
    }
}



#ifdef MIDI_ENABLE
static uint16_t     midi_note_key[MATRIX_ROWS * MATRIX_COLS]      = {0};
static matrix_row_t midi_note_on[MATRIX_ROWS]                     = {0};
static uint8_t      midi_last_percent[MATRIX_ROWS * MATRIX_COLS]  = {0};
static uint16_t     midi_velocity_time[MATRIX_ROWS * MATRIX_COLS] = {0};

void init_midi(void) {
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            // Skipping missing sensors
            if (!valid_sensor(col, row)) {
                continue;
            }
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

void matrix_scan_midi(uint8_t index, uint8_t row, uint8_t col, uint8_t percent, uint16_t time) {
    uint8_t midi_note = midi_note_key[index] - QK_MIDI_NOTE_C_0 + ((midi_config.octave - 2) * 12);
    uint8_t velocity  = 0;
    if ((midi_note_on[row] & (1 << col))) {
        // Release
        if (percent < HALL_MIDI_THRESHOLD - HALL_PRESS_RELEASE_MARGIN) {
            midi_send_noteoff(&midi_device, midi_config.channel, midi_note, velocity);
            midi_note_on[row] &= ~(1 << col);
#    ifdef CONSOLE_ENABLE
            uprintf("Midi note: %u octave %u velocity: %u\n", midi_note, midi_config.octave, velocity);
#    endif
        }
    } else {
        // Midi calcule velocity
        if (percent > HALL_THRESHOLD_MARGIN && midi_last_percent[index] <= HALL_THRESHOLD_MARGIN) {
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

void keyboard_post_init_user(void) {
    rgblight_disable();
    wait_ms(500);
    chBSemSignal(&init_core1);
#ifdef CONSOLE_ENABLE
    uprintf("Keyboard inited!\n");
#endif
#ifdef COUNT_SCANS
    last_time = timer_read();
#endif
}



uint8_t matrix_scan(void) {
#ifdef COUNT_SCANS
    uint16_t elapsed = timer_elapsed(last_time);
    if (elapsed >= 1000) {
        printf("%s %lu\n", "Core 0 scans:", count);
        count     = 0;
        last_time = timer_read();
    }
    count++;
#endif
#ifdef JOYSTICK_ENABLE
    // Joystick funcionality
    if (current_layer == _GAMING) {
        if (jt_state == jt_inited) {
            matrix_scan_joystick();
        }
        return false;
    }
#endif
    // Keyboard funcionality
    return matrix_scan_keyboard();
}




*/