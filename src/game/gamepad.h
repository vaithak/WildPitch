/* gamepad.h - SDL3 gamepad support (CP0.4): hotplug, raw pad -> WpSeatInput mapping, pad -> menu keys.
 * The first connected pad drives seat 0 alongside the keyboard. Presentation/platform only: the sim sees
 * WpSeatInput, never pads. The mapping itself is a pure function (no SDL calls) so it is unit-checked headless.
 *
 * Default pad layout (positional names; Xbox labels in brackets):
 *   left stick move (analogue: magnitude = speed), D-pad move (digital, when the stick is centred)
 *   South [A] pass / tackle   East [B] shoot   West [X] lofted pass   North [Y] chip
 *   right shoulder [RB] or right trigger [RT] sprint   left shoulder [LB] switch player
 *   right stick click [R3] zoom   left stick click [L3] change look   Back [View] replay   Start [Menu] pause menu
 * Menu: D-pad navigates, South selects, East backs, Start closes. */
#ifndef WP_GAMEPAD_H
#define WP_GAMEPAD_H
#include <stdbool.h>
#include <stdint.h>
#include <SDL3/SDL.h>
#include "sim/sim.h"

#define WP_PAD_DEADZONE   0.2f     /* radial, fraction of full tilt */
#define WP_PAD_TRIGGER_ON 16384    /* right trigger counts as "held" past half travel */

/* raw pad state: axes as SDL reports them (stick Y positive = down), buttons as 1 << SDL_GamepadButton */
typedef struct {
    int16_t  lx, ly, rt;
    uint32_t buttons;
} WpPadRaw;

/* pure: raw pad -> seat input (radial deadzone, rescaled so full tilt = 127, +y = up the screen like the keyboard) */
WpSeatInput wp_pad_map(const WpPadRaw *raw);
/* pure: keyboard + pad for one seat. Stick: the keyboard wins when it has any direction, else the pad;
 * buttons OR together. With a zero pad the keyboard input comes back bit-identical. */
WpSeatInput wp_seat_input_merge(WpSeatInput kb, WpSeatInput pad);

/* in-game (non-sim) actions a pad button press triggers */
typedef enum { WP_PAD_UI_NONE, WP_PAD_UI_MENU, WP_PAD_UI_ZOOM, WP_PAD_UI_LOOK, WP_PAD_UI_REPLAY, WP_PAD_UI_OTHER } WpPadUi;
WpPadUi wp_pad_ui_action(uint8_t sdl_button);         /* pure */
/* pure: pad button -> the scancode the pause menu understands (SDL_SCANCODE_UNKNOWN = ignore).
 * While the Controls screen waits for a key only East (back) maps (to Escape = cancel), so a pad press never
 * rebinds a keyboard action to Return/arrows. */
SDL_Scancode wp_pad_menu_key(uint8_t sdl_button, bool menu_waiting);

typedef struct {
    SDL_Gamepad   *pad;     /* the active pad (seat 0), or NULL */
    SDL_JoystickID id;
} WpGamepad;

void        wp_gamepad_init(WpGamepad *g);                       /* after SDL_Init: opens a pad already plugged in */
bool        wp_gamepad_event(WpGamepad *g, const SDL_Event *e);  /* hotplug; true if the active pad changed */
bool        wp_gamepad_connected(const WpGamepad *g);
bool        wp_gamepad_is_active_button(const WpGamepad *g, const SDL_Event *e);  /* button-down from the active pad */
WpSeatInput wp_gamepad_read(const WpGamepad *g);                 /* zero input when no pad */
/* "Pad: A pass  B shoot ..." with this pad's face labels (A/B/X/Y or Cross/Circle/...) */
void        wp_gamepad_hint(const WpGamepad *g, char *buf, int cap);
void        wp_gamepad_close(WpGamepad *g);

int         wp_gamepad_check(void);                              /* --check-gamepad: pure mapping checks */
#endif
