/* Frame advance: find where the game keeps the time of its last frame, and move it on while paused.
 *
 * A frame let through by frame advance plays 3-5 frames of physics: the game times its frames by the time
 * since the last one, and none of its calls that were hooked (nn::os::GetSystemTick, ConvertToTimeSpan,
 * the event waits, the display's vsync) is where it gets that from, so it presumably reads the timer
 * register itself. What it keeps, though, is in memory: a timestamp that moves on by a frame each frame.
 * This finds such values (as timer ticks, as nanoseconds, or as seconds in a double), and when frame
 * advance lets a frame through, moves them on by the time it was held, so the game sees one frame's time. */
#ifndef TIMEFINDER_H
#define TIMEFINDER_H

#include <stdbool.h>
#include <stdint.h>

/* Once a frame from the frame hook, while the game runs (not held): scans memory a slice at a time once,
 * then checks what it found. */
void timefinder_frame(void);
/* Moves what was found on by `ticks` (timer ticks; negative moves it back), so the game's next frame
 * time is that much shorter. */
void timefinder_shift(int64_t ticks);
/* How far the found values moved since the last call, as text for the log ("" if none found yet). */
void timefinder_take(char *buf, int len);

#endif
