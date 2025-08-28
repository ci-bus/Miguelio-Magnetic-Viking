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

static matrix_row_t matrix[MATRIX_ROWS]                          = {0};
static pin_t        row_pins[MATRIX_ROWS]                        = MATRIX_ROW_PINS;
static pin_t        col_pins[MATRIX_COLS]                        = MATRIX_COL_PINS;
static uint32_t     all_sensors_average                          = 0;
static uint32_t     hall_sum_values[MATRIX_COLS * MATRIX_ROWS]   = {0};
static uint16_t     hall_count_values[MATRIX_COLS * MATRIX_ROWS] = {0};
static uint16_t     hall_last_values[MATRIX_COLS * MATRIX_ROWS]  = {0};
static uint16_t     hall_diffs[MATRIX_COLS * MATRIX_ROWS]        = {0};
static uint16_t     hall_max_diff                                = 0;
static bool         error_reads                                  = false;
static uint16_t     hall_min_value                               = 1024;
static uint16_t     hall_max_value                               = 0;

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

bool valid_sensor(uint8_t col, uint8_t row) {
    if (col == 17 && row == 0) {
        return false;
    }
    if (col == 17 && row == 1) {
        return false;
    }
    return true;
}

void hall_get_average(void) {
    uint16_t count_valid_sensors = 0;
    uint16_t raw_value           = 0;
    for (uint8_t round = 0; round < HALL_GET_AVERAGE_ROUNDS; round++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            gpio_write_pin_low(col_pins[col]);
            wait_us(HALL_WAIT_US);

            for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
                if (round > HALL_SKIP_ROUNDS) {
                    if (valid_sensor(col, row)) {
                        count_valid_sensors++;
                        raw_value = analogReadPin(row_pins[row]);
                        all_sensors_average += raw_value;
                    }
                } else {
                    raw_value = analogReadPin(row_pins[row]);
                }
            }

            gpio_write_pin_high(col_pins[col]);
        }
    }
    all_sensors_average = all_sensors_average / count_valid_sensors;
    uprintf("Average: %lu\n", all_sensors_average);
}

uint16_t hall_get_sensor_value(uint8_t col, uint8_t row) {
    uint8_t index = (row * MATRIX_COLS) + col;
    // Read value
    uint16_t raw_value = analogReadPin(row_pins[row]);
    // Set max and min
    if (raw_value > hall_max_value) {
        hall_max_value = raw_value;
    } else if (raw_value < hall_min_value) {
        hall_min_value = raw_value;
    }
    // If different value
    if (hall_last_values[index] != 0 && hall_last_values[index] != raw_value) {
        uint16_t temp_diff = abs(hall_last_values[index] - raw_value);
        // If higher diff
        if (temp_diff > hall_diffs[index]) {
            hall_diffs[index] = temp_diff;
            if (temp_diff > hall_max_diff) {
                hall_max_diff = temp_diff;
            }
        }
    }
    // Store last value to compare
    hall_last_values[index] = raw_value;

    return raw_value;
}

uint16_t hall_check_individual_sensors(void) {
    error_reads                  = false;
    uint16_t average_percent     = 0;
    uint16_t count_valid_sensors = 0;
    for (uint8_t round = 0; round < HALL_GET_AVERAGE_ROUNDS; round++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            gpio_write_pin_low(col_pins[col]);
            wait_us(HALL_WAIT_US);
            for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
                if (round > HALL_SKIP_ROUNDS && valid_sensor(col, row)) {
                    // Array index
                    uint8_t index = (row * MATRIX_COLS) + col;
                    // Get sensor value
                    uint16_t temp_hall_value = hall_get_sensor_value(col, row);
                    // Sum values
                    hall_sum_values[index] += temp_hall_value;
                    // Increase count
                    hall_count_values[index]++;
                    // Last loop
                    if (round + 1 == HALL_GET_AVERAGE_ROUNDS) {
                        // Calcule sensor average
                        uint16_t temp_sensor_average = hall_sum_values[index] / hall_count_values[index];
                        uint16_t compare_result      = abs(all_sensors_average - temp_sensor_average);
                        compare_result               = 100 - (compare_result * 100 / all_sensors_average);
                        // Sum global average percent
                        average_percent += compare_result;
                        count_valid_sensors++;
                        // Show log
                        if (col < 10) {
                            uprintf("Col:  %u, Row: %u ", col, row);
                        } else {
                            uprintf("Col: %u, Row: %u ", col, row);
                        }
                        if (compare_result < HALL_BROKEN_PERCENT) {
                            uprintf("[ DAMAGED SENSOR ]");
                            error_reads = true;
                        } else if (hall_diffs[index] > HALL_ALERT_DIFF) {
                            uprintf("[ ALERT DIFF EXCEEDED ]: %u", hall_diffs[index]);
                            error_reads = true;
                        } else {
                            uprintf("[ OK ]");
                        }
                        // uprintf(": %u %%\n", compare_result);
                        uprintf("\n");
                    }
                } else {
                    analogReadPin(row_pins[row]);
                }
            }
            gpio_write_pin_high(col_pins[col]);
        }
    }
    return average_percent / count_valid_sensors;
}

void hall_sensor_tests(void) {
    // Get sensor average value
    hall_get_average();

    // Check individual sensor
    uint16_t average_percent = hall_check_individual_sensors();

    // Final logs
    uprintf("Result health: %u %%\n", average_percent);
    uprintf("Result max diff: %u\n", hall_max_diff);
    uprintf("Result better wait_us: %u\n", HALL_WAIT_US);

    // Max and min
    uprintf("Result max value: %u\n", hall_max_value);
    uprintf("Result min value: %u\n", hall_min_value);

    // Check RGB
    rgblight_enable_noeeprom();
}

void keyboard_pre_init_kb(void) {
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        gpio_set_pin_output(col_pins[col]);
        gpio_write_pin_high(col_pins[col]);
    }
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        gpio_set_pin_input(row_pins[row]);
        analogReadPin(row_pins[row]);
    }
}

void matrix_init(void) {
    matrix_init_kb();
}

void keyboard_post_init_user(void) {
    wait_ms(1000);
    uprintf("Running tests...\n");
    wait_ms(1000);
    hall_sensor_tests();
}

uint8_t matrix_scan(void) {
    return false;
}
