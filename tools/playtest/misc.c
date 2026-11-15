#include "pt.h"
static SimState s;
int main(void){
  printf("Untargeted (no mate in 55deg cone), stick N, stand still:\n");
  for(int H=2;H<=40;H+=6){
    setup_empty(&s,3); for(int i=0;i<10;i++) if(i!=3) s.players[i].pos=fv2_make(FX_INT(i*2-10),-FX(60)); s.seat_player[0]=3; s.players[3].pos=fv2_make(-FX(10),-FX(8)); s.ball.pos=fv2_make(-FX(9.6),-FX(8));
    for(int i=0;i<5;i++) step1(&s,(WpSeatInput){0,0,0});
    for(int i=0;i<H;i++) step1(&s,(WpSeatInput){0,127,WP_BTN_PASS});
    double v0=0; fv2 p0={0,0}; int kt=-1;
    for(int t=0;t<400;t++){ step1(&s,(WpSeatInput){0,(int8_t)(kt<0?127:0),0}); if((s.events&WP_EV_KICK)&&kt<0){kt=t;v0=spd(s.ball.vel);p0=s.ball.pos;} if(kt>=0&&spd(s.ball.vel)<0.01)break; if(s.phase==WP_PHASE_OUT)break;}
    printf("  hold %2d ticks (%s): v0=%.1f m/s, rolled %.1f m%s\n",H,H<=9?"tap":"through",v0,tod(fv2_len(fv2_sub(s.ball.pos,p0))), s.phase==WP_PHASE_OUT?" (OUT)":"");
  }
  printf("Targeted tap vs hold, standing mate 10 m ahead (+x), passer standing:\n");
  for(int H=2;H<=40;H+=2){
    setup_empty(&s,3); s.seat_player[0]=3; s.players[3].pos=fv2_make(-FX(10),0); s.players[4].pos=fv2_make(FX(0),0); s.ball.pos=fv2_make(-FX(9.6),0);
    for(int i=0;i<5;i++) step1(&s,(WpSeatInput){0,0,0});
    for(int i=0;i<H;i++) step1(&s,(WpSeatInput){127,0,WP_BTN_PASS});
    double v0=0; int kt=-1; int rel=-1;
    for(int t=0;t<400;t++){ step1(&s,(WpSeatInput){127,0,0}); if((s.events&WP_EV_KICK)&&kt<0){kt=t;v0=spd(s.ball.vel);rel=t;} if(kt>=0&&(s.ball.owner>=0||spd(s.ball.vel)<0.01))break;}
    printf("  hold %2d (%3.0f ms): release->contact %d ticks, v0=%4.1f, spot/landing: ball x=%.1f owner=%d\n",H,H*16.7,rel+1,v0,tod(s.ball.pos.x),s.ball.owner);
  }
}
