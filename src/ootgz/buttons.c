/* The practice ROM's buttons (buttons.h): the shortcut button, the layer button, each shortcut's
 * chord, the menu's buttons and the slow play delay, all set in settings > buttons and kept in
 * settings.ini as text ("L+Down"). */
#include "buttons.h"

#include "gamefont.h"
#include "gfx.h"
#include "input.h"
#include "lang.h"
#include "menu.h"
#include "sys.h"

#define BUTTON_BITS 0xFFFF /* the buttons, not the stick-direction bits above them */
#define DPAD_BITS (BTN_DLEFT | BTN_DUP | BTN_DRIGHT | BTN_DDOWN)

binding_t command_bindings[CMD_COUNT];
uint32_t menu_buttons[MB_COUNT];
int32_t slow_play_delay_ms;

static const binding_t default_bindings[CMD_COUNT] = {
  [CMD_MENU] = { false, BTN_PLUS },
  [CMD_LOAD_POSITION] = { false, BTN_DDOWN },
  [CMD_SAVE_POSITION] = { false, BTN_DUP },
  [CMD_FRAME_ADVANCE] = { false, BTN_LSTICK },
  [CMD_LEVITATE] = { false, BTN_DLEFT },
  [CMD_WATCHES] = { false, BTN_DRIGHT },
  [CMD_TIMER] = { true, BTN_DUP },
  [CMD_TIMER_RESET] = { true, BTN_DDOWN },
  [CMD_LAG_RESET] = { true, BTN_DLEFT },
  [CMD_JS_HEIGHT_RESET] = { true, BTN_DRIGHT },
};
static const uint32_t default_menu_buttons[MB_COUNT] = { BTN_A, BTN_B, BTN_ZL, BTN_ZR };
#define DEFAULT_SHORTCUT BTN_L
#define DEFAULT_LAYER BTN_R
#define DEFAULT_SLOW_PLAY_DELAY 400

static const char *const command_keys[CMD_COUNT] = {
  "menu", "load_position", "save_position", "frame_advance", "levitate", "watches",
  "timer", "timer_reset", "lag_reset", "js_height_reset",
};
static const char *const command_labels[CMD_COUNT] = {
  "open / close menu", "load position", "save position", "frame advance", "levitate", "show / hide watches",
  "timer start / stop", "timer reset", "lag counter reset", "JS height reset",
};
static const char *const menu_button_keys[MB_COUNT] = { "select", "back", "minus_10", "plus_10" };
static const char *const menu_button_labels[MB_COUNT] = { "select", "back", "-10", "+10" };

/* by bit (input.h's BTN_*): settings.ini's names, the menu's, the game font's glyphs */
static const char *const key_names[16] = {
  "A", "B", "X", "Y", "L3", "R3", "L", "R", "ZL", "ZR", "Plus", "Minus", "Left", "Up", "Right", "Down",
};
static const char *const shown_names[16] = {
  "A", "B", "X", "Y", "L3", "R3", "L", "R", "ZL", "ZR", "Plus", "Minus", "D-left", "D-up", "D-right", "D-down",
};
static const uint32_t glyphs[16] = {
  GF_BTN_A, GF_BTN_B, GF_BTN_X, GF_BTN_Y, GF_LSTICK_PRESS, GF_RSTICK_PRESS, GF_BTN_L, GF_BTN_R,
  GF_BTN_ZL, GF_BTN_ZR, GF_PLUS, GF_MINUS, GF_DPAD_LEFT, GF_DPAD_UP, GF_DPAD_RIGHT, GF_DPAD_DOWN,
};

/* one button's bit number, -1 if it isn't exactly one of the 16 buttons */
static int bit_of(uint32_t button)
{
  if (!button || (button & (button - 1)) || !(button & BUTTON_BITS))
    return -1;
  return __builtin_ctz(button);
}

static uint32_t lowest_button(uint32_t buttons)
{
  buttons &= BUTTON_BITS;
  return buttons & -buttons;
}

const char *button_name(uint32_t button)
{
  int bit = bit_of(button);
  return bit < 0 ? "?" : shown_names[bit];
}

const char *button_key(uint32_t button)
{
  int bit = bit_of(button);
  return bit < 0 ? "?" : key_names[bit];
}

