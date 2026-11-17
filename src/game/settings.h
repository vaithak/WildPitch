/* settings.h - the player's settings (settings.cfg in the pref dir, separate from controls.cfg) and the rows that
 * edit them. One struct, one line-based "key = value" file; unknown keys are ignored and missing keys keep their
 * defaults, so old files (difficulty / assist / switching only) still load. The pause menu (menu.c) and the title
 * screen (title.c) show the same rows through wp_settings_row_text / wp_settings_row_change. */
#ifndef WP_SETTINGS_H
#define WP_SETTINGS_H
#include <stdbool.h>

typedef struct {
    int difficulty;     /* WP_AI_EASY / NORMAL / HARD for the opposition (applies on a new match) */
    int assist;         /* WP_ASSIST_OFF..HIGH: movement assist toward the ball (D-037) */
    int switching;      /* WP_SWITCH_MANUAL / EVENTS / ASSISTED (D-050) */
    int vol_master;     /* 0..10 */
    int vol_fx;         /* 0..10: kicks, bounces, posts, net, whistle */
    int vol_crowd;      /* 0..10 */
    bool muted;         /* M in game (not a menu row) */
    bool battery;       /* frame rate cap: false = 60 fps, true = 30 fps (battery) (D-056) */
    int half_min;       /* real minutes per half: 3 / 5 / 8 (the clock still shows 45' a half) */
    int format;         /* WP_FORMAT_7V7 / WP_FORMAT_5V5 */
    int kit[2];         /* kits.h index for RED (team 0, us) and BLUE (team 1) */
} WpSettings;

enum { WP_FORMAT_7V7 = 0, WP_FORMAT_5V5 = 1 };

/* menu rows over WpSettings */
typedef enum {
    WP_SET_FORMAT, WP_SET_DIFFICULTY, WP_SET_ASSIST, WP_SET_SWITCHING, WP_SET_HALF, WP_SET_KIT0, WP_SET_KIT1,
    WP_SET_VOL_MASTER, WP_SET_VOL_FX, WP_SET_VOL_CROWD, WP_SET_FPS, WP_SET_COUNT
} WpSetRow;

void wp_settings_defaults(WpSettings *s);
void wp_settings_from_text(WpSettings *s, const char *text);   /* tolerant: keeps defaults for missing / bad keys */
int  wp_settings_to_text(const WpSettings *s, char *buf, int cap);
void wp_settings_load(WpSettings *s);                         /* defaults, then the file if there is one */
bool wp_settings_save(const WpSettings *s);

/* "Difficulty: < Normal >" (no selection marker) */
void wp_settings_row_text(const WpSettings *s, WpSetRow row, char *buf, int cap);
/* Left (dir -1) / Right or Enter (+1): change the row's value, keeping it valid */
void wp_settings_row_change(WpSettings *s, WpSetRow row, int dir);

int  wp_check_settings(void);   /* --check-settings: defaults, round trip, old file, row cycling (no window) */
#endif
