/* gamepad.c - see gamepad.h. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game/gamepad.h"

#define PB(b) (1u << (b))

WpSeatInput wp_pad_map(const WpPadRaw *raw) {
    WpSeatInput in = {0, 0, 0};
    float x = (float)raw->lx / 32767.0f, y = -(float)raw->ly / 32767.0f;   /* SDL: +y down; sim: +y up the screen */
    if (x < -1.0f) x = -1.0f;                                                /* -32768 */
    if (y > 1.0f) y = 1.0f;
    float mag = sqrtf(x * x + y * y);
    if (mag > WP_PAD_DEADZONE) {
        float m = (mag - WP_PAD_DEADZONE) / (1.0f - WP_PAD_DEADZONE);
        if (m > 1.0f) m = 1.0f;                                              /* square-gate pads exceed 1 on diagonals */
        float k = m * 127.0f / mag;
        in.move_x = (int8_t)lrintf(x * k);
        in.move_y = (int8_t)lrintf(y * k);
    } else {                                                                 /* D-pad: digital, like the arrow keys */
        int dx = ((raw->buttons & PB(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) != 0) - ((raw->buttons & PB(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) != 0);
        int dy = ((raw->buttons & PB(SDL_GAMEPAD_BUTTON_DPAD_UP)) != 0) - ((raw->buttons & PB(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) != 0);
        in.move_x = (int8_t)(dx * 127);
        in.move_y = (int8_t)(dy * 127);
    }
    uint32_t b = raw->buttons;
    if (b & PB(SDL_GAMEPAD_BUTTON_SOUTH)) in.buttons |= WP_BTN_PASS;
    if (b & PB(SDL_GAMEPAD_BUTTON_EAST))  in.buttons |= WP_BTN_SHOOT;
    if (b & PB(SDL_GAMEPAD_BUTTON_WEST))  in.buttons |= WP_BTN_LOB;
    if (b & PB(SDL_GAMEPAD_BUTTON_NORTH)) in.buttons |= WP_BTN_CHIP;
    if ((b & PB(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) || raw->rt >= WP_PAD_TRIGGER_ON) in.buttons |= WP_BTN_SPRINT;
    if (b & PB(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) in.buttons |= WP_BTN_SWITCH;
    return in;
}

WpSeatInput wp_seat_input_merge(WpSeatInput kb, WpSeatInput pad) {
    WpSeatInput out = kb;
    if (kb.move_x == 0 && kb.move_y == 0) { out.move_x = pad.move_x; out.move_y = pad.move_y; }
    out.buttons = (uint16_t)(kb.buttons | pad.buttons);
    return out;
}

WpPadUi wp_pad_ui_action(uint8_t b) {
    switch (b) {
    case SDL_GAMEPAD_BUTTON_START:       return WP_PAD_UI_MENU;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return WP_PAD_UI_ZOOM;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:  return WP_PAD_UI_LOOK;
    case SDL_GAMEPAD_BUTTON_BACK:        return WP_PAD_UI_REPLAY;
    default:                             return WP_PAD_UI_OTHER;
    }
}

SDL_Scancode wp_pad_menu_key(uint8_t b, bool waiting) {
    if (waiting) return b == SDL_GAMEPAD_BUTTON_EAST ? SDL_SCANCODE_ESCAPE : SDL_SCANCODE_UNKNOWN;
    switch (b) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP:    return SDL_SCANCODE_UP;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:  return SDL_SCANCODE_DOWN;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:  return SDL_SCANCODE_LEFT;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return SDL_SCANCODE_RIGHT;
    case SDL_GAMEPAD_BUTTON_SOUTH:      return SDL_SCANCODE_RETURN;
    case SDL_GAMEPAD_BUTTON_EAST:       return SDL_SCANCODE_ESCAPE;
    default:                            return SDL_SCANCODE_UNKNOWN;
    }
}

/* ---- SDL side ---- */
static void open_first(WpGamepad *g) {
    int n = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&n);
    for (int i = 0; ids && i < n && !g->pad; i++) {
        g->pad = SDL_OpenGamepad(ids[i]);
        if (g->pad) g->id = ids[i];
    }
    SDL_free(ids);
}

void wp_gamepad_init(WpGamepad *g) {
    g->pad = NULL; g->id = 0;
    /* we poll stick/trigger state each tick, so continuous gamepad-motion events are noise (SDL keeps updating the
     * polled state with them off). JOYSTICK_* events stay on: SDL derives gamepad button events from them (an event
     * watcher), e.g. D-pads/triggers mapped to joystick axes. The paused menu ignores the rest (dirty flag in main.c). */
    static const Uint32 quiet[] = {SDL_EVENT_GAMEPAD_AXIS_MOTION, SDL_EVENT_GAMEPAD_SENSOR_UPDATE,
                                   SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION, SDL_EVENT_GAMEPAD_UPDATE_COMPLETE};
    for (size_t i = 0; i < sizeof quiet / sizeof quiet[0]; i++) SDL_SetEventEnabled(quiet[i], false);
    open_first(g);
}

bool wp_gamepad_event(WpGamepad *g, const SDL_Event *e) {
    if (e->type == SDL_EVENT_GAMEPAD_ADDED && !g->pad) {
        g->pad = SDL_OpenGamepad(e->gdevice.which);
        if (g->pad) g->id = e->gdevice.which;
        return g->pad != NULL;
    }
    if (e->type == SDL_EVENT_GAMEPAD_REMOVED && g->pad && e->gdevice.which == g->id) {
        SDL_CloseGamepad(g->pad);
        g->pad = NULL; g->id = 0;
        open_first(g);                                    /* another pad still plugged in takes over */
        return true;
    }
    return false;
}

bool wp_gamepad_connected(const WpGamepad *g) { return g->pad != NULL; }

bool wp_gamepad_is_active_button(const WpGamepad *g, const SDL_Event *e) {
    return g->pad && e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && e->gbutton.which == g->id;
}

WpSeatInput wp_gamepad_read(const WpGamepad *g) {
    WpSeatInput zero = {0, 0, 0};
    if (!g->pad) return zero;
    WpPadRaw raw;
    raw.lx = SDL_GetGamepadAxis(g->pad, SDL_GAMEPAD_AXIS_LEFTX);
    raw.ly = SDL_GetGamepadAxis(g->pad, SDL_GAMEPAD_AXIS_LEFTY);
    raw.rt = SDL_GetGamepadAxis(g->pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    raw.buttons = 0;
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT && b < 32; b++)
        if (SDL_GetGamepadButton(g->pad, (SDL_GamepadButton)b)) raw.buttons |= PB(b);
    return wp_pad_map(&raw);
}

static const char *face(const WpGamepad *g, SDL_GamepadButton b) {
    switch (SDL_GetGamepadButtonLabel(g->pad, b)) {
    case SDL_GAMEPAD_BUTTON_LABEL_A:        return "A";
    case SDL_GAMEPAD_BUTTON_LABEL_B:        return "B";
    case SDL_GAMEPAD_BUTTON_LABEL_X:        return "X";
    case SDL_GAMEPAD_BUTTON_LABEL_Y:        return "Y";
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS:    return "Cross";
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:   return "Circle";
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:   return "Square";
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE: return "Triangle";
    default: break;
    }
    static const char *pos[4] = {"South", "East", "West", "North"};
    return pos[b - SDL_GAMEPAD_BUTTON_SOUTH];
}

void wp_gamepad_hint(const WpGamepad *g, char *buf, int cap) {
    if (!g->pad) { buf[0] = 0; return; }
    snprintf(buf, (size_t)cap, "Pad: stick move  %s pass/tackle  %s shoot  %s lob  %s chip  RB/RT sprint  LB switch  "
             "R3 zoom  L3 look  Back replay  Start menu",
             face(g, SDL_GAMEPAD_BUTTON_SOUTH), face(g, SDL_GAMEPAD_BUTTON_EAST), face(g, SDL_GAMEPAD_BUTTON_WEST),
             face(g, SDL_GAMEPAD_BUTTON_NORTH));
}

void wp_gamepad_close(WpGamepad *g) {
    if (g->pad) SDL_CloseGamepad(g->pad);
    g->pad = NULL; g->id = 0;
}

/* ---- --check-gamepad ---- */
static int expect(const char *what, WpSeatInput got, int mx, int my, int buttons) {
    bool ok = got.move_x == mx && got.move_y == my && got.buttons == buttons;
    if (!ok) printf("gamepad check FAIL %s: got (%d,%d,%04x) want (%d,%d,%04x)\n", what, got.move_x, got.move_y, got.buttons,
                    mx, my, (unsigned)buttons);
    return ok ? 0 : 1;
}

int wp_gamepad_check(void) {
    int bad = 0;
    WpPadRaw r = {0, 0, 0, 0};
    bad += expect("centre", wp_pad_map(&r), 0, 0, 0);
    r.lx = 5000; r.ly = -3000;                                       /* inside the 0.2 radial deadzone */
    bad += expect("deadzone", wp_pad_map(&r), 0, 0, 0);
    r.lx = 32767; r.ly = 0;
    bad += expect("full right", wp_pad_map(&r), 127, 0, 0);
    r.lx = -32768;
    bad += expect("full left", wp_pad_map(&r), -127, 0, 0);
    r.lx = 0; r.ly = -32768;                                         /* stick up = +y, like the Up arrow */
    bad += expect("full up", wp_pad_map(&r), 0, 127, 0);
    r.ly = 32767;
    bad += expect("full down", wp_pad_map(&r), 0, -127, 0);
    r.lx = 16384; r.ly = 0;                                          /* half tilt: (0.5-0.2)/0.8 = 0.375 */
    bad += expect("half right", wp_pad_map(&r), 48, 0, 0);
    r.lx = 32767; r.ly = -32768;                                     /* square-gate corner: clamped to the circle */
    WpSeatInput d = wp_pad_map(&r);
    int m2 = d.move_x * d.move_x + d.move_y * d.move_y;
    if (d.move_x < 85 || d.move_x > 95 || d.move_y < 85 || d.move_y > 95 || m2 > 127 * 127 + 254) {
        printf("gamepad check FAIL diagonal: (%d,%d)\n", d.move_x, d.move_y); bad++;
    }
    r.lx = 0; r.ly = 0;
    r.buttons = PB(SDL_GAMEPAD_BUTTON_DPAD_UP) | PB(SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    bad += expect("dpad", wp_pad_map(&r), -127, 127, 0);
    r.lx = 32767; r.buttons = PB(SDL_GAMEPAD_BUTTON_DPAD_UP);        /* the stick wins over the D-pad */
    bad += expect("stick over dpad", wp_pad_map(&r), 127, 0, 0);
    r.lx = 0;
    static const struct { int b; int bit; const char *n; } BT[] = {
        {SDL_GAMEPAD_BUTTON_SOUTH, WP_BTN_PASS, "south"},   {SDL_GAMEPAD_BUTTON_EAST, WP_BTN_SHOOT, "east"},
        {SDL_GAMEPAD_BUTTON_WEST, WP_BTN_LOB, "west"},      {SDL_GAMEPAD_BUTTON_NORTH, WP_BTN_CHIP, "north"},
        {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, WP_BTN_SPRINT, "rb"}, {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, WP_BTN_SWITCH, "lb"},
        {SDL_GAMEPAD_BUTTON_START, 0, "start"},             {SDL_GAMEPAD_BUTTON_BACK, 0, "back"},
        {SDL_GAMEPAD_BUTTON_LEFT_STICK, 0, "l3"},           {SDL_GAMEPAD_BUTTON_RIGHT_STICK, 0, "r3"},
    };
    for (size_t i = 0; i < sizeof BT / sizeof BT[0]; i++) {
        r.buttons = PB(BT[i].b);
        bad += expect(BT[i].n, wp_pad_map(&r), 0, 0, BT[i].bit);
    }
    r.buttons = 0; r.rt = WP_PAD_TRIGGER_ON - 1;
    bad += expect("rt light", wp_pad_map(&r), 0, 0, 0);
    r.rt = 32767;
    bad += expect("rt full", wp_pad_map(&r), 0, 0, WP_BTN_SPRINT);
    /* merge: keyboard-only passes through bit-identically; pad stick only when the keyboard has none */
    WpSeatInput kb = {127, -127, WP_BTN_PASS | WP_BTN_SPRINT}, none = {0, 0, 0}, pad = {40, 60, WP_BTN_SHOOT};
    bad += expect("merge kb only", wp_seat_input_merge(kb, none), 127, -127, WP_BTN_PASS | WP_BTN_SPRINT);
    bad += expect("merge kb wins", wp_seat_input_merge(kb, pad), 127, -127, WP_BTN_PASS | WP_BTN_SPRINT | WP_BTN_SHOOT);
    WpSeatInput kbb = {0, 0, WP_BTN_LOB};
    bad += expect("merge pad stick", wp_seat_input_merge(kbb, pad), 40, 60, WP_BTN_LOB | WP_BTN_SHOOT);
    /* menu adapter + ui actions */
    bad += wp_pad_menu_key(SDL_GAMEPAD_BUTTON_DPAD_DOWN, false) != SDL_SCANCODE_DOWN;
    bad += wp_pad_menu_key(SDL_GAMEPAD_BUTTON_SOUTH, false) != SDL_SCANCODE_RETURN;
    bad += wp_pad_menu_key(SDL_GAMEPAD_BUTTON_SOUTH, true) != SDL_SCANCODE_UNKNOWN;   /* never rebinds to Return */
    bad += wp_pad_menu_key(SDL_GAMEPAD_BUTTON_EAST, true) != SDL_SCANCODE_ESCAPE;
    bad += wp_pad_ui_action(SDL_GAMEPAD_BUTTON_START) != WP_PAD_UI_MENU;
    bad += wp_pad_ui_action(SDL_GAMEPAD_BUTTON_BACK) != WP_PAD_UI_REPLAY;
    printf("gamepad check: %s\n", bad ? "FAIL" : "ok");
    return bad ? 1 : 0;
}
