/* The game's clock (main's nn::os::GetSystemTick), stopped while frame advance holds the game. */
#ifndef GAMECLOCK_H
#define GAMECLOCK_H

#include <stdbool.h>
#include <stdint.h>

/* settings > frame advance > stop the clock: on, the game's clock stands still while it's held and moves
 * on by one frame's time for each frame let through */
extern bool game_clock_stops;

/* The game is held from now: its clock stands still. */
void game_clock_hold(void);
/* The game is let go: its clock goes on from where it stood, at least a frame (1/60 s) on from where it
 * was when the game was last let go, so the frame let through takes a frame's time to it. */
void game_clock_release(void);
/* Whether the game's clock is standing still now (frame advance holds the game, with the setting on). */
bool game_clock_held(void);
/* How many times main read the clock since the last call (frame advance diagnostics). */
uint32_t game_clock_reads_take(void);
/* Point main's nn::os::GetSystemTick import at the wrapper (module init); returns 0 or a message. */
const char *game_clock_hook(uintptr_t main_base);

#endif