uint32_t button_glyph_of(uint32_t button)
{
  int bit = bit_of(button);
  return bit < 0 ? 0 : glyphs[bit];
}

static bool same_name(const char *a, const char *b, int len)
{
  for (int i = 0; i < len; i++) {
    char x = a[i], y = b[i];
    if (x >= 'A' && x <= 'Z')
      x += 'a' - 'A';
    if (y >= 'A' && y <= 'Z')
      y += 'a' - 'A';
    if (x != y || !y)
      return false;
  }
  return !b[len];
}

uint32_t button_from_name(const char *name, int len)
{
  while (len > 0 && (*name == ' ' || *name == '\t'))
    name++, len--;
  while (len > 0 && (name[len - 1] == ' ' || name[len - 1] == '\t'))
    len--;
  for (int bit = 0; bit < 16; bit++)
    if (same_name(name, key_names[bit], len) || same_name(name, shown_names[bit], len))
      return 1u << bit;
  return 0;
}

const char *command_key(command_t c) { return c < CMD_COUNT ? command_keys[c] : "?"; }
const char *command_label(command_t c) { return c < CMD_COUNT ? command_labels[c] : "?"; }
const char *menu_button_key(menu_button_t b) { return b < MB_COUNT ? menu_button_keys[b] : "?"; }
const char *menu_button_label(menu_button_t b) { return b < MB_COUNT ? menu_button_labels[b] : "?"; }

bool command_pressed(command_t c)
{
  return c < CMD_COUNT && input_shortcut(command_bindings[c].layer, command_bindings[c].button);
}

static void chord_text(binding_t b, char *buf, int len)
{
  if (b.layer)
    snprintf(buf, len, "%s + %s + %s", button_name(input_shortcut_button), button_name(input_layer_button),
             button_name(b.button));
  else
    snprintf(buf, len, "%s + %s", button_name(input_shortcut_button), button_name(b.button));
}

void command_chord_text(command_t c, char *buf, int len)
{
  chord_text(command_bindings[c], buf, len);
}

void command_chord_key(command_t c, char *buf, int len)
{
  binding_t b = command_bindings[c];
  if (b.layer)
    snprintf(buf, len, "%s+%s+%s", button_key(input_shortcut_button), button_key(input_layer_button),
             button_key(b.button));
  else
    snprintf(buf, len, "%s+%s", button_key(input_shortcut_button), button_key(b.button));
}

/* "L+Down" or "L+R+Down": the last name is the shortcut's own button; three names: with the layer
 * button. (The first ones are whatever the shortcut and layer buttons were when it was saved: they're
 * set on their own lines.) */
bool command_chord_parse(const char *text, binding_t *out)
{
  const char *parts[4];
  int lens[4], n = 0;
  const char *p = text;
  for (;;) {
    const char *start = p;
    while (*p && *p != '+')
      p++;
    if (n == 4)
      return false;
    parts[n] = start;
    lens[n++] = (int)(p - start);
    if (!*p)
      break;
    p++;
  }
  if (n < 2 || n > 3)
    return false;
  for (int i = 0; i < n; i++)
    if (!button_from_name(parts[i], lens[i]))
      return false;
  out->layer = n == 3;
  out->button = button_from_name(parts[n - 1], lens[n - 1]);
  return true;
}

void buttons_defaults(void)
{
  input_shortcut_button = DEFAULT_SHORTCUT;
  input_layer_button = DEFAULT_LAYER;
  for (int c = 0; c < CMD_COUNT; c++)
    command_bindings[c] = default_bindings[c];
  for (int b = 0; b < MB_COUNT; b++)
    menu_buttons[b] = default_menu_buttons[b];
  slow_play_delay_ms = DEFAULT_SLOW_PLAY_DELAY;
}

/* --- checks and swaps ---------------------------------------------------------------------- */

static bool usable_command(binding_t b)
{
  return bit_of(b.button) >= 0 && b.button != input_shortcut_button && b.button != input_layer_button;
}

static bool usable_menu_button(uint32_t button)
{
  return bit_of(button) >= 0 && !(button & DPAD_BITS) && button != input_shortcut_button;
}

static bool same_binding(binding_t a, binding_t b) { return a.layer == b.layer && a.button == b.button; }

