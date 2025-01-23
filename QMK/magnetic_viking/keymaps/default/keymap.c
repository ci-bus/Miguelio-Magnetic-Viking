#include QMK_KEYBOARD_H

enum layer_names {
    _BASE,
    _FN,
    _GAMING,
    _MIDI
};

#define XXXXXXX KC_NO

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT_60_tsangan(
        KC_ESC,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,  KC_GRV,  KC_BSPC,
        KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_BSLS,
        KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT, KC_ENT,
        KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH, RSFT_T(KC_UP), MO(_FN),
        KC_LCTL, KC_LGUI, KC_LALT,                            KC_SPC,                    RALT_T(KC_LEFT), RGUI_T(KC_DOWN), RCTL_T(KC_RIGHT)
    ),
    [_FN] = LAYOUT_60_tsangan(
        KC_TILD, KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,  _______, KC_DEL,
        _______, _______, _______, _______, _______, _______, _______, _______, TO(_MIDI), _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, TO(_GAMING), _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, KC_MPRV, KC_MNXT, KC_MUTE, KC_VOLD, KC_VOLU, _______, _______, _______,
        _______, _______, _______,                            _______,                            _______, _______, _______
    ),
    [_GAMING] = LAYOUT_60_tsangan(
        TO(_BASE), XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, KC_F12,
        KC_F10,    KC_F5,   KC_W,    KC_F6,   XXXXXXX, XXXXXXX, XXXXXXX, KC_F7,   KC_I,    KC_F8,   XXXXXXX, XXXXXXX, XXXXXXX, KC_F11,
        XXXXXXX,   KC_A,    KC_S,    KC_D,    XXXXXXX, XXXXXXX, XXXXXXX, KC_J,    KC_K,    KC_L,    XXXXXXX, XXXXXXX, KC_F9,
        XXXXXXX,   XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, KC_UP,   XXXXXXX,
        KC_F1,     KC_F2,   KC_F3,                              KC_F4,                              KC_LEFT, KC_DOWN, KC_RIGHT
    ),
    [_MIDI] = LAYOUT_60_tsangan(
        TO(_BASE), MI_Cs2,  MI_Ds2, XXXXXXX, MI_Fs2, MI_Gs2, MI_As2, XXXXXXX, MI_Cs3, MI_Ds3, XXXXXXX, MI_Fs3, MI_Gs3, MI_As3, MI_Bb3,
        MI_C2,          MI_D2,   MI_E2,   MI_F2,  MI_G2,  MI_A2,  MI_B2,   MI_C3,  MI_D3,  MI_E3,   MI_F3,  MI_G3,  MI_A3,  MI_B3,
        XXXXXXX,   XXXXXXX, MI_Cs4, MI_Ds4, XXXXXXX, MI_Fs4, MI_Gs4, MI_As4, XXXXXXX, XXXXXXX, MI_BNDD, MI_BNDU, XXXXXXX,
        XXXXXXX,         MI_C4,  MI_D4,  MI_E4,   MI_F4,  MI_G4,  MI_A4,  MI_B4, MI_OCTD, MI_OCTU, MI_AOFF, XXXXXXX, MO(_FN),
        XXXXXXX,   XXXXXXX, XXXXXXX,                            XXXXXXX,                            XXXXXXX, XXXXXXX, XXXXXXX
    )
};
