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

static matrix_row_t matrix[MATRIX_ROWS]   = {0};
static pin_t        row_pins[MATRIX_ROWS] = MATRIX_ROW_PINS;
static pin_t        col_pins[MATRIX_COLS] = MATRIX_COL_PINS;
static uint32_t     sensors_average    = 0;

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
    for (uint8_t round = 0; round < HALL_GET_AVERAGE_ROUNDS; round++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            gpio_write_pin_low(col_pins[col]);
            wait_us(HALL_WAIT_US);
            for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
                if (valid_sensor(col, row)) {
                    count_valid_sensors++;
                    sensors_average += analogReadPin(row_pins[row]);
                }
            }
            gpio_write_pin_high(col_pins[col]);
        }
    }
    sensors_average = sensors_average / count_valid_sensors;
    uprintf("Count sensors: %u\n", count_valid_sensors);
    uprintf("Average: %lu\n", sensors_average);
}

uint16_t hall_get_sensor_value(uint8_t col_read, uint8_t row_read) {
    uint16_t value = 0;
    uint8_t count_reads = 0;
    for (uint8_t round = 0; round < HALL_GET_AVERAGE_ROUNDS; round ++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            gpio_write_pin_low(col_pins[col]);
            wait_us(HALL_WAIT_US);
            for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
                if (col == col_read && row == row_read) {
                    count_reads ++;
                    value += analogReadPin(row_pins[row]);
                } else {
                    analogReadPin(row_pins[row]);
                }
            }
            gpio_write_pin_high(col_pins[col]);
        }
    }
    value = value / count_reads;

    return value;
}

void hall_check_individual_sensors(void)
{
    uint16_t average_percent = 0;
    uint8_t count_valid_sensors = 0;
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            if (valid_sensor(col, row)) {
                count_valid_sensors ++;
                // Get sensor value
                uint16_t hall_value_temp = hall_get_sensor_value(col, row);
                // Compare with average
                uint8_t compare_result = abs(sensors_average - hall_value_temp);
                compare_result = 100 - (compare_result * 100 / sensors_average);
                average_percent += compare_result;
                if (compare_result <= HALL_BROKEN_PERCENT) {
                    uprintf("BROKEN SENSOR: ");
                }
                uprintf("Col: %u, Row: %u, result: %u %%\n", col, row, compare_result);
            }
        }
    }
    average_percent = average_percent / count_valid_sensors;
    uprintf("Result average: %u %%\n", average_percent);
}



void hall_sensor_tests(void) {
    // Get sensor average value
    hall_get_average();

    // Check individual sensor
    hall_check_individual_sensors();
}

void keyboard_pre_init_kb(void) {
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        gpio_set_pin_output(col_pins[col]);
        gpio_write_pin_high(col_pins[col]);
        wait_ms(5);
    }
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        gpio_set_pin_input(row_pins[row]);
        analogReadPin(row_pins[row]);
        wait_ms(5);
    }
}

void matrix_init(void) {
    matrix_init_kb();
}

void keyboard_post_init_user(void) {
    wait_ms(3000);
    uprintf("Keyboard inited!\n");
    hall_sensor_tests();
}

uint8_t matrix_scan(void) {
    return false;
}
