#pragma once
#include <cstdint>

typedef const uint8_t key;

key MOD_LCTL = 0b00000001;
key MOD_LSHF = 0b00000010;
key MOD_LALT = 0b00000100;
key MOD_RASP = 0b00001000;
key MOD_RCTL = 0b00010000;
key MOD_RSHF = 0b00100000;
key MOD_RALT = 0b01000000;

const char lkeys[] = "abcdefghijklmnopqrstuvwxyz1234567890";
const char ukeys[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ!\"£$%^&*()";

key KEY_A = 4; // Not ASCII then...

key KEY_ENTER = 40;
key KEY_ESCAPE = 41;
key KEY_BACKSPACE = 42;
key KEY_TAB = 43;
key KEY_SPACE = 44;

key KEY_UP = 82;
key KEY_DOWN = 81;
key KEY_LEFT = 80;
key KEY_RIGHT = 79;

key KEY_F1 = 58;
key KEY_F2 = 59;
key KEY_F3 = 60;
key KEY_F4 = 61;
key KEY_F5 = 62;
key KEY_F6 = 63;
key KEY_F7 = 64;
key KEY_F8 = 65;
key KEY_F9 = 66;