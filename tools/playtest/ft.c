#include "pt.h"
/* First touch: standing receiver, pass from -x (12 m). Stick pressed when ball is within `pre` m (late), various dirs. */
static SimState s;
int main(void){
  const char*DN[]={"E(away from passer)","NE","N","NW","W(back to passer)","none"};
  int dirs[][2]={{127,0},{127,127},{0,127},{-127,127},{-127,0},{0,0}};
  for(int spr=0;spr<2;spr++) for(int d=0;d<6;d++) for(int pre=0;pre<2;pre++){
    setup_empty(&s,3); s.seat_player[0]=3;
    s.players[3].pos=fv2_make(-FX(10),0); s.players[4].pos=fv2_make(FX(2),0); s.ball.pos=fv2_make(-FX(9.6),0);
    for(int i=0;i<5;i++) step1(&s,(WpSeatInput){0,0,0});
    for(int i=0;i<3;i++) step1(&s,(WpSeatInput){127,0,WP_BTN_PASS});
    WpSeatInput in={127,0,0}; int got=-1; double vin=0;
    for(int t=0;t<200;t++){
      double bd=bdist(&s,4);
      if(s.seat_player[0]==4 && bd < (pre?1.5:0.9)) { in.move_x=(int8_t)dirs[d][0]; in.move_y=(int8_t)dirs[d][1]; in.buttons=spr?WP_BTN_SPRINT:0; }
      if(s.ball.owner<0) vin=spd(s.ball.vel);
      step1(&s,in);
      if(s.ball.owner==4){got=t;break;}
    }
    if(got<0){ printf("%-22s %s pre=%.1f: NOT RECEIVED\n",DN[d],spr?"SPR":"jog",pre?1.5:0.9); continue;}
    fv2 b0=s.ball.pos; double gmax=0; int t_dir=-1; double dot10=0;
    fv2 sd= (dirs[d][0]||dirs[d][1])? fv2_norm(fv2_make(FX_INT(dirs[d][0]),FX_INT(dirs[d][1]))) : fv2_make(FX_ONE,0);
    for(int k=1;k<=40;k++){ step1(&s,in); double g=bdist(&s,4); if(g>gmax)gmax=g;
      fv2 bv=s.ball.vel; if(t_dir<0 && fv2_len(bv)>FX(0.5) && tod(fv2_dot(fv2_norm(bv),sd))>0.8) t_dir=k;
      if(k==20){ fv2 m=fv2_sub(s.ball.pos,b0); dot10= fv2_len(m)>FX(0.05)?tod(fv2_dot(fv2_norm(m),sd)):0; } }
    printf("%-22s %s stick@%.1fm: vin=%.1f  ball heading stick-way after %2d ticks (%3.0f ms), 20-tick disp·stick=%+.2f, max gap %.2f m, owner after 40t=%d\n",
      DN[d],spr?"SPR":"jog",pre?1.5:0.9,vin,t_dir,t_dir*16.7,dot10,gmax,s.ball.owner);
  }
}
