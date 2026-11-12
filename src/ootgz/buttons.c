/* The practice ROM's buttons (buttons.h): the shortcut button, step frame's button, each
 * shortcut's combo, the menu's buttons, the slow advance delay; set in settings > buttons and kept in
 * settings.ini as text ("R+A+Left"). */
#include "buttons.h"

#include "gamefont.h"
#include "gfx.h"
#include "input.h"
#include "lang.h"
#include "menu.h"
#include "sys.h"

#define BUTTON_BITS 0xFFFF     /* the buttons, not the stick-direction bits above them */
#define BINDABLE_BITS 0xFFFFFF /* a combo can also have stick directions (gz's C buttons) */
#define DPAD_BITS (BTN_DLEFT | BTN_DUP | BTN_DRIGHT | BTN_DDOWN)
#define N_NAMES 24
#define NONE ((binding_t){ 0, 0 })
#define COMBO(held, button) ((binding_t){ (held), (button) })

binding_t command_bindings[CMD_COUNT];
uint32_t menu_buttons[MB_COUNT];
uint32_t frame_advance_button_setting;
int32_t slow_play_delay_ms;

/* --- the layouts --------------------------------------------------------------------------------- */

#define DEFAULT_SHORTCUT 0 /* none: the gz layout (master), nothing hidden from the game */
#define DEFAULT_FRAME_ADVANCE BTN_L
#define DEFAULT_SLOW_PLAY_DELAY 400

/* the remake layout: L shortcuts (the game never sees L), R left to the game (quick select, JSC) */
static const binding_t remake_bindings[CMD_COUNT] = {
  [CMD_MENU] = COMBO(BTN_L, BTN_PLUS),
  [CMD_LOAD_POSITION] = COMBO(BTN_L, BTN_DDOWN),
  [CMD_SAVE_POSITION] = COMBO(BTN_L, BTN_DUP),
  [CMD_FRAME_ADVANCE] = COMBO(BTN_L, BTN_LSTICK),
  [CMD_LEVITATE] = COMBO(BTN_L, BTN_DLEFT),
  [CMD_WATCHES] = COMBO(BTN_L, BTN_DRIGHT),
  [CMD_TIMER] = COMBO(BTN_L | BTN_R, BTN_DUP),
  [CMD_TIMER_RESET] = COMBO(BTN_L | BTN_R, BTN_DDOWN),
  [CMD_LAG_RESET] = COMBO(BTN_L | BTN_R, BTN_DLEFT),
  [CMD_JS_HEIGHT_RESET] = COMBO(BTN_L | BTN_R, BTN_DRIGHT),
  [CMD_MENU_BACK] = COMBO(0, BTN_B),
  [CMD_MENU_BACK_OTHER] = NONE,
  [CMD_NUMBER_MINUS_10] = COMBO(0, BTN_ZL),
  [CMD_NUMBER_PLUS_10] = COMBO(0, BTN_ZR),
};
static const uint32_t remake_menu_buttons[MB_COUNT] = { BTN_A };

/* the gz layout, the default: master's (origin/master's ootgz.c, position.c, watches.c, levitate.c,
 * menu.c), with no shortcut button (nothing hidden from the game) */
static const binding_t gz_bindings[CMD_COUNT] = {
  [CMD_MENU] = COMBO(BTN_R, BTN_RS_DOWN),
  [CMD_LOAD_POSITION] = COMBO(BTN_R, BTN_DDOWN),
  [CMD_SAVE_POSITION] = COMBO(BTN_R, BTN_DUP),
  [CMD_FRAME_ADVANCE] = COMBO(BTN_L, BTN_LSTICK),
  [CMD_LEVITATE] = COMBO(0, BTN_LSTICK),
  [CMD_WATCHES] = COMBO(BTN_R, BTN_DRIGHT),
  [CMD_TIMER] = COMBO(BTN_R | BTN_A, BTN_DLEFT),
  [CMD_TIMER_RESET] = COMBO(BTN_R | BTN_B, BTN_DLEFT),
  [CMD_LAG_RESET] = COMBO(BTN_R | BTN_B, BTN_DRIGHT),
  [CMD_JS_HEIGHT_RESET] = COMBO(BTN_R | BTN_A, BTN_DRIGHT),
  [CMD_MENU_BACK] = COMBO(0, BTN_B),
  [CMD_MENU_BACK_OTHER] = COMBO(BTN_R, BTN_DLEFT),
  [CMD_NUMBER_MINUS_10] = COMBO(BTN_L, BTN_DLEFT), /* select (L) held */
  [CMD_NUMBER_PLUS_10] = COMBO(BTN_L, BTN_DRIGHT),
};
static const uint32_t gz_menu_buttons[MB_COUNT] = { BTN_L };

