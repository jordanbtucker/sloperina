#include "input.h"

#include "got.h"
#include "sys.h"

/* nn::hid state structs (Switch 1 layout: sampling number, buttons, two sticks, attributes;
 * 0x28 bytes). We only touch the leading fields and read one state into a larger buffer, so
 * a longer Switch 2 struct is harmless. VERIFY the field order on hardware (BUILDING.md). */
typedef struct {
  int64_t  sampling_number;
  uint64_t buttons;
  int32_t  lx, ly, rx, ry;
  uint32_t attributes;
} npad_state_t;

#define NPAD_STATE_SIZE      0x28  /* the Switch 1 state's size: the stride between states */
#define NPAD_STATE_BUF       0x100 /* room for one state, however long the Switch 2 one is */
#define NPAD_ATTR_CONNECTED  1u    /* NpadAttribute::IsConnected */

/* nn::hid::detail::GetNpadStates(int *out_count, NpadXState *out, int count, const NpadIdType &id),
 * exported by sdk. The detail versions are what the public ones call, so they keep working
 * when the public ones are interposed below. */
#define DETAIL(style) "_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_" style "EiRKj"
void hid_detail_fullkey(int *n, void *out, int count, const uint32_t *id)
  __asm__(DETAIL("16NpadFullKeyState"));
void hid_detail_handheld(int *n, void *out, int count, const uint32_t *id)
  __asm__(DETAIL("17NpadHandheldState"));
void hid_detail_joydual(int *n, void *out, int count, const uint32_t *id)
  __asm__(DETAIL("16NpadJoyDualState"));

/* NpadIdType values */
enum { NPAD_NO1 = 0, NPAD_HANDHELD = 0x20 };

/* gz-style repeat for held buttons (menu navigation), in frames */
#define REPEAT_FIRST_DELAY 12 /* before the first repeat */
#define REPEAT_INTERVAL    3  /* then every this many */

static uint32_t buttons_held;
static uint32_t buttons_pressed;  /* went down this frame */
static uint32_t repeat_buttons;   /* held since they went down, so they repeat */
static uint32_t buttons_fired;    /* pressed, or repeating this frame */
static int32_t stick_axes[4];     /* lx, ly, rx, ry */
static int repeat_countdown;
bool input_block_game;
uint32_t input_hide_from_game;

static int controller = INPUT_PRO;

/* Add one controller's state (one style of one NpadIdType) to this frame's input: true when it's connected
 * and something on it is pressed or tilted (*connected: whether it's connected) */
static bool read_npad(void (*get_states)(int *, void *, int, const uint32_t *), uint32_t npad_id, bool *connected)
{
  uint64_t buf[NPAD_STATE_BUF / 8] = { 0 };
  int state_count = 0;
  *connected = false;
  get_states(&state_count, buf, 1, &npad_id);
  if (state_count < 1)
    return false;
  npad_state_t *state = (npad_state_t *)buf;
  if (!(state->attributes & NPAD_ATTR_CONNECTED))
    return false;
  *connected = true;
  /* buttons beyond those this module knows (A..D-pad, the sticks' directions, SL, SR): logged once each, to
   * learn what the Switch 2's controllers send (GL / GR are the system's to assign: nn::hid's
   * GetGlGrButtonConfigState; whether a game ever sees them as buttons of their own is unknown) */
  static uint64_t unknown_seen;
  uint64_t unknown = state->buttons & ~(uint64_t)0x3FFFFFF;
  if (unknown & ~unknown_seen) {
    unknown_seen |= unknown;
    log_printf("input: buttons this module doesn't know, from NpadIdType %u: 0x%llx", (unsigned)npad_id,
               (unsigned long long)unknown);
  }
  buttons_held |= (uint32_t)state->buttons;
  if (state->lx || state->ly) {
    stick_axes[0] = state->lx;
    stick_axes[1] = state->ly;
  }
  if (state->rx || state->ry) {
    stick_axes[2] = state->rx;
    stick_axes[3] = state->ry;
  }
  /* in use: a button (not the sticks' direction bits) or a stick well off centre, so a resting stick's
   * drift doesn't count */
  const int32_t off = 0x3000; /* about 40% of a stick's travel */
  return (state->buttons & 0xFFFF) || state->lx > off || state->lx < -off || state->ly > off || state->ly < -off ||
         state->rx > off || state->rx < -off || state->ry > off || state->ry < -off;
}

