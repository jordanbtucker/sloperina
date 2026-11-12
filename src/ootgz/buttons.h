/* The practice ROM's buttons, all of them user-set (settings > buttons, and settings.ini):
 * - the shortcut button (none by default, as on master; the remake layout's is L): the game never sees
 *   it, nor anything pressed while it's held
 * - the frame advance button (the shortcut button unless set): tapped while paused it's a frame, held
 *   slow play; the game never sees it while frame advance is on
 * - each shortcut: a combo of up to 3 buttons (a stick direction can be one), as in gz: it fires when
 *   all of them are held and the last one goes down, unless a bigger combo of the same group is held too
 *   (R + A + D-left beats R + D-left). A combo without the shortcut button reaches the game, as on master.
 * - the menu's back (a combo) and back (other) (a combo, or none)
 * - the menu's select / -10 / +10 (single buttons), and whether numbers go by 10 with -10 / +10 or with
 *   select held + d-left/right (master's)
 * - how long the frame advance button is held before slow play starts */
#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  CMD_MENU,          /* open / close the menu: in both groups */
  CMD_LOAD_POSITION,
  CMD_SAVE_POSITION,
  CMD_FRAME_ADVANCE, /* pause / run again */
  CMD_LEVITATE,
  CMD_WATCHES,
  CMD_TIMER,         /* start / stop */
  CMD_TIMER_RESET,
  CMD_LAG_RESET,
  CMD_JS_HEIGHT_RESET,
  CMD_MENU_BACK,     /* the menu's group from here: only while it's open */
  CMD_MENU_BACK_OTHER, /* may be none */
  CMD_COUNT
} command_t;

typedef enum { MB_SELECT, MB_MINUS_10, MB_PLUS_10, MB_COUNT } menu_button_t;

typedef struct {
  uint32_t held;   /* the combo's other buttons (BTN_* bits), held first */
  uint32_t button; /* the one that goes down last; 0: none */
} binding_t;

extern binding_t command_bindings[CMD_COUNT];
extern uint32_t menu_buttons[MB_COUNT];
extern uint32_t frame_advance_button_setting; /* 0: the shortcut button */
extern bool by_10_with_select;                /* numbers by 10: select held + d-left/right (else -10 / +10) */
extern int32_t slow_play_delay_ms;
#define SLOW_PLAY_DELAY_MIN 200
#define SLOW_PLAY_DELAY_MAX 1000
#define SLOW_PLAY_DELAY_STEP 100
#define COMBO_MAX 3

/* the frame advance button as it is now */
uint32_t frame_advance_button(void);

/* the command's combo went down this frame (none while the menu is capturing; the shortcuts, not the
 * menu's back, none during slow play) */
bool command_pressed(command_t c);

/* names, as in the menu ("D-down") and in settings.ini ("Down"); "?" for none */
const char *button_name(uint32_t button);
const char *button_key(uint32_t button);
/* one button's name (either kind) to its bit, 0 if it isn't one */
uint32_t button_from_name(const char *name, int len);
/* the game font's glyph for a button (gamefont.h's GF_*), 0 if none */
uint32_t button_glyph_of(uint32_t button);
/* "R + A + D-left" / "R+A+Left" (the shortcut button first, the last button last), "none" / "None" */
void binding_text(binding_t b, char *buf, int len);
void binding_key(binding_t b, char *buf, int len);
/* settings.ini's text back; false if it doesn't parse */
bool binding_parse(const char *text, binding_t *out);
/* the combo's buttons in binding_text's order; returns how many */
int binding_buttons(binding_t b, uint32_t out[COMBO_MAX]);

const char *command_setting_key(command_t c); /* "bind_load_position", "menu_back" */
const char *command_label(command_t c);       /* "load position" */
const char *menu_button_key(menu_button_t b); /* "select": settings.ini's menu_<key> */
const char *menu_button_label(menu_button_t b);

void buttons_defaults(void); /* the gz layout (master's) */
void buttons_remake(void);   /* the remake layout: L shortcuts */
/* after loading: anything unusable goes back to its default (and the log says so) */
void buttons_check(void);

/* setting one, with the swaps that keep every combo in a group distinct; each logs what it did; false
 * if it can't be used (the reason is logged) */
bool set_shortcut_button(uint32_t button);
bool set_frame_advance_button(uint32_t button);
bool set_command_binding(command_t c, binding_t b);
bool set_menu_button(menu_button_t mb, uint32_t button);

/* settings > buttons */
struct menu;
void buttons_build_menu(struct menu *m);

#endif