static const char *const command_setting_keys[CMD_COUNT] = {
  "bind_menu", "bind_load_position", "bind_save_position", "bind_frame_advance", "bind_levitate",
  "bind_watches", "bind_timer", "bind_timer_reset", "bind_lag_reset", "bind_js_height_reset",
  "menu_back", "menu_back_other", "menu_minus_10", "menu_plus_10",
};
static const char *const command_labels[CMD_COUNT] = {
  "open / close menu", "load position", "save position", "pause / unpause", "levitate", "show / hide watches",
  "timer start / stop", "timer reset", "lag counter reset", "JS height reset", "return", "return (alternate)",
  "number -10", "number +10",
};
static const char *const menu_button_keys[MB_COUNT] = { "select" };
static const char *const menu_button_labels[MB_COUNT] = { "select" };

/* by bit (input.h's BTN_*): settings.ini's names, the menu's, the game font's glyphs */
static const char *const key_names[N_NAMES] = {
  "A", "B", "X", "Y", "L3", "R3", "L", "R", "ZL", "ZR", "Plus", "Minus", "Left", "Up", "Right", "Down",
  "LSLeft", "LSUp", "LSRight", "LSDown", "RSLeft", "RSUp", "RSRight", "RSDown",
};
static const char *const shown_names[N_NAMES] = {
  "A", "B", "X", "Y", "L3", "R3", "L", "R", "ZL", "ZR", "Plus", "Minus", "d-left", "d-up", "d-right", "d-down",
  "LS-left", "LS-up", "LS-right", "LS-down", "RS-left", "RS-up", "RS-right", "RS-down",
};
/* (the font has a stick pushed down, not the other ways: those show the plain stick) */
static const uint32_t glyphs[N_NAMES] = {
  GF_BTN_A, GF_BTN_B, GF_BTN_X, GF_BTN_Y, GF_LSTICK_PRESS, GF_RSTICK_PRESS, GF_BTN_L, GF_BTN_R,
  GF_BTN_ZL, GF_BTN_ZR, GF_PLUS, GF_MINUS, GF_DPAD_LEFT, GF_DPAD_UP, GF_DPAD_RIGHT, GF_DPAD_DOWN,
  GF_LSTICK, GF_LSTICK, GF_LSTICK, GF_LSTICK_DOWN, GF_RSTICK, GF_RSTICK, GF_RSTICK, GF_RSTICK_DOWN,
};

/* --- names ----------------------------------------------------------------------------------------- */

/* one button's (or stick direction's) bit number, -1 if it isn't exactly one of them */
static int bit_of(uint32_t button)
{
  if (!button || (button & (button - 1)) || !(button & BINDABLE_BITS))
    return -1;
  return __builtin_ctz(button);
}

/* one of the 16 real buttons (the shortcut, frame advance and menu buttons can't be stick directions) */
static bool is_button(uint32_t button)
{
  return bit_of(button) >= 0 && (button & BUTTON_BITS);
}

static uint32_t lowest_bit(uint32_t bits) { return bits & -bits; }
static int count_bits(uint32_t bits) { return __builtin_popcount(bits); }

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

static void trim(const char **name, int *len)
{
  while (*len > 0 && (**name == ' ' || **name == '\t'))
    (*name)++, (*len)--;
  while (*len > 0 && ((*name)[*len - 1] == ' ' || (*name)[*len - 1] == '\t'))
    (*len)--;
}

uint32_t button_from_name(const char *name, int len)
{
  trim(&name, &len);
  for (int bit = 0; bit < N_NAMES; bit++)
    if (same_name(name, key_names[bit], len) || same_name(name, shown_names[bit], len))
      return 1u << bit;
  return 0;
}

const char *command_setting_key(command_t c) { return c < CMD_COUNT ? command_setting_keys[c] : "?"; }
const char *command_label(command_t c) { return c < CMD_COUNT ? command_labels[c] : "?"; }
const char *menu_button_key(menu_button_t b) { return b < MB_COUNT ? menu_button_keys[b] : "?"; }
const char *menu_button_label(menu_button_t b) { return b < MB_COUNT ? menu_button_labels[b] : "?"; }

uint32_t frame_advance_button(void)
{
  return frame_advance_button_setting ? frame_advance_button_setting : input_shortcut_button;
}

int binding_buttons(binding_t b, uint32_t out[COMBO_MAX])
{
  int n = 0;
  if (!b.button)
    return 0;
  /* the shortcut button first, then the shoulder buttons, then the rest */
  const uint32_t first[] = { input_shortcut_button, BTN_L, BTN_R, BTN_ZL, BTN_ZR };
  uint32_t rest = b.held;
  for (unsigned i = 0; i < sizeof(first) / sizeof(*first); i++)
    if ((rest & first[i]) && n < COMBO_MAX - 1) {
      out[n++] = first[i];
      rest &= ~first[i];
    }
  while (rest && n < COMBO_MAX - 1) {
    out[n++] = lowest_bit(rest);
    rest &= rest - 1;
  }
  out[n++] = b.button;
  return n;
}

