/* Real macro recording/playback (macro.c) and the nn::hid filter (input.c), with a fake game that reads
 * the controller through input.c's wrapper once a frame before the frame hook; no game process. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ootgz/input.c"
#include "../src/ootgz/macro.c"

/* --- stubs ---------------------------------------------------------------------------------------- */
void log_printf(const char *fmt, ...) { (void)fmt; }
const char *TF(const char *f) { return f; }
static const char *last_notice;
void gfx_notice(const char *s) { last_notice = s; }
static bool sd = true;
bool settings_sd(void) { return sd; }
const char *got_hook(uintptr_t m, const char *n, void *r, void *f, void **next) { return 0; }
void hid_detail_fullkey(int *n, void *out, int count, const uint32_t *id) { *n = 0; }
void hid_detail_handheld(int *n, void *out, int count, const uint32_t *id) { *n = 0; }
void hid_detail_joydual(int *n, void *out, int count, const uint32_t *id) { *n = 0; }
int hid_public_0(void *o, int c, const uint32_t *id) { return 0; }
int hid_public_1(void *o, int c, const uint32_t *id) { return 0; }
int hid_public_2(void *o, int c, const uint32_t *id) { return 0; }
int hid_public_3(void *o, int c, const uint32_t *id) { return 0; }
int hid_public_4(void *o, int c, const uint32_t *id) { return 0; }

void menu_init(menu_t *m, const char *t, int cap) { (void)m; }
menu_item_t *menu_add_heading(menu_t *m, const char *l) { return 0; }
void menu_end_group(menu_t *m) {}
menu_item_t *menu_add_text(menu_t *m, void (*t)(menu_item_t *, char *, int), intptr_t d) { return 0; }
menu_item_t *menu_add_action(menu_t *m, const char *l, void (*f)(menu_item_t *), intptr_t d) { return 0; }
menu_item_t *menu_add_number(menu_t *m, const char *l, int32_t (*g)(menu_item_t *), void (*s)(menu_item_t *, int32_t),
                             int32_t (*x)(menu_item_t *), void (*f)(menu_item_t *), intptr_t d) { return 0; }
menu_item_t *menu_add_option(menu_t *m, const char *l, int n, const char *const *c, int (*g)(menu_item_t *),
                             void (*s)(menu_item_t *, int), intptr_t d) { return 0; }
menu_item_t *menu_add_return(menu_t *m) { return 0; }
menu_item_t *menu_add_check(menu_t *m, const char *l, bool (*g)(menu_item_t *), void (*s)(menu_item_t *, bool),
                            intptr_t d) { return 0; }

/* one in-memory file for export / import */
static uint8_t file_data[1 << 22];
static int64_t file_size = -1;
nn_result_t fs_create_dir(const char *p) { return 1; }
nn_result_t fs_delete_file(const char *p) { file_size = -1; return 0; }
nn_result_t fs_create_file(const char *p, int64_t size) { file_size = size; memset(file_data, 0, (size_t)size); return 0; }
nn_result_t fs_open(fs_file_t *f, const char *p, int mode) { return file_size < 0; }
nn_result_t fs_size(int64_t *out, fs_file_t f) { *out = file_size; return 0; }
nn_result_t fs_read(size_t *got, fs_file_t f, int64_t off, void *buf, size_t size)
{
  size_t n = off + (int64_t)size > file_size ? (size_t)(file_size - off) : size;
  memcpy(buf, file_data + off, n);
  *got = n;
  return 0;
}
nn_result_t fs_write(fs_file_t f, int64_t off, const void *buf, size_t size, const fs_write_option_t *o)
{
  assert(off + (int64_t)size <= file_size);
  memcpy(file_data + off, buf, size);
  return 0;
}
void fs_close(fs_file_t f) {}

/* --- the fake game -------------------------------------------------------------------------------- */
static npad_state_t pad;  /* the controller as nn::hid has it */
static npad_state_t seen; /* what the game got this frame */

