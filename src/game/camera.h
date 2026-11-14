/* camera.h - tilted 2.5D tabletop projection (presentation only; floats OK). */
#ifndef WP_CAMERA_H
#define WP_CAMERA_H

typedef struct {
    float cx, cy;          /* world point the camera looks at (metres) */
    float ppm;             /* pixels per metre at the look-at point */
    float tilt;            /* 0 = top-down, ~0.6 = tabletop; vertical foreshortening of depth */
    float persp;           /* perspective strength per metre of depth */
    int   w, h;            /* viewport pixels */
} WpCamera;

typedef struct { float x, y, s; } WpScreenPt;   /* s = local scale (for sizing sprites) */

WpScreenPt wp_project(const WpCamera *c, float x, float y, float z);

#endif