static void join(binding_t b, char *buf, int len, const char *sep, const char *(*name)(uint32_t), const char *none)
{
  uint32_t parts[COMBO_MAX];
  int n = binding_buttons(b, parts);
  if (!n) {
    snprintf(buf, len, "%s", none);
    return;
  }
  int at = 0;
  buf[0] = 0;
  for (int i = 0; i < n && at < len; i++)
    at += snprintf(buf + at, len - at, "%s%s", i ? sep : "", name(parts[i]));
}

void binding_text(binding_t b, char *buf, int len) { join(b, buf, len, " + ", button_name, "none"); }
void binding_key(binding_t b, char *buf, int len) { join(b, buf, len, "+", button_key, "None"); }

/* buttons_fill's placeholders: a command's combo, or one of the single buttons */
static const struct {
  const char *name;
  int command; /* -1: a single button */
  int single;  /* 0: select, 1: step frame */
} fills[] = {
  { "menu", CMD_MENU, 0 },          { "save", CMD_SAVE_POSITION, 0 },     { "load", CMD_LOAD_POSITION, 0 },
  { "pause", CMD_FRAME_ADVANCE, 0 }, { "levitate", CMD_LEVITATE, 0 },    { "watches", CMD_WATCHES, 0 },
  { "js_reset", CMD_JS_HEIGHT_RESET, 0 }, { "return", CMD_MENU_BACK, 0 }, { "minus_10", CMD_NUMBER_MINUS_10, 0 },
  { "plus_10", CMD_NUMBER_PLUS_10, 0 }, { "select", -1, 0 },             { "step", -1, 1 },
};

/* where c is in s, 0 if it isn't (strchr isn't imported) */
static const char *find_char(const char *s, char c)
{
  for (; *s; s++)
    if (*s == c)
      return s;
  return 0;
}

/* the name, n characters at s, is exactly word */
static bool names_word(const char *s, int n, const char *word)
{
  int i = 0;
  while (i < n && word[i] == s[i])
    i++;
  return i == n && !word[i];
}

const char *buttons_fill(const char *text)
{
  if (!text || !find_char(text, '{'))
    return text;
  static char bufs[4][256];
  static int next;
  char *out = bufs[next];
  next = (next + 1) % 4;
  int at = 0, len = (int)sizeof(bufs[0]);
  for (const char *p = text; *p && at < len - 1;) {
    const char *close = *p == '{' ? find_char(p, '}') : 0;
    int found = -1;
    if (close)
      for (unsigned i = 0; i < sizeof(fills) / sizeof(*fills); i++)
        if (names_word(p + 1, (int)(close - p - 1), fills[i].name))
          found = (int)i;
    if (found < 0) {
      out[at++] = *p++;
      continue;
    }
    char name[64];
    if (fills[found].command >= 0)
      binding_text(command_bindings[fills[found].command], name, sizeof(name));
    else
      snprintf(name, sizeof(name), "%s",
               button_name(fills[found].single ? frame_advance_button() : menu_buttons[MB_SELECT]));
    at += snprintf(out + at, len - at, "%s", name);
    if (at > len - 1)
      at = len - 1;
    p = close + 1;
  }
  out[at] = 0;
  return out;
}

/* "R+A+Left": the last name is the one that goes down last; "None": none */
bool binding_parse(const char *text, binding_t *out)
{
  int text_len = (int)strlen(text);
  const char *t = text;
  trim(&t, &text_len);
  if (same_name(t, "none", text_len)) {
    *out = NONE;
    return true;
  }
  uint32_t parts[COMBO_MAX];
  int n = 0;
  const char *p = text;
  for (;;) {
    const char *start = p;
    while (*p && *p != '+')
      p++;
    uint32_t button = button_from_name(start, (int)(p - start));
    if (!button || n == COMBO_MAX)
      return false;
    parts[n++] = button;
    if (!*p)
      break;
    p++;
  }
  binding_t b = { 0, parts[n - 1] };
  for (int i = 0; i < n - 1; i++)
    b.held |= parts[i];
  if ((b.held & b.button) || count_bits(b.held | b.button) != n)
    return false;
  *out = b;
  return true;
}

/* --- firing ------------------------------------------------------------------------------------------ */

static uint32_t all_of(binding_t b) { return b.held | b.button; }
static bool same_binding(binding_t a, binding_t b) { return a.held == b.held && a.button == b.button; }

