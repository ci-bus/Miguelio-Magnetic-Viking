// Copyright 2025 Miguelio (teclados@miguelio.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Hall sensor configs
#define HALL_GET_AVERAGE_ROUNDS 200 // Rounds to get average value
#define HALL_SKIP_ROUNDS 100        // Skip firsts rounds
#define HALL_WAIT_US 30             // Wait to change column active
#define HALL_BROKEN_PERCENT 70      // Limit percent to broken sensors
#define HALL_ALERT_DIFF 5           // Limit to show alert by diff between reands
