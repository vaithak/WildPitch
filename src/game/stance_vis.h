/* stance_vis.h - the avatar's aerial contact stance from SimState (D-052). Presentation only: reads the sim's
 * contact plan before a touch and the player's last-touch kind after it; never writes the sim. */
#ifndef WP_STANCE_VIS_H
#define WP_STANCE_VIS_H
#include "game/avatar.h"
#include "sim/sim.h"

typedef struct {                 /* per player, presentation state: the contact being shown */
    uint8_t  kind;               /* WP_LT_* */
    uint16_t tick;               /* its tick (low 16 bits of SimState.tick) */
    float    x, y, z;            /* the ball's centre there (world, m) */
    bool     valid;
} WpStanceVis;

/* Fill pose->stance / stance_leg / stance_t / stance_tgt (and head_turn for a header pass) for player i, drawn at
 * (px, py) facing pose->facing, between prev and cur at alpha. The contact tick is drawn at alpha 0 of the frame
 * whose `cur` holds the touch (the ball where `prev` has it). */
void wp_stance_fill(const SimState *prev, const SimState *cur, int i, float alpha, float px, float py,
                    WpStanceVis *c, WpAvatarPose *pose);
#endif