/* in-game shortcuts are one group, the menu's (open / close, back) another */
static bool in_game_group(int c) { return c < CMD_MENU_BACK; }
static bool in_menu_group(int c) { return c == CMD_MENU || c >= CMD_MENU_BACK; }
static bool same_group(int c, int d)
{
  return (in_game_group(c) && in_game_group(d)) || (in_menu_group(c) && in_menu_group(d));
}

static bool capturing; /* settings > buttons is waiting for a combo or button */

bool menu_capturing(void)
{
  if (capturing && !menu_editing) /* the edit ended some other way (the menu closed) */
    capturing = false;
  return capturing;
}

static bool command_went(command_t c, bool repeat)
{
  if (c >= CMD_COUNT || menu_capturing())
    return false;
  if (input_slow_play && in_game_group(c))
    return false;
  binding_t b = command_bindings[c];
  uint32_t held = input_held(), all = all_of(b);
  bool went = repeat ? input_repeat(b.button) : (input_pressed() & b.button) != 0;
  if (!b.button || (held & all) != all || !went)
    return false;
  /* a bigger combo of the same group held as well wins (R + A + D-left over R + D-left) */
  for (int d = 0; d < CMD_COUNT; d++) {
    uint32_t bigger = all_of(command_bindings[d]);
    if (d != (int)c && command_bindings[d].button && same_group(c, d) && bigger != all &&
        (bigger & all) == all && (held & bigger) == bigger)
      return false;
  }
  return true;
}

bool command_pressed(command_t c) { return command_went(c, false); }
bool command_repeat(command_t c) { return command_went(c, true); }

/* --- checks -------------------------------------------------------------------------------------------- */

static void layout_into_globals(const binding_t *bindings, const uint32_t *menu)
{
  for (int c = 0; c < CMD_COUNT; c++)
    command_bindings[c] = bindings[c];
  for (int b = 0; b < MB_COUNT; b++)
    menu_buttons[b] = menu[b];
}

void buttons_defaults(void)
{
  input_shortcut_button = DEFAULT_SHORTCUT;
  frame_advance_button_setting = DEFAULT_FRAME_ADVANCE;
  layout_into_globals(gz_bindings, gz_menu_buttons);
  slow_play_delay_ms = DEFAULT_SLOW_PLAY_DELAY;
}

void buttons_remake(void)
{
  input_shortcut_button = BTN_L;
  frame_advance_button_setting = 0;
  layout_into_globals(remake_bindings, remake_menu_buttons);
  slow_play_delay_ms = DEFAULT_SLOW_PLAY_DELAY;
}

/* a lone button, or 0 */
static uint32_t lone(binding_t b) { return b.button && !b.held ? b.button : 0; }

static bool is_menu_button(uint32_t button, int except)
{
  for (int m = 0; m < MB_COUNT; m++)
    if (m != except && menu_buttons[m] == button)
      return true;
  return false;
}

/* why command c can't have b, or 0 */
static const char *unusable_combo(int c, binding_t b)
{
  if (!b.button)
    return c == CMD_MENU_BACK_OTHER ? 0 : "it can't be none";
  if (bit_of(b.button) < 0 || (b.held & ~BINDABLE_BITS) || (b.held & b.button))
    return "not a combo";
  uint32_t all = all_of(b);
  if (count_bits(all) > COMBO_MAX)
    return "3 buttons at most";
  if (all == input_shortcut_button)
    return "the shortcut button alone is frame advance's";
  if (all == frame_advance_button())
    return "step frame's button alone steps frames";
  if (in_menu_group(c) && lone(b) && is_menu_button(lone(b), -1))
    return "that button alone is select";
  if (in_menu_group(c) && (lone(b) & DPAD_BITS))
    return "the d-pad alone moves the menu's cursor";
  return 0;
}

static bool plain_menu_button(uint32_t button)
{
  return is_button(button) && !(button & DPAD_BITS) && button != input_shortcut_button;
}

/* free for command c: usable and no other of its groups has it */
static bool free_for(int c, binding_t b)
{
  if (unusable_combo(c, b))
    return false;
  for (int d = 0; d < CMD_COUNT; d++)
    if (d != c && same_group(c, d) && b.button && same_binding(command_bindings[d], b))
      return false;
  return true;
}