static int fake_get_states(void *out, int count, const uint32_t *id)
{
  memset(out, 0, NPAD_STATE_SIZE);
  memcpy(out, &pad, sizeof(pad));
  ((npad_state_t *)out)->sampling_number++;
  return 1;
}

/* a game frame: read the controller through main's (wrapped) import, then the frame hook */
static void game_frame(void)
{
  uint8_t buf[NPAD_STATE_SIZE];
  uint32_t id = 0;
  hid_wrap_0(buf, 1, &id);
  memcpy(&seen, buf, sizeof(seen));
  macro_frame();
}

static void press(uint32_t buttons, int32_t lx)
{
  pad.buttons = buttons;
  pad.lx = lx;
  pad.attributes = NPAD_ATTR_CONNECTED;
}

int main(void)
{
  hid_next[0] = (void *)fake_get_states;

  /* recording: macro frame k is what the game got on the k-th frame after "record" */
  press(BTN_A, 0);
  game_frame();          /* before recording: not stored */
  macro_record();        /* from the menu, during a hook */
  for (int i = 0; i < 5; i++) {
    press(i % 2 ? BTN_B : BTN_A, 1000 * i);
    game_frame();
  }
  input_block_game = true; /* the menu open: the game gets nothing, and nothing is recorded */
  press(BTN_X, 30000);
  game_frame();
  input_block_game = false;
  macro_stop();
  assert(macro_length() == 6 && macro_position() == 6);
  const macro_input_t *f = macro_frames();
  for (int i = 0; i < 5; i++)
    assert(f[i].buttons == (i % 2 ? BTN_B : BTN_A) && f[i].stick[0] == 1000 * i);
  assert(f[5].buttons == 0 && f[5].stick[0] == 0);
  puts("PASS: recording stores what the game got each frame, from the frame after record, menu frames blank");

  /* frame advance's buffered input and hidden buttons are recorded as the game got them */
  macro_clear();
  macro_record();
  input_force_active = true;
  input_force_buttons = BTN_ZR;
  input_hide_from_game = BTN_L;
  press(BTN_L | BTN_A, 0);
  game_frame();
  input_force_active = false;
  input_hide_from_game = 0;
  macro_stop();
  assert(macro_length() == 1 && macro_frames()[0].buttons == (BTN_A | BTN_ZR));
  puts("PASS: frame advance's buffered input is recorded, the frame advance button isn't");

  /* playback replaces the controller, frame for frame, and wins over the menu's blanking */
  macro_clear();
  macro_record();
  int32_t sticks[4] = { 0, -32768, 32767, 12345 };
  for (int i = 0; i < 4; i++) {
    press(1u << i, sticks[i]);
    game_frame();
  }
  macro_stop();
  macro_rewind();
  press(BTN_Y, 999);       /* someone holding the controller */
  macro_play();            /* from the menu: the next game frame gets macro frame 0 */
  for (int i = 0; i < 4; i++) {
    input_block_game = i == 2;
    game_frame();
    assert(seen.buttons == (1u << i) && seen.lx == sticks[i]);
    /* the hook after the last frame's been played ends playback */
    assert(macro_state() == (i < 3 ? MACRO_PLAYING : MACRO_IDLE));
  }
  input_block_game = false;
  game_frame(); /* the controller is back */
  assert(seen.buttons == BTN_Y && seen.lx == 999);
  puts("PASS: playback gives each recorded frame to the matching game frame, then hands back the controller");

  /* merging: the controller's buttons are added; a centred macro stick leaves the controller's */
  macro_merge_input = true;
  macro_rewind();
  macro_play();
  press(BTN_PLUS, 500);
  game_frame();
  assert(seen.buttons == (BTN_A | BTN_PLUS) && seen.lx == 500);
  game_frame();
  assert(seen.buttons == (BTN_B | BTN_PLUS) && seen.lx == -32768);
  macro_stop();
  macro_merge_input = false;
  puts("PASS: add to controller merges the controller with the macro");

  /* rerecording from a frame overwrites from it; trim cuts the rest */
  macro_seek(2);
  macro_record();
  press(BTN_ZL, 0);
  game_frame();
  macro_stop();
  assert(macro_length() == 4 && macro_frames()[2].buttons == BTN_ZL && macro_frames()[3].buttons == (1u << 3));
  macro_trim();
  assert(macro_length() == 3);
  macro_seek(100);
  assert(macro_position() == 3);
  puts("PASS: seek, overwrite and trim");

  /* export / import round trip, and a bad file is refused */
  macro_merge_input = false;
  assert(macro_export(3) == 0);
  macro_clear();
  assert(macro_import(3) == 0 && macro_length() == 3);
  assert(macro_frames()[0].buttons == BTN_A && macro_frames()[1].stick[0] == -32768 && macro_frames()[2].buttons == BTN_ZL);
  file_data[8] = 99; /* a newer version */
  assert(macro_import(3) != 0);
  file_size = 10;
  assert(macro_import(3) != 0);
  macro_clear();
  assert(macro_export(0) != 0); /* empty */
  sd = false;
  assert(macro_import(0) != 0);
  sd = true;
  puts("PASS: export / import round trip; bad, newer and empty files refused");

  /* looping: frames 0-1 play once, then 2-4 over and over with no gap, until stopped */
  macro_clear();
  macro_record();
  for (int i = 0; i < 5; i++) {
    press(1u << i, 0);
    game_frame();
  }
  macro_stop();
  macro_set_loop(true, 2);
  assert(macro_loop_on() && macro_loop_from() == 2);
  macro_rewind();
  macro_play();
  int expect[] = { 0, 1, 2, 3, 4, 2, 3, 4, 2, 3, 4, 2 };
  for (int i = 0; i < (int)(sizeof(expect) / sizeof(*expect)); i++) {
    game_frame();
    assert(seen.buttons == 1u << expect[i]);
    assert(macro_state() == MACRO_PLAYING);
  }
  macro_stop();
  /* the loop is kept in the file; an old file (0 there) has none */
  assert(macro_export(4) == 0);
  macro_clear();
  assert(!macro_loop_on());
  assert(macro_import(4) == 0 && macro_loop_on() && macro_loop_from() == 2);
  file_data[20] = 0;
  assert(macro_import(4) == 0 && !macro_loop_on());
  file_data[20] = 9; /* past the end */
  assert(macro_import(4) == 0 && !macro_loop_on());
  /* trimming at or before the loop's first frame turns it off; a loop past the end can't be set */
  macro_set_loop(true, 2);
  macro_seek(3);
  macro_trim();
  assert(macro_loop_on());
  macro_seek(2);
  macro_trim();
  assert(!macro_loop_on());
  macro_set_loop(true, 5);
  assert(!macro_loop_on());
  /* a one-frame loop at the end */
  macro_set_loop(true, 1);
  macro_rewind();
  macro_play();
  for (int i = 0; i < 4; i++) {
    game_frame();
    assert(seen.buttons == (i == 0 ? 1u : 2u));
  }
  macro_stop();
  puts("PASS: looping from a frame, kept in the file, turned off by trim");

  /* growth past the first allocation */
  macro_clear();
  macro_record();
  for (int i = 0; i < FIRST_CAPACITY * 3; i++) {
    press((uint32_t)i & 0xFFFF, 0);
    game_frame();
  }
  macro_stop();
  assert(macro_length() == FIRST_CAPACITY * 3);
  for (int i = 0; i < FIRST_CAPACITY * 3; i++)
    assert(macro_frames()[i].buttons == ((uint32_t)i & 0xFFFF));
  puts("PASS: a long recording grows its buffer and keeps every frame");
  return 0;
}
