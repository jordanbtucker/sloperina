#include "timefinder.h"

#include "gfx.h"
#include "sys.h"

#define MEM_TYPE_MASK       0xFF
#define MEM_TYPE_CODE_DATA  0x04 /* a module's .data / .bss */
#define MEM_ATTR_LOCKED     1

#define START_AFTER_FRAMES  900              /* let the game settle first */
#define SCAN_BUDGET         (8u << 20)       /* bytes read a frame */
#define WINDOW_MS           150              /* "now": within the last this many ms */
#define MAX_HITS            1024
#define STALE_CHECKS        4                /* a hit that hasn't changed in this many frames is dropped */
#define CHECK_FRAMES        40               /* frames each hit is followed for */
#define RETRY_AFTER_FRAMES  300              /* after finding nothing, look again this much later */
#define MAX_FOUND           16

typedef enum { D_TICKS, D_NS, D_US, D_SECONDS, D_COUNT } domain_t;
static const char *const domain_names[] = { "ticks", "ns", "us", "s" }; /* not menu text */

typedef struct {
  uintptr_t addr;
  domain_t domain;
  uint64_t last;      /* the value at the last frame (a double's bits for D_SECONDS) */
  uint32_t framely;   /* frames it moved on by about one frame */
  uint32_t still;     /* frames in a row it hasn't changed */
} hit_t;

bool game_clock_stops = true;

static enum { IDLE, SCANNING, CHECKING, DONE } state;
static uint32_t frames;
static uintptr_t scan_pos;
static hit_t hits[MAX_HITS];
static int n_hits;
static uint32_t hits_seen[D_COUNT], hits_dropped;

/* while scanning: drop the hits that stopped changing (a stale timestamp isn't the frame's), so the
 * table has room for the rest of memory */
static void drop_stale(void)
{
  int kept = 0;
  for (int i = 0; i < n_hits; i++) {
    hit_t *h = &hits[i];
    uint64_t v = sys_mapped(h->addr, 8, SYS_PERM_R) ? *(const volatile uint64_t *)h->addr : h->last;
    h->still = v == h->last ? h->still + 1 : 0;
    h->last = v;
    if (h->still < STALE_CHECKS)
      hits[kept++] = *h;
    else
      hits_dropped++;
  }
  n_hits = kept;
}
static uint32_t checked_frames;
static hit_t found[MAX_FOUND];
static int n_found;
static bool held_since_check; /* frame advance held the game since the last check: that frame's move isn't one */
static uintptr_t own_lo, own_hi; /* this module's own data: it keeps timestamps too */

static double ticks_per_ms(void) { return (double)sys_tick_freq() / 1000.0; }

static uint64_t ticks_to_ns(uint64_t ticks)
{
  uint64_t freq = sys_tick_freq();
  return ticks / freq * 1000000000ull + ticks % freq * 1000000000ull / freq;
}

/* the value's time, in ms of timer time (comparable across domains) */
static double value_ms(domain_t domain, uint64_t value)
{
  switch (domain) {
    case D_TICKS: return (double)value / ticks_per_ms();
    case D_NS: return (double)value / 1e6;
    case D_US: return (double)value / 1e3;
    default: {
      double seconds;
      __builtin_memcpy(&seconds, &value, 8);
      return seconds * 1000.0;
    }
  }
}

static void add_hit(uintptr_t addr, domain_t domain, uint64_t value)
{
  if (n_hits >= MAX_HITS || (addr >= own_lo && addr < own_hi))
    return;
  hits[n_hits++] = (hit_t){ addr, domain, value, 0, 0 };
  hits_seen[domain]++;
}

static void scan_slice(void)
{
  uint64_t now = sys_tick();
  uint64_t window = (uint64_t)(WINDOW_MS * ticks_per_ms());
  uint64_t now_ns = ticks_to_ns(now), window_ns = (uint64_t)WINDOW_MS * 1000000ull;
  uint64_t now_us = now_ns / 1000, window_us = (uint64_t)WINDOW_MS * 1000ull;
  double now_s = (double)now / (double)sys_tick_freq();
  uint32_t budget = SCAN_BUDGET;
  for (int guard = 0; guard < 4096 && budget >= 8; guard++) {
    sys_meminfo_t info;
    if (sys_query_memory(&info, scan_pos) != 0 || info.size == 0 || info.addr + info.size <= scan_pos) {
      state = CHECKING;
      return;
    }
    uintptr_t end = (uintptr_t)(info.addr + info.size);
    uint32_t type = info.type & MEM_TYPE_MASK;
    /* Only the modules' own data (.data/.bss): reading the heap (the GPU's memory among it) leaves Eden at
     * a third of its frame rate for good (measured: 60 fps to under 25 after a heap scan, even with the
     * search off again). Two of the game's frame timestamps are in main's data (found each run). */
    if (type != MEM_TYPE_CODE_DATA || (info.perm & 3) != 3 ||
        (info.attr & MEM_ATTR_LOCKED)) {
      scan_pos = end;
      continue;
    }
    uintptr_t stop = end - scan_pos > budget ? scan_pos + budget : end;
    for (uintptr_t p = scan_pos; p + 8 <= stop; p += 8) {
      uint64_t v = *(const volatile uint64_t *)p;
      if (v - (now - window) <= window)
        add_hit(p, D_TICKS, v);
      else if (v - (now_ns - window_ns) <= window_ns)
        add_hit(p, D_NS, v);
      else if (v - (now_us - window_us) <= window_us)
        add_hit(p, D_US, v);
      else if ((v >> 52) == 0x40D || (v >> 52) == 0x40E) { /* a double from 16384 to 65535: quick filter */
        double d;
        __builtin_memcpy(&d, &v, 8);
        if (d <= now_s && d >= now_s - WINDOW_MS / 1000.0)
          add_hit(p, D_SECONDS, v);
      }
    }
    budget -= (uint32_t)(stop - scan_pos);
    scan_pos = stop;
    if (stop == end && end == 0)
      break;
  }
}

