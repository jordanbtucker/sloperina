#include "timeprobe.h"

#include "gameclock.h"
#include "got.h"
#include "sys.h"
#include "z64hd.h"

/* main binds its imports on first call; until then its GOT slot points at main's own PLT, which a wrapper
 * mustn't call (got.h). So each is hooked from the frame hook once it's bound, with no definition of our
 * own to fall back on (none of these is in lib/imports.txt). */
typedef struct {
  const char *name;  /* mangled */
  const char *label; /* in the log */
  void *wrapper;
  void *next;
  bool hooked, missing;
  volatile uint32_t calls;
  volatile int64_t max;     /* the largest argument (ConvertToTimeSpan: ticks; SleepThread: ns) */
  volatile uintptr_t object; /* the event / object waited on most recently */
  volatile uint32_t signalled; /* waits that returned true */
} probe_t;

enum { P_TO_TIMESPAN, P_SLEEP, P_WAIT_SYS, P_TRYWAIT_SYS, P_TIMEDWAIT_SYS, P_TIMEDWAIT_LIGHT, P_WAIT_LIGHT,
       P_COUNT };

static int64_t w_to_timespan(int64_t tick);
static void w_sleep(int64_t ns);
static void w_wait_sys(void *event);
static bool w_trywait_sys(void *event);
static bool w_timedwait_sys(void *event, int64_t ns);
static bool w_timedwait_light(void *event, int64_t ns);
static void w_wait_light(void *event);

static probe_t probes[P_COUNT] = {
  [P_TO_TIMESPAN] = { "_ZN2nn2os17ConvertToTimeSpanENS0_4TickE", "totimespan", (void *)w_to_timespan }, /* not menu text */
  [P_SLEEP] = { "_ZN2nn2os11SleepThreadENS_8TimeSpanE", "sleep", (void *)w_sleep }, /* not menu text */
  [P_WAIT_SYS] = { "_ZN2nn2os15WaitSystemEventEPNS0_15SystemEventTypeE", "waitsys", (void *)w_wait_sys }, /* not menu text */
  [P_TRYWAIT_SYS] = { "_ZN2nn2os18TryWaitSystemEventEPNS0_15SystemEventTypeE", "trywaitsys", (void *)w_trywait_sys }, /* not menu text */
  [P_TIMEDWAIT_SYS] = { "_ZN2nn2os20TimedWaitSystemEventEPNS0_15SystemEventTypeENS_8TimeSpanE", "timedwaitsys", /* not menu text */
                        (void *)w_timedwait_sys }, /* not menu text */
  [P_TIMEDWAIT_LIGHT] = { "_ZN2nn2os19TimedWaitLightEventEPNS0_14LightEventTypeENS_8TimeSpanE", "timedwaitlight", /* not menu text */
                          (void *)w_timedwait_light }, /* not menu text */
  [P_WAIT_LIGHT] = { "_ZN2nn2os14WaitLightEventEPNS0_14LightEventTypeE", "waitlight", (void *)w_wait_light }, /* not menu text */
};

static void note(probe_t *p, int64_t value, void *object)
{
  __atomic_add_fetch(&p->calls, 1, __ATOMIC_RELAXED);
  if (value > p->max)
    p->max = value;
  if (object)
    p->object = (uintptr_t)object;
}

static int64_t w_to_timespan(int64_t tick)
{
  probe_t *p = &probes[P_TO_TIMESPAN];
  note(p, tick, 0);
  return ((int64_t (*)(int64_t))p->next)(tick);
}

static void w_sleep(int64_t ns)
{
  probe_t *p = &probes[P_SLEEP];
  note(p, ns, 0);
  ((void (*)(int64_t))p->next)(ns);
}

/* The display's refresh (vsync) event, as far as can be told from outside: the system event main waits on
 * most, with waits that block for a while (a refresh is 16.7 ms apart). The game seems to count refreshes
 * to time its frames (a step of frame advance played 3-4 frames of physics), so while frame advance holds
 * the game (with "stop the clock" on), whoever waits on it is held too, and the refreshes that came
 * meanwhile are dropped: a frame let through then sees one refresh. */
#define WAIT_OBJECTS 8
static struct { volatile uintptr_t object; volatile uint32_t count; volatile uint64_t ticks; } waits[WAIT_OBJECTS];
static volatile uintptr_t vsync_event;
static volatile uint32_t vsync_held; /* waits held back, for the log */

static void note_wait(void *event, uint64_t ticks)
{
  for (int i = 0; i < WAIT_OBJECTS; i++) {
    uintptr_t object = waits[i].object;
    if (object == (uintptr_t)event || (!object && __atomic_compare_exchange_n(&waits[i].object, &object,
                                                                             (uintptr_t)event, false,
                                                                             __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))) {
      __atomic_add_fetch(&waits[i].count, 1, __ATOMIC_RELAXED);
      __atomic_add_fetch(&waits[i].ticks, ticks, __ATOMIC_RELAXED);
      return;
    }
    if (object == (uintptr_t)event)
      return;
  }
}

