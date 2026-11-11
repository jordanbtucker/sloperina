/* gz's macros: record the input the game is given, a frame at a time, and play it back (input.c gives
 * it to the game in place of the controller). With frame advance, a frame-perfect input sequence can be
 * built a frame at a time and replayed. Kept in memory; exported to / imported from the SD card
 * (sd:/sloperinaGZ/macros/macro<slot>.sgm). The menu is macro > ... (macro_menu). */
#ifndef MACRO_H
#define MACRO_H

#include <stdbool.h>
#include <stdint.h>

#include "menu.h"
#include "sys.h"

typedef enum { MACRO_IDLE, MACRO_RECORDING, MACRO_PLAYING } macro_state_t;

/* One frame of input as the game was given it: the buttons (input.h's BTN_* bits, the sticks'
 * direction bits too) and the sticks, -32768..32767. */
typedef struct {
  uint32_t buttons;
  int16_t stick[4]; /* lx, ly, rx, ry */
} macro_input_t;

#define MACRO_MAX_FRAMES (60 * 60 * GAME_FPS) /* an hour of frames (about 2.6 MB) */
#define MACRO_SLOTS 10

/* 1: log every frame hook's input (what the game read, what a macro gave it) and frame advance's steps,
 * to check how the game's frames line up with the hook (make OPTIONS=MACRO_TRACE=1). */
#ifndef MACRO_TRACE
#define MACRO_TRACE 0
#endif
#define MACRO_TRACE_LINES 3000 /* the most trace lines a game start writes */
void macro_trace_note(const char *what); /* a frame advance step, in the trace */

/* settings > macro input: false (default) a playing macro replaces the controller; true it's added */
extern bool macro_merge_input;

extern menu_t macro_menu;
void macro_build_menu(void);

/* Frame hook, right after input_update and before the menu: stores the frame the game just played
 * (recording) or hands the game the next one (playing). */
void macro_frame(void);
/* Frame advance let a frame through: what the game read while it was held isn't that frame's. */
void macro_frame_released(void);

macro_state_t macro_state(void);
void macro_record(void);   /* record from the current frame on (overwriting what's after it) */
void macro_play(void);     /* play from the current frame */
void macro_stop(void);
void macro_rewind(void);   /* the current frame to 0 */
void macro_trim(void);     /* forget everything from the current frame on */
void macro_clear(void);
uint32_t macro_position(void);
uint32_t macro_length(void);
void macro_seek(uint32_t frame);
const macro_input_t *macro_frames(void);

/* The file's layout (little-endian): MACRO_MAGIC, then u32 version, u32 frames, u32 frame size
 * (sizeof(macro_input_t)), u32 0, then the frames. Returns 0 or what went wrong. */
#define MACRO_MAGIC "SGZMACRO"
#define MACRO_FILE_VERSION 1
#define MACRO_HEADER_SIZE 24
const char *macro_export(int slot);
const char *macro_import(int slot);
/* checks a file's header: its frame count, or -1 if it isn't a macro this build can read */
int64_t macro_parse_header(const uint8_t *data, uint64_t size);

#endif