void buttons_check(void)
{
  if (input_shortcut_button && !is_button(input_shortcut_button)) {
    log_printf("buttons: shortcut button unusable: back to none");
    input_shortcut_button = DEFAULT_SHORTCUT;
  }
  if (frame_advance_button_setting == input_shortcut_button)
    frame_advance_button_setting = 0;
  if (!frame_advance_button_setting && !input_shortcut_button) /* no shortcut button to be it */
    frame_advance_button_setting = DEFAULT_FRAME_ADVANCE;
  if (frame_advance_button_setting && !is_button(frame_advance_button_setting)) {
    log_printf("buttons: step frame unusable: back to the shortcut button");
    frame_advance_button_setting = 0;
  }
  for (int m = 0; m < MB_COUNT; m++) {
    uint32_t button = menu_buttons[m];
    bool ok = plain_menu_button(button);
    for (int n = 0; ok && n < m; n++)
      ok = menu_buttons[n] != button;
    if (!ok) {
      log_printf("buttons: menu %s (%s) unusable", menu_button_labels[m], button_name(button));
      menu_buttons[m] = 0;
    }
  }
  for (int m = 0; m < MB_COUNT; m++) {
    if (menu_buttons[m])
      continue;
    for (int bit = -1; bit < 16 && !menu_buttons[m]; bit++) { /* its default, else the first free one */
      uint32_t candidate = bit < 0 ? remake_menu_buttons[m] : 1u << bit;
      if (plain_menu_button(candidate) && !is_menu_button(candidate, m))
        menu_buttons[m] = candidate;
    }
    log_printf("buttons: menu %s: %s", menu_button_labels[m], button_name(menu_buttons[m]));
  }
  /* unusable, or the same combo as one before it in a group: its default if free, else a free one */
  binding_t bad = { 0, 0xFFFFFFFF };
  for (int c = 0; c < CMD_COUNT; c++) {
    bool ok = !unusable_combo(c, command_bindings[c]);
    for (int d = 0; ok && d < c; d++)
      ok = !(same_group(c, d) && command_bindings[c].button &&
             same_binding(command_bindings[c], command_bindings[d]));
    if (!ok) {
      char was[48];
      binding_text(command_bindings[c], was, sizeof(was));
      log_printf("buttons: %s (%s) unusable", command_labels[c], was);
      command_bindings[c] = bad;
    }
  }
  for (int c = 0; c < CMD_COUNT; c++) {
    if (!same_binding(command_bindings[c], bad))
      continue;
    command_bindings[c] = NONE;
    if (free_for(c, remake_bindings[c])) {
      command_bindings[c] = remake_bindings[c];
    } else {
      for (int bit = 0; bit < 16 && !command_bindings[c].button; bit++) {
        binding_t candidate = COMBO(input_shortcut_button, 1u << bit);
        if ((1u << bit) != input_shortcut_button && free_for(c, candidate))
          command_bindings[c] = candidate;
      }
    }
    char now[48];
    binding_text(command_bindings[c], now, sizeof(now));
    log_printf("buttons: %s: %s", command_labels[c], now);
  }
  if (slow_play_delay_ms < SLOW_PLAY_DELAY_MIN || slow_play_delay_ms > SLOW_PLAY_DELAY_MAX ||
      slow_play_delay_ms % SLOW_PLAY_DELAY_STEP) {
    log_printf("buttons: slow advance delay %d ms unusable: back to %d", (int)slow_play_delay_ms, DEFAULT_SLOW_PLAY_DELAY);
    slow_play_delay_ms = DEFAULT_SLOW_PLAY_DELAY;
  }
}

/* --- setting one ------------------------------------------------------------------------------------- */

static uint32_t swap_bits(uint32_t bits, uint32_t a, uint32_t b)
{
  bool has_a = bits & a, has_b = bits & b;
  bits &= ~(a | b);
  return bits | (has_a ? b : 0) | (has_b ? a : 0);
}

/* The shortcut button moves to `button`: every combo with the old one gets `button` instead, and one
 * that had `button` gets the old one (so "L + D-down" becomes "X + D-down"). */
bool set_shortcut_button(uint32_t button)
{
  uint32_t old = input_shortcut_button;
  if (button && !is_button(button)) {
    gfx_logf("the shortcut button can't be a stick direction");
    return false;
  }
  if (button == old)
    return true;
  if (!button || !old) {
    /* to or from none: the combos stay as they are (what's hidden from the game is all that changes) */
    if (!button && !frame_advance_button_setting)
      frame_advance_button_setting = old; /* frame advance keeps its button */
    input_shortcut_button = button;
    if (frame_advance_button_setting == button)
      frame_advance_button_setting = 0;
    bool moved[MB_COUNT] = { false };
    for (int m = 0; button && m < MB_COUNT; m++)
      if (menu_buttons[m] == button) {
        menu_buttons[m] = 0; /* buttons_check finds it another */
        moved[m] = true;
      }
    gfx_logf("shortcut button: %s", button ? button_name(button) : "none (all buttons are passed to the game)");
    buttons_check();
    for (int m = 0; m < MB_COUNT; m++)
      if (moved[m])
        gfx_logf("menu %s moved to %s", menu_button_labels[m], button_name(menu_buttons[m]));
    return true;
  }
  for (int c = 0; c < CMD_COUNT; c++) {
    command_bindings[c].held = swap_bits(command_bindings[c].held, old, button);
    command_bindings[c].button = swap_bits(command_bindings[c].button, old, button);
  }
  if (frame_advance_button_setting == button)
    frame_advance_button_setting = old;
  input_shortcut_button = button;
  for (int m = 0; m < MB_COUNT; m++)
    if (menu_buttons[m] == button) {
      menu_buttons[m] = old;
      gfx_logf("menu %s moved to %s", menu_button_labels[m], button_name(old));
    }
  gfx_logf("shortcut button: %s (in every shortcut with %s)", button_name(button), button_name(old));
  buttons_check(); /* (nothing should be unusable now: the log would say) */
  return true;
}

