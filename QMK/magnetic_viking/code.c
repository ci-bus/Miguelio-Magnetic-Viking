#include "mv_main.h"

// static uint16_t     matrix_hall_fast_release[MATRIX_ROWS * MATRIX_COLS] = {0};
// static bool         calibrating                                         = false;
// static bool         fast_release                                        = false;
static uint8_t current_layer = 0;


#ifdef JOYSTICK_ENABLE
#    include "mv_joystick.c"
#endif

#ifdef MIDI_ENABLE
#    include "mv_midi.c"
#endif

// Init multexes to secure update hall values
void init_mutexes(void) {
    for (int i = 0; i < MATRIX_COLS; i++) {
        chMtxObjectInit(&mutex_cols[i]);
    }
}



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


void keyboard_post_init_user(void) {
  rgblight_disable();
#ifdef CONSOLE_ENABLE
  uprintf("Keyboard inited!\n");
#endif
}

uint8_t matrix_scan_keyboard(void) {
  bool changed = false;
#ifdef MIDI_ENABLE
  uint16_t time = timer_read();
#endif
  for (uint8_t col = 0; col < MATRIX_COLS; col++) {
      for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
          /*
          uint8_t  index     = (row * MATRIX_COLS) + col;
          uint16_t temp_diff = column_reads[row] < matrix_hall_base[index] ? matrix_hall_base[index] - column_reads[row] : column_reads[row] - matrix_hall_base[index];
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

          if (matrix[row] & (1 << col)) {
              // Check released
              if (fast_release) {
                  if (percent < matrix_hall_fast_release[index] - HALL_FAST_RELEASE_MARGIN) {
                      matrix[row] &= ~(1 << col);
                      changed                         = true;
                      matrix_hall_fast_release[index] = percent;
#ifdef CONSOLE_ENABLE
                      uprintf("Release col: %u row: %u base: %u range: %u value: %u\n", col, row, matrix_hall_base[index], matrix_hall_range[index], column_reads[row]);
#endif
                  } else if (percent > matrix_hall_fast_release[index]) {
                      matrix_hall_fast_release[index] = percent;
                  }
              } else if (percent < hall_thresholds[index] - HALL_PRESS_RELEASE_MARGIN) {
                  matrix[row] &= ~(1 << col);
                  changed = true;
#ifdef CONSOLE_ENABLE
                  uprintf("Release col: %u row: %u base: %u range: %u value: %u\n", col, row, matrix_hall_base[index], matrix_hall_range[index], column_reads[row]);
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
                      uprintf("Press col: %u row: %u base: %u range: %u value: %u threshold: %u\n", col, row, matrix_hall_base[index], matrix_hall_range[index], column_reads[row], hall_threshold);
#endif
                  } else if (percent < matrix_hall_fast_release[index]) {
                      matrix_hall_fast_release[index] = percent;
                  }
              } else if (percent > hall_thresholds[index]) {
                  matrix[row] |= (1 << col);
                  changed = true;
#ifdef CONSOLE_ENABLE
                  uprintf("Press col: %u row: %u base: %u range: %u value: %u threshold: %u\n", col, row, matrix_hall_base[index], matrix_hall_range[index], column_reads[row], hall_threshold);
#endif
              }
          }
          // Doing calibrate
          if (calibrating) {
              uint16_t temp_diff = column_reads[row] < matrix_hall_base[index] ? matrix_hall_base[index] - column_reads[row] : column_reads[row] - matrix_hall_base[index];
              // Update range
              if (temp_diff > matrix_hall_range[index]) {
                  matrix_hall_range[index] = temp_diff;
#ifdef CONSOLE_ENABLE
                  uprintf("Col: %u row: %u range: %u\n", col, row, matrix_hall_range[index]);
#endif
              }
          }
              */
      }
  }
  /*
      chMtxLock(&uprintf_mutex);
      uprintf("hay mensaje");
      chMtxUnlock(&uprintf_mutex);
  */
  if (core1_flag == 0) {
      core1_flag = 1;
      wait_ms(1000);
      uprintf("Primer mensaje");
  }
  wait_ms(1);
  return changed;
}

