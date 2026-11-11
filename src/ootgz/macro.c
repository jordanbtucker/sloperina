/* gz's macros (macro.h). How a frame lines up:
 *
 * The frame hook runs once per game frame, after the game has read the controller for that frame
 * (input.c's wrappers see every read). So at hook N, input_take_game_seen gives what the game was given
 * for frame N: recording stores it as the next macro frame. Playing, hook N hands input.c the macro
 * frame for frame N + 1; starting playback (from the menu, during a hook) hands over the first frame at
 * once. Recorded and played frames are the same frames of the game, if the game reads the controller
 * before the hook each frame (which the log line "macro: the game read the controller ..." checks).
 *
 * With frame advance, the game is held inside the hook: nothing is stored or played while it's held, and
 * the frame let through is recorded with the buffered input it was given (input.c adds that before
 * noting what the game got).
 *
 * The menu blanks the game's input while it's open (input_block_game): recording with the menu open
 * stores blank frames. A playing macro wins over the menu, so it plays back the same blank frames. */
#include "macro.h"

#include "gfx.h"
#include "input.h"
#include "lang.h"
#include "sdfs.h"
#include "settings.h"

void *aligned_alloc(size_t align, size_t size); /* from main */
void free(void *p);                             /* from main */

#define FIRST_CAPACITY (60 * GAME_FPS) /* a minute; doubled as needed */
#define MACRO_DIR SD_DIR "/macros"
#define READS_LOG_FRAMES GAME_FPS /* recording: how often the game read input, logged after this many frames */

bool macro_merge_input;

static macro_state_t state;
static macro_input_t *frames;
static uint32_t capacity, length, position;
static bool loop_on;
static uint32_t loop_from;
static int slot;
/* recording's check of how the game reads the controller (logged once per recording) */
static uint32_t check_frames, check_reads, check_empty;
static bool check_logged;
static const char *const state_names_short[] = { "idle", "rec", "play" }; /* not menu text */

macro_state_t macro_state(void) { return state; }
uint32_t macro_position(void) { return position; }
uint32_t macro_length(void) { return length; }
const macro_input_t *macro_frames(void) { return frames; }

/* room for at least `want` frames (up to MACRO_MAX_FRAMES); false if there isn't */
static bool reserve(uint32_t want)
{
  if (want <= capacity)
    return true;
  if (want > MACRO_MAX_FRAMES)
    return false;
  uint32_t grown = capacity ? capacity : FIRST_CAPACITY;
  while (grown < want)
    grown *= 2;
  if (grown > MACRO_MAX_FRAMES)
    grown = MACRO_MAX_FRAMES;
  macro_input_t *bigger = aligned_alloc(16, (size_t)grown * sizeof(macro_input_t));
  if (!bigger)
    return false;
  if (frames) {
    memcpy(bigger, frames, (size_t)length * sizeof(macro_input_t));
    free(frames);
  }
  frames = bigger;
  capacity = grown;
  return true;
}

static int16_t to_axis(int32_t v)
{
  return (int16_t)(v > INT16_MAX ? INT16_MAX : v < INT16_MIN ? INT16_MIN : v);
}

static void give_to_game(const macro_input_t *in)
{
  input_macro_buttons = in->buttons;
  for (int i = 0; i < 4; i++)
    input_macro_stick[i] = in->stick[i];
  input_macro_merge = macro_merge_input;
  input_macro_active = true;
}

static void stop_playing(void)
{
  input_macro_active = false;
  input_macro_buttons = 0;
  memset(input_macro_stick, 0, sizeof(input_macro_stick));
}

void macro_stop(void)
{
  if (state == MACRO_RECORDING)
    log_printf("macro: recorded up to frame %u (%u frames)", (unsigned)position, (unsigned)length);
  stop_playing();
  state = MACRO_IDLE;
}

void macro_record(void)
{
  uint32_t ignored_buttons;
  int32_t ignored_stick[4];
  macro_stop();
  input_take_game_seen(&ignored_buttons, ignored_stick); /* what the game read before now isn't recorded */
  check_frames = check_reads = check_empty = 0;
  check_logged = false;
  state = MACRO_RECORDING;
}

void macro_play(void)
{
  macro_stop();
  if (position >= length) {
    gfx_notice("macro: nothing to play from this frame");
    return;
  }
  state = MACRO_PLAYING;
  give_to_game(&frames[position++]); /* the next frame the game plays */
}

