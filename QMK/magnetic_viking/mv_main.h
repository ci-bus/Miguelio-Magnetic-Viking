// Copyright 2025 Miguelio (teclados@miguelio.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "via.h"

#define HALL_CALIBRATE_ROUNDS 10
#define HALL_DEFAULT_RANGE 10
#define HALL_WAIT_US 1000
#define HALL_DEFAULT_THRESHOLD 50
#define HALL_DEFAULT_THRESHOLD_MIN 10
#define HALL_DEFAULT_THRESHOLD_MAX 90
#define HALL_DEFAULT_PRESS_RELEASE_MARGIN 5

#define EEPROM_CUSTOM_CONFIG (VIA_EEPROM_CUSTOM_CONFIG_ADDR)
#define EEPROM_HALL_RANGE_START (EEPROM_CUSTOM_CONFIG + 5)

enum via_custom_value_id {
  id_hall_sensors_calibrate = 1,
  id_layout_reset_keymap,
  id_hall_threshold,
  id_hall_fast_trigger,
  id_hall_curve
};
