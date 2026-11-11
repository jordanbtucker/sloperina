#include "gameclock.h"

#include "got.h"
#include "sys.h"

/* The game reads the time from nn::os::GetSystemTick: a tick count (19.2 MHz) it can compare frames by.
 * Frame advance holds the game's thread in the frame hook for as long as it's paused, and the game's
 * clock-timed parts (menus opening, the game's own pauses taking effect...) would count that time as
 * having played: a step after a long pause would play them on by many frames at once. So the clock main
 * sees is the system's less the time it was held (and, while held, stands still).
 * nn::os::Tick is a class holding one int64_t, returned in x0. */
#define GETSYSTEMTICK "_ZN2nn2os13GetSystemTickEv"
typedef uint64_t (*get_tick_t)(void);
static void *next_get_tick;

/* What GetSystemTick does (the counter register), for when main hasn't bound its import yet: not
 * imported from sdk, so the module doesn't need it in lib/imports.txt. */
static uint64_t system_tick(void)
{
  return sys_tick();
}

bool game_clock_stops = true;

static volatile uint64_t offset;     /* game's clock = system's - offset */
static volatile uint64_t stood_at;   /* the game's clock while held */
static volatile bool held;
static uint64_t released_game_time;  /* the game's clock when it was last let go */
static volatile uint32_t reads;

static uint64_t real_tick(void)
{
  return ((get_tick_t)next_get_tick)();
}

static uint64_t game_tick(void)
{
  __atomic_add_fetch(&reads, 1, __ATOMIC_RELAXED);
  if (__atomic_load_n(&held, __ATOMIC_ACQUIRE))
    return stood_at;
  return real_tick() - __atomic_load_n(&offset, __ATOMIC_ACQUIRE);
}

/* Stopping main's GetSystemTick didn't change how much a frame let through plays (the game times its
 * frames by something else), so it's off: the wrapper only counts reads. timefinder.c does the stopping. */
#define FREEZE_SYSTEM_TICK 0

void game_clock_hold(void)
{
  if (!FREEZE_SYSTEM_TICK || !next_get_tick || !game_clock_stops || held)
    return;
  stood_at = real_tick() - offset;
  __atomic_store_n(&held, true, __ATOMIC_RELEASE);
}

void game_clock_release(void)
{
  if (!next_get_tick)
    return;
  uint64_t frame = sys_tick_freq() / GAME_FPS;
  if (held) {
    /* on from where it stood, at least a frame on from the last release */
    uint64_t game_time = stood_at;
    if (released_game_time && game_time - released_game_time < frame)
      game_time = released_game_time + frame;
    __atomic_store_n(&offset, real_tick() - game_time, __ATOMIC_RELEASE);
    __atomic_store_n(&held, false, __ATOMIC_RELEASE);
  }
  released_game_time = real_tick() - offset;
}

bool game_clock_held(void)
{
  return __atomic_load_n(&held, __ATOMIC_ACQUIRE);
}

uint32_t game_clock_reads_take(void)
{
  return __atomic_exchange_n(&reads, 0, __ATOMIC_RELAXED);
}

const char *game_clock_hook(uintptr_t main_base)
{
  return got_hook(main_base, GETSYSTEMTICK, (void *)game_tick, (void *)system_tick, &next_get_tick);
}
