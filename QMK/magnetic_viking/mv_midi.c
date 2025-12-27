extern MidiDevice midi_device;

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
#ifdef CONSOLE_ENABLE
                uprintf("Midi key index: %u col: %u row: %u\n", index, col, row);
#endif
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
#ifdef CONSOLE_ENABLE
            uprintf("Midi note: %u octave %u velocity: %u\n", midi_note, midi_config.octave, velocity);
#endif
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
#ifdef CONSOLE_ENABLE
            uprintf("Midi note: %u octave: %u velocity: %u\n", midi_note, midi_config.octave, velocity);
#endif
        }
    }
    midi_last_percent[index] = percent;
}