bool set_frame_advance_button(uint32_t button)
{
  if (!is_button(button)) {
    gfx_logf("step frame can't be a stick direction");
    return false;
  }
  if (button == input_shortcut_button)
    button = 0;
  for (int c = 0; button && c < CMD_COUNT; c++)
    if (all_of(command_bindings[c]) == button) {
      gfx_logf("%s alone is %s: change that first", button_name(button), command_labels[c]);
      return false;
    }
  frame_advance_button_setting = button;
  gfx_logf("step frame: %s", button ? button_name(button) : "the shortcut button");
  return true;
}

bool set_command_binding(command_t c, binding_t b)
{
  if (c >= CMD_COUNT)
    return false;
  char text[48];
  binding_text(b, text, sizeof(text));
  const char *why = unusable_combo(c, b);
  if (why) {
    gfx_logf("%s: not %s (%s)", command_labels[c], text, why);
    return false;
  }
  binding_t old = command_bindings[c];
  if (same_binding(old, b))
    return true;
  for (int d = 0; d < CMD_COUNT; d++)
    if (d != (int)c && same_group(c, d) && b.button && same_binding(command_bindings[d], b) &&
        unusable_combo(d, old)) {
      gfx_logf("%s: not %s (it's %s's)", command_labels[c], text, command_labels[d]);
      return false;
    }
  for (int d = 0; d < CMD_COUNT; d++)
    if (d != (int)c && same_group(c, d) && b.button && same_binding(command_bindings[d], b)) {
      command_bindings[d] = old;
      char now[48];
      binding_text(old, now, sizeof(now));
      gfx_logf("%s swapped to %s", command_labels[d], now);
    }
  command_bindings[c] = b;
  gfx_logf("%s: %s", command_labels[c], text);
  return true;
}

bool set_menu_button(menu_button_t mb, uint32_t button)
{
  if (mb >= MB_COUNT)
    return false;
  uint32_t old = menu_buttons[mb];
  if (button == old)
    return true;
  if (!plain_menu_button(button))
    return false;
  /* a back combo that's this button alone: swapped with this one's old button */
  for (int c = CMD_MENU; c < CMD_COUNT; c++)
    if (in_menu_group(c) && lone(command_bindings[c]) == button) {
      command_bindings[c] = COMBO(0, old);
      gfx_logf("%s swapped to %s", command_labels[c], button_name(old));
    }
  for (int m = 0; m < MB_COUNT; m++)
    if (m != (int)mb && menu_buttons[m] == button) {
      menu_buttons[m] = old;
      gfx_logf("menu %s swapped to %s", menu_button_labels[m], button_name(old));
    }
  menu_buttons[mb] = button;
  gfx_logf("menu %s: %s", menu_button_labels[mb], button_name(button));
  return true;
}

/* --- settings > buttons ------------------------------------------------------------------------------ */

enum { ROW_SHORTCUT, ROW_FRAME_ADVANCE, ROW_COMMAND, ROW_MENU_BUTTON };
#define ROW(kind, index) ((kind) << 8 | (index))
#define ROW_KIND(data) ((int)((data) >> 8))
#define ROW_INDEX(data) ((int)((data) & 0xFF))

static char capture_hint[128];
static bool capture_armed; /* everything was let go since the capture started */
static uint32_t capture_all, capture_last;

static void row_value(menu_item_t *it, char *buf, int len)
{
  int index = ROW_INDEX(it->data);
  switch (ROW_KIND(it->data)) {
    case ROW_SHORTCUT: snprintf(buf, len, "%s", input_shortcut_button ? button_name(input_shortcut_button) : "none"); break;
    case ROW_FRAME_ADVANCE:
      if (frame_advance_button_setting)
        snprintf(buf, len, "%s", button_name(frame_advance_button_setting));
      else
        snprintf(buf, len, "shortcut button (%s)", button_name(input_shortcut_button));
      break;
    case ROW_COMMAND: binding_text(command_bindings[index], buf, len); break;
    default: snprintf(buf, len, "%s", button_name(menu_buttons[index])); break;
  }
}

/* what cancels a combo capture: the back combo when it's one button alone, else B */
static uint32_t cancel_button(void)
{
  uint32_t back = lone(command_bindings[CMD_MENU_BACK]);
  return back ? back : BTN_B;
}

