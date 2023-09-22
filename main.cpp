#include "pico/stdlib.h"
#include <cstdio>
#include <iterator>
#include <sstream>
#include "usb.hpp"
#include "scancodes.h"

#include "drivers/dv_display/dv_display.hpp"
#include "libraries/pico_graphics/pico_graphics_dv.hpp"

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

#define LUA_MAXINPUT		256
#define EOFMARK		"<eof>"
#define marklen		(sizeof(EOFMARK)/sizeof(char) - 1)

char input_buffer[LUA_MAXINPUT];
char *input_ptr = &input_buffer[0];
uint8_t lua_prompt = 0;

std::vector<std::pair<uint8_t, std::string>> history;
std::vector<std::string> multiline;

void write_history(uint8_t cursor, const char *msg, size_t length) {
    history.emplace_back(cursor, msg);
}

void mouse_callback(int8_t x, int8_t y, uint8_t buttons, int8_t wheel) {
    cursor += Point(x, y);
    cursor = cursor.clamp(graphics.bounds);

    cursor_size -= wheel;
    cursor_size = std::max(cursor_size, (int16_t)10);
    cursor_size = std::min(cursor_size, (int16_t)100);
}

const char* get_prompt(uint8_t p) {
    switch(p){
        case 0:
            return "> ";
        case 1:
            return ">>";
        case 2:
            return "  ";
        default:
            return "--";
    }
}

static void l_message (const char *pname, const char *msg) {
  if (pname) lua_writestringerror("%s: ", pname);
  lua_writestringerror("%s\n", msg);
}

static void l_print (lua_State *L) {
  int n = lua_gettop(L);
  if (n > 0) {  /* any result to be printed? */
    luaL_checkstack(L, LUA_MINSTACK, "too many results to print");
    lua_getglobal(L, "print");
    lua_insert(L, 1);
    if (lua_pcall(L, n, 0, 0) != LUA_OK)
      l_message("lua", lua_pushfstring(L, "error calling 'print' (%s)",
                                             lua_tostring(L, -1)));
  }
}

static int msghandler (lua_State *L) {
  const char *msg = lua_tostring(L, 1);
  if (msg == NULL) {  /* is error object not a string? */
    if (luaL_callmeta(L, 1, "__tostring") &&  /* does it have a metamethod */
        lua_type(L, -1) == LUA_TSTRING)  /* that produces a string? */
      return 1;  /* that is the message */
    else
      msg = lua_pushfstring(L, "(error object is a %s value)",
                               luaL_typename(L, 1));
  }
  luaL_traceback(L, L, msg, 1);  /* append a standard traceback */
  return 1;  /* return the traceback */
}

static int docall (lua_State *L, int narg, int nres) {
  int status;
  int base = lua_gettop(L) - narg;  /* function index */
  lua_pushcfunction(L, msghandler);  /* push message handler */
  lua_insert(L, base);  /* put it under function and args */
  status = lua_pcall(L, narg, nres, base);
  lua_remove(L, base);  /* remove message handler from the stack */
  return status;
}

