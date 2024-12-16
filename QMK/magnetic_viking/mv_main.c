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

static matrix_row_t matrix[MATRIX_ROWS]                              = {0};
static int8_t       matrix_hall_base[MATRIX_ROWS * MATRIX_COLS]      = {0};
static int8_t       matrix_hall_range[MATRIX_ROWS * MATRIX_COLS]     = {0};
static int8_t       matrix_hall_fast_mode[MATRIX_ROWS * MATRIX_COLS] = {0};
static pin_t        row_pins[MATRIX_ROWS]                            = MATRIX_ROW_PINS;
static pin_t        col_pins[MATRIX_COLS]                            = MATRIX_COL_PINS;
static int8_t       hall_threshold                                   = HALL_DEFAULT_THRESHOLD;
static int8_t       hall_release                                     = HALL_DEFAULT_THRESHOLD - HALL_DEFAULT_PRESS_RELEASE_MARGIN;
static int16_t      raw_value                                        = 0;
static bool         calibrating                                      = false;
static bool         fast_mode                                        = false;
static float        hall_curve                                       = 1;
static int8_t       curve_table[101]                                 = {0};

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
            if (col == 0) {
                gpio_write_pin_low(col_pins[MATRIX_COLS - 1]);
            } else {
                gpio_write_pin_low(col_pins[col - 1]);
            }
            gpio_write_pin_high(col_pins[col]);
            wait_us(HALL_WAIT_US);
            for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
                matrix_hall_base[(row * MATRIX_COLS) + col] = analogReadPin(row_pins[row]) / 4;
            }
        }
    }
}

void matrix_hall_get_range(void) {
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            uint8_t index            = (row * MATRIX_COLS) + col;
            matrix_hall_range[index] = eeprom_read_byte((uint8_t *)EEPROM_HALL_RANGE_START + index);
            //  Needs to calibrate, get default range
            if (matrix_hall_range[index] <= 0 || matrix_hall_range[index] > 127) {
                matrix_hall_range[index] = HALL_DEFAULT_RANGE;
            }
        }
    }
}

void matrix_hall_reset_range(bool reset_eeprom) {
    memset(matrix_hall_range, 0, sizeof(matrix_hall_range));
    if (reset_eeprom) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
                uint8_t index = (row * MATRIX_COLS) + col;
                eeprom_update_byte((uint8_t *)EEPROM_HALL_RANGE_START + index, 0);
            }
        }
    }
}

void get_configurations(void) {
    // Calibrating
    calibrating = (eeprom_read_byte((uint8_t *)EEPROM_CUSTOM_CONFIG) == 1);
    if (calibrating) {
        matrix_hall_reset_range(true);
#ifdef CONSOLE_ENABLE
        uprintf("Range reseted!\n");
        uprintf("Calibrating...\n");
#endif
    }
    // Threshold and release points
    hall_threshold = eeprom_read_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + 2);
    if (hall_threshold < HALL_DEFAULT_THRESHOLD_MIN || hall_threshold > HALL_DEFAULT_THRESHOLD_MAX) {
        hall_threshold = HALL_DEFAULT_THRESHOLD;
        eeprom_update_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + 2, hall_threshold);
    }
    hall_release = hall_threshold - HALL_DEFAULT_PRESS_RELEASE_MARGIN;
    // Fast release
    fast_mode = (eeprom_read_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + 3) == 1);
    // Response curve
    switch (eeprom_read_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + 4)) {
        case 1:
            hall_curve = 1.5;
            break;
        case 2:
            hall_curve = 2;
            break;
        case 3:
            hall_curve = 3;
            break;
        default:
            hall_curve = 1;
            break;
    }
    // Calcule curve response
    if (hall_curve > 1) {
        for (int x = 0; x <= 100; x++) {
            curve_table[x] = calcule_curve(x, hall_curve);
        }
    } else {
        // Lineal response
        for (int x = 0; x <= 100; x++) {
            curve_table[x] = x;
        }
    }
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
    matrix_hall_get_base();
    matrix_hall_reset_range(false);
    matrix_hall_get_range();
    get_configurations();
    matrix_init_kb();
}

