#include "pt.h"
static SimState s;
static void park(void){ for(int i=5;i<10;i++){ if(i==5) continue; s.players[i].pos=fv2_make(-FX(15)+FX_INT(i*2),FX(40)); s.players[i].vel=fv2_make(0,0);} }
int main(void){
  int xs[]={15}; int ys[]={9};
  int outs=0,n=0,got=0;
  for(int xi=0;xi<1;xi++) for(int yi=0;yi<1;yi++) for(int H=14;H<=14;H+=11) for(int pol=0;pol<1;pol++){
    WpMatchConfig c=sim_default_config(); sim_init(&s,&c);
    s.seat_player[0]=3; s.seat_player[1]=4;
    s.players[3].pos=fv2_make(FX(0),0); s.players[4].pos=fv2_make(FX_INT(xs[xi]-3),FX_INT(ys[yi]));
    s.ball.pos=fv2_make(FX(0.4),0);
    WpInput in; memset(&in,0,sizeof in);
    for(int t=0;t<8;t++){ park(); sim_step(&s,&in);} 
    in.seat[1]=(WpSeatInput){127,0,WP_BTN_SPRINT};
    for(int t=0;t<24;t++){ park(); sim_step(&s,&in);} 
    for(int t=0;t<H;t++){ in.seat[0]=stick_to(s.players[3].pos,s.players[4].pos,WP_BTN_PASS,1); park(); sim_step(&s,&in);} 
    WpSeatInput keep=in.seat[0]; keep.buttons=0; in.seat[0]=keep; if(pol) in.seat[0].move_x=in.seat[0].move_y=0;
    s.seat_player[1]=-1; memset(&in.seat[1],0,sizeof in.seat[1]);
    int kt=-1, out=0, g=-1, goal=0; double vrec=0, maxx=-99; fv2 pk={0,0};
    for(int t=0;t<300;t++){ park(); sim_step(&s,&in); if((s.events&WP_EV_KICK)&&kt<0){kt=t; pk=s.players[4].pos;}
      if(kt<0)continue; if(tod(s.ball.pos.x)>maxx)maxx=tod(s.ball.pos.x); if((t-kt)%4==0) printf("  t+%d ball(%.1f,%.1f) v(%.1f,%.1f) st(%.1f,%.1f) v(%.1f,%.1f) pt=%d lt=%d own=%d seat=%d\n",t-kt,tod(s.ball.pos.x),tod(s.ball.pos.y),tod(s.ball.vel.x),tod(s.ball.vel.y),tod(s.players[4].pos.x),tod(s.players[4].pos.y),tod(s.players[4].vel.x),tod(s.players[4].vel.y),s.pass_target,s.ball.last_touch,s.ball.owner,s.seat_player[0]);
      if(s.ball.owner>=0){g=s.ball.owner; vrec=spd(s.players[4].vel); break;}
      if(s.phase==WP_PHASE_OUT){out=1;break;} if(s.phase==WP_PHASE_GOAL){goal=1;break;} }
    n++; outs+=out; got+=(g==4);
    printf("striker@kick(%5.1f,%4.1f) H=%2d %s: got=%2d out=%d goal=%d ball max x=%5.1f rcv x=%5.1f rcv speed %.1f keeper(%.1f)\n",tod(pk.x),tod(pk.y),H,pol?"stick released":"stick kept   ",g,out,goal,maxx,tod(s.players[4].pos.x),vrec,tod(s.players[5].pos.x));
  }
  printf("TOTAL: %d/%d received, %d out\n",got,n,outs);
}