void lua_handle_user_input(char *line) {
    int status = -1;
    static bool gather_incomplete = false;

    if (line[0] == '\0') return;

    multiline.emplace_back(line);
    history.emplace_back(lua_prompt, line);

    std::ostringstream join_multiline;
    std::copy(multiline.begin(), multiline.end(),
              std::ostream_iterator<std::string>(join_multiline, "\n"));
              
    status = luaL_loadbuffer(L, join_multiline.str().c_str(), join_multiline.str().length(), "=stdin");
    if (status == LUA_ERRSYNTAX) {
        size_t lmsg;
        const char *msg = lua_tolstring(L, -1, &lmsg);
        // The error has an EOF mark, indicating an incomplete statement
        if (lmsg >= marklen && strcmp(msg + lmsg - marklen, EOFMARK) == 0) {
            lua_prompt = 1;
            lua_pop(L, 1);

        // It's a real syntax error, but a single statement
        // let's try to just execute it prefixed with `return`
        } else if (multiline.size() == 1) {
            lua_pop(L, 1);
            const char *line_with_return = lua_pushfstring(L, "return %s", multiline[0].c_str());
            status = luaL_loadbuffer(L, line_with_return, strlen(line_with_return), "=stdin");
            if(status == LUA_OK) {
                // Remove the loaded string, or it ends up on the stack for print()
                lua_remove(L, -2);
                status = docall(L, 0, LUA_MULTRET);
                if (status == LUA_OK) l_print(L);
                multiline.clear();
                lua_prompt = 0;
            } else {
                lua_pop(L, 2);
            }
        // It's a real syntax error
        } else {
            printf("SYNTAX ERROR: Top %i\n", lua_gettop(L));
            // Bail with the syntax error
            status = docall(L, 0, LUA_MULTRET);
            if (status == LUA_OK) l_print(L);
            multiline.clear();
            lua_prompt = 0;
        }
    } else if (status == LUA_OK) {
        status = docall(L, 0, LUA_MULTRET);
        if (status == LUA_OK) l_print(L);
        multiline.clear();
        lua_prompt = 0;
    }

/*
    // Try to evaluate "return <line>"
    if (!gather_incomplete) {
        const char *line_with_return = lua_pushfstring(L, "return %s", line);
        status = luaL_loadbuffer(L, line_with_return, strlen(line_with_return), "=stdin");
    }
    if (status == LUA_OK) { // evaluation of "return <line>" was successful
        lua_assert(lua_gettop(L) == 1);
        printf("EXECUTING SINGLE STATEMENT: Top %i\n", lua_gettop(L));
        history.emplace_back(line);
        status = docall(L, 0, LUA_MULTRET);
        if (status == LUA_OK) l_print(L);
    } else { // "return <line>" failed
        // pop luaL_loadbuffer result and modified line
        if(!gather_incomplete) lua_pop(L, 2); 

        multiline.emplace_back(line);
        history.emplace_back(line);

        std::ostringstream join_multiline;
        std::copy(multiline.begin(), multiline.end(),
                std::ostream_iterator<std::string>(join_multiline, "\n"));

        // Try to evaluate the line as-is
        status = luaL_loadbuffer(L, join_multiline.str().c_str(), join_multiline.str().length(), "=stdin");
        if (status == LUA_ERRSYNTAX) { // Syntax error, incomplete statement?
            size_t lmsg;
            const char *msg = lua_tolstring(L, -1, &lmsg);
            if (lmsg >= marklen && strcmp(msg + lmsg - marklen, EOFMARK) == 0) {
                lua_pop(L, 1);
                printf("STARTING GATHER: Top %i\n", lua_gettop(L));
                gather_incomplete = true;
            } else {
                lua_pop(L, 1);
                printf("SYNTAX ERROR: Top %i\n", lua_gettop(L));
                // Bail with the syntax error
                gather_incomplete = false;
                multiline.clear();
                status = docall(L, 0, LUA_MULTRET);
                if (status == LUA_OK) l_print(L);
            }
        } else if (status == LUA_OK) {
                lua_pop(L, 1);
            // Execute the completed statement
            gather_incomplete = false;
            multiline.clear();
            lua_pop(L, 1);
            lua_assert(lua_gettop(L) == 1);
            printf("FINISHED GATHER: Top %i\n", lua_gettop(L));
            status = docall(L, 0, LUA_MULTRET);
        }
    }
    */

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
        } else {
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
                lua_handle_user_input(input_buffer);
                // Clear the line
                memset(input_buffer, 0, LUA_MAXINPUT);
                input_ptr = &input_buffer[0];
                
            } else if (key == KEY_SPACE) {
                *input_ptr = ' ';
                input_ptr++;
            } else if (key == KEY_BACKSPACE) {
                if(input_ptr > &input_buffer[0]) {
                    input_ptr--;
                    *input_ptr = '\0';
                }
            } else if (key == 45) { // -_
                *input_ptr = modifiers & (MOD_RSHF | MOD_LSHF) ? '_' : '-';
                input_ptr++;
            } else if (key == 46) { // -_
                *input_ptr = modifiers & (MOD_RSHF | MOD_LSHF) ? '+' : '=';
                input_ptr++;
            } else if (key == 47) { // -_
                *input_ptr = modifiers & (MOD_RSHF | MOD_LSHF) ? '{' : '[';
                input_ptr++;
            } else if (key == 48) { // -_
                *input_ptr = modifiers & (MOD_RSHF | MOD_LSHF) ? '}' : ']';
                input_ptr++;
            } else if (key == 54) { // -_
                *input_ptr = modifiers & (MOD_RSHF | MOD_LSHF) ? '<' : ',';
                input_ptr++;
            } else if (key == 55) { // -_
                *input_ptr = modifiers & (MOD_RSHF | MOD_LSHF) ? '>' : '.';
                input_ptr++;
            } else if (key == 56) { // -_
                *input_ptr = modifiers & (MOD_RSHF | MOD_LSHF) ? '?' : '/';
                input_ptr++;
            }
         }
   }
}
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
    display.preinit();
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

        while(history.size() > 22) {
            history.erase(history.begin());
        }

        graphics.set_pen(graphics.create_pen(0, 0, 0));
        graphics.clear();
        graphics.set_pen(graphics.create_pen(255, 255, 255));
        graphics.text("PicoVision Micro Computer v0.1 Alpha", Point(0, 0), FRAME_WIDTH);
        
        int y = 20;
        for(auto line : history) {
            graphics.text(get_prompt(line.first), Point(0, y), FRAME_WIDTH);
            graphics.text(line.second, Point(30, y), FRAME_WIDTH);
            y += 20;
        }

        graphics.text(get_prompt(lua_prompt), Point(0, y), FRAME_WIDTH);
        graphics.text(input_buffer, Point(30, y), FRAME_WIDTH);

        graphics.set_pen(graphics.create_pen(255, 0, 0));
        graphics.circle(cursor, cursor_size);

        graphics.set_pen(graphics.create_pen(0, 0, 255));
        graphics.rectangle(Rect(key_cursor.x - 5, key_cursor.y - 5, 10, 10));

        graphics.set_pen(graphics.create_pen(0, 255, 0));

        display.flip();

        //sleep_ms(1000 / 30);

        clear_pressed_states();
    }
}