void buttons_check(void)
{
  if (bit_of(input_shortcut_button) < 0 || bit_of(input_layer_button) < 0 ||
      input_shortcut_button == input_layer_button) {
    log_printf("buttons: shortcut / layer button unusable (%s, %s): back to %s, %s", button_name(input_shortcut_button),
               button_name(input_layer_button), button_name(DEFAULT_SHORTCUT), button_name(DEFAULT_LAYER));
    input_shortcut_button = DEFAULT_SHORTCUT;
    input_layer_button = DEFAULT_LAYER;
  }
  /* unusable, or the same chord as one before it: given its default if that's free, else the first free one */
  for (int c = 0; c < CMD_COUNT; c++) {
    bool ok = usable_command(command_bindings[c]);
    for (int d = 0; ok && d < c; d++)
      ok = !same_binding(command_bindings[c], command_bindings[d]);
    if (!ok) {
      char was[48];
      chord_text(command_bindings[c], was, sizeof(was));
      log_printf("buttons: %s (%s) unusable", command_labels[c], was);
      command_bindings[c] = (binding_t){ false, 0 };
    }
  }
  for (int c = 0; c < CMD_COUNT; c++) {
    if (command_bindings[c].button)
      continue;
    binding_t candidates[33];
    candidates[0] = default_bindings[c];
    for (int i = 0; i < 32; i++)
      candidates[i + 1] = (binding_t){ i >= 16, 1u << (i % 16) };
    for (int i = 0; i < 33 && !command_bindings[c].button; i++) {
      bool taken = !usable_command(candidates[i]);
      for (int d = 0; !taken && d < CMD_COUNT; d++)
        taken = same_binding(command_bindings[d], candidates[i]);
      if (!taken)
        command_bindings[c] = candidates[i];
    }
    char now[48];
    chord_text(command_bindings[c], now, sizeof(now));
    log_printf("buttons: %s: %s", command_labels[c], now);
  }
  for (int m = 0; m < MB_COUNT; m++) {
    bool ok = usable_menu_button(menu_buttons[m]);
    for (int n = 0; ok && n < m; n++)
      ok = menu_buttons[m] != menu_buttons[n];
    if (!ok) {
      log_printf("buttons: menu %s (%s) unusable: back to default", menu_button_labels[m], button_name(menu_buttons[m]));
      menu_buttons[m] = 0;
    }
  }
  for (int m = 0; m < MB_COUNT; m++) {
    if (menu_buttons[m])
      continue;
    /* its default if free, else the first free button */
    uint32_t candidates[17];
    candidates[0] = default_menu_buttons[m];
    for (int bit = 0; bit < 16; bit++)
      candidates[bit + 1] = 1u << bit;
    for (int i = 0; i < 17 && !menu_buttons[m]; i++) {
      bool taken = !usable_menu_button(candidates[i]);
      for (int n = 0; !taken && n < MB_COUNT; n++)
        taken = menu_buttons[n] == candidates[i];
      if (!taken)
        menu_buttons[m] = candidates[i];
    }
    log_printf("buttons: menu %s: %s", menu_button_labels[m], button_name(menu_buttons[m]));
  }
  if (slow_play_delay_ms < SLOW_PLAY_DELAY_MIN || slow_play_delay_ms > SLOW_PLAY_DELAY_MAX ||
      slow_play_delay_ms % SLOW_PLAY_DELAY_STEP) {
    log_printf("buttons: slow play delay %d ms unusable: back to %d", (int)slow_play_delay_ms, DEFAULT_SLOW_PLAY_DELAY);
    slow_play_delay_ms = DEFAULT_SLOW_PLAY_DELAY;
  }
}

/* A menu button on the new shortcut button can't be pressed on its own any more: it gets `freed`. */
static void move_menu_buttons_off(uint32_t button, uint32_t freed)
{
  for (int m = 0; m < MB_COUNT; m++)
    if (menu_buttons[m] == button) {
      menu_buttons[m] = freed;
      gfx_logf("menu %s moved to %s", menu_button_labels[m], button_name(freed));
    }
}

/* Shortcuts on what's now the shortcut or layer button get `freed`. */
static void move_commands_off(uint32_t button, uint32_t freed)
{
  for (int c = 0; c < CMD_COUNT; c++)
    if (command_bindings[c].button == button) {
      command_bindings[c].button = freed;
      char now[48];
      chord_text(command_bindings[c], now, sizeof(now));
      gfx_logf("%s moved to %s", command_labels[c], now);
    }
}

