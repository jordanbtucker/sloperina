/* The practice ROM's buttons, all of them user-set (settings > buttons, and settings.ini):
 * - the shortcut button (L): the game never sees it, nor anything while it's held; tapped while paused it's
 *   frame advance's step, held its slow play
 * - the layer button (R): held with it, the rare shortcuts
 * - each shortcut: the shortcut button (+ the layer button) + one button
 * - the menu's select / back / -10 / +10
 * - how long the shortcut button is held before slow play starts */
#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  CMD_MENU,          /* open / close the menu */
  CMD_LOAD_POSITION,
  CMD_SAVE_POSITION,
  CMD_FRAME_ADVANCE, /* pause / run again */
  CMD_LEVITATE,
  CMD_WATCHES,
  CMD_TIMER,         /* start / stop */
  CMD_TIMER_RESET,
  CMD_LAG_RESET,
  CMD_JS_HEIGHT_RESET,
  CMD_COUNT
} command_t;

typedef enum { MB_SELECT, MB_BACK, MB_MINUS_10, MB_PLUS_10, MB_COUNT } menu_button_t;

typedef struct {
  bool layer;      /* with the layer button too */
  uint32_t button; /* one BTN_* bit */
} binding_t;

extern binding_t command_bindings[CMD_COUNT];
extern uint32_t menu_buttons[MB_COUNT];
extern int32_t slow_play_delay_ms;
#define SLOW_PLAY_DELAY_MIN 200
#define SLOW_PLAY_DELAY_MAX 1000
#define SLOW_PLAY_DELAY_STEP 100

/* the command's chord went down this frame (input_shortcut) */
bool command_pressed(command_t c);

/* names, as in settings.ini and the menu: "L", "Down", "Plus", ... ("?" for none) */
const char *button_name(uint32_t button);
/* settings.ini's name: "Down" for the menu's "D-down" */
const char *button_key(uint32_t button);
/* one button's name (either kind) to its bit, 0 if it isn't one */
uint32_t button_from_name(const char *name, int len);
/* "L + R + Down": the command's whole chord */
void command_chord_text(command_t c, char *buf, int len);
/* "L+R+Down" for settings.ini, and back (false if it doesn't parse) */
void command_chord_key(command_t c, char *buf, int len);
bool command_chord_parse(const char *text, binding_t *out);
const char *command_key(command_t c);   /* "load_position": settings.ini's bind_<key> */
const char *command_label(command_t c); /* "load position": the menu's */
const char *menu_button_key(menu_button_t b);   /* "select": settings.ini's menu_<key> */
const char *menu_button_label(menu_button_t b); /* "select" */
/* the game font's glyph for a button (gamefont.h's GF_*), 0 if none */
uint32_t button_glyph_of(uint32_t button);

void buttons_defaults(void);
/* after loading: anything unusable goes back to its default (and the log says so) */
void buttons_check(void);

/* setting one, with the swaps that keep every button in a group distinct; each logs what it did */
void set_shortcut_button(uint32_t button);
void set_layer_button(uint32_t button);
void set_command_binding(command_t c, binding_t b);
void set_menu_button(menu_button_t mb, uint32_t button);

/* settings > buttons */
struct menu;
void buttons_build_menu(struct menu *m);

#endif