void macro_rewind(void)
{
  macro_seek(0);
}

void macro_seek(uint32_t frame)
{
  position = frame > length ? length : frame;
  if (state == MACRO_PLAYING) {
    if (position < length)
      give_to_game(&frames[position++]);
    else
      macro_stop();
  }
}

bool macro_loop_on(void) { return loop_on; }
uint32_t macro_loop_from(void) { return loop_from; }

void macro_set_loop(bool on, uint32_t from)
{
  loop_from = from;
  loop_on = on && from < length;
}

void macro_trim(void)
{
  length = position;
  if (loop_from >= length)
    loop_on = false;
}

void macro_clear(void)
{
  macro_stop();
  length = position = 0;
  loop_on = false;
  loop_from = 0;
}

static uint32_t trace_lines, trace_hooks;

static bool trace_room(void)
{
  return MACRO_TRACE && trace_lines < MACRO_TRACE_LINES && (trace_lines++, true);
}

void macro_trace_note(const char *what)
{
  if (trace_room())
    log_printf("trace hook %u: %s", (unsigned)trace_hooks, what);
}

/* one line per hook: what the game read since the last one, and what it's given next */
static void trace_frame(int reads, uint32_t buttons, const int32_t stick[4])
{
  trace_hooks++;
  if (!trace_room())
    return;
  log_printf("trace hook %u: %s pos %u, read %d: %x (%d,%d) next %x (%d,%d)%s", (unsigned)trace_hooks,
             state_names_short[state], (unsigned)position, reads, (unsigned)buttons, (int)stick[0], (int)stick[1],
             (unsigned)input_macro_buttons, (int)input_macro_stick[0], (int)input_macro_stick[1],
             input_macro_active ? "" : " (off)"); /* not menu text */
}

static void record_frame(int reads, uint32_t buttons, const int32_t stick[4])
{
  if (!check_logged) {
    check_frames++;
    check_reads += (uint32_t)reads;
    check_empty += reads == 0;
    if (check_frames == READS_LOG_FRAMES) {
      check_logged = true;
      log_printf("macro: the game read the controller %u times in %u frames (%u frames without a read)",
                 (unsigned)check_reads, (unsigned)check_frames, (unsigned)check_empty);
    }
  }
  if (!reserve(position + 1)) {
    macro_stop();
    gfx_notice("macro: full, recording stopped");
    return;
  }
  macro_input_t *in = &frames[position++];
  in->buttons = buttons;
  for (int i = 0; i < 4; i++)
    in->stick[i] = to_axis(stick[i]);
  if (position > length)
    length = position;
}

void macro_frame(void)
{
  uint32_t buttons;
  int32_t stick[4];
  int reads = input_take_game_seen(&buttons, stick); /* taken every frame: nothing builds up while idle */
  if (state == MACRO_RECORDING) {
    record_frame(reads, buttons, stick);
  } else if (state == MACRO_PLAYING) {
    if (position >= length && loop_on && loop_from < length)
      position = loop_from; /* the loop: on from its first frame, with no gap */
    if (position < length) {
      give_to_game(&frames[position++]);
    } else {
      macro_stop();
      gfx_notice("macro: finished");
    }
  }
  if (MACRO_TRACE)
    trace_frame(reads, buttons, stick);
}

void macro_frame_released(void)
{
  uint32_t ignored_buttons;
  int32_t ignored_stick[4];
  input_take_game_seen(&ignored_buttons, ignored_stick);
}

/* --- files ----------------------------------------------------------------------------------------- */