void set_shortcut_button(uint32_t button)
{
  uint32_t old = input_shortcut_button;
  if (bit_of(button) < 0 || button == old)
    return;
  if (button == input_layer_button) {
    input_layer_button = old;
    gfx_logf("layer button swapped to %s", button_name(old));
  } else {
    move_commands_off(button, old);
  }
  input_shortcut_button = button;
  move_menu_buttons_off(button, old);
  gfx_logf("shortcut button: %s", button_name(button));
}

void set_layer_button(uint32_t button)
{
  uint32_t old = input_layer_button;
  if (bit_of(button) < 0 || button == old)
    return;
  if (button == input_shortcut_button) {
    input_shortcut_button = old;
    gfx_logf("shortcut button swapped to %s", button_name(old));
    move_menu_buttons_off(old, button); /* the old layer button is the shortcut button now */
  } else {
    move_commands_off(button, old);
  }
  input_layer_button = button;
  gfx_logf("layer button: %s", button_name(button));
}

void set_command_binding(command_t c, binding_t b)
{
  if (c >= CMD_COUNT || !usable_command(b))
    return;
  binding_t old = command_bindings[c];
  if (same_binding(old, b))
    return;
  for (int d = 0; d < CMD_COUNT; d++)
    if (d != (int)c && same_binding(command_bindings[d], b)) {
      command_bindings[d] = old;
      char now[48];
      chord_text(old, now, sizeof(now));
      gfx_logf("%s swapped to %s", command_labels[d], now);
    }
  command_bindings[c] = b;
  char now[48];
  chord_text(b, now, sizeof(now));
  gfx_logf("%s: %s", command_labels[c], now);
}

void set_menu_button(menu_button_t mb, uint32_t button)
{
  if (mb >= MB_COUNT || !usable_menu_button(button))
    return;
  uint32_t old = menu_buttons[mb];
  if (button == old)
    return;
  for (int m = 0; m < MB_COUNT; m++)
    if (m != (int)mb && menu_buttons[m] == button) {
      menu_buttons[m] = old;
      gfx_logf("menu %s swapped to %s", menu_button_labels[m], button_name(old));
    }
  menu_buttons[mb] = button;
  gfx_logf("menu %s: %s", menu_button_labels[mb], button_name(button));
}

/* --- settings > buttons ------------------------------------------------------------------------- */

enum { ROW_SHORTCUT, ROW_LAYER, ROW_COMMAND, ROW_MENU_BUTTON };
#define ROW(kind, index) ((kind) << 8 | (index))
#define ROW_KIND(data) ((int)((data) >> 8))
#define ROW_INDEX(data) ((int)((data) & 0xFF))

static char capture_hint[96];

static void row_value(menu_item_t *it, char *buf, int len)
{
  int index = ROW_INDEX(it->data);
  switch (ROW_KIND(it->data)) {
    case ROW_SHORTCUT: snprintf(buf, len, "%s", button_name(input_shortcut_button)); break;
    case ROW_LAYER: snprintf(buf, len, "%s", button_name(input_layer_button)); break;
    case ROW_COMMAND: command_chord_text((command_t)index, buf, len); break;
    default: snprintf(buf, len, "%s", button_name(menu_buttons[index])); break;
  }
}

/* a shortcut: the shortcut button (+ the layer button) held and a button pressed; the back button on
 * its own cancels */
static bool capture_chord(menu_item_t *it)
{
  uint32_t pressed = input_pressed() & BUTTON_BITS, held = input_held();
  if (!pressed)
    return true;
  if (held & input_shortcut_button) {
    uint32_t button = lowest_button(pressed & ~(input_shortcut_button | input_layer_button));
    if (!button)
      return true; /* the shortcut or layer button going down: wait for the chord's own button */
    set_command_binding((command_t)ROW_INDEX(it->data), (binding_t){ (held & input_layer_button) != 0, button });
    return false;
  }
  if (pressed & menu_buttons[MB_BACK]) {
    gfx_logf("%s: unchanged", command_labels[ROW_INDEX(it->data)]);
    return false;
  }
  return true;
}

