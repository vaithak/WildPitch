#include "pt.h"
/* Through-ball geometry vs a runner who keeps running straight (seat 1 = co-op mate, no assist). */
static SimState s;
int main(void){
  struct { const char*n; double dx,dy; int8_t rx,ry; uint16_t rb; } R[]={
    {"ahead-left jog +x",  6, 4, 127,0,0}, {"ahead-left SPRINT +x",6,4,127,0,WP_BTN_SPRINT},
    {"level-wide SPRINT +x",0,8,127,0,WP_BTN_SPRINT}, {"ahead SPRINT diag in",6,8,127,-127,WP_BTN_SPRINT},
    {"ahead standing",8,4,0,0,0}, {"far ahead SPRINT +x",12,3,127,0,WP_BTN_SPRINT}};
  for(int r=0;r<6;r++){
   printf("\n== runner %s, starts at MF+(%.0f,%.0f) ==\n",R[r].n,R[r].dx,R[r].dy);
   for(int H=4; H<=40; H+=6){
    int hold=H<9?3:H;
    setup_empty(&s,3); s.seat_player[0]=3; s.seat_player[1]=4;
    s.players[3].pos=fv2_make(-FX(16),-FX(4)); s.players[4].pos=fv2_make(-FX(16)+FX_INT(0)+(fx)(R[r].dx*65536)-(fx)(0), -FX(4)+(fx)(R[r].dy*65536));
    s.ball.pos=fv2_add(s.players[3].pos,fv2_make(FX(0.4),0));
    WpInput in; memset(&in,0,sizeof in);
    for(int t=0;t<5;t++) sim_step(&s,&in);
    /* runner runs; MF dribbles +x at jog for 30 ticks to have a natural speed */
    in.seat[1]=(WpSeatInput){R[r].rx,R[r].ry,R[r].rb};
    in.seat[0]=(WpSeatInput){127,0,0};
    /* back up runner so he is at the stated offset when the pass starts */
    for(int t=0;t<30;t++) sim_step(&s,&in);
    fv2 rel0=fv2_sub(s.players[4].pos,s.players[3].pos);
    for(int t=0;t<hold;t++){ in.seat[0]=stick_to(s.players[3].pos,s.players[4].pos,WP_BTN_PASS,1); sim_step(&s,&in);} 
    in.seat[0].buttons=0;
    int kt=-1; double best=99, bt=0, ahead=0, v0=0, vat=0; fv2 rp0={0,0}; int got=-1,tg=-1,out=0; double rest_off=0;
    for(int t=0;t<400;t++){
      sim_step(&s,&in);
      if((s.events&WP_EV_KICK)&&kt<0){kt=t; v0=spd(s.ball.vel); rp0=s.players[4].pos;}
      if(kt<0) continue;
      fv2 d=fv2_sub(s.ball.pos,s.players[4].pos); double dd=tod(fv2_len(d));
      fv2 rv=s.players[4].vel; double rs=spd(rv);
      if(s.ball.owner<0 && dd<best){best=dd; bt=(t-kt)/60.0; vat=spd(s.ball.vel);
         ahead= rs>0.5? tod(fv2_dot(d,fv2_norm(rv))) : tod(d.x);}
      if(s.ball.owner>=0){got=s.ball.owner;tg=t-kt;break;}
      if(s.phase==WP_PHASE_OUT){out=1;break;}
      if(spd(s.ball.vel)<0.02 && t-kt>20){ fv2 dd2=fv2_sub(s.ball.pos,s.players[4].pos); rest_off=tod(fv2_len(dd2)); break;}
    }
    (void)rp0;
    printf(" H=%2d (%s) rel@press(%.1f,%.1f) v0=%5.1f  closest=%5.2f m at %.2fs (ball %+.1f m along run, ball v %.1f)  got=%d t=%d out=%d stop_off=%.1f\n",
       hold, hold<=9?"tap":"thru", tod(rel0.x),tod(rel0.y), v0,best,bt,ahead,vat,got,tg,out,rest_off);
   }
  }
}