static void w_wait_sys(void *event)
{
  probe_t *p = &probes[P_WAIT_SYS];
  note(p, 0, event);
  if (vsync_event && (uintptr_t)event == vsync_event && game_clock_held()) {
    __atomic_add_fetch(&vsync_held, 1, __ATOMIC_RELAXED);
    while (game_clock_held())
      sys_sleep_ns(500 * 1000);
    if (probes[P_TRYWAIT_SYS].next) /* the refresh signalled while held: not the frame let through's */
      ((bool (*)(void *))probes[P_TRYWAIT_SYS].next)(event);
  }
  uint64_t start = sys_tick();
  ((void (*)(void *))p->next)(event);
  note_wait(event, sys_tick() - start);
}

/* After a few seconds of waits, take the most waited-on event whose waits block (2 ms or more on average)
 * as the refresh event. */
static void pick_vsync_event(void)
{
  int best = -1;
  for (int i = 0; i < WAIT_OBJECTS; i++) {
    uint32_t count = waits[i].count;
    if (!waits[i].object || count < 60)
      continue;
    double average_ms = (double)waits[i].ticks * 1000.0 / (double)sys_tick_freq() / count;
    log_printf("time probe: system event @%llx waited %u times, %.2f ms on average", /* not menu text */
               (unsigned long long)(waits[i].object & 0xFFFFFF), (unsigned)count, average_ms);
    if (average_ms >= 2.0 && (best < 0 || count > waits[best].count))
      best = i;
  }
  if (best >= 0) {
    vsync_event = waits[best].object;
    log_printf("time probe: taking @%llx as the refresh event", /* not menu text */
               (unsigned long long)(vsync_event & 0xFFFFFF));
  }
}

static bool w_trywait_sys(void *event)
{
  probe_t *p = &probes[P_TRYWAIT_SYS];
  note(p, 0, event);
  bool got = ((bool (*)(void *))p->next)(event);
  if (got)
    __atomic_add_fetch(&p->signalled, 1, __ATOMIC_RELAXED);
  return got;
}

static bool w_timedwait_sys(void *event, int64_t ns)
{
  probe_t *p = &probes[P_TIMEDWAIT_SYS];
  note(p, ns, event);
  bool got = ((bool (*)(void *, int64_t))p->next)(event, ns);
  if (got)
    __atomic_add_fetch(&p->signalled, 1, __ATOMIC_RELAXED);
  return got;
}

static bool w_timedwait_light(void *event, int64_t ns)
{
  probe_t *p = &probes[P_TIMEDWAIT_LIGHT];
  note(p, ns, event);
  bool got = ((bool (*)(void *, int64_t))p->next)(event, ns);
  if (got)
    __atomic_add_fetch(&p->signalled, 1, __ATOMIC_RELAXED);
  return got;
}

static void w_wait_light(void *event)
{
  probe_t *p = &probes[P_WAIT_LIGHT];
  note(p, 0, event);
  ((void (*)(void *))p->next)(event);
}

void timeprobe_frame(void)
{
  static uint32_t frames;
  if (++frames == 600 && probes[P_WAIT_SYS].hooked)
    pick_vsync_event();
  static uint32_t tries;
  if (tries > 3600)
    return;
  tries++;
  for (int i = 0; i < P_COUNT; i++) {
    probe_t *p = &probes[i];
    if (p->hooked || p->missing)
      continue;
    const char *error = got_hook(z64hd_main, p->name, p->wrapper, 0, &p->next);
    if (!error) {
      p->hooked = true;
      log_printf("time probe: %s hooked", p->label); /* not menu text */
    } else if (error[0] == 'i' && error[1] == 'm') { /* "import not found" */
      p->missing = true;
      log_printf("time probe: main doesn't import %s", p->label); /* not menu text */
    }
  }
}

void timeprobe_take(char *buf, int len)
{
  int n = 0;
  buf[0] = 0;
  uint32_t held = __atomic_exchange_n(&vsync_held, 0, __ATOMIC_RELAXED);
  if (held)
    n += snprintf(buf, len, " vsync waits held %u", (unsigned)held); /* not menu text */
  for (int i = 0; i < P_COUNT; i++) { /* every probe is cleared, whatever fits in buf */
    probe_t *p = &probes[i];
    if (!p->hooked)
      continue;
    uint32_t calls = __atomic_exchange_n(&p->calls, 0, __ATOMIC_RELAXED);
    int64_t max = p->max;
    p->max = 0;
    uint32_t signalled = __atomic_exchange_n(&p->signalled, 0, __ATOMIC_RELAXED);
    if (!calls || n >= len)
      continue;
    n += snprintf(buf + n, len - n, " %s %u", p->label, (unsigned)calls); /* not menu text */
    if (n < len && max)
      n += snprintf(buf + n, len - n, " max %.2fms", /* not menu text */
                    i == P_TO_TIMESPAN ? (double)max * 1000.0 / (double)sys_tick_freq() : (double)max / 1e6);
    if (n < len && p->object)
      n += snprintf(buf + n, len - n, " @%llx ok %u", (unsigned long long)(p->object & 0xFFFFFF), /* not menu text */
                    (unsigned)signalled);
  }
}