/* each frame: which hits moved on by about as much as the time since the last frame (frames run long when
 * the emulator is slow: 30 to 110 ms measured) */
static uint64_t last_check_tick;
static void check_hits(void)
{
  bool held = held_since_check || last_check_tick == 0;
  held_since_check = false;
  uint64_t now = sys_tick();
  double frame_ms = (double)(now - last_check_tick) / ticks_per_ms();
  last_check_tick = now;
  double slack = frame_ms * 0.3 > 4.0 ? frame_ms * 0.3 : 4.0;
  for (int i = 0; i < n_hits; i++) {
    hit_t *h = &hits[i];
    if (!sys_mapped(h->addr, 8, SYS_PERM_R))
      continue;
    uint64_t v = *(const volatile uint64_t *)h->addr;
    double moved = value_ms(h->domain, v) - value_ms(h->domain, h->last);
    if (!held && moved >= frame_ms - slack && moved <= frame_ms + slack)
      h->framely++;
    h->last = v;
  }
  if (held) /* only frames played in real time count */
    return;
  if (++checked_frames < CHECK_FRAMES)
    return;
  for (int i = 0; i < n_hits && n_found < MAX_FOUND; i++)
    if (hits[i].framely >= CHECK_FRAMES * 3 / 4) {
      found[n_found] = hits[i];
      sys_meminfo_t info;
      sys_query_memory(&info, hits[i].addr);
      log_printf("time finder: %#lx (%s, region %#lx type %u) moves a frame a frame, %u of %u", /* not menu text */
                 (unsigned long)hits[i].addr, domain_names[hits[i].domain], (unsigned long)info.addr,
                 (unsigned)(info.type & MEM_TYPE_MASK), (unsigned)hits[i].framely, (unsigned)CHECK_FRAMES);
      n_found++;
    }
  log_printf("time finder: %d of %d values near the time follow the frames", n_found, n_hits); /* not menu text */
  for (int i = 0, shown = 0; i < n_hits && shown < 12; i++)
    if (hits[i].framely < CHECK_FRAMES * 3 / 4) {
      sys_meminfo_t info;
      sys_query_memory(&info, hits[i].addr);
      log_printf("time finder: not %#lx (%s, type %u): a frame's move in %u of %u frames", /* not menu text */
                 (unsigned long)hits[i].addr, domain_names[hits[i].domain], (unsigned)(info.type & MEM_TYPE_MASK),
                 (unsigned)hits[i].framely, (unsigned)CHECK_FRAMES);
      shown++;
    }
  if (n_found > 0) {
    state = DONE;
    gfx_notice("frame advance: game clock found");
    return;
  }
  /* nothing found: look again a little later */
  log_printf("time finder: looking again"); /* not menu text */
  n_hits = 0;
  hits_dropped = 0;
  for (int d = 0; d < D_COUNT; d++)
    hits_seen[d] = 0;
  checked_frames = 0;
  last_check_tick = 0;
  frames = START_AFTER_FRAMES - RETRY_AFTER_FRAMES;
  state = IDLE;
}

void timefinder_held(void)
{
  held_since_check = true;
}

void timefinder_frame(void)
{
  switch (state) {
    case IDLE:
      if (++frames < START_AFTER_FRAMES)
        return;
      {
        extern char __bss_end__[];
        sys_meminfo_t info;
        static int marker;
        if (sys_query_memory(&info, (uintptr_t)&marker) == 0) {
          own_lo = (uintptr_t)info.addr;
          own_hi = (uintptr_t)(info.addr + info.size);
          if ((uintptr_t)__bss_end__ > own_hi)
            own_hi = (uintptr_t)__bss_end__;
        }
      }
      scan_pos = 0;
      state = SCANNING;
      log_printf("time finder: scanning"); /* not menu text */
      gfx_notice("frame advance: looking for the game clock");
      return;
    case SCANNING:
      drop_stale();
      scan_slice();
      if (state == CHECKING)
        log_printf("time finder: %d values near the time kept (seen as ticks %u, ns %u, us %u, s %u; %u stopped " /* not menu text */
                   "changing)", n_hits, (unsigned)hits_seen[D_TICKS], (unsigned)hits_seen[D_NS], /* not menu text */
                   (unsigned)hits_seen[D_US], (unsigned)hits_seen[D_SECONDS], (unsigned)hits_dropped);
      return;
    case CHECKING:
      check_hits();
      return;
    case DONE:
      return;
  }
}

void timefinder_shift(int64_t ticks)
{
  if (state != DONE)
    return;
  uint64_t mag = ticks < 0 ? (uint64_t)-ticks : (uint64_t)ticks;
  uint64_t ns = ticks_to_ns(mag);
  uint64_t by[D_COUNT] = {mag, ns, ns / 1000, 0};
  double seconds = (double)ticks / (double)sys_tick_freq();
  for (int i = 0; i < n_found; i++) {
    hit_t *h = &found[i];
    if (!sys_mapped(h->addr, 8, SYS_PERM_R | SYS_PERM_W))
      continue;
    volatile uint64_t *p = (volatile uint64_t *)h->addr;
    uint64_t v = *p;
    if (h->domain == D_SECONDS) {
      double d;
      __builtin_memcpy(&d, &v, 8);
      d += seconds;
      __builtin_memcpy(&v, &d, 8);
    } else {
      v = ticks < 0 ? v - by[h->domain] : v + by[h->domain];
    }
    *p = v;
    h->last = v; /* the shift isn't the game's own movement */
  }
}
