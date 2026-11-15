#include "pt.h"
static SimState s;
int main(void){
  printf(" dist | launch | arrival | flight\n");
  for(int d=4; d<=30; d+=2){
    setup_empty(&s,3); for(int i=0;i<10;i++) if(i!=3&&i!=4) s.players[i].pos=fv2_make(FX_INT(i*2-10),-FX(60));
    s.seat_player[0]=3; s.players[3].pos=fv2_make(-FX(14),0); s.players[4].pos=fv2_make(-FX(14)+FX_INT(d),FX(0.0)); s.ball.pos=fv2_make(-FX(13.6),0);
    for(int i=0;i<5;i++) step1(&s,(WpSeatInput){0,0,0});
    for(int i=0;i<3;i++) step1(&s,(WpSeatInput){127,0,WP_BTN_PASS});
    int kt=-1; double v0=0,va=0; int tg=-1;
    for(int t=0;t<400;t++){ if(s.ball.owner<0) va=spd(s.ball.vel); step1(&s,(WpSeatInput){127,0,0}); if((s.events&WP_EV_KICK)&&kt<0){kt=t;v0=spd(s.ball.vel);} if(kt>=0&&s.ball.owner>=0){tg=t-kt;break;} }
    printf(" %2d m | %5.1f | %5.1f | %.2f s %s\n",d,v0,va,tg/60.0,tg<0?"NOT RECEIVED":"");
  }
}
