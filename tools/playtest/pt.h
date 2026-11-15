#define M_PI 3.14159265358979323846
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "sim_test_helpers.h"
#include "sim/sim_internal.h"
static WpInput IN;
static uint16_t step1(SimState*s, WpSeatInput si){ memset(&IN,0,sizeof IN); IN.seat[0]=si; sim_step(s,&IN); return s->events; }
static double bdist(const SimState*s,int i){ return tod(fv2_len(fv2_sub(s->ball.pos,s->players[i].pos))); }
static double spd(fv2 v){ return tod(fv2_len(v)); }
static WpSeatInput stick_to(fv2 from, fv2 to, uint16_t b, int eight){
  fv2 d=fv2_sub(to,from); double x=tod(d.x),y=tod(d.y),l=sqrt(x*x+y*y); WpSeatInput si={0,0,b};
  if(l<1e-6) return si; x/=l;y/=l;
  if(eight){ double a=atan2(y,x); int o=(int)lround(a/(M_PI/4)); x=cos(o*M_PI/4); y=sin(o*M_PI/4);
     si.move_x=(int8_t)(fabs(x)<0.1?0:(x>0?127:-127)); si.move_y=(int8_t)(fabs(y)<0.1?0:(y>0?127:-127)); return si;}
  si.move_x=(int8_t)lround(x*127); si.move_y=(int8_t)lround(y*127); return si; }
/* result of one pass */
typedef struct { int kicked, kick_t, recv_t, who, out, stopped, steals; double v0, v_at_recv, rv_at_recv, rel_at_recv,
  closest, rel_at_closest, run_dist, max_back, stop_ticks, spill_max, lead_err, ft_dir_dot, ball_len_kick;
  double recv_x0, recv_y0, recv_xr, recv_yr, ballspot_x, ballspot_y; int took_over_t; } PR;
static void pr_print(const char*tag, PR*r){
  printf("%-34s kick=%d v0=%5.1f recv_t=%4d who=%2d out=%d v_ball=%4.1f v_rcv=%4.1f rel=%4.1f closest=%4.2f(rel %4.1f) run=%4.1f back=%4.1f stopT=%3.0f spill=%4.2f ftdot=%5.2f takeover=%d\n",
   tag,r->kicked,r->v0,r->recv_t,r->who,r->out,r->v_at_recv,r->rv_at_recv,r->rel_at_recv,r->closest,r->rel_at_closest,r->run_dist,r->max_back,r->stop_ticks,r->spill_max,r->ft_dir_dot,r->took_over_t); }
