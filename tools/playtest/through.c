#include "pt.h"
static SimState s;
/* policy after J release: 0 keep stick (toward striker at press), 1 neutral, 2 +x (toward goal), 3 keep + sprint held, 4 +x sprint */
static const char*PN[]={"keep-stick","release-stick","stick +x","keep+SPRINT","+x SPRINT","tap(keep)"};
static int setup_live(int dribble){
  WpMatchConfig c=sim_default_config(); sim_init(&s,&c);
  for(int t=0;t<30;t++) step1(&s,(WpSeatInput){127,0,0});
  WpSeatInput ps=stick_to(s.players[4].pos,s.players[3].pos,0,1);
  for(int t=0;t<3;t++){ WpSeatInput a=ps; a.buttons=WP_BTN_PASS; step1(&s,a);} 
  for(int t=0;t<200 && !(s.ball.owner==3);t++) step1(&s,ps);
  for(int t=0;t<dribble;t++) step1(&s,(WpSeatInput){127,0,0});
  return s.ball.owner==3 && s.seat_player[0]==3;
}
int main(int argc,char**argv){
  int verbose=argc>1?atoi(argv[1]):-1;
  int drib[]={60,120,150};
  for(int di=0;di<3;di++)
  for(int pol=0;pol<6;pol++)
  for(int H=10; H<=40; H+=6){
    if(pol==5 && H>10) continue;
    if(!setup_live(drib[di])){ printf("setup fail\n"); continue; }
    uint16_t spr=(pol==3||pol==4)?WP_BTN_SPRINT:0;
    WpPlayer*q=&s.players[4];
    WpSeatInput ps=stick_to(s.players[3].pos,q->pos,spr,1);
    int hold= pol==5?3:H;
    for(int t=0;t<hold;t++){ ps=stick_to(s.players[3].pos,q->pos,spr,1); WpSeatInput a=ps; a.buttons|=WP_BTN_PASS; step1(&s,a);} 
    int kt=-1; double sx=0,sy=0, qx0=0,qy0=0,qvx=0,qvy=0, v0=0; int got=-1,t_got=-1,out=0; double minsp=99, run=0, maxsp=0, back=0, vin=0, relin=0, qv_at=0; int slow_ticks=0;
    double ftgap=0; int seat_end=-1;
    for(int t=0;t<400;t++){
      WpSeatInput in=ps;
      if(kt>=0){ if(pol==1) in.move_x=in.move_y=0; if(pol==2||pol==4){in.move_x=127;in.move_y=0;} }
      if(kt<0) in=ps; /* releasing J (buttons w/o pass) */
      uint16_t ev=step1(&s,in);
      if((ev&WP_EV_KICK)&&kt<0){ kt=t; v0=spd(s.ball.vel); qx0=tod(q->pos.x); qy0=tod(q->pos.y); qvx=tod(q->vel.x); qvy=tod(q->vel.y);
         sx=tod(s.ball.pos.x); sy=tod(s.ball.pos.y); }
      if(kt<0) continue;
      double sp=spd(q->vel); run+=sp/60; if(t-kt>5 && sp<minsp && s.ball.owner<0) minsp=sp; if(sp>maxsp)maxsp=sp; if(-tod(q->vel.x)>back) back=-tod(q->vel.x);
      if(s.ball.owner<0 && sp<1.5 && t-kt>5) slow_ticks++;
      if(s.ball.owner<0){ vin=spd(s.ball.vel); relin=spd(fv2_sub(s.ball.vel,q->vel)); qv_at=sp; }
      if(verbose==pol*100+H && ((t-kt)%6==0 || (ev&(WP_EV_BODY|WP_EV_TOUCH)))) printf("   t+%3d ball(%5.1f,%5.1f) v=%4.1f  st(%5.1f,%5.1f) v=(%4.1f,%4.1f) own=%d seat=%d pt=%d ps=%d lt=%d ev=%x\n",t-kt,tod(s.ball.pos.x),tod(s.ball.pos.y),spd(s.ball.vel),tod(q->pos.x),tod(q->pos.y),tod(q->vel.x),tod(q->vel.y),s.ball.owner,s.seat_player[0],s.pass_target,s.pass_stick[0],s.ball.last_touch,ev);
      if(s.ball.owner>=0){ got=s.ball.owner; t_got=t-kt; seat_end=s.seat_player[0];
         for(int k=0;k<20;k++){ step1(&s,in); double dd=bdist(&s,got); if(dd>ftgap)ftgap=dd; } break; }
      if(s.phase==WP_PHASE_OUT){out=1;break;}
      if(t-kt>300) break;
    }
    /* projected runner position at reception if he'd kept his velocity */
    printf("drib%3d %-13s H=%2d v0=%4.1f striker@kick(%5.1f,%5.1f) v=(%4.1f,%4.1f) got=%2d t=%3d(%.2fs) out=%d vin=%4.1f rel=%4.1f rcv_v=%4.1f minV=%4.1f slowT=%3d maxV=%3.1f back=%3.1f run=%4.1f end(%5.1f,%5.1f) ftgap=%.2f\n",
      drib[di],PN[pol],hold,v0,qx0,qy0,qvx,qvy,got,t_got,t_got/60.0,out,vin,relin,qv_at,minsp,slow_ticks,maxsp,back,run,tod(q->pos.x),tod(q->pos.y),ftgap);
    (void)sx;(void)sy;(void)seat_end;
  }
}
