// Copyright 2025 Miguelio (teclados@miguelio.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mv_main.h"

static void init_pins(void) {
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        gpio_set_pin_output(col_pins[col]);
        gpio_write_pin_high(col_pins[col]);
    }
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        gpio_set_pin_output(row_pins[row]);
        gpio_write_pin_low(row_pins[row]);
    }
}

void c1_init_kb(void) {
    wait_ms(600);
    init_pins();
}

__attribute__((section(".ramfunc"))) void c1_main_kb(void) {
    if (atomic_load(&run_core_1)) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            gpio_write_pin_low(col_pins[col]);
            wait_us(HALL_WAIT_US_LOAD);
            for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
                uint16_t raw = analogReadPin(row_pins[row]);
                atomic_store(&matrix_hall_raw[row * MATRIX_COLS + col], raw);
                gpio_set_pin_output(row_pins[row]);
                gpio_write_pin_low(row_pins[row]);
            }
            gpio_write_pin_high(col_pins[col]);
            wait_us(HALL_WAIT_US_DISCHARGE);
        }
    } else {
        chBSemWait(&init_core1);
    }
}