int input_controller(void)
{
  return controller;
}

void input_update(void)
{
  uint32_t previously_held = buttons_held;
  buttons_held = 0;
  stick_axes[0] = stick_axes[1] = stick_axes[2] = stick_axes[3] = 0;
  /* the controller is the one used last; one that's gone gives way to one that's there */
  bool connected[3], used[3];
  used[INPUT_PRO] = read_npad(hid_detail_fullkey, NPAD_NO1, &connected[INPUT_PRO]);
  used[INPUT_JOYCONS] = read_npad(hid_detail_joydual, NPAD_NO1, &connected[INPUT_JOYCONS]);
  used[INPUT_HANDHELD] = read_npad(hid_detail_handheld, NPAD_HANDHELD, &connected[INPUT_HANDHELD]);
  for (int i = 0; i < 3; i++)
    if (used[i] || (!connected[controller] && connected[i]))
      controller = i;
  buttons_pressed = buttons_held & ~previously_held;

  buttons_fired = buttons_pressed;
  if (buttons_pressed) {
    repeat_buttons = buttons_pressed;
    repeat_countdown = REPEAT_FIRST_DELAY;
  } else if (buttons_held & repeat_buttons) {
    repeat_buttons &= buttons_held;
    if (--repeat_countdown <= 0) {
      repeat_countdown = REPEAT_INTERVAL;
      buttons_fired = repeat_buttons;
    }
  } else {
    repeat_buttons = 0;
  }
}

uint32_t input_held(void) { return buttons_held; }
uint32_t input_pressed(void) { return buttons_pressed; }

bool input_combo(uint32_t mask)
{
  return (buttons_held & mask) == mask && (buttons_pressed & mask);
}

bool input_repeat(uint32_t button)
{
  return (buttons_fired & button) != 0;
}

void input_sticks(int32_t *lx, int32_t *ly, int32_t *rx, int32_t *ry)
{
  *lx = stick_axes[0];
  *ly = stick_axes[1];
  *rx = stick_axes[2];
  *ry = stick_axes[3];
}

/* --- hiding input from the game -------------------------------------------------------------
 * main's calls to the public nn::hid::GetNpadStates overloads go through its GOT; module init
 * points those slots at the wrappers below (got.c). While the menu is open the game then sees
 * the controller idle. Only the buttons and sticks of each state are cleared (Switch 1 layout,
 * 0x28 bytes per state: a longer Switch 2 state is still only written inside the buffer). */
#define PUBLIC(style) "_ZN2nn3hid13GetNpadStatesEPNS0_" style "EiRKj"
typedef int (*hid_public_t)(void *out, int count, const uint32_t *id);

static const char *const hid_public_names[] = {
  PUBLIC("16NpadFullKeyState"), PUBLIC("17NpadHandheldState"), PUBLIC("16NpadJoyDualState"),
  PUBLIC("16NpadJoyLeftState"), PUBLIC("17NpadJoyRightState"),
};
#define N_HID (sizeof(hid_public_names) / sizeof(*hid_public_names))
/* the sdk functions (imported by this module, so they bypass main's GOT) */
int hid_public_0(void *, int, const uint32_t *) __asm__(PUBLIC("16NpadFullKeyState"));
int hid_public_1(void *, int, const uint32_t *) __asm__(PUBLIC("17NpadHandheldState"));
int hid_public_2(void *, int, const uint32_t *) __asm__(PUBLIC("16NpadJoyDualState"));
int hid_public_3(void *, int, const uint32_t *) __asm__(PUBLIC("16NpadJoyLeftState"));
int hid_public_4(void *, int, const uint32_t *) __asm__(PUBLIC("17NpadJoyRightState"));
static void *const hid_public[N_HID] = { hid_public_0, hid_public_1, hid_public_2, hid_public_3, hid_public_4 };
/* what each wrapper calls: another module's wrapper hooked earlier, or the sdk's function (got_hook) */
static void *hid_next[N_HID];

bool input_force_active;
uint32_t input_force_buttons;
bool input_force_sticks[2];
int32_t input_force_stick[4];

