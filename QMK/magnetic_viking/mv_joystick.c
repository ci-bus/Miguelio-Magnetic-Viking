#include "joystick.h"

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
#ifdef CONSOLE_ENABLE
                uprintf("Joystick key index: %u col: %u row: %u\n", index, col, row);
#endif
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
        matrix_column_reads(col);
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            uint8_t index = (row * MATRIX_COLS) + col;
            if (jt_keys[jt_index].key.row != row) {
                continue;
            }
            uint16_t temp_diff  = column_reads[row] < matrix_hall_base[index] ? matrix_hall_base[index] - column_reads[row] : column_reads[row] - matrix_hall_base[index];
            uint8_t  axis_value = 0;
            if (temp_diff > HALL_PRESS_RELEASE_MARGIN) {
                axis_value = temp_diff * 127 / matrix_hall_range[index];
                if (axis_value > 127) {
                    axis_value = 127;
                }
            }
            // Axes values
            if (jt_keys[jt_index].index < JOYSTICK_AXIS_COUNT * 2) {
                // Even sum and odd subtract
                jt_axes_values[jt_keys[jt_index].index / 2] += axis_value * (jt_keys[jt_index].index & 1 ? 1 : -1);
#ifdef CONSOLE_ENABLE
                // Secure reads
                if (axis_value > jt_keys[jt_index].value + JT_MARGIN || axis_value < jt_keys[jt_index].value - JT_MARGIN || (axis_value > 127 - JT_MARGIN && axis_value > jt_keys[jt_index].value) || (axis_value < JT_MARGIN && axis_value < jt_keys[jt_index].value)) {
                    uprintf("Joystick axis index: %u value: %d\n", jt_keys[jt_index].index, axis_value);
                    jt_keys[jt_index].value = axis_value;
                }
#endif
            } else { // Buttons
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
#ifdef CONSOLE_ENABLE
                            uprintf("Release joystick button index: %u value: %u\n", jt_button_index, column_reads[row]);
#endif
                        } else if (percent > matrix_hall_fast_release[index]) {
                            matrix_hall_fast_release[index] = percent;
                        }
                    } else if (percent < hall_thresholds[index] - HALL_PRESS_RELEASE_MARGIN) {
                        joystick_state.buttons[jt_button_index / 8] &= ~(1 << (jt_button_index % 8));
                        joystick_state.dirty = true;
#ifdef CONSOLE_ENABLE
                        uprintf("Release joystick button index: %u value: %u\n", jt_button_index, column_reads[row]);
#endif
                    }
                } else {
                    // Check press
                    if (fast_release) {
                        if (percent > hall_thresholds[index] && percent > matrix_hall_fast_release[index] + HALL_PRESS_RELEASE_MARGIN) {
                            joystick_state.buttons[jt_button_index / 8] |= 1 << (jt_button_index % 8);
                            joystick_state.dirty            = true;
                            matrix_hall_fast_release[index] = percent;
#ifdef CONSOLE_ENABLE
                            uprintf("Press joystick button index: %u value: %u\n", jt_button_index, column_reads[row]);
#endif
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
#ifdef CONSOLE_ENABLE
                            uprintf("Press joystick button index: %u value: %u\n", jt_button_index, column_reads[row]);
#endif
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