//[{ VIA }]//////////////////////////////////////////////////////////////////

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
              /*
              // Save ranges
              for (uint8_t index = 0; index < MATRIX_ROWS * MATRIX_COLS; index++) {
                  void *address = ((void *)EEPROM_HALL_RANGE_START) + (index * 2);
                  // Secure min range
                  if (matrix_hall_range[index] <= HALL_MIN_RANGE) {
                      matrix_hall_range[index] = HALL_MIN_RANGE;
                  }
                  eeprom_update_byte(address, (uint8_t)(matrix_hall_range[index] >> 8));
                  eeprom_update_byte(address + 1, (uint8_t)(matrix_hall_range[index] & 0xFF));
              }
#ifdef CONSOLE_ENABLE
              uprintf("Ranges saved!\n");
#endif
              */
          }
          // get_configuration_calibrating();
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
          // get_configuration_fast_release();
          break;
      case id_hall_threshold:
          // get_configuration_hall_thresholds();
          break;
      case id_hall_curve:
          // get_configuration_curves();
          // get_configuration_hall_thresholds();
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

void on_via_change_threshold_by_key(uint8_t row, uint8_t col, uint16_t keycode) {
  /*
  uint8_t index          = (row * MATRIX_COLS) + col;
  hall_thresholds[index] = hall_keycode_to_threshold(keycode, hall_threshold);
#ifdef CONSOLE_ENABLE
  uprintf("Changed key threshold, col: %u, row: %u, threshold: %u\n", col, row, hall_thresholds[index]);
#endif
  // Secure min threshold with curve
  check_minimun_threshold(index);
  */
}

void on_via_change_curve_by_key(uint8_t row, uint8_t col, uint16_t keycode) {
  /*
  uint8_t index      = (row * MATRIX_COLS) + col;
  hall_curves[index] = hall_keycode_to_curve(keycode, hall_curve);
#ifdef CONSOLE_ENABLE
  uprintf("Changed key curve, col: %u, row: %u, curve: %u\n", col, row, hall_curves[index]);
#endif
  // Secure min threshold with curve
  check_minimun_threshold(index);
  */
}

bool via_command_kb(uint8_t *data, uint8_t length) {
  uint8_t *command_id   = &(data[0]);
  uint8_t *command_data = &(data[1]);
  if (*command_id == id_dynamic_keymap_set_keycode) {
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
  }
  return false;
}





static bool calibrating  = false;
static bool fast_release = false;



void matrix_col_reads(uint8_t col) {
    gpio_write_pin_low(col_pins[col]);
    chThdSleepMicroseconds(HALL_WAIT_US_LOAD);
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        col_reads[row] = analogReadPin(row_pins[row]);
    }
    gpio_write_pin_high(col_pins[col]);
    hall_discharge_capacitors();
}

extern bool valid_sensor(uint8_t col, uint8_t row);



void matrix_hall_reset_range(void) {
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            uint8_t index = (row * MATRIX_COLS) + col;
            if (valid_sensor(col, row)) {
                matrix_hall_range[index] = HALL_MIN_RANGE;
            } else {
                matrix_hall_range[index] = HALL_MAX_RANGE;
            }
        }
    }
}

void get_configuration_calibrating(void) {
    calibrating = (eeprom_read_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + id_hall_sensors_calibrate) == 1);
    if (calibrating) {
        // Go to base layer
        layer_clear();
        // Reset base values
        matrix_hall_get_base();
        // Reset range eeprom
        matrix_hall_reset_range();
#ifdef CONSOLE_ENABLE
        uprintf("Range reseted!\n");
        uprintf("Calibrating...\n");
#endif
    }
}









void get_configuration_fast_release(void) {
    fast_release = (eeprom_read_byte((uint8_t *)EEPROM_CUSTOM_CONFIG + id_hall_fast_release) == 1);
}

void get_configurations(void) {

    // Calibrating
    get_configuration_calibrating();

    // Fast trigger
    get_configuration_fast_release();
}