/* Edit the states the game just read: blank them while the menu is open, otherwise add frame
 * advance's buffered input and take out the buttons the game mustn't see. */
static void filter_game_states(void *states, int count)
{
  if (!input_block_game && !input_hide_from_game && !input_force_active)
    return;
  for (int i = 0; i < count; i++) {
    npad_state_t *state = (npad_state_t *)((char *)states + i * NPAD_STATE_SIZE);
    if (input_block_game) {
      state->buttons = 0;
      state->lx = state->ly = state->rx = state->ry = 0;
      continue;
    }
    if (input_force_active) {
      state->buttons |= input_force_buttons;
      if (input_force_sticks[0]) {
        state->lx = input_force_stick[0];
        state->ly = input_force_stick[1];
      }
      if (input_force_sticks[1]) {
        state->rx = input_force_stick[2];
        state->ry = input_force_stick[3];
      }
    }
    state->buttons &= ~(uint64_t)input_hide_from_game;
  }
}

/* Frame advance diagnostics: how, when and from which thread the game reads the controller. Written
 * from whatever thread the game reads on, read and cleared by the frame hook; a torn count only skews
 * one log line. */
volatile input_reads_t input_reads;

static uintptr_t current_thread(void)
{
  uintptr_t tls;
  __asm__ volatile("mrs %0, tpidrro_el0" : "=r"(tls));
  return tls;
}

static void note_game_read(unsigned style, void *states, int count, int returned)
{
  volatile input_reads_t *r = &input_reads;
  uint64_t now = sys_tick();
  if (r->calls == 0) {
    r->first_tick = now;
    r->first_style = style;
  }
  r->last_tick = now;
  r->calls++;
  if (input_force_active)
    r->forced++;
  if ((uint32_t)count > r->max_count)
    r->max_count = (uint32_t)count;
  r->styles |= 1u << style;
  if (returned > 0) {
    if ((uint32_t)returned > r->max_returned)
      r->max_returned = (uint32_t)returned;
    /* sampling numbers are per controller style: follow the first style read */
    static int64_t previous_sampling[8];
    int64_t sampling = ((npad_state_t *)states)->sampling_number;
    if (style == r->first_style) {
      if (!r->have_sampling) {
        r->first_sampling = previous_sampling[style]; /* the read before this count started */
        r->have_sampling = true;
      }
      r->last_sampling = sampling;
    }
    previous_sampling[style] = sampling;
  }
  uintptr_t thread = current_thread();
  bool known = false;
  for (uint32_t i = 0; i < r->n_threads && i < INPUT_READS_THREADS; i++)
    if (r->threads[i] == thread)
      known = true;
  if (!known && r->n_threads < INPUT_READS_THREADS)
    r->threads[r->n_threads++] = thread;
}

void input_reads_take(input_reads_t *out)
{
  *out = *(const input_reads_t *)&input_reads;
  input_reads.calls = 0;
  input_reads.forced = 0;
  input_reads.max_count = 0;
  input_reads.max_returned = 0;
  input_reads.have_sampling = false;
  input_reads.n_threads = 0;
  input_reads.styles = 0;
}

uintptr_t input_thread(void) { return current_thread(); }

/* hid_wrap_N: whatever slot N held before (hid_next), then filter_game_states */
#define WRAP(n)                                                     \
  static int hid_wrap_##n(void *out, int count, const uint32_t *id) \
  {                                                                 \
    int result = ((hid_public_t)hid_next[n])(out, count, id);       \
    int states = result < count ? result : count;                   \
    note_game_read(n, out, count, states);                          \
    filter_game_states(out, states);                                \
    return result;                                                  \
  }
WRAP(0) WRAP(1) WRAP(2) WRAP(3) WRAP(4)

static void *const hid_wraps[N_HID] = { hid_wrap_0, hid_wrap_1, hid_wrap_2, hid_wrap_3, hid_wrap_4 };

const char *input_hook_game(uintptr_t main_base)
{
  const char *last_error = 0;
  for (unsigned i = 0; i < N_HID; i++) {
    const char *error = got_hook(main_base, hid_public_names[i], hid_wraps[i], hid_public[i], &hid_next[i]);
    if (error)
      last_error = error;
  }
  return last_error;
}
