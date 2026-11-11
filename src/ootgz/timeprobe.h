/* Frame advance diagnostics: which of main's time and wait imports the game calls each frame, and with
 * what, to find what it times its frames by (a step of frame advance plays as long a frame as the time
 * since the last step, and main's nn::os::GetSystemTick isn't it). */
#ifndef TIMEPROBE_H
#define TIMEPROBE_H

#include <stdint.h>

/* Once a frame (from the frame hook): hooks main's imports once main has bound them. */
void timeprobe_frame(void);
/* What was called since the last call, as text for a log line ("" if nothing is hooked). */
void timeprobe_take(char *buf, int len);

#endif
