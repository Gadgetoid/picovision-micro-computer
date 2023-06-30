#include "pico/stdlib.h"
#include <cstdio>
#include "usb.hpp"
#include "scancodes.h"

#include "drivers/dv_display/dv_display.hpp"
#include "libraries/pico_graphics/pico_graphics.hpp"

#include "lua.hpp"

#define FRAME_WIDTH 640
#define FRAME_HEIGHT 480

using namespace pimoroni;

lua_State *L;

DVDisplay display;
PicoGraphics_PenDV_RGB555 graphics(FRAME_WIDTH, FRAME_HEIGHT, display);

Point cursor(FRAME_WIDTH / 2, FRAME_HEIGHT / 2);
int16_t cursor_size = 10;

Point key_cursor(FRAME_WIDTH / 2, FRAME_HEIGHT / 2);

uint8_t keys_pressed[6];
uint8_t keys_state[6];
uint8_t key_modifier = 0;

uint8_t KEY_STATE_NONE    = 0b00000000;
uint8_t KEY_STATE_PRESS   = 0b00000001;
uint8_t KEY_STATE_RELEASE = 0b00000010;
uint8_t KEY_STATE_REPEAT  = 0b00000100;

char input_buffer[256];
char *input_ptr = &input_buffer[0];


void mouse_callback(int8_t x, int8_t y, uint8_t buttons, int8_t wheel) {
    cursor += Point(x, y);
    cursor = cursor.clamp(graphics.bounds);

    cursor_size -= wheel;
    cursor_size = std::max(cursor_size, (int16_t)10);
    cursor_size = std::min(cursor_size, (int16_t)100);
}

bool key_state_any(key k, uint8_t status_mask) {
    for(auto i = 0u; i < 6; i++) {
        if (keys_pressed[i] == k && (keys_state[i] & status_mask)) {
            return true;
        }
    }
    return false;
}

bool key_pressed_or_held(key k) {
    return key_state_any(k, KEY_STATE_PRESS | KEY_STATE_REPEAT);
}

bool key_pressed(key k) {
    return key_state_any(k, KEY_STATE_PRESS);
}

void clear_pressed_states() {
    for(auto i = 0u; i < 6; i++) {
        if(keys_state[i] == KEY_STATE_PRESS) {
            keys_state[i] = KEY_STATE_REPEAT;
        }
    }
}

void update_arrow_key_cursor() {
    if (key_pressed_or_held(KEY_UP)) {
        key_cursor += Point(0, -1);
    }
    if (key_pressed_or_held(KEY_DOWN)) {
        key_cursor += Point(0, 1);
    }
    if (key_pressed_or_held(KEY_LEFT)) {
        key_cursor += Point(-1, 0);
    }
    if (key_pressed_or_held(KEY_RIGHT)) {
        key_cursor += Point(1, 0);
    }
    key_cursor = key_cursor.clamp(graphics.bounds);
}

bool find_key_in(uint8_t key, uint8_t *keys) {
    for(auto i = 0u; i < 6u; i++) {
        if(keys[i] == key) return true;
    }
    return false;
}

void keyboard_callback(uint8_t *keys, uint8_t modifiers) {
    // Clear released keys
    for(auto i = 0u; i < 6; i++) {
        if(keys_state[i] == KEY_STATE_RELEASE) {
            keys_state[i] = KEY_STATE_NONE;
            keys_pressed[i] = 0;
        }
    }

    /*
    For every key in `keys_pressed` that isn't in `keys`,
    mark it as released.
    */
    for(auto i = 0u; i < 6; i++) {
        if (!find_key_in(keys_pressed[i], keys)) {
            keys_state[i] = KEY_STATE_RELEASE;
        }
    }


   for(auto i = 0u; i < 6; i++) {
            /*
            For every key in `keys` that IS in `keys_pressed`,
            update it to held.
            */
           if(find_key_in(keys[i], keys_pressed)) {
            for(auto j = 0u; j < 6; j++) {
                if(keys_pressed[j] == keys[i]) {
                    keys_state[j] = KEY_STATE_REPEAT;
                }
            }
           }else{
            /*
            For every key in `keys` that isn't in `keys_pressed`,
            find it a slot.
            */
            for(auto j = 0u; j < 6; j++) {
                if(keys_pressed[j] == 0) {
                    keys_pressed[j] = keys[i];
                    keys_state[j] = KEY_STATE_PRESS;
                    break;
                }
            }
           }
   }

   for(auto i = 0u; i < 6; i++) {
        auto key = keys_pressed[i];
        auto state = keys_state[i];

        if(key && state == KEY_STATE_PRESS) {
            if(key >= KEY_A && key < KEY_A + 26 + 10) {
                if(modifiers & (MOD_RSHF | MOD_LSHF)) {
                    *input_ptr = ukeys[key - KEY_A];
                } else {
                    *input_ptr = lkeys[key - KEY_A];
                }
                input_ptr++;
            } else if (key == KEY_ENTER) {
                luaL_dostring(L, input_buffer);
                input_ptr = &input_buffer[0];
                memset(input_buffer, 0, sizeof(input_buffer));
            } else if (key == KEY_SPACE) {
                *input_ptr = ' ';
                input_ptr++;
            } else if (key == KEY_BACKSPACE) {
                if(input_ptr > &input_buffer[0]) {
                    input_ptr--;
                    *input_ptr = '\0';
                }
            }
        }
   }
}

struct line {
    Point a;
    Point b;
    line(Point a, Point b) : a(a), b(b) {};
};

std::vector<line> lines;

int main() {
    stdio_init_all();
    printf("Hello World\n");

    printf("Init USB...\n");
    init_usb();
    printf("Done!\n");

    printf("Init Lua...\n");
    L = luaL_newstate();
    luaL_openlibs(L);
    luaL_dostring(L, "x = \"Lua Hello World\"");
    luaL_dostring(L, "print(x)");
    printf("Done!\n");

    printf("Init Video...\n");
    display.init(FRAME_WIDTH, FRAME_HEIGHT, DVDisplay::MODE_RGB555);
    graphics.set_pen(graphics.create_pen(0, 0, 0));
    graphics.clear();
    display.flip();
    graphics.clear();
    printf("Done!\n");

    graphics.set_font("bitmap8");

    Point last_cursor = cursor;

    while(true) {
        update_usb();
        update_arrow_key_cursor();

        /*if(key_pressed(KEY_SPACE)) {
            luaL_dostring(L, "x = \"Lua Hello World\"");
            luaL_dostring(L, "print(x)");
        }*/

        graphics.set_pen(graphics.create_pen(0, 0, 0));
        graphics.clear();
        graphics.set_pen(graphics.create_pen(255, 255, 255));
        graphics.text("PicoVision Micro Computer v0.1 Alpha", Point(0, 0), FRAME_WIDTH);
        graphics.text(input_buffer, Point(0, 20), FRAME_WIDTH);

        graphics.set_pen(graphics.create_pen(255, 0, 0));
        graphics.circle(cursor, cursor_size);

        graphics.set_pen(graphics.create_pen(0, 0, 255));
        graphics.rectangle(Rect(key_cursor.x - 5, key_cursor.y - 5, 10, 10));

        graphics.set_pen(graphics.create_pen(0, 255, 0));

        for (auto &line : lines) {
            graphics.line(line.a, line.b);
        }

        display.flip();

        //sleep_ms(1000 / 30);

        clear_pressed_states();
    }
}