/* a combo: held, then saved when one of its buttons is let go (gz-style) */
static bool capture_combo(menu_item_t *it)
{
  uint32_t held = input_held() & BINDABLE_BITS, pressed = input_pressed() & BINDABLE_BITS;
  if (!capture_armed) { /* wait for select (and anything else) to be let go first */
    capture_armed = !held;
    capture_all = capture_last = 0;
    return true;
  }
  if (pressed) {
    capture_all |= pressed;
    capture_last = lowest_bit(pressed);
  }
  if (!capture_all || (held & capture_all) == capture_all)
    return true;
  /* one was let go: that's the combo */
  int c = ROW_INDEX(it->data);
  binding_t b = { capture_all & ~capture_last, capture_last };
  uint32_t all = capture_all;
  capture_armed = false; /* (if it's tried again) */
  bool back_row = c == CMD_MENU_BACK || c == CMD_MENU_BACK_OTHER;
  if (!back_row && all == cancel_button()) {
    gfx_logf("%s: unchanged", command_labels[c]);
    capturing = false;
    return false;
  }
  if (count_bits(all) > COMBO_MAX) {
    gfx_logf("3 buttons at most: try again");
    return true;
  }
  if (!set_command_binding((command_t)c, b))
    return true; /* why is logged: try again */
  capturing = false;
  return false;
}

/* a single button: the next one pressed */
static bool capture_button(menu_item_t *it)
{
  uint32_t held = input_held() & BUTTON_BITS, pressed = input_pressed() & BUTTON_BITS;
  if (!capture_armed) {
    capture_armed = !held;
    return true;
  }
  uint32_t button = lowest_bit(pressed);
  if (!button)
    return true;
  int index = ROW_INDEX(it->data);
  bool done;
  switch (ROW_KIND(it->data)) {
    case ROW_SHORTCUT: done = set_shortcut_button(button); break;
    case ROW_FRAME_ADVANCE: done = set_frame_advance_button(button); break;
    default: /* not the d-pad (it moves the cursor) nor the shortcut button: keep waiting */
      done = set_menu_button((menu_button_t)index, button);
      break;
  }
  if (done)
    capturing = false;
  return !done;
}

