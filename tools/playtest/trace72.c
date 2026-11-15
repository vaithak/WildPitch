#include "pt.h"
static SimState s;
/* MATE at +(12,0), stick N pressed at tick `delay` after kick, jog. Trace distance at control() time. */
int main(int argc,char**argv){ int delay=atoi(argv[1]); int dx=argc>2?atoi(argv[2]):0; int dy=argc>3?atoi(argv[3]):127;
  setup_empty(&s,3); s.seat_player[0]=3; s.players[3].pos=fv2_make(-FX(10),0); s.players[4].pos=fv2_make(FX(2),0); s.ball.pos=fv2_make(-FX(9.6),0);
  for(int i=0;i<5;i++) step1(&s,(WpSeatInput){0,0,0});
  for(int i=0;i<3;i++) step1(&s,(WpSeatInput){127,0,WP_BTN_PASS});
  int kt=-1;
  for(int t=0;t<150;t++){
    WpSeatInput in={127,0,0}; if(kt>=0&&t-kt>=delay){in.move_x=(int8_t)dx;in.move_y=(int8_t)dy;}
    /* emulate: player moves first then control: compute predicted distance after player move = pos+vel*dt approx */
    fv2 bp=s.ball.pos; 
    step1(&s,in);
    if((s.events&WP_EV_KICK)&&kt<0)kt=t;
    if(kt<0)continue;
    WpPlayer*q=&s.players[4];
    double dctl=tod(fv2_len(fv2_sub(bp,q->pos))); /* ball pos before ball_step vs player pos after move = what control() saw */
    if(dctl<1.6||s.ball.owner>=0) printf("t+%d ctl_dist=%.2f after_dist=%.2f bz=%.2f rel=%.1f tc=%d act=%d ev=%x own=%d seat=%d pv=(%.1f,%.1f)\n",t-kt,dctl,bdist(&s,4),tod(s.ball.z),spd(fv2_sub(s.ball.vel,q->vel)),q->touch_cooldown,q->action,s.events,s.ball.owner,s.seat_player[0],tod(q->vel.x),tod(q->vel.y));
    if(s.ball.owner>=0)break;
  }
}