uint8_t matrix_scan(void) {
    bool changed = false;

    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        if (col == 0) {
            gpio_write_pin_low(col_pins[MATRIX_COLS - 1]);
        } else {
            gpio_write_pin_low(col_pins[col - 1]);
        }
        gpio_write_pin_high(col_pins[col]);
        wait_us(HALL_WAIT_US);

        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            raw_value     = analogReadPin(row_pins[row]) / 4;
            uint8_t index = (row * MATRIX_COLS) + col;

            if (raw_value > HALL_DEFAULT_RANGE) {
                if (calibrating) {
                    // Update range
                    if (raw_value > matrix_hall_base[index] && raw_value - matrix_hall_base[index] > matrix_hall_range[index]) {
                        matrix_hall_range[index] = raw_value - matrix_hall_base[index];
#ifdef CONSOLE_ENABLE
                        uprintf("New col: %u row: %u range value: %u\n", col, row, matrix_hall_range[index]);
#endif
                    }
                } else {
                    uint8_t percent = 0;
                    if (raw_value > matrix_hall_base[index]) {
                        // Calcule percent pressed
                        percent = (raw_value - matrix_hall_base[index]) * 100 / matrix_hall_range[index];
                        if (percent > 100) {
                            percent = 100;
                        }
                        // Get curve value
                        percent = curve_table[percent];
                    }
                    if (matrix[row] & (1 << col)) {
                        // Check released
                        if (fast_mode) {
                            if (percent < matrix_hall_fast_mode[index] - HALL_DEFAULT_PRESS_RELEASE_MARGIN) {
                                matrix[row] &= ~(1 << col);
                                changed                      = true;
                                matrix_hall_fast_mode[index] = percent;
#ifdef CONSOLE_ENABLE
                                uprintf("Release col: %u row: %u base: %u range: %u value: %u\n", col, row, matrix_hall_base[index], matrix_hall_range[index], raw_value);
#endif
                            } else if (percent > matrix_hall_fast_mode[index]) {
                                matrix_hall_fast_mode[index] = percent;
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
                        if (fast_mode) {
                            if (percent > hall_threshold && percent > matrix_hall_fast_mode[index] + HALL_DEFAULT_PRESS_RELEASE_MARGIN) {
                                matrix[row] |= (1 << col);
                                changed                      = true;
                                matrix_hall_fast_mode[index] = percent;
#ifdef CONSOLE_ENABLE
                                uprintf("Press col: %u row: %u base: %u range: %u value: %u\n", col, row, matrix_hall_base[index], matrix_hall_range[index], raw_value);
#endif
                            } else if (percent < matrix_hall_fast_mode[index]) {
                                matrix_hall_fast_mode[index] = percent;
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
            }
        }
    }

    matrix_scan_kb();
    return changed;
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
    switch (*value_id) {
        case id_hall_sensors_calibrate:
            if (value_data[0] == 0) {
                for (uint8_t index = 0; index < sizeof(matrix_hall_range); index++) {
                    eeprom_update_byte((uint8_t *)EEPROM_HALL_RANGE_START + index, matrix_hall_range[index]);
                }
            }
#ifdef CONSOLE_ENABLE
            uprintf("Ranges saved!\n");
#endif
            break;
    }
    eeprom_update_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + *value_id - 1, value_data[0]);
}

void custom_get_value(uint8_t *data) {
    uint8_t *value_id   = &(data[0]);
    uint8_t *value_data = &(data[1]);
    value_data[0]       = eeprom_read_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + *value_id - 1);
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
                get_configurations();
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

    // DO NOT call raw_hid_send(data,length) here, let caller do this
}