static void start_capture(menu_item_t *it)
{
  int kind = ROW_KIND(it->data), index = ROW_INDEX(it->data);
  if (kind == ROW_COMMAND && index == CMD_MENU_BACK_OTHER)
    snprintf(capture_hint, sizeof(capture_hint), "hold up to 3 buttons (the last one pressed last), let go to save");
  else if (kind == ROW_COMMAND && index == CMD_MENU_BACK)
    snprintf(capture_hint, sizeof(capture_hint), "hold up to 3 buttons (the last one pressed last), let go to save");
  else if (kind == ROW_COMMAND)
    snprintf(capture_hint, sizeof(capture_hint), "hold up to 3 buttons (the last one pressed last), let go to save   %s alone: cancel",
             button_name(cancel_button()));
  else if (kind == ROW_MENU_BUTTON)
    snprintf(capture_hint, sizeof(capture_hint), "press the new button (not the d-pad or %s)",
             button_name(input_shortcut_button));
  else if (kind == ROW_FRAME_ADVANCE && input_shortcut_button)
    snprintf(capture_hint, sizeof(capture_hint), "press the new button (%s: the shortcut button)",
             button_name(input_shortcut_button));
  else
    snprintf(capture_hint, sizeof(capture_hint), "press the new button");
  capture_armed = false;
  capture_all = capture_last = 0;
  capturing = true;
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


/* d-left/right on the rows that can be none: none, or back to what it was */
static void toggle_none(menu_item_t *it, int dir)
{
  (void)dir;
  static uint32_t last_shortcut = BTN_L;
  static binding_t last_back = COMBO(BTN_R, BTN_DLEFT);
  if (ROW_KIND(it->data) == ROW_SHORTCUT) {
    if (input_shortcut_button) {
      last_shortcut = input_shortcut_button;
      set_shortcut_button(0);
    } else {
      set_shortcut_button(last_shortcut);
    }
    return;
  }
  binding_t *back = &command_bindings[CMD_MENU_BACK_OTHER];
  if (back->button) {
    last_back = *back;
    *back = NONE;
    gfx_logf("%s: none", command_labels[CMD_MENU_BACK_OTHER]);
  } else {
    set_command_binding(CMD_MENU_BACK_OTHER, last_back); /* (says why if it can't) */
  }
}

static void act_gz(menu_item_t *it)
{
  (void)it;
  buttons_defaults();
  gfx_logf("buttons: gz preset");
}

static void act_remake(menu_item_t *it)
{
  (void)it;
  buttons_remake();
  gfx_logf("buttons: remake preset");
}

static menu_item_t *capture_row(menu_t *m, const char *label, int data, bool combo, const char *hint)
{
  menu_item_t *it = menu_add_action(m, label, start_capture, data);
  if (it) {
    it->text = row_value;
    it->edit = combo ? capture_combo : capture_button;
    it->edit_hint = capture_hint;
    it->capture = true;
    it->log_label = ""; /* the change logs itself */
    it->hint = hint;
  }
  return it;
}

/* what each shortcut does, for its row's hint */
static const char *const command_hints[CMD_COUNT] = {
  [CMD_MENU] = "opens or closes this menu",
  [CMD_SAVE_POSITION] = "saves Link's position",
  [CMD_LOAD_POSITION] = "moves Link to the saved position",
  [CMD_LEVITATE] = "starts or stops levitating (needs settings > levitate > enabled)",
  [CMD_WATCHES] = "shows or hides the watch window",
  [CMD_TIMER] = "starts or stops the timer",
  [CMD_TIMER_RESET] = "resets the timer",
  [CMD_LAG_RESET] = "resets the lag counter",
  [CMD_JS_HEIGHT_RESET] = "sets jump slash (JS) height, the takeoff height your last jump saved, back to its maximum, "
                          "as returning to the title screen does",
};

void buttons_build_menu(menu_t *m)
{
  menu_init(m, "buttons", 28);
  menu_item_t *shortcut = capture_row(m, "shortcut button", ROW(ROW_SHORTCUT, 0), false,
                                      "hold for shortcuts: it, and anything pressed while it's held, is not passed "
                                      "to the game; press d-left/right to clear");
  if (shortcut)
    shortcut->on_change = toggle_none;
  menu_add_heading(m, "shortcuts");
  static const command_t shortcuts[] = {
    CMD_MENU, CMD_SAVE_POSITION, CMD_LOAD_POSITION, CMD_LEVITATE, CMD_WATCHES,
    CMD_TIMER, CMD_TIMER_RESET, CMD_LAG_RESET, CMD_JS_HEIGHT_RESET,
  };
  for (unsigned i = 0; i < sizeof(shortcuts) / sizeof(*shortcuts); i++)
    capture_row(m, command_labels[shortcuts[i]], ROW(ROW_COMMAND, shortcuts[i]), true, command_hints[shortcuts[i]]);
  menu_end_group(m);
  menu_add_heading(m, "frame advance");
  capture_row(m, command_labels[CMD_FRAME_ADVANCE], ROW(ROW_COMMAND, CMD_FRAME_ADVANCE), true,
              "pause or unpause the game for frame advance (needs settings > frame advance > enabled)");
  capture_row(m, "step frame", ROW(ROW_FRAME_ADVANCE, 0), false,
              "while paused by frame advance: tap to advance one frame, hold to advance frames slowly; not passed "
              "to the game while frame advance is enabled");
  menu_item_t *slow = menu_add_option(m, "slow advance delay", SLOW_PLAY_CHOICES, 0, get_slow_play, set_slow_play, 0);
  if (slow) {
    slow->choice_text = slow_play_text;
    slow->hint = "how long step frame is held before frames advance slowly";
  }
  menu_end_group(m);
  menu_add_heading(m, "menu buttons");
  capture_row(m, menu_button_labels[MB_SELECT], ROW(ROW_MENU_BUTTON, MB_SELECT), false,
              "selects the highlighted menu item");
  capture_row(m, command_labels[CMD_MENU_BACK], ROW(ROW_COMMAND, CMD_MENU_BACK), true,
              "returns to the previous page; on the first page, closes the menu");
  menu_item_t *alternate = capture_row(m, command_labels[CMD_MENU_BACK_OTHER], ROW(ROW_COMMAND, CMD_MENU_BACK_OTHER),
                                       true, "another combo that returns to the previous page; press d-left/right to clear");
  if (alternate)
    alternate->on_change = toggle_none;
  capture_row(m, command_labels[CMD_NUMBER_MINUS_10], ROW(ROW_COMMAND, CMD_NUMBER_MINUS_10), true,
              "lowers the selected number by 10");
  capture_row(m, command_labels[CMD_NUMBER_PLUS_10], ROW(ROW_COMMAND, CMD_NUMBER_PLUS_10), true,
              "raises the selected number by 10");
  menu_end_group(m);
  menu_item_t *gz = menu_add_action(m, "use gz preset (default)", act_gz, 0);
  if (gz)
    gz->hint = "R for shortcuts, L for frame advance and select, as in gz; all buttons are passed to the game";
  menu_item_t *remake = menu_add_action(m, "use remake preset", act_remake, 0);
  if (remake)
    remake->hint = "L is used for shortcuts, and that button is not passed to the game, so R stays free for quick "
                   "select; A selects, B returns";
  menu_add_return(m);
}
