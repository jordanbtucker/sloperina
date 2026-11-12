# Using sloperinaGZ

A quick guide to get you practising. For every option in every menu, see the [manual](MANUAL.md).

## Contents

- [Getting started](#getting-started)
- [Controls](#controls)
- [Shortcuts in the game](#shortcuts-in-the-game)
- [Changing the buttons](#changing-the-buttons)
- [How do I...?](#how-do-i)
- [Things on the screen](#things-on-the-screen)
- [Your settings](#your-settings)
- [Something's not right](#somethings-not-right)

## Getting started

1. **Install it.** Follow [INSTALLATION.md](../INSTALLATION.md) (an emulator's mod folder, Atmosphère, or a patched copy).
   sloperinaGZ works with **The Legend of Zelda: Ocarina of Time (Switch 2), versions 1.0.0 and 1.1.1**.
2. **Start the game.** For a few seconds the bottom-right corner shows sloperinaGZ's name, version and
   build date, the website, and how to open the menu. That means it's running.
3. **Open the menu:** hold **R** and push the **right stick down** (with the remake preset: **L + Plus**). Do the
   same again to close it. The start-up message shows the buttons set now.

While the menu is open, the game doesn't see your controller, so you can't accidentally walk off a ledge while
picking an option.

## Controls

The menu works like gz's: the D-pad moves, **select** selects. Every button can be changed
([Changing the buttons](#changing-the-buttons)); these are the two presets':

| Button | gz preset (the default) | remake preset |
|---|---|---|
| Open / close the menu | **R + right stick down** | **L + Plus** |
| Move | **D-pad up / down** | **D-pad up / down** |
| Change a value, or pick a button in a row | **D-pad left / right** | **D-pad left / right** |
| **select**: open a menu, tick a box, press a button | **L** | **A** |
| **number -10 / +10** | **L + D-pad left / right** | **ZL / ZR** |
| **return**: back one page (on the first page: close the menu) | **B**, or **R + D-pad left** | **B** |

The **return** item at the end of each page also goes back one page. The line at the bottom of the menu always
tells you what the buttons do for the item you're on, with the buttons set now. This guide calls them by what they
do: **select**, **return**, **number -10 / +10**.

**What the items look like**

| Looks like | It is | Use |
|---|---|---|
| `items >` | A menu | **select** opens it |
| `☑ name` | A checkbox | **select** (or D-pad left / right) ticks and unticks it |
| `name  < value >` | A switch | **D-pad left / right** (or **select**) changes it |
| `name  < 3 > / 9` | A number | **D-pad left / right**; **number -10 / +10** to change by 10; **select** to fill (where it says so) |
| grey text | A heading or information | Can't be selected |
| `✕ ☑ 🔧 label ...` | A row of buttons | **D-pad left / right** picks a button (it turns blue), **select** presses it |

## Shortcuts in the game

These work with the menu closed.

| Shortcut | gz preset (the default) | remake preset |
|---|---|---|
| Open / close the menu | **R + right stick down** | **L + Plus** |
| Save position: where Link is and which way he faces | **R + D-pad up** | **L + D-pad up** |
| Load position: put Link back there (same map only) | **R + D-pad down** | **L + D-pad down** |
| Show / hide the watch window | **R + D-pad right** | **L + D-pad right** |
| Start / stop the timer | **R + A + D-pad left** | **L + R + D-pad up** |
| Reset the timer | **R + B + D-pad left** | **L + R + D-pad down** |
| Reset the lag counter | **R + B + D-pad right** | **L + R + D-pad left** |
| Reset **JS Height** (jump slash height) to its maximum, as returning to the title screen does | **R + A + D-pad right** | **L + R + D-pad right** |
| Levitate: start / stop floating (when enabled) | **L3** | **L + D-pad left** |
| Frame advance: pause / unpause (when enabled) | **L + L3** | **L + L3** |
| Frame advance, while paused: tap for a frame, hold to advance frames slowly | **L** | **L** |
| **X / B** (held, while floating) | Levitate up / down; release to hover | the same |
| **R3** (held, while floating) | Hold height even if X/B is held | the same |

The gz preset passes every button to the game, as gz does. The remake preset uses **L** as the shortcut button:
L, and anything pressed while it's held, isn't passed to the game, so **R** stays free for quick select. Pick
either in **settings > buttons**, or set each one yourself there.

Everything you do shows for a moment in the bottom-right corner (gz's log), for example `infinite magic on` or
`warp to kokiri forest`.

## Changing the buttons

**settings > buttons** lists every shortcut and menu button with the buttons set now.

- Select a row, then hold up to 3 buttons (a stick direction can be one) and let go: that's the new combo. The
  last button you press is the one that has to go down last. **return** alone cancels (except on the return
  rows, where it's a valid choice).
- A combo another row already has swaps with it.
- **shortcut button**: hold it for shortcuts. It, and anything pressed while it's held, isn't passed to the game.
  D-pad left / right clears it (none: every button is passed to the game).
- **step frame**: the frame advance button, used while frame advance has the game paused. **slow advance delay**:
  how long it's held before frames advance slowly.
- **use gz preset (default)** and **use remake preset** set them all at once.

They're saved in `sloperinaGZ/settings.ini` with the other settings.

## How do I...?

**...go somewhere?**
**warps > places**, pick the area (overworld child or adult, dungeons, boss rooms, other), the map, then where in
the map. You arrive straight away, with the game's own fade.

**...be adult (or child)?**
**link > age** to **adult**, then **link > reload map**. The map reloads as the adult version. (Sword and shield
swap with the ones you had as the other age.)

**...get an item?**
**inventory > items** (child, adult or universal), **equipment items** (swords, shields, tunics, boots...) or
**collection** (ocarinas, strength, scale, wallets and bags...). Tick it. Switches like **hookshot**
(none / hookshot / longshot) and **big sword** give you one at a time.

**...refill health, magic or rupees?**
**link > health** (or **rupees**), then **select**. Magic is in **inventory > collection > magic**.

**...never run out?**
**cheats**: infinite energy, rupees, magic, and each kind of ammo.

**...see my position, speed or angle?**
**watches**: open the folder (📁) and pick from the **known watches**. They show on the screen in a box. Tick or
untick the checkbox on a watch to show or hide just that one.

**...watch a value in memory?**
**watches**, press the **+**, type the address (D-pad left / right to pick a digit, up / down to change it),
**select** to add it. Or open **debug > memory**, find the value, edit the row (**select**) and press **select** on
its address.

**...move the watches box?**
**watches > watches box > move watches position**, then the D-pad (hold it to go faster). **select** when it's in place.

**...see the invisible walls?**
**scene > collision > show**. Cyan walls stop Link; red ones are walls he's inside.

**...float around?**
Turn on **settings > levitate > enabled**, close the menu, press the levitate shortcut (**L3**, or **L + D-pad left** with the remake preset). Push the left stick to move; hold **X** to rise or **B** to descend. Release both to hover.
With **through walls** enabled, Link can pass through walls and ceilings;
held B commands descent through floors. Exit geometry before toggling floating off.
**vertical speed** controls both directions; an old saved value of 0 uses 4 units/s for X/B.
The levitate shortcut again stops floating, restores collision and starts the fall with zero vertical speed. Normal
gravity resumes, with fall-distance protection through the first landing. Void triggers still apply.

**...look at a trick frame by frame?**
Turn on frame advance with **settings > frame advance > enabled** (it's off to begin with). In the game, press
**L + L3** to pause, tap **L** for one frame or hold it to advance frames slowly, and press **L + L3** again to
unpause (the same in both presets). Buttons and stick directions you press while paused are given to
the next frame.

**...see my inputs on screen?**
**settings > input display > show**. Choose **style: controller** for a picture of a Pro Controller.

**...make the menu smaller?**
**settings > size**: small, medium or large.

## Things on the screen

| Where | What |
|---|---|
| Top left, under the D-pad icon | The menu |
| Any corner (you choose) | The watches box |
| Bottom right | The log: what you just did, fading out after a second |
| Bottom left, or anywhere (you choose) | The input display (if it's on) |
| Top left corner | The pause icon, while frame advance has the game paused |
| Over the game | Air walls (if they're on) |

## Your settings

Everything you change in **settings**, **cheats** and **watches** (and the watches box position) is saved by
itself a couple of seconds after you close the menu, and comes back next time you start the game. It's kept on
the SD card in `sloperinaGZ/settings.ini`, plain text you can edit. Your game saves are never touched.

## Something's not right

- **Nothing happens when I press R + right stick down.** With the remake preset it's **L + Plus**; the start-up
  message shows the buttons set now. Check the sloperinaGZ splash showed at start-up. If not,
  sloperinaGZ needs the module and the IPS for the active game build. See [INSTALLATION.md](../INSTALLATION.md).
- **The game doesn't react to a button.** Frame advance's **step frame** button (**L**) isn't passed to the game
  while frame advance is enabled (except in the game's pause screen, unless **settings > frame advance > in pause
  screen** is on). Levitate takes **L3** while enabled, plus **X/B/R3** while floating. With a **shortcut button**
  set (the remake preset's **L**), it and anything pressed while it's held aren't passed to the game. Turn these
  off or change them in **settings** if you need those buttons.
- **The screen froze.** Frame advance may have paused the game: press **L + L3** (or the combo set for
  **pause / unpause**).
- **Something I ticked didn't change in the game.** Some things only show after the game redraws them: open and
  close the pause screen, or go through a loading zone. Story stages apply after a loading zone.
- **"settings won't be kept".** The game couldn't open the SD card; everything works, it just won't remember your
  settings.

Still stuck? The **log** menu (on the main menu) lists what sloperinaGZ did and what went wrong (set **type** to
**problems**), and everything is also saved to `sloperinaGZ/log.txt` on the SD card: send that file with any
problem report. The [manual](MANUAL.md) explains every option.