static void put32(uint8_t *p, uint32_t v)
{
  p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p)
{
  return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

int64_t macro_parse_header(const uint8_t *data, uint64_t size)
{
  if (size < MACRO_HEADER_SIZE)
    return -1;
  for (int i = 0; i < 8; i++)
    if (data[i] != (uint8_t)MACRO_MAGIC[i])
      return -1;
  uint32_t count = get32(data + 12);
  if (get32(data + 8) != MACRO_FILE_VERSION || get32(data + 16) != sizeof(macro_input_t) ||
      count > MACRO_MAX_FRAMES || size < MACRO_HEADER_SIZE + (uint64_t)count * sizeof(macro_input_t))
    return -1;
  return count;
}

bool macro_parse_loop(const uint8_t *data, uint32_t count, uint32_t *from)
{
  uint32_t loop = get32(data + 20);
  if (loop == 0 || loop > count)
    return false;
  *from = loop - 1;
  return true;
}

static void slot_path(char *buf, size_t len, int n)
{
  snprintf(buf, len, MACRO_DIR "/macro%d.sgm", n);
}

/* The frames are written as they are in memory: little-endian, as the console and every PC
 * emulator are. */
const char *macro_export(int n)
{
  if (!settings_sd())
    return "no SD card access";
  if (!length)
    return "the macro is empty";
  char path[96];
  slot_path(path, sizeof(path), n);
  uint8_t header[MACRO_HEADER_SIZE] = { 0 };
  memcpy(header, MACRO_MAGIC, 8);
  put32(header + 8, MACRO_FILE_VERSION);
  put32(header + 12, length);
  put32(header + 16, sizeof(macro_input_t));
  put32(header + 20, loop_on ? loop_from + 1 : 0);
  int64_t body = (int64_t)length * sizeof(macro_input_t);
  fs_create_dir(SD_DIR);
  fs_create_dir(MACRO_DIR); /* fails harmlessly if it exists */
  fs_delete_file(path);
  fs_file_t file;
  if (fs_create_file(path, MACRO_HEADER_SIZE + body) != 0 || fs_open(&file, path, FS_WRITE) != 0)
    return "couldn't create the file";
  fs_write_option_t none = { 0 }, flush = { FS_WRITE_FLUSH };
  nn_result_t result = fs_write(file, 0, header, MACRO_HEADER_SIZE, &none);
  if (result == 0)
    result = fs_write(file, MACRO_HEADER_SIZE, frames, (size_t)body, &flush);
  fs_close(file);
  if (result != 0)
    return "couldn't write the file";
  log_printf("macro: exported %u frames to %s", (unsigned)length, path);
  return 0;
}

const char *macro_import(int n)
{
  if (!settings_sd())
    return "no SD card access";
  char path[96];
  slot_path(path, sizeof(path), n);
  fs_file_t file;
  if (fs_open(&file, path, FS_READ) != 0)
    return "no macro in this slot";
  int64_t size = 0;
  fs_size(&size, file);
  uint8_t header[MACRO_HEADER_SIZE];
  size_t got = 0;
  if (size < MACRO_HEADER_SIZE || fs_read(&got, file, 0, header, sizeof(header)) != 0 || got != sizeof(header)) {
    fs_close(file);
    return "not a macro file";
  }
  int64_t count = macro_parse_header(header, (uint64_t)size);
  if (count < 0) {
    fs_close(file);
    return "not a macro file (or a newer one)";
  }
  macro_clear();
  if (!reserve((uint32_t)count)) {
    fs_close(file);
    return "not enough memory";
  }
  size_t body = (size_t)count * sizeof(macro_input_t);
  got = 0;
  nn_result_t result = body ? fs_read(&got, file, MACRO_HEADER_SIZE, frames, body) : 0;
  fs_close(file);
  if (result != 0 || got != body)
    return "couldn't read the file";
  length = (uint32_t)count;
  position = 0;
  uint32_t from;
  if (macro_parse_loop(header, length, &from))
    macro_set_loop(true, from);
  log_printf("macro: imported %u frames from %s", (unsigned)length, path);
  return 0;
}

/* --- menu (macro) ---------------------------------------------------------------------------------- */

menu_t macro_menu;

static const char *const state_names[] = { "stopped", "recording", "playing" };
static const char *const input_mode_names[] = { "replace controller", "add to controller" };

static void text_status(menu_item_t *it, char *buf, int len)
{
  snprintf(buf, len, TF("%s  frame %u / %u"), state_names[state], (unsigned)position, (unsigned)length);
}

static void act_record(menu_item_t *it)
{
  macro_record();
  gfx_notice("macro: recording (close the menu to give the game input)");
}

static void act_play(menu_item_t *it)
{
  macro_play();
  if (state == MACRO_PLAYING)
    gfx_notice("macro: playing");
}

static void act_stop(menu_item_t *it)
{
  macro_stop();
  gfx_notice("macro: stopped");
}

static void act_rewind(menu_item_t *it)
{
  macro_rewind();
}

static void act_trim(menu_item_t *it)
{
  macro_trim();
  gfx_notice("macro: trimmed after this frame");
}

static void act_clear(menu_item_t *it)
{
  macro_clear();
  gfx_notice("macro: cleared");
}

static int32_t get_frame(menu_item_t *it) { return (int32_t)position; }
static void set_frame(menu_item_t *it, int32_t v) { macro_seek(v < 0 ? 0 : (uint32_t)v); }
static int32_t max_frame(menu_item_t *it) { return (int32_t)length; }

static bool get_loop(menu_item_t *it) { return loop_on; }

static void set_loop(menu_item_t *it, bool on)
{
  macro_set_loop(on, loop_from);
  if (on && !loop_on)
    gfx_notice("macro: the loop's first frame is past the end");
}

static int32_t get_loop_from(menu_item_t *it) { return (int32_t)loop_from; }
static void set_loop_from(menu_item_t *it, int32_t v) { macro_set_loop(loop_on, v < 0 ? 0 : (uint32_t)v); }
static int32_t max_loop_from(menu_item_t *it) { return length ? (int32_t)length - 1 : 0; }

/* L on "loop from": the loop starts at the current macro frame, and looping is on */
static void loop_here(menu_item_t *it)
{
  macro_set_loop(true, position);
  gfx_notice(loop_on ? "macro: loops from this frame" : "macro: the loop's first frame is past the end");
}

static int32_t get_slot(menu_item_t *it) { return slot; }
static void set_slot(menu_item_t *it, int32_t v) { slot = v < 0 ? 0 : v >= MACRO_SLOTS ? MACRO_SLOTS - 1 : v; }
static int32_t max_slot(menu_item_t *it) { return MACRO_SLOTS - 1; }

static void act_export(menu_item_t *it)
{
  const char *error = macro_export(slot);
  gfx_notice(error ? error : "macro: exported");
}

static void act_import(menu_item_t *it)
{
  const char *error = macro_import(slot);
  gfx_notice(error ? error : "macro: imported");
}

static int get_input_mode(menu_item_t *it) { return macro_merge_input; }

static void set_input_mode(menu_item_t *it, int choice)
{
  macro_merge_input = choice != 0;
  input_macro_merge = macro_merge_input;
}

void macro_build_menu(void)
{
  menu_t *m = &macro_menu;
  menu_init(m, "macro", 20);
  menu_add_heading(m, "macro");
  menu_add_text(m, text_status, 0);
  menu_item_t *it = menu_add_action(m, "record", act_record, 0);
  if (it)
    it->hint = "records the game's input from this frame on (frame advance: a frame at a time)";
  it = menu_add_action(m, "play", act_play, 0);
  if (it)
    it->hint = "plays from this frame; start where the recording started for it to match";
  menu_add_action(m, "stop", act_stop, 0);
  it = menu_add_number(m, "macro frame", get_frame, set_frame, max_frame, 0, 0);
  if (it)
    it->hint = "d-left / d-right: go to another frame (recording overwrites from it)";
  menu_add_action(m, "rewind", act_rewind, 0);
  it = menu_add_action(m, "trim", act_trim, 0);
  if (it)
    it->hint = "forgets every frame after this one";
  menu_add_action(m, "clear", act_clear, 0);
  it = menu_add_check(m, "loop", get_loop, set_loop, 0);
  if (it)
    it->hint = "at the end, playback goes on from the loop's first frame until it's stopped";
  it = menu_add_number(m, "loop from", get_loop_from, set_loop_from, max_loop_from, loop_here, 0);
  if (it)
    it->hint = "the loop's first frame (the frames before it play once)   L: the current frame";
  menu_end_group(m);
  menu_add_heading(m, "file");
  it = menu_add_number(m, "slot", get_slot, set_slot, max_slot, 0, 0);
  if (it)
    it->hint = "sd:/sloperinaGZ/macros/macro<slot>.sgm";
  menu_add_action(m, "export", act_export, 0);
  menu_add_action(m, "import", act_import, 0);
  menu_end_group(m);
  menu_add_heading(m, "options");
  it = menu_add_option(m, "macro input", 2, input_mode_names, get_input_mode, set_input_mode, 0);
  if (it)
    it->hint = "while playing: the controller is ignored, or its input is added to the macro's";
  menu_end_group(m);
  menu_add_return(m);
}