/* a single button: the next one pressed (menu buttons: not the d-pad, nor the shortcut button) */
static bool capture_button(menu_item_t *it)
{
  uint32_t button = lowest_button(input_pressed());
  if (!button)
    return true;
  int index = ROW_INDEX(it->data);
  switch (ROW_KIND(it->data)) {
    case ROW_SHORTCUT: set_shortcut_button(button); break;
    case ROW_LAYER: set_layer_button(button); break;
    default:
      if (!usable_menu_button(button))
        return true; /* the d-pad moves the cursor, the shortcut button makes shortcuts: keep waiting */
      set_menu_button((menu_button_t)index, button);
      break;
  }
  return false;
}

static void start_capture(menu_item_t *it)
{
  int kind = ROW_KIND(it->data);
  if (kind == ROW_COMMAND) {
    snprintf(capture_hint, sizeof(capture_hint), "hold %s (+ %s for the rare layer) and press a button   %s: cancel",
             button_name(input_shortcut_button), button_name(input_layer_button), button_name(menu_buttons[MB_BACK]));
  } else if (kind == ROW_MENU_BUTTON) {
    snprintf(capture_hint, sizeof(capture_hint), "press the new button (not the d-pad or %s)",
             button_name(input_shortcut_button));
  } else {
    snprintf(capture_hint, sizeof(capture_hint), "press the new button");
  }
  menu_edit(it);
}

static int get_slow_play(menu_item_t *it)
{
  (void)it;
  return (slow_play_delay_ms - SLOW_PLAY_DELAY_MIN) / SLOW_PLAY_DELAY_STEP;
}

static void set_slow_play(menu_item_t *it, int choice)
{
  (void)it;
  slow_play_delay_ms = SLOW_PLAY_DELAY_MIN + choice * SLOW_PLAY_DELAY_STEP;
}

#define SLOW_PLAY_CHOICES ((SLOW_PLAY_DELAY_MAX - SLOW_PLAY_DELAY_MIN) / SLOW_PLAY_DELAY_STEP + 1)

static void slow_play_text(menu_item_t *it, int choice, char *buf, int len)
{
  (void)it;
  snprintf(buf, len, "%d ms", SLOW_PLAY_DELAY_MIN + choice * SLOW_PLAY_DELAY_STEP);
}

static void act_reset(menu_item_t *it)
{
  (void)it;
  buttons_defaults();
  gfx_logf("buttons reset to defaults");
}

static menu_item_t *capture_row(menu_t *m, const char *label, int data, bool chord, const char *hint)
{
  menu_item_t *it = menu_add_action(m, label, start_capture, data);
  if (it) {
    it->text = row_value;
    it->edit = chord ? capture_chord : capture_button;
    it->edit_hint = capture_hint;
    it->capture = true;
    it->log_label = ""; /* the change logs itself */
    it->hint = hint;
  }
  return it;
}

void buttons_build_menu(menu_t *m)
{
  menu_init(m, "buttons", 26);
  menu_add_heading(m, "shortcut buttons");
  capture_row(m, "shortcut button", ROW(ROW_SHORTCUT, 0), false,
              "held for every shortcut; tapped / held while paused: frame advance. The game never sees it: on a "
              "button the game uses, the game loses that button completely");
  capture_row(m, "layer button", ROW(ROW_LAYER, 0), false, "held with the shortcut button for the rare shortcuts");
  menu_end_group(m);
  menu_add_heading(m, "shortcuts");
  for (int c = 0; c < CMD_COUNT; c++)
    capture_row(m, command_labels[c], ROW(ROW_COMMAND, c), true,
                "select, then hold the shortcut button (+ the layer button) and press a button");
  menu_end_group(m);
  menu_add_heading(m, "menu buttons");
  for (int b = 0; b < MB_COUNT; b++)
    capture_row(m, menu_button_labels[b], ROW(ROW_MENU_BUTTON, b), false, "select, then press the new button");
  menu_end_group(m);
  menu_add_heading(m, "frame advance");
  menu_item_t *slow = menu_add_option(m, "slow play delay", SLOW_PLAY_CHOICES, 0, get_slow_play, set_slow_play, 0);
  if (slow) {
    slow->choice_text = slow_play_text;
    slow->hint = "how long the shortcut button is held, while paused, before slow play starts";
  }
  menu_end_group(m);
  menu_add_action(m, "reset buttons to defaults", act_reset, 0);
  menu_add_return(m);
}
