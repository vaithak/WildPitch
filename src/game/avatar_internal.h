/* avatar_internal.h - skeleton internals shared by avatar.c (poses, solve, draw) and avatar_stance.c (the aerial
 * contact stances, D-052). Presentation only. */
#ifndef WP_AVATAR_INTERNAL_H
#define WP_AVATAR_INTERNAL_H
#include "game/avatar.h"
#include <math.h>

#define PI_F 3.14159265f

typedef struct { float x, y, z; } V3;
static inline V3 v3(float x, float y, float z) { V3 v = {x, y, z}; return v; }
static inline V3 add(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline V3 mul(V3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }

enum { J_PELVIS, J_CHEST, J_HEAD, J_HIP_L, J_KNEE_L, J_ANK_L, J_TOE_L, J_HIP_R, J_KNEE_R, J_ANK_R, J_TOE_R,
       J_SH_L, J_ELB_L, J_HAND_L, J_SH_R, J_ELB_R, J_HAND_R, J_COUNT };

typedef struct {             /* floats only: blended field-by-field */
    float lean;                 /* torso pitch forward (rad) */
    float hip[2], knee[2];      /* leg swing (fwd +), knee bend (>=0); [0]=left [1]=right */
    float leg_out[2];           /* lateral splay (rad) */
    float sh[2], elb[2], ab[2]; /* arm swing fwd, elbow bend, abduction */
    float nod;                  /* extra head pitch */
    float crouch;               /* extra pelvis drop (m) */
    float roll;                 /* whole-body sideways tilt about the facing axis (rad, + = toward the left) */
    float shift_f, shift_l;     /* pelvis moved forward / to the left of the sim position (m): stance lunges */
    float rise;                 /* body lifted off the grass (m): a stance's tip-toe / little hop */
} Pose;

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float smooth01(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }

/* the joints for a pose at (x, y) (avatar.c) */
void avatar_solve(const WpAvatarPose *P, const WpAvatarLook *L, const WpAvatarStyle *S, float x, float y,
                  V3 *J, float *head_r);
/* avatar_stance.c: the stance's pose over the base pose `a` (returns its blend weight), and its fit to the target */
float avatar_stance_pose(const WpAvatarPose *P, Pose *a);
void  avatar_stance_fit(WpAvatarPose *P, const WpAvatarLook *L, const WpAvatarStyle *S);
void  avatar_header_reach(const WpAvatarPose *P, Pose *o);   /* a header's nudge onto the ball (after its pose) */
#endif
